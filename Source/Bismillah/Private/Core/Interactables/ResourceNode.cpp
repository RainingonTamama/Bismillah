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
    // Root is InteractionSphere (set in AInteractableBase). Attach mesh to it so
    // the actor's world location stays governed by the sphere, and designers can
    // offset the mesh freely in the BP without moving the interaction volume.
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(InteractionSphere);

    // Decorative by default: the sphere handles interaction range, the mesh should
    // not interfere with character movement or overlap queries.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetMobility(EComponentMobility::Movable);

    // No mesh asset assigned here on purpose. Designers assign per-BP. For the
    // placeholder pass, assign /Engine/BasicShapes/Sphere on BP_ResourceNode_Rock.

    // -----------------------------------------------------------------------
    // UPGRADE PATH (polishing milestone): to switch to a skeletal mesh later:
    //   1. In ResourceNode.h:
    //        class UStaticMeshComponent;   ->  class USkeletalMeshComponent;
    //        UStaticMeshComponent* MeshComponent; -> USkeletalMeshComponent* MeshComponent;
    //        UStaticMeshComponent* GetMeshComponent() const ... -> USkeletalMeshComponent*
    //   2. In ResourceNode.cpp:
    //        #include "Components/StaticMeshComponent.h"
    //          -> #include "Components/SkeletalMeshComponent.h"
    //        CreateDefaultSubobject<UStaticMeshComponent>(...) -> USkeletalMeshComponent
    //   3. On BP_ResourceNode_Rock, assign a Skeletal Mesh + Anim Class.
    // Everything else (state events, replication) is unchanged.
    // -----------------------------------------------------------------------
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

    float Now = GetWorld()->GetTimeSeconds();
    if (const AGameStateBase* GS = GetWorld()->GetGameState())
    {
        Now = GS->GetServerWorldTimeSeconds();
    }

    return FMath::Max(0.0f, RechargeEndTime - Now);
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

    const float TotalTime = FMath::Max(CachedSampleData.BaseCollectionTime, 0.01f);
    CollectionProgress = FMath::Clamp(CollectionProgress + (DeltaSeconds / TotalTime), 0.0f, 1.0f);

    OnCollectionProgressChanged(CollectionProgress);

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

    if (CollectionProgress >= 1.0f)
    {
        APawn* Collector = CurrentCollector;

        bBeingCollected = false;
        CurrentCollector = nullptr;
        bDepleted = true;

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