// DisturbanceIndicatorWidget.cpp

#include "UI/DisturbanceIndicatorWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"

void UDisturbanceIndicatorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetVisibility(ESlateVisibility::Hidden);

    if (IndicatorVisual)
    {
        IndicatorVisual->SetRenderOpacity(1.0f);

        // Force the canvas slot to be centered on the position we set, anchored to the
        // top-left of the canvas. Without this, SetPosition refers to the widget's
        // top-left corner instead of its center.
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorVisual->Slot))
        {
            CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            CanvasSlot->SetPosition(FVector2D(0.0f, 0.0f));
        }
    }
}

void UDisturbanceIndicatorWidget::ShowIndicator(FVector WorldLocation)
{
    TargetWorldLocation = WorldLocation;
    RemainingTime = DisplayDuration;
    bActive = true;

    if (IndicatorVisual)
    {
        IndicatorVisual->SetRenderOpacity(1.0f);
    }

    // HitTestInvisible: drawn, but does not block input to the game.
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDisturbanceIndicatorWidget::HideIndicator()
{
    bActive = false;
    RemainingTime = 0.0f;
    SetVisibility(ESlateVisibility::Hidden);
}

APlayerController* UDisturbanceIndicatorWidget::GetOwningPC() const
{
    return GetOwningPlayer();
}

void UDisturbanceIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bActive)
    {
        return;
    }

    RemainingTime -= InDeltaTime;
    if (RemainingTime <= 0.0f)
    {
        HideIndicator();
        return;
    }

    // Fade out over the last FadeOutDuration seconds.
    if (IndicatorVisual)
    {
        if (FadeOutDuration > 0.0f && RemainingTime < FadeOutDuration)
        {
            const float Alpha = FMath::Clamp(RemainingTime / FadeOutDuration, 0.0f, 1.0f);
            IndicatorVisual->SetRenderOpacity(Alpha);
        }
        else
        {
            IndicatorVisual->SetRenderOpacity(1.0f);
        }
    }

    FVector2D ScreenPos;
    if (ComputeScreenPosition(ScreenPos))
    {
        ApplyVisualPosition(ScreenPos);
    }
}

bool UDisturbanceIndicatorWidget::ComputeScreenPosition(FVector2D& OutScreenPosition) const
{
    APlayerController* PC = GetOwningPC();
    if (!PC)
    {
        return false;
    }

    APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
    if (!CamMgr)
    {
        return false;
    }

    const FVector CamLocation = CamMgr->GetCameraLocation();
    const FRotator CamRotation = CamMgr->GetCameraRotation();

    const FVector Forward = CamRotation.Vector();
    const FVector Right = FRotationMatrix(CamRotation).GetScaledAxis(EAxis::Y);

    const FVector ToTarget = TargetWorldLocation - CamLocation;
    const float ForwardDot = FVector::DotProduct(ToTarget, Forward);
    const float RightDot = FVector::DotProduct(ToTarget, Right);

    const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
    if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
    {
        return false;
    }

    const float Margin = ScreenMargin;
    const float BottomY = ViewportSize.Y - Margin;

    // ---- Case 1: target is in front of the camera -> exact screen projection. ----
    if (ForwardDot > 0.0f)
    {
        FVector2D Projected;
        const bool bProjected = PC->ProjectWorldLocationToScreen(TargetWorldLocation, Projected, /*bPlayerViewportRelative=*/true);
        if (!bProjected)
        {
            Projected = FVector2D(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);
        }

        OutScreenPosition.X = FMath::Clamp(Projected.X, Margin, ViewportSize.X - Margin);
        OutScreenPosition.Y = FMath::Clamp(Projected.Y, Margin, ViewportSize.Y - Margin);
        return true;
    }

    // ---- Case 2: target is behind the camera -> clamp to BOTTOM edge. ----
    //
    // Horizontal bearing relative to camera forward:
    //    0°   = dead ahead
    //   90°   = directly right
    //  180°   = directly behind
    //  -90°   = directly left
    // -180°   = directly behind (wraps)
    //
    // sin(bearing) gives a smooth X across the bottom:
    //    90°   -> sin = +1  -> bottom-right area
    //   180°   -> sin =  0  -> bottom-center
    //   -90°   -> sin = -1  -> bottom-left area
    //  -180°   -> sin =  0  -> bottom-center
    //
    // Because the entire "behind" branch covers bearings with |angle| > 90°, the X
    // varies smoothly as the killer rotates; the discontinuity only occurs at the
    // 90° boundary itself, where the indicator jumps from the bottom edge up to the
    // in-front projected position. That jump is intentional per design.
    const float Bearing = FMath::Atan2(RightDot, ForwardDot); // radians, (-pi, pi]
    const float SinBearing = FMath::Sin(Bearing);

    const float UsableWidth = ViewportSize.X - 2.0f * Margin;
    const float ScreenX = Margin + ((SinBearing + 1.0f) * 0.5f) * UsableWidth;

    OutScreenPosition.X = ScreenX;
    OutScreenPosition.Y = BottomY;
    return true;
}

void UDisturbanceIndicatorWidget::ApplyVisualPosition(const FVector2D& ScreenPosition)
{
    if (!IndicatorVisual)
    {
        return;
    }

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorVisual->Slot))
    {
        CanvasSlot->SetPosition(ScreenPosition);
    }
}