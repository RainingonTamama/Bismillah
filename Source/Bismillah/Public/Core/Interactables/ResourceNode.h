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
 *
 * VISUAL
 * ------
 * MeshComponent is the visible representation. Assign a StaticMesh asset on the
 * BP child (BP_ResourceNode_Rock) — C++ provides no default. Collision is off by
 * default; see the constructor for how to make it solid.
 *
 * UPGRADE PATH (if you later want authored skeletal animations):
 *   Change UStaticMeshComponent to USkeletalMeshComponent here, and in the
 *   .cpp include + CreateDefaultSubobject call. Assign an Anim Class on the BP.
 *   Everything else (state events, replication) stays identical.
 *
 * PERFORMANCE / MULTIPLAYER NOTES
 * --------------------------------
 * - FSampleData resolved ONCE at BeginPlay and cached in CachedSampleData.
 * - Tick runs only on the server (disabled on clients in BeginPlay).
 * - Recharge uses FTimerManager, not Tick — does not consume tick budget.
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

    // ---- Visual -------------------------------------------------------------

    /** The visible representation. Assign a StaticMesh asset on the BP child. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    UStaticMeshComponent* MeshComponent;

    /** Convenience accessor for BP (self -> GetMeshComponent). */
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

    UPROPERTY(ReplicatedUsing = OnRep_Depleted, BlueprintReadOnly, Category = "Collection")
    bool bDepleted = false;

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

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Collection")
    float GetRechargeTimeRemaining() const;

    UFUNCTION(BlueprintCallable, Category = "Collection")
    bool StartCollection(APawn* Collector);

    UFUNCTION(BlueprintCallable, Category = "Collection")
    void StopCollection(bool bResetProgress = true);

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

    UFUNCTION(BlueprintImplementableEvent, Category = "Collection")
    void OnDisturbanceTriggered();

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

private:
    void ServerTickCollection(float DeltaSeconds);
    void OnRechargeComplete();

    float LastDisturbanceCheckTime = 0.0f;
    FTimerHandle RechargeTimerHandle;
};