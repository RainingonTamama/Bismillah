// SampleTypes.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SampleTypes.generated.h"

UENUM(BlueprintType)
enum class ESampleRarity : uint8
{
    Common      UMETA(DisplayName = "Common"),
    Uncommon    UMETA(DisplayName = "Uncommon"),
    Rare        UMETA(DisplayName = "Rare")
};

USTRUCT(BlueprintType)
struct FSampleData : public FTableRowBase
{
    GENERATED_BODY()

    /** Unique identifier for this sample. Row name in the DataTable should match this. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FName SampleID;

    /** Player-facing name shown in UI. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FText DisplayName;

    /** Rarity tier of the sample. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    ESampleRarity Rarity = ESampleRarity::Common;

    /** Contribution toward the future ARC deposit threshold. Stored only for now. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    int32 ResearchValue = 0;

    /** Seconds required to fully collect this node (used by a future milestone). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float BaseCollectionTime = 1.0f;

    /** Scales disturbance chance in the future interruption system. Stored only for now. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float InterruptionOddsMultiplier = 1.0f;
};