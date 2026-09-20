// DisturbanceIndicatorWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DisturbanceIndicatorWidget.generated.h"

class UCanvasPanel;
class UWidget;
class APlayerController;

/**
 * HUD widget that points at a disturbance event.
 * Debug pass: temporary UE_LOG lines added in ShowIndicator and NativeTick.
 */
UCLASS(Abstract)
class BISMILLAH_API UDisturbanceIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Disturbance")
    void ShowIndicator(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "Disturbance")
    void HideIndicator();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Disturbance")
    bool IsShowing() const { return bActive; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* RootCanvas;

    UPROPERTY(meta = (BindWidget))
    UWidget* IndicatorVisual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disturbance")
    float DisplayDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disturbance")
    float FadeOutDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disturbance")
    float ScreenMargin = 60.0f;

private:
    bool bActive = false;
    FVector TargetWorldLocation = FVector::ZeroVector;
    float RemainingTime = 0.0f;

    APlayerController* GetOwningPC() const;
    bool ComputeScreenPosition(FVector2D& OutScreenPosition) const;
    void ApplyVisualPosition(const FVector2D& ScreenPosition);
};