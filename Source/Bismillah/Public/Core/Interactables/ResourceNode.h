// ResourceNode.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Interactables/InteractableBase.h"
#include "Core/Enums/SampleTypes.h"
#include "ResourceNode.generated.h"

class UDataTable;
class APawn;

/**
 * A collectible sample node.
 * Milestone 2.1: server-authoritative collection timer, replicated progress,
 * movement-cancel support, and depleted/recharge cycle.
 *
 * PERFORMANCE / MULTIPLAYER NOTES
 * --------------------------------
 * - FSampleData resolved ONCE at BeginPlay and cached in CachedSampleData.
 * - Tick runs only on the server (disabled on clients in BeginPlay).
 * - Recharge is implemented via FTimerManager, not Tick. It does not consume tick budget.
 * - All collection/recharge state mutated only under HasAuthority().
 *   Clients react via OnRep_*.
 */
UCLASS()
class BISMILLAH_API AResourceNode : public AInteractableBase
{
    GENERATED_BODY()

public:
    AResourceNode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ---- Design-time config -------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FName SampleID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    UDataTable* SampleDataTable;

    /** How often the disturbance chance is rolled while collecting, in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collection")
    float DisturbanceCheckInterval = 0.5f;

    /** Base chance per roll. Final = BaseDisturbanceChance * InterruptionOddsMultiplier. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collection")
    float BaseDisturbanceChance = 0.1f;

    // ---- Cached sample data (transient, per-machine) ------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Sample")
    FSampleData CachedSampleData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Sample")
    bool bHasValidSampleData = false;

    // ---- Replicated collection state ---------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_BeingCollected, BlueprintReadOnly, Category = "Collection")
    bool bBeingCollected = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Collection")
    TObjectPtr<APawn> CurrentCollector = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_CollectionProgress, BlueprintReadOnly, Category = "Collection")
    float CollectionProgress = 0.0f;

    /** True while depleted (post-collection, pre-recharge). */
    UPROPERTY(ReplicatedUsing = OnRep_Depleted, BlueprintReadOnly, Category = "Collection")
    bool bDepleted = false;

    /**
     * Server world time at which the recharge finishes. 0 when not depleted.
     * Clients can use GetRechargeTimeRemaining() for a countdown.
     */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Collection")
    float RechargeEndTime = 0.0f;

    // ---- Public API ---------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Sample")
    bool GetSampleData(FSampleData& OutSampleData) const;

    UFUNCTION(BlueprintCallable, Category = "Sample")
    bool RefreshSampleData();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    float GetCollectionProgress() const { return CollectionProgress; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    bool IsBeingCollected() const { return bBeingCollected; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    bool IsDepleted() const { return bDepleted; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    APawn* GetCurrentCollector() const { return CurrentCollector; }

    /** 0 means no recharge pending. Otherwise seconds until the node is collectable again. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    float GetRechargeTimeRemaining() const;

    /** Server-only. Begin collecting. Fails if already occupied, depleted, or data missing. */
    UFUNCTION(BlueprintCallable, Category = "Collection")
    bool StartCollection(APawn* Collector);

    /** Server-only. Stop collecting. bResetProgress=true resets progress to 0. */
    UFUNCTION(BlueprintCallable, Category = "Collection")
    void StopCollection(bool bResetProgress = true);

    // ---- Interaction contract overrides ------------------------------------

    virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
    virtual void OnInteract_Implementation(APawn* InstigatorPawn) override;

    // ---- Blueprint hooks (fired on server and mirrored on clients via OnRep) ----

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionProgressChanged(float Progress);

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionCancelled();

    /** Server fires with the collector; clients fire with null (collector pointer not guaranteed). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionCompleted(APawn* Collector);

    /** Server-only hook for the future interruption mini-game. Not implemented yet. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnDisturbanceTriggered();

    /** Node became depleted (just collected). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnDepleted();

    /** Node finished recharging and is collectable again. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnRecharged();

protected:
    UFUNCTION()
    void OnRep_BeingCollected();

    UFUNCTION()
    void OnRep_CollectionProgress();

    UFUNCTION()
    void OnRep_Depleted();

private:
    void ServerTickCollection(float DeltaSeconds);
    void OnRechargeComplete();

    float LastDisturbanceCheckTime = 0.0f;
    FTimerHandle RechargeTimerHandle;
};