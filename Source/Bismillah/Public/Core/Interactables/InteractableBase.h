// InteractableBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableBase.generated.h"

class USphereComponent;

/**
 * Lightweight shared base for anything a Survivor can interact with.
 * Intentionally contains no gameplay logic — it is a contract only.
 * Future: sample resource nodes, sample bags, killer sabotage points all derive from this.
 */
UCLASS(Blueprintable)
class BISMILLAH_API AInteractableBase : public AActor
{
    GENERATED_BODY()

public:
    AInteractableBase();

    /** Trigger volume that defines "in range" for interaction. Editable per-instance. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    USphereComponent* InteractionSphere;

    /** Convenience accessor used by the Survivor's proximity check. */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

    /**
     * Returns whether the given pawn may interact right now.
     * Default: true. Override in C++ or Blueprint for gating (state, team, items, etc.).
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool CanInteract(APawn* InstigatorPawn);
    virtual bool CanInteract_Implementation(APawn* InstigatorPawn);

    /**
     * Called on the server when a pawn interacts.
     * Default: empty. Subclasses and Blueprint children override to add behavior.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void OnInteract(APawn* InstigatorPawn);
    virtual void OnInteract_Implementation(APawn* InstigatorPawn);
};