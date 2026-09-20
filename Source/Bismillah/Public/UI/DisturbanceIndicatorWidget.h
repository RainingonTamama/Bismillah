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
 *     with X based on the target's horizontal bearing:
 *       dead behind -> bottom-center
 *       behind-right -> bottom-right area
 *       behind-left  -> bottom-left area
 *   - Distance and obstacles are IGNORED by design (map-wide indicator).
 *   - Lifetime: DisplayDuration seconds, with FadeOutDuration fade at the end.
 *     A second ShowIndicator call resets the lifetime and retargets.
 *
 * POSITIONING
 * -----------
 * All positioning is done in RootCanvas LOCAL space (Slate units, DPI-independent).
 * Canvas local size comes from RootCanvas->GetCachedGeometry().GetLocalSize(), with
 * a fallback of GetViewportSize / GetViewportScale. Projected pixel positions from
 * ProjectWorldLocationToWidgetPosition are already in Slate units. This keeps the
 * mark in the same unit space as UCanvasPanelSlot::SetPosition, fixing the "2/3 of
 * the way down" bug caused by mixing pixels and Slate units on high-DPI displays.
 *
 * VISUAL
 * ------
 * IndicatorVisual is typed UWidget, so the BP child can use any widget as the icon
 * (Text block, Image, Overlay). C++ only moves/shows/hides/fades it.
 */
UCLASS(Abstract)
class BISMILLAH_API UDisturbanceIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Show the indicator pointing at WorldLocation. Resets the lifetime timer. */
    UFUNCTION(BlueprintCallable, Category = "Disturbance")
    void ShowIndicator(FVector WorldLocation);

    /** Hide immediately, bypassing the fade. */
    UFUNCTION(BlueprintCallable, Category = "Disturbance")
    void HideIndicator();

    /** True while the indicator is actively displayed. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Disturbance")
    bool IsShowing() const { return bActive; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ---- Widget bindings ---------------------------------------------------
    // These must exist in the BP child (WBP_DisturbanceIndicator) with exactly
    // these names, or the widget will fail to compile.

    /** Root canvas. Required. */
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* RootCanvas;

    /** The visual icon. Required. Any widget type works. */
    UPROPERTY(meta = (BindWidget))
    UWidget* IndicatorVisual;

    // ---- Tunables ----------------------------------------------------------

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

    /** Returns the canvas's local (Slate) size. Falls back to ViewportSize / ViewportScale. */
    FVector2D GetCanvasLocalSize() const;

    /**
     * Computes the mark's position in RootCanvas local space.
     * All coordinates are Slate units, consistent with UCanvasPanelSlot::SetPosition.
     */
    bool ComputeScreenPosition(FVector2D& OutLocalPosition) const;

    void ApplyVisualPosition(const FVector2D& LocalPosition);
};