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

// ---------------- Cancellation ----------------

bool ABismillahSurvivor::CancelCollection(const FString& Reason)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!CurrentCollectingNode)
    {
        return false;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("CancelCollection: '%s' cancelling '%s' (reason: %s)"),
        *GetName(), *CurrentCollectingNode->GetName(), *Reason);

    CurrentCollectingNode->StopCollection(true);
    SetCurrentCollectingNode(nullptr);
    return true;
}

void ABismillahSurvivor::Server_CancelCollection_Implementation()
{
    CancelCollection(TEXT("player requested"));
}

// ---------------- Movement cancel ----------------

void ABismillahSurvivor::OnMoveInputForCollection(const FInputActionValue& Value)
{
    if (!CurrentCollectingNode)
    {
        return;
    }

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

// ---------------- Server validation ----------------

void ABismillahSurvivor::ServerValidateCollection()
{
    if (!CurrentCollectingNode)
    {
        return;
    }

    if (!IsValid(CurrentCollectingNode)
        || !CurrentCollectingNode->IsBeingCollected()
        || CurrentCollectingNode->GetCurrentCollector() != this)
    {
        SetCurrentCollectingNode(nullptr);
        return;
    }

    // Movement check.
    const float HorizVelocitySq = GetVelocity().SizeSquared2D();
    if (HorizVelocitySq > FMath::Square(CollectionMovementVelocityThreshold))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ServerValidateCollection: '%s' is moving (v^2=%.1f) while collecting '%s', cancelling."),
            *GetName(), HorizVelocitySq, *CurrentCollectingNode->GetName());

        CancelCollection(TEXT("moved while collecting"));
        return;
    }

    // Distance check.
    const USphereComponent* NodeSphere = CurrentCollectingNode->GetInteractionSphere();
    const float NodeRadius = NodeSphere ? NodeSphere->GetScaledSphereRadius() : 0.0f;
    const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
    const float MaxDist = NodeRadius + CapsuleRadius + 50.0f;

    const float DistSq = FVector::DistSquared(GetActorLocation(), CurrentCollectingNode->GetActorLocation());
    if (DistSq > FMath::Square(MaxDist))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ServerValidateCollection: '%s' moved out of range of '%s' (dist^2=%.0f, max^2=%.0f), cancelling."),
            *GetName(), *CurrentCollectingNode->GetName(), DistSq, FMath::Square(MaxDist));

        CancelCollection(TEXT("moved out of range"));
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
    if (!IsLocallyControlled())
    {
        return;
    }

    // Mini-game routing: if a mini-game is active on the node we're collecting,
    // this press resolves it instead of interacting.
    if (CurrentCollectingNode && CurrentCollectingNode->IsAwaitingMiniGame())
    {
        UE_LOG(LogTemp, Warning, TEXT("TryInteract: routing to Server_NotifyMiniGamePress (mini-game active)."));
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

    UE_LOG(LogTemp, Warning, TEXT("TryInteract: found interactable '%s'"), *BestInteractable->GetName());

    if (!BestInteractable->CanInteract(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryInteract: '%s' refused interaction (CanInteract=false)"), *BestInteractable->GetName());
        return;
    }

    Server_Interact(BestInteractable);
}

void ABismillahSurvivor::Server_NotifyMiniGamePress_Implementation()
{
    if (!CurrentCollectingNode)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_NotifyMiniGamePress: no CurrentCollectingNode."));
        return;
    }

    if (!CurrentCollectingNode->IsBeingCollected() || CurrentCollectingNode->GetCurrentCollector() != this)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_NotifyMiniGamePress: we are not the collector of '%s'."),
            *CurrentCollectingNode->GetName());
        return;
    }

    if (!CurrentCollectingNode->IsAwaitingMiniGame())
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_NotifyMiniGamePress: node '%s' is not awaiting a mini-game."),
            *CurrentCollectingNode->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Server_NotifyMiniGamePress: forwarding to node '%s' for resolution."),
        *CurrentCollectingNode->GetName());

    CurrentCollectingNode->ResolveMiniGame();
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

    if (CurrentCollectingNode && CurrentCollectingNode != Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: switching from '%s' to '%s'"),
            *CurrentCollectingNode->GetName(), *Target->GetName());

        CancelCollection(TEXT("switched to a different interactable"));
    }

    UE_LOG(LogTemp, Warning, TEXT("Server_Interact: executing OnInteract on '%s' for '%s'"),
        *Target->GetName(), *GetName());

    Target->OnInteract(this);

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
}