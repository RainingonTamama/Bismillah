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
 *   - Target in FRONT of the camera  -> indicator sits at the exact screen projection
 *                                       of the target, clamped inside the visible area.
 *   - Target BEHIND the camera       -> indicator sits on the BOTTOM edge of the screen.
 *                                       Its X is driven by the target's horizontal bearing
 *                                       relative to the camera's forward:
 *                                         dead behind      -> bottom-center
 *                                         behind-right     -> bottom-right area
 *                                         behind-left      -> bottom-left area
 *                                       As the killer rotates toward the target, the circle
 *                                       slides along the bottom and then up into the
 *                                       play area once the target crosses in front.
 *   - Distance and obstacles are IGNORED by design (this is a "map-wide" indicator).
 *   - Lifetime: DisplayDuration seconds, with an optional FadeOutDuration at the end.
 *     A second ShowIndicator call resets the lifetime and retargets.
 *
 * VISUAL
 * ------
 * IndicatorVisual is intentionally typed as UWidget (not UImage) so the BP child can use
 * ANY widget as the icon — Text block, Overlay, SizeBox, Image, etc. The C++ only moves,
 * shows, hides, and fades it; the BP child decides what it looks like.
 *
 * USAGE (BP):
 *   Widget->ShowIndicator(WorldLocation);
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

    /**
     * The visual icon. Required. Can be any widget type in the BP child — a Text block
     * with an "X", an Image, an Overlay, etc. The C++ only moves/shows/hides/fades it.
     */
    UPROPERTY(meta = (BindWidget))
    UWidget* IndicatorVisual;

    // ---- Tunables ----------------------------------------------------------

    /** Total display time, seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disturbance")
    float DisplayDuration = 2.0f;

    /** Fade-out duration at the end, seconds. 0 disables the fade (instant hide). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disturbance")
    float FadeOutDuration = 0.5f;

    /** Inset from screen edges in pixels. Keeps the icon from hiding under HUD chrome. */
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