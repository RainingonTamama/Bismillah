// ResourceNode.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Interactables/InteractableBase.h"
#include "Core/Enums/SampleTypes.h"
#include "ResourceNode.generated.h"

class UDataTable;
class APawn;
class UStaticMeshComponent;

/**
 * A collectible sample node.
 * Milestone 2.1: server-authoritative collection timer, replicated progress,
 * movement-cancel support, and depleted/recharge cycle.
 * Milestone 3: interruption mini-game (press-to-resolve), 30% progress penalty
 * on failure, NetMulticast alert broadcast for killer awareness.
 *
 * PERFORMANCE / MULTIPLAYER NOTES
 * --------------------------------
 * - FSampleData resolved ONCE at BeginPlay and cached in CachedSampleData.
 * - Tick runs only on the server (disabled on clients in BeginPlay).
 * - Recharge uses FTimerManager, not Tick — does not consume tick budget.
 * - All collection/mini-game state mutated only under HasAuthority().
 * - Mini-game deadline uses ServerWorldTimeSeconds so client and server agree.
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

    // ---- Visual -------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    UStaticMeshComponent* MeshComponent;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Visual")
    UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // ---- Design-time config -------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FName SampleID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    UDataTable* SampleDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collection")
    float DisturbanceCheckInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collection")
    float BaseDisturbanceChance = 0.1f;

    /** How long the player has to press Interact once the mini-game prompt appears. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniGame")
    float MiniGameWindowSeconds = 2.0f;

    /** Fraction of progress lost on a failed mini-game (0.3 = lose 30%). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniGame", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MiniGameFailurePenalty = 0.3f;

    // ---- Cached sample data -------------------------------------------------

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

    UPROPERTY(ReplicatedUsing = OnRep_Depleted, BlueprintReadOnly, Category = "Collection")
    bool bDepleted = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Collection")
    float RechargeEndTime = 0.0f;

    // ---- Replicated mini-game state ----------------------------------------

    UPROPERTY(ReplicatedUsing = OnRep_AwaitingMiniGame, BlueprintReadOnly, Category = "MiniGame")
    bool bAwaitingMiniGame = false;

    /** Server world time at which the mini-game window expires. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "MiniGame")
    float MiniGameDeadline = 0.0f;

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

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    float GetRechargeTimeRemaining() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MiniGame")
    bool IsAwaitingMiniGame() const { return bAwaitingMiniGame; }

    /** Seconds left in the mini-game window. 0 if no mini-game is active. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MiniGame")
    float GetMiniGameTimeRemaining() const;

    /** Total window length in seconds (for building a 0..1 countdown ratio). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MiniGame")
    float GetMiniGameWindowSeconds() const { return MiniGameWindowSeconds; }

    UFUNCTION(BlueprintCallable, Category = "Collection")
    bool StartCollection(APawn* Collector);

    UFUNCTION(BlueprintCallable, Category = "Collection")
    void StopCollection(bool bResetProgress = true);

    /**
     * Server-only. Resolve the currently active mini-game.
     * Success is determined server-side by comparing current server time to
     * MiniGameDeadline — the caller does NOT pass a success flag.
     * Safe to call from the survivor's RPC path or the internal timeout.
     */
    void ResolveMiniGame();

    // ---- Interaction contract overrides ------------------------------------

    virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
    virtual void OnInteract_Implementation(APawn* InstigatorPawn) override;

    // ---- Blueprint hooks ---------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionProgressChanged(float Progress);

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionCancelled();

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnCollectionCompleted(APawn* Collector);

    /** Fires on the server when the disturbance roll succeeds; also on clients via OnRep. */
    UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame")
    void OnDisturbanceTriggered();

    /** Fires on all machines when the mini-game resolves. bSuccess = player pressed in time. */
    UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame")
    void OnMiniGameResolved(bool bSuccess);

    /**
     * Fires on all clients (via NetMulticast) when a mini-game fails.
     * Use this to spawn a sound cue + particle at Location to alert the Killer.
     * Sound attenuation determines whether the killer actually hears it.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame")
    void OnDisturbanceAlert(FVector Location);

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnDepleted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnRecharged();

protected:
    UFUNCTION()
    void OnRep_BeingCollected();

    UFUNCTION()
    void OnRep_CollectionProgress();

    UFUNCTION()
    void OnRep_Depleted();

    UFUNCTION()
    void OnRep_AwaitingMiniGame();

private:
    void ServerTickCollection(float DeltaSeconds);
    void OnRechargeComplete();
    float GetServerWorldTime() const;

    /** NetMulticast alert broadcast. Fires OnDisturbanceAlert on the server and every client. */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_BroadcastDisturbanceAlert(FVector Location);

    float LastDisturbanceCheckTime = 0.0f;
    FTimerHandle RechargeTimerHandle;
};