// TRGBag.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Interactables/InteractableBase.h"
#include "TRGBag.generated.h"

class AResourceNode;
class ABismillahSurvivor;
class UStaticMeshComponent;

/**
 * A TRG deposit bag. Survivors carrying a sample hold interact for DepositDuration
 * seconds to deposit it. Server-authoritative; progress replicates for client HUD.
 *
 * No disturbance rolls during deposit — the walk is the risk.
 */
UCLASS()
class BISMILLAH_API ATRGBag : public AInteractableBase
{
    GENERATED_BODY()

public:
    ATRGBag();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    UStaticMeshComponent* MeshComponent;

    /** Total seconds to deposit a sample. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deposit")
    float DepositDuration = 20.0f;

    // ---- Replicated state ---------------------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_BeingUsed, BlueprintReadOnly, Category = "Deposit")
    bool bBeingUsed = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deposit")
    TObjectPtr<APawn> CurrentDepositor = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_DepositProgress, BlueprintReadOnly, Category = "Deposit")
    float DepositProgress = 0.0f;

    // ---- Getters ------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deposit")
    bool IsBeingUsed() const { return bBeingUsed; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deposit")
    APawn* GetCurrentDepositor() const { return CurrentDepositor; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deposit")
    float GetDepositProgress() const { return DepositProgress; }

    // ---- Server API ---------------------------------------------------------

    /** Server-only. Begin depositing the survivor's carried sample. */
    UFUNCTION(BlueprintCallable, Category = "Deposit")
    bool StartDeposit(APawn* Depositor);

    /** Server-only. Stop depositing. bResetProgress=true resets progress to 0. */
    UFUNCTION(BlueprintCallable, Category = "Deposit")
    void StopDeposit(bool bResetProgress = true);

    // ---- Interaction contract overrides ------------------------------------

    virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
    virtual void OnInteract_Implementation(APawn* InstigatorPawn) override;

    // ---- BP hooks -----------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Deposit")
    void OnDepositStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Deposit")
    void OnDepositProgressChanged(float Progress);

    UFUNCTION(BlueprintImplementableEvent, Category = "Deposit")
    void OnDepositCancelled();

    /** Value = research value contributed. SampleID = which sample was deposited. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Deposit")
    void OnDepositCompleted(int32 ResearchValue, FName SampleID);

protected:
    UFUNCTION()
    void OnRep_BeingUsed();

    UFUNCTION()
    void OnRep_DepositProgress();

private:
    void CompleteDeposit(ABismillahSurvivor* Survivor);
};