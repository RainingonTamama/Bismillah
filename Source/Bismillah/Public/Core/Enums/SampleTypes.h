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

    /** Unique identifier for this sample. Matched against AResourceNode::SampleID. */
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

    /** Seconds required to fully collect this node. Set to 20.0 for the Rock. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float BaseCollectionTime = 20.0f;

    /** Scales disturbance chance in the future interruption system. Stored only for now. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float InterruptionOddsMultiplier = 1.0f;

    /** Seconds the node stays depleted after being fully collected. Set to 80.0 for the Rock. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float RechargeTime = 80.0f;
};