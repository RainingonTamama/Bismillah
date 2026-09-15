// ResourceNode.h

#pragma once

#include "CoreMinimal.h"
#include "Core/Interactables/InteractableBase.h"
#include "Core/Enums/SampleTypes.h"
#include "ResourceNode.generated.h"

class UDataTable;

/**
 * A collectible sample node placed in the world.
 * Milestone 1: interaction shell only — no collection timer, no interruption, no deposit.
 *
 * PERFORMANCE / MULTIPLAYER NOTES
 * --------------------------------
 * - FSampleData is resolved ONCE at BeginPlay and cached in CachedSampleData.
 *   Do NOT call SampleDataTable->FindRow<>() or GetAllRows<>() from Tick.
 * - The DataTable is a UAsset present on both server and every client, so each
 *   machine resolves its own cache locally. FSampleData is therefore NOT replicated.
 *   This is intentional: it keeps the network payload small and gives clients the
 *   fields they need (BaseCollectionTime, DisplayName, etc.) for future UI without
 *   extra RPCs.
 * - Future replicated state (collection progress, collector pawn, depleted flag)
 *   MUST be UPROPERTY(Replicated / ReplicatedUsing) and mutated only under
 *   HasAuthority(). Those go here in Milestone 2, not in CachedSampleData.
 */
UCLASS()
class BISMILLAH_API AResourceNode : public AInteractableBase
{
    GENERATED_BODY()

public:
    AResourceNode();

    virtual void BeginPlay() override;

    /** Row name in SampleDataTable that identifies which sample this node yields. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FName SampleID;

    /** DataTable of FSampleData rows. Assigned per-instance by the level designer. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    UDataTable* SampleDataTable;

    /**
     * Cached row data. Populated at BeginPlay, and refreshable via RefreshSampleData().
     * Transient: never serialized, never replicated — each machine rebuilds it locally.
     * Safe to read every Tick if a future milestone needs to.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Sample")
    FSampleData CachedSampleData;

    /** True if CachedSampleData holds a valid row. Check before reading. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Sample")
    bool bHasValidSampleData = false;

    /**
     * Returns the cached row data. BlueprintCallable for consistency with Milestone 1.
     * @return true if cached data is valid (OutSampleData is filled).
     */
    UFUNCTION(BlueprintCallable, Category = "Sample")
    bool GetSampleData(FSampleData& OutSampleData) const;

    /**
     * Re-resolves CachedSampleData from SampleDataTable using SampleID.
     * Safe to call at runtime if SampleID is ever changed dynamically (it usually isn't).
     * @return true if a row was found and cached.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample")
    bool RefreshSampleData();

    /** Server-side interaction entry point. Logs only for this milestone. */
    virtual void OnInteract_Implementation(APawn* InstigatorPawn) override;
};