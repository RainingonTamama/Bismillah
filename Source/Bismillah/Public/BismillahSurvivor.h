// BismillahSurvivor.h

#pragma once

#include "CoreMinimal.h"
#include "BismillahCharacter.h"
#include "BismillahSurvivor.generated.h"

class UInputAction;
class AInteractableBase;
class AResourceNode;
struct FInputActionValue;

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

public:
    ABismillahSurvivor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // ---- Health / State -----------------------------------------------------

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

    // ---- Interaction input --------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractionCheckRadius = 200.0f;

    // ---- Collection tracking ------------------------------------------------

    /**
     * The node this survivor is currently collecting, or null.
     * Replicated so client UI can bind a progress bar to that specific node.
     *
     * DO NOT assign this directly from C++. Always call SetCurrentCollectingNode()
     * so the OnCollectingNodeChanged event fires correctly on BOTH the authority
     * (listen server) and clients. The authority does not receive OnRep callbacks.
     */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentCollectingNode, BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AResourceNode> CurrentCollectingNode = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void OnDisturbanceWhileCollecting(AResourceNode* Node);

    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void OnCollectingNodeChanged(AResourceNode* NewNode);

protected:
    UFUNCTION()
    void OnRep_CurrentHealth();

    UFUNCTION()
    void OnRep_SurvivorState();

    UFUNCTION()
    void OnRep_CurrentCollectingNode();

    void TryInteract();

    UFUNCTION(Server, Reliable)
    void Server_Interact(AInteractableBase* Target);

    /**
     * Sent when the player presses Interact while a mini-game is active on the
     * node they're collecting. Server validates timing internally.
     */
    UFUNCTION(Server, Reliable)
    void Server_NotifyMiniGamePress();

    void OnMoveInputForCollection(const FInputActionValue& Value);

    UFUNCTION(Server, Reliable)
    void Server_CancelCollection();

private:
    void SetCurrentCollectingNode(AResourceNode* NewNode);
    void ServerValidateCollection();
};