// DisturbanceIndicatorWidget.cpp

#include "UI/DisturbanceIndicatorWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"

// ---- DEBUG FLAG --------------------------------------------------------
// Set to 0 when you're done diagnosing to silence the logs (or just remove
// the UE_LOG lines entirely).
#define DISTURBANCE_INDICATOR_DEBUG 1

#if DISTURBANCE_INDICATOR_DEBUG
#define DI_LOG(Format, ...) UE_LOG(LogTemp, Warning, TEXT("[DistIndicator] ") Format, ##__VA_ARGS__)
#else
#define DI_LOG(Format, ...) do {} while (0)
#endif

void UDisturbanceIndicatorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    DI_LOG("NativeConstruct ran. Outer=%s", *GetNameSafe(GetOuter()));

    SetVisibility(ESlateVisibility::Hidden);

    DI_LOG("NativeConstruct: RootCanvas=%s IndicatorVisual=%s",
        RootCanvas ? *RootCanvas->GetName() : TEXT("NULL"),
        IndicatorVisual ? *IndicatorVisual->GetName() : TEXT("NULL"));

    if (IndicatorVisual)
    {
        IndicatorVisual->SetRenderOpacity(1.0f);

        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorVisual->Slot))
        {
            CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            CanvasSlot->SetPosition(FVector2D(0.0f, 0.0f));

            DI_LOG("NativeConstruct: applied anchors=(0,0) alignment=(0.5,0.5) pos=(0,0)");
        }
        else
        {
            DI_LOG("NativeConstruct: WARNING — IndicatorVisual's Slot is NOT a UCanvasPanelSlot. "
                "Is IndicatorVisual actually a direct child of RootCanvas in the widget tree?");
        }
    }
}

void UDisturbanceIndicatorWidget::ShowIndicator(FVector WorldLocation)
{
    // (1) Entry point + world location
    DI_LOG("ShowIndicator called. WorldLocation=%s", *WorldLocation.ToString());

    // (6) Is the widget actually in the viewport?
    DI_LOG("ShowIndicator: IsInViewport=%s SelfVis=%d",
        IsInViewport() ? TEXT("true") : TEXT("false"),
        (int32)GetVisibility());

    // (5) Are the bound widgets valid?
    DI_LOG("ShowIndicator: RootCanvas=%s IndicatorVisual=%s",
        RootCanvas ? *RootCanvas->GetName() : TEXT("NULL"),
        IndicatorVisual ? *IndicatorVisual->GetName() : TEXT("NULL"));

    // (2) Owning player controller
    APlayerController* PC = GetOwningPC();
    DI_LOG("ShowIndicator: OwningPC=%s", PC ? *PC->GetName() : TEXT("NULL"));

    if (PC)
    {
        DI_LOG("ShowIndicator: PC->IsLocalController=%s PC->PlayerCameraManager=%s",
            PC->IsLocalController() ? TEXT("true") : TEXT("false"),
            PC->PlayerCameraManager ? TEXT("valid") : TEXT("NULL"));

        // (3) Projection test — do it here once for a quick sanity check
        if (PC->PlayerCameraManager)
        {
            FVector2D Projected = FVector2D::ZeroVector;
            const bool bOK = PC->ProjectWorldLocationToScreen(WorldLocation, Projected, /*bPlayerViewportRelative=*/true);
            DI_LOG("ShowIndicator: projection bOK=%s Projected=(%.1f,%.1f)",
                bOK ? TEXT("true") : TEXT("false"), Projected.X, Projected.Y);
        }
    }

    // (4) Viewport size + scale
    const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
    const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
    DI_LOG("ShowIndicator: ViewportSize=(%.1f,%.1f) ViewportScale=%.3f",
        ViewportSize.X, ViewportSize.Y, ViewportScale);

    // ---- Actual state change ----
    TargetWorldLocation = WorldLocation;
    RemainingTime = DisplayDuration;
    bActive = true;

    if (IndicatorVisual)
    {
        IndicatorVisual->SetRenderOpacity(1.0f);
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);

    DI_LOG("ShowIndicator: state applied. bActive=%s RemainingTime=%.2f SelfVis(after)=%d",
        bActive ? TEXT("true") : TEXT("false"), RemainingTime, (int32)GetVisibility());
}

void UDisturbanceIndicatorWidget::HideIndicator()
{
    DI_LOG("HideIndicator called (bActive was %s)", bActive ? TEXT("true") : TEXT("false"));

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

    FVector2D ScreenPos = FVector2D::ZeroVector;
    const bool bComputed = ComputeScreenPosition(ScreenPos);

    if (bComputed)
    {
        ApplyVisualPosition(ScreenPos);
    }

    // Throttled per-frame logging: about 4 lines per second, so the log stays readable.
    static float LastDiagLogTime = -1.0f;
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (Now - LastDiagLogTime >= 0.25f)
    {
        LastDiagLogTime = Now;

        FVector2D SlotPos = FVector2D::ZeroVector;
        if (IndicatorVisual)
        {
            // Renamed local from 'Slot' to 'VisualCanvasSlot' to avoid shadowing
            // UWidget::Slot (which is a protected member of the base class).
            if (const UCanvasPanelSlot* VisualCanvasSlot = Cast<UCanvasPanelSlot>(IndicatorVisual->Slot))
            {
                SlotPos = VisualCanvasSlot->GetPosition();
            }
        }

        DI_LOG("Tick: Remaining=%.2f bComputed=%s ScreenTarget=(%.1f,%.1f) SlotPos=(%.1f,%.1f) "
            "VisualVis=%d SelfVis=%d Opacity=%.2f",
            RemainingTime,
            bComputed ? TEXT("true") : TEXT("false"),
            ScreenPos.X, ScreenPos.Y,
            SlotPos.X, SlotPos.Y,
            IndicatorVisual ? (int32)IndicatorVisual->GetVisibility() : -1,
            (int32)GetVisibility(),
            IndicatorVisual ? IndicatorVisual->GetRenderOpacity() : -1.0f);
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

    if (ForwardDot > 0.0f)
    {
        FVector2D Projected;
        const bool bProjected = PC->ProjectWorldLocationToScreen(TargetWorldLocation, Projected, true);
        if (!bProjected)
        {
            Projected = FVector2D(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);
        }

        OutScreenPosition.X = FMath::Clamp(Projected.X, Margin, ViewportSize.X - Margin);
        OutScreenPosition.Y = FMath::Clamp(Projected.Y, Margin, ViewportSize.Y - Margin);
        return true;
    }

    const float Bearing = FMath::Atan2(RightDot, ForwardDot);
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