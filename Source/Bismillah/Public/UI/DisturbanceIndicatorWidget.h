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
 *
 * Behavior:
 *   - Target in FRONT of the camera and on-screen -> mark sits at the exact screen
 *     projection, clamped inside ScreenMargin.
 *   - Target in FRONT but off-screen               -> mark clamps to the nearest edge
 *     along the target's screen direction.
 *   - Target BEHIND the camera                     -> mark sits on the BOTTOM edge,
 *     with X based on the target's horizontal bearing.
 *   - Distance and obstacles are IGNORED by design (map-wide indicator).
 *   - Lifetime: DisplayDuration seconds, with FadeOutDuration fade at the end.
 *
 * VISIBILITY (startup fix)
 * ------------------------
 * IndicatorVisual is explicitly set to Collapsed in NativeConstruct, and to Visible
 * in ShowIndicator, and back to Collapsed in HideIndicator. Do not rely on the outer
 * UserWidget's SetVisibility(Hidden) to hide the child on the first frame - UMG does
 * not always propagate that before the widget's initial tick, which causes a brief
 * flash of the icon at game start.
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
    FVector2D GetCanvasLocalSize() const;
    bool ComputeScreenPosition(FVector2D& OutLocalPosition) const;
    void ApplyVisualPosition(const FVector2D& LocalPosition);

    /** Puts IndicatorVisual into the fully-hidden state and resets internal flags. */
    void EnterHiddenState();
};