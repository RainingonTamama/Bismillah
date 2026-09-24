// BismillahSurvivor.cpp

#include "BismillahSurvivor.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Interactables/InteractableBase.h"
#include "Core/Interactables/ResourceNode.h"
#include "Core/Interactables/TRGBag.h"

static constexpr float ActiveInteractionMovementVelocityThreshold = 20.0f;

ABismillahSurvivor::ABismillahSurvivor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABismillahSurvivor::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth = MaxHealth;
    UE_LOG(LogTemp, Warning, TEXT("Survivor spawned"));
}

void ABismillahSurvivor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABismillahSurvivor, CurrentHealth);
    DOREPLIFETIME(ABismillahSurvivor, SurvivorState);
    DOREPLIFETIME(ABismillahSurvivor, CurrentCollectingNode);
    DOREPLIFETIME(ABismillahSurvivor, CurrentDepositBag);
    DOREPLIFETIME(ABismillahSurvivor, CarriedSample);
}

void ABismillahSurvivor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (InteractAction)
        {
            EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ABismillahSurvivor::TryInteract);
        }
        if (MoveAction)
        {
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABismillahSurvivor::OnMoveInputForCollection);
        }
    }
}

void ABismillahSurvivor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        return;
    }

    ServerValidateActiveInteraction();
}

// ---------------- Setters / OnRep ----------------

void ABismillahSurvivor::SetCurrentCollectingNode(AResourceNode* NewNode)
{
    if (CurrentCollectingNode == NewNode)
    {
        return;
    }

    CurrentCollectingNode = NewNode;

    if (HasAuthority())
    {
        OnCollectingNodeChanged(CurrentCollectingNode);
    }
}

void ABismillahSurvivor::SetCurrentDepositBag(ATRGBag* NewBag)
{
    if (CurrentDepositBag == NewBag)
    {
        return;
    }

    CurrentDepositBag = NewBag;

    if (HasAuthority())
    {
        OnDepositBagChanged(CurrentDepositBag);
    }
}

void ABismillahSurvivor::OnRep_CurrentCollectingNode()
{
    OnCollectingNodeChanged(CurrentCollectingNode);
}

void ABismillahSurvivor::OnRep_CurrentDepositBag()
{
    OnDepositBagChanged(CurrentDepositBag);
}

void ABismillahSurvivor::OnRep_CarriedSample()
{
    OnCarriedSampleChanged(CarriedSample.SampleID, CarriedSample.DisplayName, CarriedSample.ResearchValue);
}

// ---------------- Carried sample ----------------

bool ABismillahSurvivor::GiveSample(const FSampleData& SampleData)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (CarriedSample.IsValid())
    {
        return false;
    }

    if (SampleData.SampleID.IsNone())
    {
        return false;
    }

    CarriedSample.SampleID = SampleData.SampleID;
    CarriedSample.DisplayName = SampleData.DisplayName;
    CarriedSample.ResearchValue = SampleData.ResearchValue;

    UE_LOG(LogTemp, Warning,
        TEXT("Survivor '%s': now carrying sample '%s' (value=%d)"),
        *GetName(), *CarriedSample.SampleID.ToString(), CarriedSample.ResearchValue);

    if (HasAuthority())
    {
        OnCarriedSampleChanged(CarriedSample.SampleID, CarriedSample.DisplayName, CarriedSample.ResearchValue);
    }

    return true;
}

void ABismillahSurvivor::ClearSample()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!CarriedSample.IsValid())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Survivor '%s': cleared carried sample '%s'"),
        *GetName(), *CarriedSample.SampleID.ToString());

    CarriedSample.Reset();

    OnCarriedSampleChanged(CarriedSample.SampleID, CarriedSample.DisplayName, CarriedSample.ResearchValue);
}

// ---------------- Cancel / validation ----------------

bool ABismillahSurvivor::CancelActiveInteraction(const FString& Reason)
{
    if (!HasAuthority())
    {
        return false;
    }

    bool bCancelled = false;

    if (CurrentCollectingNode)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CancelActiveInteraction: '%s' cancelling collection of '%s' (reason: %s)"),
            *GetName(), *CurrentCollectingNode->GetName(), *Reason);

        CurrentCollectingNode->StopCollection(true);
        SetCurrentCollectingNode(nullptr);
        bCancelled = true;
    }

    if (CurrentDepositBag)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CancelActiveInteraction: '%s' cancelling deposit at '%s' (reason: %s)"),
            *GetName(), *CurrentDepositBag->GetName(), *Reason);

        CurrentDepositBag->StopDeposit(true);
        SetCurrentDepositBag(nullptr);
        bCancelled = true;
    }

    return bCancelled;
}

void ABismillahSurvivor::Server_CancelActiveInteraction_Implementation()
{
    CancelActiveInteraction(TEXT("player requested"));
}

void ABismillahSurvivor::OnMoveInputForCollection(const FInputActionValue& Value)
{
    if (!CurrentCollectingNode && !CurrentDepositBag)
    {
        return;
    }

    const FVector2D Axis = Value.Get<FVector2D>();
    if (Axis.IsNearlyZero())
    {
        return;
    }

    Server_CancelActiveInteraction();
}

void ABismillahSurvivor::ServerValidateActiveInteraction()
{
    // Guard against invalid/missing node or bag first.
    if (CurrentCollectingNode)
    {
        if (!IsValid(CurrentCollectingNode)
            || !CurrentCollectingNode->IsBeingCollected()
            || CurrentCollectingNode->GetCurrentCollector() != this)
        {
            SetCurrentCollectingNode(nullptr);
        }
    }

    if (CurrentDepositBag)
    {
        if (!IsValid(CurrentDepositBag)
            || !CurrentDepositBag->IsBeingUsed()
            || CurrentDepositBag->GetCurrentDepositor() != this)
        {
            SetCurrentDepositBag(nullptr);
        }
    }

    if (!CurrentCollectingNode && !CurrentDepositBag)
    {
        return;
    }

    // Movement check.
    const float HorizVelocitySq = GetVelocity().SizeSquared2D();
    if (HorizVelocitySq > FMath::Square(ActiveInteractionMovementVelocityThreshold))
    {
        CancelActiveInteraction(TEXT("moved during interaction"));
        return;
    }

    // Distance check against the active target's sphere.
    USphereComponent* TargetSphere = nullptr;
    AActor* TargetActor = nullptr;

    if (CurrentCollectingNode)
    {
        TargetSphere = CurrentCollectingNode->GetInteractionSphere();
        TargetActor = CurrentCollectingNode;
    }
    else if (CurrentDepositBag)
    {
        TargetSphere = CurrentDepositBag->GetInteractionSphere();
        TargetActor = CurrentDepositBag;
    }

    if (!TargetSphere || !TargetActor)
    {
        return;
    }

    const float TargetRadius = TargetSphere->GetScaledSphereRadius();
    const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
    const float MaxDist = TargetRadius + CapsuleRadius + 50.0f;

    const float DistSq = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());
    if (DistSq > FMath::Square(MaxDist))
    {
        CancelActiveInteraction(TEXT("moved out of range"));
    }
}

// ---------------- Health / State ----------------

void ABismillahSurvivor::ModifyHealth(float Amount)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);

    if (CurrentHealth <= 0.0f)
    {
        OnDeath();
    }
}

float ABismillahSurvivor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (!HasAuthority())
    {
        return 0.0f;
    }

    ModifyHealth(-DamageAmount);
    return DamageAmount;
}

void ABismillahSurvivor::OnRep_CurrentHealth()
{
}

void ABismillahSurvivor::SetSurvivorState(ESurvivorState NewState)
{
    if (!HasAuthority())
    {
        return;
    }

    if (SurvivorState == NewState)
    {
        return;
    }

    SurvivorState = NewState;

    // Entering a state that prevents carrying destroys the sample.
    if (NewState == ESurvivorState::Downed ||
        NewState == ESurvivorState::BeingDragged ||
        NewState == ESurvivorState::Captured)
    {
        if (CarriedSample.IsValid())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Survivor '%s': entered state %s while carrying '%s' — sample destroyed."),
                *GetName(), *UEnum::GetValueAsString(NewState), *CarriedSample.SampleID.ToString());
            ClearSample();
        }

        // Also cancel any active interaction (collection or deposit).
        CancelActiveInteraction(TEXT("state changed to non-collecting"));
    }
}

void ABismillahSurvivor::OnRep_SurvivorState()
{
}

// ---------------- Interaction ----------------

void ABismillahSurvivor::TryInteract()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (CurrentCollectingNode && CurrentCollectingNode->IsAwaitingMiniGame())
    {
        Server_NotifyMiniGamePress();
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("TryInteract: attempted by '%s'"), *GetName());

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<AActor*> OverlappingActors;
    UKismetSystemLibrary::SphereOverlapActors(
        this,
        GetActorLocation(),
        InteractionCheckRadius,
        ObjectTypes,
        AInteractableBase::StaticClass(),
        ActorsToIgnore,
        OverlappingActors
    );

    const FVector SurvivorLoc = GetActorLocation();
    const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;

    AInteractableBase* BestInteractable = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();

    for (AActor* Actor : OverlappingActors)
    {
        AInteractableBase* Interactable = Cast<AInteractableBase>(Actor);
        if (!Interactable)
        {
            continue;
        }

        USphereComponent* TheirSphere = Interactable->GetInteractionSphere();
        if (!TheirSphere)
        {
            continue;
        }

        const float DistSq = FVector::DistSquared(SurvivorLoc, TheirSphere->GetComponentLocation());
        const float Reach = TheirSphere->GetScaledSphereRadius() + CapsuleRadius;

        if (DistSq <= FMath::Square(Reach) && DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestInteractable = Interactable;
        }
    }

    if (!BestInteractable)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryInteract: no interactable in range"));
        return;
    }

    if (!BestInteractable->CanInteract(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryInteract: '%s' refused interaction (CanInteract=false)"), *BestInteractable->GetName());
        return;
    }

    Server_Interact(BestInteractable);
}

void ABismillahSurvivor::Server_NotifyMiniGamePress_Implementation()
{
    if (!CurrentCollectingNode || !CurrentCollectingNode->IsAwaitingMiniGame())
    {
        return;
    }
    CurrentCollectingNode->ResolveMiniGame();
}

void ABismillahSurvivor::Server_Interact_Implementation(AInteractableBase* Target)
{
    if (!Target)
    {
        return;
    }

    if (!Target->CanInteract(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: '%s' refused interaction on server"), *Target->GetName());
        return;
    }

    // Switching between interactions: cancel whichever is active if it's not this target.
    if (CurrentCollectingNode && CurrentCollectingNode != Target)
    {
        CancelActiveInteraction(TEXT("switched to a different interactable"));
    }
    if (CurrentDepositBag && CurrentDepositBag != Target)
    {
        CancelActiveInteraction(TEXT("switched to a different interactable"));
    }

    Target->OnInteract(this);

    // Sync tracking pointers based on post-OnInteract state.
    if (AResourceNode* Node = Cast<AResourceNode>(Target))
    {
        if (Node->IsBeingCollected() && Node->GetCurrentCollector() == this)
        {
            SetCurrentCollectingNode(Node);
            if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
            {
                MoveComp->StopMovementImmediately();
            }
        }
        else
        {
            SetCurrentCollectingNode(nullptr);
        }
    }
    else if (ATRGBag* Bag = Cast<ATRGBag>(Target))
    {
        if (Bag->IsBeingUsed() && Bag->GetCurrentDepositor() == this)
        {
            SetCurrentDepositBag(Bag);
            if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
            {
                MoveComp->StopMovementImmediately();
            }
        }
        else
        {
            SetCurrentDepositBag(nullptr);
        }
    }
}