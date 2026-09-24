// Portal.cpp

#include "Core/Portal.h"
#include "Core/BismillahGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

APortal::APortal()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f; // 10 Hz is plenty for a 50s animation

    bReplicates = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetMobility(EComponentMobility::Movable);
}

void APortal::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        CachedGameState = World->GetGameState<ABismillahGameState>();
    }
}

bool APortal::IsOpening() const
{
    const ABismillahGameState* GS = CachedGameState.Get();
    return GS && GS->IsPortalsOpening();
}

float APortal::GetOpenProgress() const
{
    const ABismillahGameState* GS = CachedGameState.Get();
    return GS ? GS->GetPortalOpenProgress() : 0.0f;
}

void APortal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Late-join support: GameState may not exist at BeginPlay on very early actors.
    ABismillahGameState* GS = CachedGameState.Get();
    if (!GS)
    {
        if (UWorld* World = GetWorld())
        {
            GS = World->GetGameState<ABismillahGameState>();
            CachedGameState = GS;
        }
        if (!GS)
        {
            return;
        }
    }

    if (!GS->IsPortalsOpening() && !bNotifiedFullyOpen)
    {
        return;
    }

    if (!bNotifiedStart && GS->IsPortalsOpening())
    {
        bNotifiedStart = true;
        OnPortalStartedOpening();
    }

    const float Progress = GS->GetPortalOpenProgress();

    if (!FMath::IsNearlyEqual(Progress, LastReportedProgress, 0.001f))
    {
        LastReportedProgress = Progress;
        OnPortalProgressChanged(Progress);
    }

    if (!bNotifiedFullyOpen && Progress >= 1.0f)
    {
        bNotifiedFullyOpen = true;
        OnPortalFullyOpened();

        UE_LOG(LogTemp, Warning, TEXT("Portal '%s': fully opened."), *GetName());
    }
}