// DisturbanceIndicatorWidget.cpp

#include "UI/DisturbanceIndicatorWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"

// ---- DEBUG FLAG --------------------------------------------------------
// Set to 0 when you're done diagnosing to silence the log line.
#define DISTURBANCE_INDICATOR_DEBUG 1

#if DISTURBANCE_INDICATOR_DEBUG
#define DI_LOG(Format, ...) UE_LOG(LogTemp, Warning, TEXT("[DistIndicator] ") Format, ##__VA_ARGS__)
#else
#define DI_LOG(Format, ...) do {} while (0)
#endif

void UDisturbanceIndicatorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Configure IndicatorVisual's slot exactly once. Result is independent of any
    // anchors/alignment the designer set in the BP.
    if (IndicatorVisual)
    {
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorVisual->Slot))
        {
            CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            CanvasSlot->SetPosition(FVector2D(0.0f, 0.0f));
        }
    }

    // Fully hidden by default. See EnterHiddenState for why this is more than just
    // hiding the outer UserWidget.
    EnterHiddenState();
}

void UDisturbanceIndicatorWidget::EnterHiddenState()
{
    bActive = false;
    RemainingTime = 0.0f;

    if (IndicatorVisual)
    {
        IndicatorVisual->SetVisibility(ESlateVisibility::Collapsed);
        IndicatorVisual->SetRenderOpacity(1.0f); // reset for next show
    }

    // Keep the outer widget hidden too (defensive; also prevents the UserWidget's
    // own slot from intercepting input).
    SetVisibility(ESlateVisibility::Hidden);
}

void UDisturbanceIndicatorWidget::ShowIndicator(FVector WorldLocation)
{
    TargetWorldLocation = WorldLocation;
    RemainingTime = DisplayDuration;
    bActive = true;

    // Show the outer widget, then the visual, then reset opacity.
    // HitTestInvisible: drawn, but does not block input to the game.
    SetVisibility(ESlateVisibility::HitTestInvisible);

    if (IndicatorVisual)
    {
        IndicatorVisual->SetVisibility(ESlateVisibility::Visible);
        IndicatorVisual->SetRenderOpacity(1.0f);
    }
}

void UDisturbanceIndicatorWidget::HideIndicator()
{
    EnterHiddenState();
}

APlayerController* UDisturbanceIndicatorWidget::GetOwningPC() const
{
    return GetOwningPlayer();
}

FVector2D UDisturbanceIndicatorWidget::GetCanvasLocalSize() const
{
    if (RootCanvas)
    {
        const FVector2D LocalSize = RootCanvas->GetCachedGeometry().GetLocalSize();
        if (LocalSize.X > 1.0f && LocalSize.Y > 1.0f)
        {
            return LocalSize;
        }
    }

    // Fallback: viewport size is in pixels; divide by DPI scale to get Slate units.
    const FVector2D ViewportSizePixels = UWidgetLayoutLibrary::GetViewportSize(this);
    const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
    if (ViewportScale > KINDA_SMALL_NUMBER)
    {
        return ViewportSizePixels / ViewportScale;
    }
    return ViewportSizePixels;
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

    FVector2D LocalPosition = FVector2D::ZeroVector;
    if (ComputeScreenPosition(LocalPosition))
    {
        ApplyVisualPosition(LocalPosition);
    }

    // Debug log, throttled to ~4 lines/sec so the Output Log stays readable.
    static float LastLogTime = -1.0f;
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (Now - LastLogTime >= 0.25f)
    {
        LastLogTime = Now;

        const FVector2D LocalSize = GetCanvasLocalSize();
        const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);

        DI_LOG("LocalSize=(%.1f,%.1f) ViewportScale=%.2f TargetWorldLoc=%s FinalPos=(%.1f,%.1f)",
            LocalSize.X, LocalSize.Y,
            ViewportScale,
            *TargetWorldLocation.ToString(),
            LocalPosition.X, LocalPosition.Y);
    }
}

bool UDisturbanceIndicatorWidget::ComputeScreenPosition(FVector2D& OutLocalPosition) const
{
    APlayerController* PC = GetOwningPC();
    if (!PC || !PC->PlayerCameraManager)
    {
        return false;
    }

    const FVector2D LocalSize = GetCanvasLocalSize();
    if (LocalSize.X < 1.0f || LocalSize.Y < 1.0f)
    {
        return false;
    }

    const FVector CamLocation = PC->PlayerCameraManager->GetCameraLocation();
    const FRotator CamRotation = PC->PlayerCameraManager->GetCameraRotation();

    const FVector Forward = CamRotation.Vector();
    const FRotationMatrix CamMatrix(CamRotation);
    const FVector Right = CamMatrix.GetScaledAxis(EAxis::Y);
    const FVector Up = CamMatrix.GetScaledAxis(EAxis::Z);

    const FVector ToTarget = TargetWorldLocation - CamLocation;
    const float ForwardDot = FVector::DotProduct(ToTarget, Forward);
    const float RightDot = FVector::DotProduct(ToTarget, Right);
    const float UpDot = FVector::DotProduct(ToTarget, Up);

    const float Margin = ScreenMargin;
    const float CenterX = LocalSize.X * 0.5f;
    const float CenterY = LocalSize.Y * 0.5f;

    // ---- Case 1: target in front of the camera. ----
    if (ForwardDot > 0.0f)
    {
        FVector2D ProjectedLocal = FVector2D::ZeroVector;
        const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            PC, TargetWorldLocation, ProjectedLocal, /*bPlayerViewportRelative=*/false);

        if (bProjected)
        {
            const bool bOnScreen =
                ProjectedLocal.X >= 0.0f && ProjectedLocal.X <= LocalSize.X &&
                ProjectedLocal.Y >= 0.0f && ProjectedLocal.Y <= LocalSize.Y;

            if (bOnScreen)
            {
                OutLocalPosition.X = FMath::Clamp(ProjectedLocal.X, Margin, LocalSize.X - Margin);
                OutLocalPosition.Y = FMath::Clamp(ProjectedLocal.Y, Margin, LocalSize.Y - Margin);
                return true;
            }
        }

        // In front but off-screen: clamp along the target's screen direction.
        float DirX = RightDot;
        float DirY = -UpDot;
        const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
        if (DirLen > KINDA_SMALL_NUMBER)
        {
            DirX /= DirLen;
            DirY /= DirLen;

            const float HalfW = CenterX - Margin;
            const float HalfH = CenterY - Margin;

            const float ScaleX = (FMath::Abs(DirX) > KINDA_SMALL_NUMBER) ? HalfW / FMath::Abs(DirX) : TNumericLimits<float>::Max();
            const float ScaleY = (FMath::Abs(DirY) > KINDA_SMALL_NUMBER) ? HalfH / FMath::Abs(DirY) : TNumericLimits<float>::Max();
            const float Scale = FMath::Min(ScaleX, ScaleY);

            OutLocalPosition.X = CenterX + DirX * Scale;
            OutLocalPosition.Y = CenterY + DirY * Scale;
            return true;
        }

        // Degenerate: straight ahead but off-screen. Bottom center.
        OutLocalPosition.X = CenterX;
        OutLocalPosition.Y = LocalSize.Y - Margin;
        return true;
    }

    // ---- Case 2: target behind the camera. Bottom edge, X from bearing. ----
    const float Bearing = FMath::Atan2(RightDot, ForwardDot);
    const float SinBearing = FMath::Sin(Bearing);

    const float UsableWidth = LocalSize.X - 2.0f * Margin;
    OutLocalPosition.X = Margin + ((SinBearing + 1.0f) * 0.5f) * UsableWidth;
    OutLocalPosition.Y = LocalSize.Y - Margin;
    return true;
}

void UDisturbanceIndicatorWidget::ApplyVisualPosition(const FVector2D& LocalPosition)
{
    if (!IndicatorVisual)
    {
        return;
    }

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorVisual->Slot))
    {
        CanvasSlot->SetPosition(LocalPosition);
    }
}