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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FName SampleID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    ESampleRarity Rarity = ESampleRarity::Common;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    int32 ResearchValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float BaseCollectionTime = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float InterruptionOddsMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sample")
    float RechargeTime = 80.0f;
};

/**
 * A sample currently held by a survivor.
 * SampleID == NAME_None means no sample is being carried.
 */
USTRUCT(BlueprintType)
struct FCarriedSample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Sample")
    FName SampleID;

    UPROPERTY(BlueprintReadOnly, Category = "Sample")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Sample")
    int32 ResearchValue = 0;

    bool IsValid() const { return !SampleID.IsNone(); }
    void Reset() { SampleID = NAME_None; DisplayName = FText::GetEmpty(); ResearchValue = 0; }
};