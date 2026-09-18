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
    PrimaryActorTick.TickInterval = 0.05f; // 20 Hz server tick for smooth progress

    bReplicates = true;

    // ---- Visual mesh ------------------------------------------------------
    // Root is InteractionSphere (set in AInteractableBase). Mesh is attached to it
    // so the actor's world location stays governed by the sphere.
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(InteractionSphere);

    // Decorative by default: the sphere handles interaction range, the mesh should
    // not interfere with character movement or overlap queries.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetMobility(EComponentMobility::Movable);
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
    DOREPLIFETIME(AResourceNode, bAwaitingMiniGame);
    DOREPLIFETIME(AResourceNode, MiniGameDeadline);
}

float AResourceNode::GetServerWorldTime() const
{
    if (!GetWorld())
    {
        return 0.0f;
    }

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

    // If a mini-game is active on this node for this collector, ignore the press here.
    // The survivor's TryInteract routes presses to the mini-game resolve path instead.
    if (bAwaitingMiniGame && CurrentCollector == InstigatorPawn)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode '%s'::OnInteract: mini-game active for '%s'; ignoring interact."),
            *GetName(), *InstigatorPawn->GetName());
        return;
    }

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
    bAwaitingMiniGame = false;
    MiniGameDeadline = 0.0f;
    LastDisturbanceCheckTime = GetServerWorldTime();

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
    bAwaitingMiniGame = false;
    MiniGameDeadline = 0.0f;

    if (bResetProgress)
    {
        CollectionProgress = 0.0f;
    }

    UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': StopCollection (was collected by '%s')"),
        *GetName(), *GetNameSafe(PreviousCollector));

    OnCollectionProgressChanged(CollectionProgress);
    OnCollectionCancelled();
}

void AResourceNode::ResolveMiniGame()
{
    if (!HasAuthority() || !bBeingCollected || !bAwaitingMiniGame)
    {
        return;
    }

    const float Now = GetServerWorldTime();
    const bool bSuccess = (Now <= MiniGameDeadline);

    // Clear the mini-game state first so any re-entrant call can't double-resolve.
    bAwaitingMiniGame = false;
    MiniGameDeadline = 0.0f;

    if (bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': mini-game SUCCESS. Collection resumes."), *GetName());
        OnMiniGameResolved(true);
    }
    else
    {
        CollectionProgress = FMath::Max(0.0f, CollectionProgress - MiniGameFailurePenalty);

        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode '%s': mini-game FAILED (timed out). Progress reduced by %.0f%%, now %.2f. Broadcasting alert."),
            *GetName(), MiniGameFailurePenalty * 100.0f, CollectionProgress);

        OnMiniGameResolved(false);

        // Broadcast the alert so every client can play sound + VFX at the node location.
        Multicast_BroadcastDisturbanceAlert(GetActorLocation());

        // Also notify the survivor that they failed (server-side, for server-hosted UI).
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

    if (!IsValid(CurrentCollector))
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': collector became invalid, cancelling."), *GetName());
        StopCollection(true);
        return;
    }

    // ---- Mini-game check FIRST: if active, hold progress and check for timeout.
    if (bAwaitingMiniGame)
    {
        if (GetServerWorldTime() > MiniGameDeadline)
        {
            UE_LOG(LogTemp, Warning, TEXT("ResourceNode '%s': mini-game deadline passed with no input."), *GetName());
            ResolveMiniGame(); // computes bSuccess=false because Now > Deadline
        }
        return; // no progress while waiting for input
    }

    // ---- Advance progress.
    const float TotalTime = FMath::Max(CachedSampleData.BaseCollectionTime, 0.01f);
    CollectionProgress = FMath::Clamp(CollectionProgress + (DeltaSeconds / TotalTime), 0.0f, 1.0f);

    OnCollectionProgressChanged(CollectionProgress);

    // ---- Completion check BEFORE the disturbance roll: never fire a mini-game
    //      on the same tick that collection completes.
    if (CollectionProgress >= 1.0f)
    {
        APawn* Collector = CurrentCollector;

        bBeingCollected = false;
        CurrentCollector = nullptr;
        bDepleted = true;

        const float RechargeTime = FMath::Max(CachedSampleData.RechargeTime, 0.01f);
        RechargeEndTime = GetServerWorldTime() + RechargeTime;

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

    // ---- Disturbance roll.
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
                TEXT("ResourceNode '%s': disturbance triggered for '%s' — mini-game window %.2fs, deadline %.2f."),
                *GetName(), *GetNameSafe(CurrentCollector), MiniGameWindowSeconds, MiniGameDeadline);

            OnDisturbanceTriggered();
            // OnDisturbanceTriggered also fires on clients via OnRep_AwaitingMiniGame.
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
    // Fire the disturbance event on clients when the mini-game becomes active.
    if (bAwaitingMiniGame)
    {
        OnDisturbanceTriggered();
    }
}

void AResourceNode::Multicast_BroadcastDisturbanceAlert_Implementation(FVector Location)
{
    // Fires on the server and every client that has this actor replicated.
    // BP child handles the actual sound cue + particle.
    OnDisturbanceAlert(Location);
}