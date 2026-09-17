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

/** Horizontal velocity above which the server treats the survivor as "moving" during a collection. */
static constexpr float CollectionMovementVelocityThreshold = 20.0f;

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
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ABismillahSurvivor: InteractAction is not assigned (set it on BP_BismillahSurvivor)."));
        }

        // Second binding on MoveAction: fires whenever the player provides movement input.
        // The base character's DoMove still fires (movement is not blocked here — we cancel
        // the collection instead, which is what "if they move, cancel" requires).
        if (MoveAction)
        {
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABismillahSurvivor::OnMoveInputForCollection);
        }
    }
}

void ABismillahSurvivor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Only the server decides whether a collection is still valid.
    if (!HasAuthority())
    {
        return;
    }

    ServerValidateCollection();
}

// ---------------- Collection node tracking ----------------

void ABismillahSurvivor::SetCurrentCollectingNode(AResourceNode* NewNode)
{
    if (CurrentCollectingNode == NewNode)
    {
        return;
    }

    CurrentCollectingNode = NewNode;

    // OnRep only fires on clients. On the authority (listen server) we must fire
    // the event explicitly, otherwise the host's own player never gets notified.
    if (HasAuthority())
    {
        OnCollectingNodeChanged(CurrentCollectingNode);
    }
}

void ABismillahSurvivor::OnRep_CurrentCollectingNode()
{
    OnCollectingNodeChanged(CurrentCollectingNode);
}

// ---------------- Movement cancel ----------------

void ABismillahSurvivor::OnMoveInputForCollection(const FInputActionValue& Value)
{
    // We only need to react when a collection is active.
    if (!CurrentCollectingNode)
    {
        return;
    }

    // Ignore zero-input frames (input can fire while keys are held, and on release).
    const FVector2D Axis = Value.Get<FVector2D>();
    if (Axis.IsNearlyZero())
    {
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("OnMoveInputForCollection: movement input detected for '%s' during collection of '%s', requesting cancel."),
        *GetName(), *CurrentCollectingNode->GetName());

    Server_CancelCollection();
}

void ABismillahSurvivor::Server_CancelCollection_Implementation()
{
    if (!CurrentCollectingNode)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Server_CancelCollection: cancelling collection of '%s' for '%s'."),
        *CurrentCollectingNode->GetName(), *GetName());

    CurrentCollectingNode->StopCollection(true);
    SetCurrentCollectingNode(nullptr);
}

// ---------------- Server validation ----------------

void ABismillahSurvivor::ServerValidateCollection()
{
    if (!CurrentCollectingNode)
    {
        return;
    }

    // If the node stopped collecting for any reason, clear our pointer.
    if (!IsValid(CurrentCollectingNode)
        || !CurrentCollectingNode->IsBeingCollected()
        || CurrentCollectingNode->GetCurrentCollector() != this)
    {
        SetCurrentCollectingNode(nullptr);
        return;
    }

    // ---- Movement check -----------------------------------------------------
    // Even though the client sends Server_CancelCollection on movement input, we also
    // enforce it server-side so a silent client cannot keep moving and collecting.
    const float HorizVelocitySq = GetVelocity().SizeSquared2D();
    if (HorizVelocitySq > FMath::Square(CollectionMovementVelocityThreshold))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ServerValidateCollection: '%s' is moving (v^2=%.1f) while collecting '%s', cancelling."),
            *GetName(), HorizVelocitySq, *CurrentCollectingNode->GetName());

        CurrentCollectingNode->StopCollection(true);
        SetCurrentCollectingNode(nullptr);
        return;
    }

    // ---- Distance check -----------------------------------------------------
    // Real physical proximity against the node's own trigger sphere, same rule as
    // Milestone 1's start check. Never "nearest node in the level".
    const USphereComponent* NodeSphere = CurrentCollectingNode->GetInteractionSphere();
    const float NodeRadius = NodeSphere ? NodeSphere->GetScaledSphereRadius() : 0.0f;
    const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
    const float MaxDist = NodeRadius + CapsuleRadius + 50.0f; // 50cm forgiveness

    const float DistSq = FVector::DistSquared(GetActorLocation(), CurrentCollectingNode->GetActorLocation());
    if (DistSq > FMath::Square(MaxDist))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ServerValidateCollection: '%s' moved out of range of '%s' (dist^2=%.0f, max^2=%.0f), cancelling."),
            *GetName(), *CurrentCollectingNode->GetName(), DistSq, FMath::Square(MaxDist));

        CurrentCollectingNode->StopCollection(true);
        SetCurrentCollectingNode(nullptr);
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
}

void ABismillahSurvivor::OnRep_SurvivorState()
{
}

// ---------------- Interaction ----------------

void ABismillahSurvivor::TryInteract()
{
    UE_LOG(LogTemp, Warning, TEXT("TryInteract: attempted by '%s'"), *GetName());

    if (!IsLocallyControlled())
    {
        return;
    }

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

    UE_LOG(LogTemp, Warning, TEXT("TryInteract: found interactable '%s'"), *BestInteractable->GetName());

    if (!BestInteractable->CanInteract(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryInteract: '%s' refused interaction (CanInteract=false)"), *BestInteractable->GetName());
        return;
    }

    Server_Interact(BestInteractable);
}

void ABismillahSurvivor::Server_Interact_Implementation(AInteractableBase* Target)
{
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: target was null"));
        return;
    }

    if (!Target->CanInteract(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: '%s' refused interaction on server"), *Target->GetName());
        return;
    }

    // If we're currently collecting a *different* node, stop that one first.
    if (CurrentCollectingNode && CurrentCollectingNode != Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: switching from '%s' to '%s'"),
            *CurrentCollectingNode->GetName(), *Target->GetName());

        CurrentCollectingNode->StopCollection(true);
        SetCurrentCollectingNode(nullptr);
    }

    UE_LOG(LogTemp, Warning, TEXT("Server_Interact: executing OnInteract on '%s' for '%s'"),
        *Target->GetName(), *GetName());

    Target->OnInteract(this);

    // Sync our tracking pointer with the node's actual state after OnInteract ran.
    if (AResourceNode* Node = Cast<AResourceNode>(Target))
    {
        if (Node->IsBeingCollected() && Node->GetCurrentCollector() == this)
        {
            SetCurrentCollectingNode(Node);

            // Zero residual velocity so the survivor is cleanly stationary on start.
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
}