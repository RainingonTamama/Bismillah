// BismillahGameState.cpp

#include "Core/BismillahGameState.h"
#include "Net/UnrealNetwork.h"

ABismillahGameState::ABismillahGameState()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ABismillahGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABismillahGameState, DepositedResearchValue);
    DOREPLIFETIME(ABismillahGameState, bPortalsOpening);
    DOREPLIFETIME(ABismillahGameState, PortalOpenStartTime);
}

float ABismillahGameState::GetServerWorldTime() const
{
    return GetServerWorldTimeSeconds();
}

void ABismillahGameState::AddResearchValue(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0)
    {
        return;
    }

    DepositedResearchValue += Amount;

    UE_LOG(LogTemp, Warning,
        TEXT("BismillahGameState: research +%d -> %d / %d"),
        Amount, DepositedResearchValue, RequiredResearchValue);

    // Server-side immediate event; clients get it via OnRep_DepositedResearchValue.
    OnResearchValueChanged(DepositedResearchValue, RequiredResearchValue);

    // Threshold crossed?
    if (!bPortalsOpening && DepositedResearchValue >= RequiredResearchValue)
    {
        bPortalsOpening = true;
        PortalOpenStartTime = GetServerWorldTime();

        UE_LOG(LogTemp, Warning,
            TEXT("BismillahGameState: threshold reached. Portals opening over %.1fs."),
            PortalOpenDuration);

        // Server-side immediate event; clients get it via OnRep_PortalsOpening.
        OnPortalsStartedOpening();
    }
}

float ABismillahGameState::GetPortalOpenProgress() const
{
    if (!bPortalsOpening || PortalOpenStartTime <= 0.0f)
    {
        return 0.0f;
    }

    const float Elapsed = GetServerWorldTime() - PortalOpenStartTime;
    if (PortalOpenDuration <= 0.0f)
    {
        return 1.0f;
    }
    return FMath::Clamp(Elapsed / PortalOpenDuration, 0.0f, 1.0f);
}

float ABismillahGameState::GetPortalTimeRemaining() const
{
    if (!bPortalsOpening)
    {
        return 0.0f;
    }

    const float Elapsed = GetServerWorldTime() - PortalOpenStartTime;
    return FMath::Max(0.0f, PortalOpenDuration - Elapsed);
}

void ABismillahGameState::OnRep_DepositedResearchValue()
{
    OnResearchValueChanged(DepositedResearchValue, RequiredResearchValue);
}

void ABismillahGameState::OnRep_PortalsOpening()
{
    if (bPortalsOpening)
    {
        OnPortalsStartedOpening();
    }
}