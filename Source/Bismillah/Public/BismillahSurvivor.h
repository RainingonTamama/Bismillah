// BismillahSurvivor.h

#pragma once

#include "CoreMinimal.h"
#include "BismillahCharacter.h"
#include "Core/Enums/SampleTypes.h"
#include "BismillahSurvivor.generated.h"

class UInputAction;
class AInteractableBase;
class AResourceNode;
class ATRGBag;
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

    // ---- Input --------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractionCheckRadius = 200.0f;

    // ---- Active interaction tracking ----------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_CurrentCollectingNode, BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AResourceNode> CurrentCollectingNode = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentDepositBag, BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<ATRGBag> CurrentDepositBag = nullptr;

    // ---- Carried sample -----------------------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_CarriedSample, BlueprintReadOnly, Category = "Sample")
    FCarriedSample CarriedSample;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Sample")
    bool IsCarryingSample() const { return CarriedSample.IsValid(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Sample")
    FCarriedSample GetCarriedSample() const { return CarriedSample; }

    UFUNCTION(BlueprintCallable, Category = "Sample")
    bool GiveSample(const FSampleData& SampleData);

    UFUNCTION(BlueprintCallable, Category = "Sample")
    void ClearSample();

    // ---- BP events ----------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void OnDisturbanceWhileCollecting(AResourceNode* Node);

    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void OnCollectingNodeChanged(AResourceNode* NewNode);

    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void OnDepositBagChanged(ATRGBag* NewBag);

    // NOTE: FText is passed by const-ref here, matching what UHT generates for
    // BlueprintImplementableEvent parameters of that type. Passing it by value
    // (FText DisplayName) causes a "overloaded member function not found" build error.
    UFUNCTION(BlueprintImplementableEvent, Category = "Sample")
    void OnCarriedSampleChanged(FName SampleID, const FText& DisplayName, int32 ResearchValue);

    // ---- Public server API --------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool CancelActiveInteraction(const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetCurrentCollectingNode(AResourceNode* NewNode);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetCurrentDepositBag(ATRGBag* NewBag);

protected:
    UFUNCTION()
    void OnRep_CurrentHealth();

    UFUNCTION()
    void OnRep_SurvivorState();

    UFUNCTION()
    void OnRep_CurrentCollectingNode();

    UFUNCTION()
    void OnRep_CurrentDepositBag();

    UFUNCTION()
    void OnRep_CarriedSample();

    void TryInteract();

    UFUNCTION(Server, Reliable)
    void Server_Interact(AInteractableBase* Target);

    UFUNCTION(Server, Reliable)
    void Server_NotifyMiniGamePress();

    void OnMoveInputForCollection(const FInputActionValue& Value);

    UFUNCTION(Server, Reliable)
    void Server_CancelActiveInteraction();

private:
    void ServerValidateActiveInteraction();
};