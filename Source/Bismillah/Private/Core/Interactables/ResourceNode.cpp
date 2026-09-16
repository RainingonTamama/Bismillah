// ResourceNode.cpp

#include "Core/Interactables/ResourceNode.h"
#include "BismillahSurvivor.h"
#include "Components/SphereComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AResourceNode::AResourceNode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f; // 20 Hz server tick for smooth progress

    bReplicates = true;
}

void AResourceNode::BeginPlay()
{
    Super::BeginPlay();

    // Only the server needs to tick the collection timer.
    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
    }

    RefreshSampleData();

    if (!bHasValidSampleData)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode '%s': BeginPlay could not resolve FSampleData (SampleID='%s', SampleDataTable=%s)."),
            *GetName(),
            *SampleID.ToString(),
            SampleDataTable ? *SampleDataTable->GetName() : TEXT("<null>"));
    }
}

void AResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AResourceNode, bBeingCollected);
    DOREPLIFETIME(AResourceNode, CurrentCollector);
    DOREPLIFETIME(AResourceNode, CollectionProgress);
    DOREPLIFETIME(AResourceNode, bDepleted);
    DOREPLIFETIME(AResourceNode, RechargeEndTime);
}

bool AResourceNode::RefreshSampleData()
{
    bHasValidSampleData = false;
    CachedSampleData = FSampleData();

    if (!SampleDataTable || SampleID.IsNone())
    {
        return false;
    }

    // DataTable::FindRow looks up by *row name*, not by the SampleID field.
    // We treat the SampleID field as the identity (row name stays cosmetic), so we
    // iterate rows and match on the field. This is the ONLY place that walks the table.
    static const FString ContextString(TEXT("AResourceNode::RefreshSampleData"));

    TArray<FSampleData*> AllRows;
    SampleDataTable->GetAllRows<FSampleData>(ContextString, AllRows);

    for (const FSampleData* Row : AllRows)
    {
        if (Row && Row->SampleID == SampleID)
        {
            CachedSampleData = *Row;
            bHasValidSampleData = true;
            return true;
        }
    }

    return false;
}

bool AResourceNode::GetSampleData(FSampleData& OutSampleData) const
{
    if (!bHasValidSampleData)
    {
        return false;
    }

    OutSampleData = CachedSampleData;
    return true;
}

float AResourceNode::GetRechargeTimeRemaining() const
{
    if (!bDepleted || RechargeEndTime <= 0.0f || !GetWorld())
    {
        return 0.0f;
    }

    // Use server world time when available; fall back to local world time.
    float Now = GetWorld()->GetTimeSeconds();
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
    {
        Now = GS->GetServerWorldTimeSeconds();
    }

    return FMath::Max(0.0f, RechargeEndTime - Now);
}

bool AResourceNode::CanInteract_Implementation(APawn* InstigatorPawn)
{
    // A node whose data couldn't resolve has no duration, so it cannot be collected.
    if (!bHasValidSampleData)
    {
        return false;
    }

    if (bDepleted)
    {
        return false;
    }

    // If someone else is collecting, refuse. The current collector may re-trigger
    // (interpreted as cancel in OnInteract).
    if (bBeingCollected && CurrentCollector != InstigatorPawn)
    {
        return false;
    }

    return true;
}

void AResourceNode::OnInteract_Implementation(APawn* InstigatorPawn)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!InstigatorPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s'::OnInteract: null instigator pawn."), *GetName());
        return;
    }

    if (bDepleted)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s'::OnInteract: already depleted."), *GetName());
        return;
    }

    if (!bHasValidSampleData)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s'::OnInteract: no valid sample data."), *GetName());
        return;
    }

    // Same collector pressing again => cancel.
    if (bBeingCollected && CurrentCollector == InstigatorPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s'::OnInteract: '%s' re-pressed, cancelling collection."),
            *GetName(), *InstigatorPawn->GetName());
        StopCollection(true);
        return;
    }

    if (!CanInteract(InstigatorPawn))
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s'::OnInteract: CanInteract refused for '%s'."),
            *GetName(), *InstigatorPawn->GetName());
        return;
    }

    StartCollection(InstigatorPawn);
}

bool AResourceNode::StartCollection(APawn* Collector)
{
    if (!HasAuthority() || bDepleted || !Collector || !bHasValidSampleData)
    {
        return false;
    }

    if (bBeingCollected)
    {
        return false;
    }

    bBeingCollected = true;
    CurrentCollector = Collector;
    CollectionProgress = 0.0f;
    LastDisturbanceCheckTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    UE_LOG(LogTemp, Warning,
        TEXT("ResourceNode '%s': StartCollection by '%s' (BaseCollectionTime=%.2f, RechargeTime=%.2f, InterruptionOddsMultiplier=%.2f)"),
        *GetName(),
        *Collector->GetName(),
        CachedSampleData.BaseCollectionTime,
        CachedSampleData.RechargeTime,
        CachedSampleData.InterruptionOddsMultiplier);

    OnCollectionStarted();
    OnCollectionProgressChanged(CollectionProgress);

    return true;
}

void AResourceNode::StopCollection(bool bResetProgress)
{
    if (!HasAuthority() || !bBeingCollected)
    {
        return;
    }

    APawn* PreviousCollector = CurrentCollector;

    bBeingCollected = false;
    CurrentCollector = nullptr;
    if (bResetProgress)
    {
        CollectionProgress = 0.0f;
    }

    UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': StopCollection (was collected by '%s')"),
        *GetName(), *GetNameSafe(PreviousCollector));

    OnCollectionProgressChanged(CollectionProgress);
    OnCollectionCancelled();
}

void AResourceNode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Only the server advances collection; clients only receive replicated state.
    if (!HasAuthority() || !bBeingCollected)
    {
        return;
    }

    ServerTickCollection(DeltaSeconds);
}

void AResourceNode::ServerTickCollection(float DeltaSeconds)
{
    if (bDepleted)
    {
        StopCollection(false);
        return;
    }

    // Safety: collector gone (destroyed, disconnected).
    if (!IsValid(CurrentCollector))
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': collector became invalid, cancelling."), *GetName());
        StopCollection(true);
        return;
    }

    // Advance progress.
    const float TotalTime = FMath::Max(CachedSampleData.BaseCollectionTime, 0.01f);
    CollectionProgress = FMath::Clamp(CollectionProgress + (DeltaSeconds / TotalTime), 0.0f, 1.0f);

    OnCollectionProgressChanged(CollectionProgress);

    // Disturbance roll.
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (Now - LastDisturbanceCheckTime >= DisturbanceCheckInterval)
    {
        LastDisturbanceCheckTime = Now;

        const float Chance = FMath::Clamp(
            BaseDisturbanceChance * CachedSampleData.InterruptionOddsMultiplier, 0.0f, 1.0f);

        if (FMath::FRand() < Chance)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ResourceNode '%s': disturbance triggered for '%s' (chance=%.3f). Mini-game not yet implemented."),
                *GetName(), *GetNameSafe(CurrentCollector), Chance);

            OnDisturbanceTriggered();

            if (ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(CurrentCollector))
            {
                Survivor->OnDisturbanceWhileCollecting(this);
            }
        }
    }

    // Completed?
    if (CollectionProgress >= 1.0f)
    {
        APawn* Collector = CurrentCollector;

        bBeingCollected = false;
        CurrentCollector = nullptr;
        bDepleted = true;

        // Set up recharge.
        const float RechargeTime = FMath::Max(CachedSampleData.RechargeTime, 0.01f);
        const float ServerNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        RechargeEndTime = ServerNow + RechargeTime;

        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode '%s': collection COMPLETE by '%s'. Recharging for %.2fs."),
            *GetName(), *GetNameSafe(Collector), RechargeTime);

        OnCollectionCompleted(Collector);
        OnDepleted();

        if (GetWorld())
        {
            GetWorldTimerManager().SetTimer(
                RechargeTimerHandle, this, &AResourceNode::OnRechargeComplete, RechargeTime, false);
        }
    }
}

void AResourceNode::OnRechargeComplete()
{
    if (!HasAuthority())
    {
        return;
    }

    bDepleted = false;
    CollectionProgress = 0.0f;
    RechargeEndTime = 0.0f;

    UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': recharged, collectable again."), *GetName());

    OnRecharged();
}

void AResourceNode::OnRep_BeingCollected()
{
    if (bBeingCollected)
    {
        OnCollectionStarted();
        OnCollectionProgressChanged(CollectionProgress);
    }
    else if (!bDepleted)
    {
        // Stopped without going depleted => cancel.
        OnCollectionCancelled();
    }
    // If bDepleted is true, OnRep_Depleted handles the completion signal.
}

void AResourceNode::OnRep_CollectionProgress()
{
    OnCollectionProgressChanged(CollectionProgress);
}

void AResourceNode::OnRep_Depleted()
{
    if (bDepleted)
    {
        // Collector info is not guaranteed on the client in this exact frame.
        OnCollectionCompleted(nullptr);
        OnDepleted();
    }
    else
    {
        OnRecharged();
    }
}