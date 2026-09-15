// InteractableBase.cpp

#include "Core/Interactables/InteractableBase.h"
#include "Components/SphereComponent.h"

AInteractableBase::AInteractableBase()
{
    PrimaryActorTick.bCanEverTick = false;

    // Replicate so the actor exists on all clients; interaction itself is server-authoritative.
    bReplicates = true;

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->InitSphereRadius(150.0f);

    // Query-only trigger: nothing blocks on it, everything can overlap it.
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    InteractionSphere->SetGenerateOverlapEvents(true);

    RootComponent = InteractionSphere;
}

bool AInteractableBase::CanInteract_Implementation(APawn* InstigatorPawn)
{
    // Default contract: anything can interact. Override for gating.
    return true;
}

void AInteractableBase::OnInteract_Implementation(APawn* InstigatorPawn)
{
    // Default: do nothing. Subclasses (C++ or Blueprint) override this.
    // NOTE: The caller (ABismillahSurvivor::Server_Interact_Implementation) only
    // invokes this on the server, so any future replicated state changes here are
    // already on the authority path.
}