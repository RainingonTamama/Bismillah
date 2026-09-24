// ResourceNode.cpp

#include "Core/Interactables/ResourceNode.h"
#include "BismillahSurvivor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AResourceNode::AResourceNode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;

    bReplicates = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(InteractionSphere);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetMobility(EComponentMobility::Movable);
}

void AResourceNode::BeginPlay()
{
    Super::BeginPlay();

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
    DOREPLIFETIME(AResourceNode, bAwaitingMiniGame);
    DOREPLIFETIME(AResourceNode, MiniGameDeadline);
}

float AResourceNode::GetServerWorldTime() const
{
    if (!GetWorld()) return 0.0f;
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
    {
        return GS->GetServerWorldTimeSeconds();
    }
    return GetWorld()->GetTimeSeconds();
}

bool AResourceNode::RefreshSampleData()
{
    bHasValidSampleData = false;
    CachedSampleData = FSampleData();

    if (!SampleDataTable || SampleID.IsNone())
    {
        return false;
    }

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
    return FMath::Max(0.0f, RechargeEndTime - GetServerWorldTime());
}

float AResourceNode::GetMiniGameTimeRemaining() const
{
    if (!bAwaitingMiniGame || MiniGameDeadline <= 0.0f)
    {
        return 0.0f;
    }
    return FMath::Max(0.0f, MiniGameDeadline - GetServerWorldTime());
}

bool AResourceNode::CanInteract_Implementation(APawn* InstigatorPawn)
{
    if (!bHasValidSampleData)
    {
        return false;
    }

    if (bDepleted)
    {
        return false;
    }

    if (bBeingCollected && CurrentCollector != InstigatorPawn)
    {
        return false;
    }

    // Survivors carrying a sample cannot collect another (one per trip).
    if (const ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(InstigatorPawn))
    {
        if (Survivor->IsCarryingSample())
        {
            return false;
        }
    }

    return true;
}

void AResourceNode::OnInteract_Implementation(APawn* InstigatorPawn)
{
    if (!HasAuthority() || !InstigatorPawn) return;
    if (bDepleted) return;
    if (!bHasValidSampleData) return;

    if (bAwaitingMiniGame && CurrentCollector == InstigatorPawn)
    {
        return;
    }

    if (bBeingCollected && CurrentCollector == InstigatorPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s'::OnInteract: '%s' re-pressed, cancelling."),
            *GetName(), *InstigatorPawn->GetName());
        StopCollection(true);
        return;
    }

    if (!CanInteract(InstigatorPawn))
    {
        return;
    }

    StartCollection(InstigatorPawn);
}

bool AResourceNode::StartCollection(APawn* Collector)
{
    if (!HasAuthority() || bDepleted || !Collector || !bHasValidSampleData) return false;
    if (bBeingCollected) return false;

    bBeingCollected = true;
    CurrentCollector = Collector;
    CollectionProgress = 0.0f;
    bAwaitingMiniGame = false;
    MiniGameDeadline = 0.0f;
    LastDisturbanceCheckTime = GetServerWorldTime();

    OnCollectionStarted();
    OnCollectionProgressChanged(CollectionProgress);
    return true;
}

void AResourceNode::StopCollection(bool bResetProgress)
{
    if (!HasAuthority() || !bBeingCollected) return;

    bBeingCollected = false;
    CurrentCollector = nullptr;
    bAwaitingMiniGame = false;
    MiniGameDeadline = 0.0f;

    if (bResetProgress)
    {
        CollectionProgress = 0.0f;
    }

    OnCollectionProgressChanged(CollectionProgress);
    OnCollectionCancelled();
}

void AResourceNode::ResolveMiniGame()
{
    if (!HasAuthority() || !bBeingCollected || !bAwaitingMiniGame) return;

    const float Now = GetServerWorldTime();
    const bool bSuccess = (Now <= MiniGameDeadline);

    bAwaitingMiniGame = false;
    MiniGameDeadline = 0.0f;

    if (bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': mini-game SUCCESS."), *GetName());
        OnMiniGameResolved(true);
    }
    else
    {
        CollectionProgress = FMath::Max(0.0f, CollectionProgress - MiniGameFailurePenalty);

        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode '%s': mini-game FAILED. Progress now %.2f."),
            *GetName(), CollectionProgress);

        OnMiniGameResolved(false);
        Multicast_BroadcastDisturbanceAlert(GetActorLocation());

        if (ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(CurrentCollector))
        {
            Survivor->OnDisturbanceWhileCollecting(this);
        }
    }

    OnCollectionProgressChanged(CollectionProgress);
}

void AResourceNode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !bBeingCollected) return;
    ServerTickCollection(DeltaSeconds);
}

void AResourceNode::ServerTickCollection(float DeltaSeconds)
{
    if (bDepleted)
    {
        StopCollection(false);
        return;
    }

    if (!IsValid(CurrentCollector))
    {
        StopCollection(true);
        return;
    }

    if (bAwaitingMiniGame)
    {
        if (GetServerWorldTime() > MiniGameDeadline)
        {
            ResolveMiniGame();
        }
        return;
    }

    const float TotalTime = FMath::Max(CachedSampleData.BaseCollectionTime, 0.01f);
    CollectionProgress = FMath::Clamp(CollectionProgress + (DeltaSeconds / TotalTime), 0.0f, 1.0f);
    OnCollectionProgressChanged(CollectionProgress);

    if (CollectionProgress >= 1.0f)
    {
        APawn* Collector = CurrentCollector;

        bBeingCollected = false;
        CurrentCollector = nullptr;
        bDepleted = true;

        const float RechargeTime = FMath::Max(CachedSampleData.RechargeTime, 0.01f);
        RechargeEndTime = GetServerWorldTime() + RechargeTime;

        // Give the sample to the collector.
        if (ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(Collector))
        {
            if (!Survivor->GiveSample(CachedSampleData))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("ResourceNode '%s': collector '%s' refused sample (already carrying?)."),
                    *GetName(), *Survivor->GetName());
            }
        }

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
        return;
    }

    const float Now = GetServerWorldTime();
    if (Now - LastDisturbanceCheckTime >= DisturbanceCheckInterval)
    {
        LastDisturbanceCheckTime = Now;

        const float Chance = FMath::Clamp(
            BaseDisturbanceChance * CachedSampleData.InterruptionOddsMultiplier, 0.0f, 1.0f);

        if (FMath::FRand() < Chance)
        {
            bAwaitingMiniGame = true;
            MiniGameDeadline = Now + MiniGameWindowSeconds;

            UE_LOG(LogTemp, Warning,
                TEXT("ResourceNode '%s': disturbance triggered for '%s'."),
                *GetName(), *GetNameSafe(CurrentCollector));

            OnDisturbanceTriggered();
        }
    }
}

void AResourceNode::OnRechargeComplete()
{
    if (!HasAuthority()) return;

    bDepleted = false;
    CollectionProgress = 0.0f;
    RechargeEndTime = 0.0f;
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
        OnCollectionCancelled();
    }
}

void AResourceNode::OnRep_CollectionProgress()
{
    OnCollectionProgressChanged(CollectionProgress);
}

void AResourceNode::OnRep_Depleted()
{
    if (bDepleted)
    {
        OnCollectionCompleted(nullptr);
        OnDepleted();
    }
    else
    {
        OnRecharged();
    }
}

void AResourceNode::OnRep_AwaitingMiniGame()
{
    if (bAwaitingMiniGame)
    {
        OnDisturbanceTriggered();
    }
}

void AResourceNode::Multicast_BroadcastDisturbanceAlert_Implementation(FVector Location)
{
    OnDisturbanceAlert(Location);
}