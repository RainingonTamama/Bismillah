// BismillahSurvivor.cpp

#include "BismillahSurvivor.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/Interactables/InteractableBase.h"

void ABismillahSurvivor::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;

    UE_LOG(LogTemp, Warning, TEXT("Survivor spawned"));
}

void ABismillahSurvivor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // Keep the base Move/Look/Jump bindings from ABismillahCharacter.
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
    }
}

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

void ABismillahSurvivor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABismillahSurvivor, CurrentHealth);
    DOREPLIFETIME(ABismillahSurvivor, SurvivorState);
}

// ---------------- Interaction ----------------

void ABismillahSurvivor::TryInteract()
{
    UE_LOG(LogTemp, Warning, TEXT("TryInteract: attempted by '%s'"), *GetName());

    // Only the locally-controlled instance should send input to the server.
    if (!IsLocallyControlled())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // --- Proximity scan: sphere overlap centered on the Survivor, filtered to AInteractableBase. ---
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

    // --- Validate *real* physical proximity to each candidate's own trigger sphere. ---
    // We do NOT auto-select the nearest interactable in the level regardless of distance;
    // the Survivor's capsule must actually be inside (or touching) that node's own sphere.
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

    // Client -> Server RPC (same shape as ABismillahKiller::PerformAttack -> Server_PerformAttack).
    Server_Interact(BestInteractable);
}

void ABismillahSurvivor::Server_Interact_Implementation(AInteractableBase* Target)
{
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: target was null"));
        return;
    }

    // Re-validate on the server (client state cannot be trusted).
    if (!Target->CanInteract(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_Interact: '%s' refused interaction on server"), *Target->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Server_Interact: executing OnInteract on '%s' for '%s'"),
        *Target->GetName(), *GetName());

    Target->OnInteract(this);
}