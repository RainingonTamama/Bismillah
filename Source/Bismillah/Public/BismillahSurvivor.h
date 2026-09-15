// BismillahSurvivor.h

#pragma once

#include "CoreMinimal.h"
#include "BismillahCharacter.h"
#include "BismillahSurvivor.generated.h"

class UInputAction;
class AInteractableBase;

UENUM(BlueprintType)
enum class ESurvivorState : uint8
{
    Healthy         UMETA(DisplayName = "Healthy"),
    Injured         UMETA(DisplayName = "Injured"),
    Downed          UMETA(DisplayName = "Downed"),
    BeingDragged    UMETA(DisplayName = "Being Dragged"),
    Captured        UMETA(DisplayName = "Captured")
};

UCLASS()
class BISMILLAH_API ABismillahSurvivor : public ABismillahCharacter
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

    // NEW: Survivor now overrides SetupPlayerInputComponent to bind InteractAction.
    // Must call Super so the inherited Move/Look/Jump bindings still fire.
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Health")
    float CurrentHealth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "Health")
    void ModifyHealth(float Amount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Health")
    void OnDeath();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_SurvivorState, Category = "State")
    ESurvivorState SurvivorState = ESurvivorState::Healthy;

    UFUNCTION(BlueprintCallable, Category = "State")
    void SetSurvivorState(ESurvivorState NewState);

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ---------------- Interaction (Milestone 1) ----------------

    /** Enhanced Input Action for interaction. Assign IA_Interact on BP_BismillahSurvivor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    /**
     * Radius (cm) of the local proximity scan used to find nearby interactables.
     * The actual accept/reject test is done against the interactable's own
     * InteractionSphere radius (see TryInteract), not against this value alone.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractionCheckRadius = 200.0f;

protected:
    UFUNCTION()
    void OnRep_CurrentHealth();

    UFUNCTION()
    void OnRep_SurvivorState();

    /** Local input entry point: finds a valid interactable and routes to Server_Interact. */
    void TryInteract();

    /** Server-authoritative execution. Mirrors the client->RPC->server pattern used by Killer melee. */
    UFUNCTION(Server, Reliable)
    void Server_Interact(AInteractableBase* Target);
};