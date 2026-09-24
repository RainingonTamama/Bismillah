// TRGBag.cpp

#include "Core/Interactables/TRGBag.h"
#include "BismillahSurvivor.h"
#include "Core/BismillahGameState.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ATRGBag::ATRGBag()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f; // 20 Hz server tick

    bReplicates = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(InteractionSphere);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetMobility(EComponentMobility::Movable);
}

void ATRGBag::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
    }
}

void ATRGBag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ATRGBag, bBeingUsed);
    DOREPLIFETIME(ATRGBag, CurrentDepositor);
    DOREPLIFETIME(ATRGBag, DepositProgress);
}

bool ATRGBag::CanInteract_Implementation(APawn* InstigatorPawn)
{
    const ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(InstigatorPawn);
    if (!Survivor)
    {
        return false;
    }

    if (!Survivor->IsCarryingSample())
    {
        return false;
    }

    if (bBeingUsed && CurrentDepositor != InstigatorPawn)
    {
        return false;
    }

    return true;
}

void ATRGBag::OnInteract_Implementation(APawn* InstigatorPawn)
{
    if (!HasAuthority())
    {
        return;
    }

    ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(InstigatorPawn);
    if (!Survivor)
    {
        UE_LOG(LogTemp, Warning, TEXT("TRGBag '%s'::OnInteract: instigator is not a survivor."), *GetName());
        return;
    }

    if (!Survivor->IsCarryingSample())
    {
        UE_LOG(LogTemp, Warning, TEXT("TRGBag '%s'::OnInteract: '%s' is not carrying a sample."),
            *GetName(), *Survivor->GetName());
        return;
    }

    if (bBeingUsed && CurrentDepositor == Survivor)
    {
        UE_LOG(LogTemp, Warning, TEXT("TRGBag '%s'::OnInteract: '%s' re-pressed, cancelling deposit."),
            *GetName(), *Survivor->GetName());
        StopDeposit(true);
        return;
    }

    if (!CanInteract(Survivor))
    {
        UE_LOG(LogTemp, Warning, TEXT("TRGBag '%s'::OnInteract: CanInteract refused for '%s'."),
            *GetName(), *Survivor->GetName());
        return;
    }

    StartDeposit(Survivor);
}

bool ATRGBag::StartDeposit(APawn* Depositor)
{
    if (!HasAuthority() || bBeingUsed || !Depositor)
    {
        return false;
    }

    ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(Depositor);
    if (!Survivor || !Survivor->IsCarryingSample())
    {
        return false;
    }

    bBeingUsed = true;
    CurrentDepositor = Depositor;
    DepositProgress = 0.0f;

    UE_LOG(LogTemp, Warning,
        TEXT("TRGBag '%s': StartDeposit by '%s' (sample=%s, value=%d, duration=%.1fs)"),
        *GetName(), *Survivor->GetName(),
        *Survivor->GetCarriedSample().SampleID.ToString(),
        Survivor->GetCarriedSample().ResearchValue,
        DepositDuration);

    OnDepositStarted();
    OnDepositProgressChanged(DepositProgress);

    return true;
}

void ATRGBag::StopDeposit(bool bResetProgress)
{
    if (!HasAuthority() || !bBeingUsed)
    {
        return;
    }

    APawn* PreviousDepositor = CurrentDepositor;

    bBeingUsed = false;
    CurrentDepositor = nullptr;
    if (bResetProgress)
    {
        DepositProgress = 0.0f;
    }

    UE_LOG(LogTemp, Warning, TEXT("TRGBag '%s': StopDeposit (was depositing by '%s')"),
        *GetName(), *GetNameSafe(PreviousDepositor));

    OnDepositProgressChanged(DepositProgress);
    OnDepositCancelled();
}

void ATRGBag::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority() || !bBeingUsed)
    {
        return;
    }

    ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(CurrentDepositor);
    if (!IsValid(Survivor) || !Survivor->IsCarryingSample())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("TRGBag '%s': depositor invalid or no longer carrying, cancelling."), *GetName());
        StopDeposit(true);
        return;
    }

    const float TotalTime = FMath::Max(DepositDuration, 0.01f);
    DepositProgress = FMath::Clamp(DepositProgress + (DeltaSeconds / TotalTime), 0.0f, 1.0f);

    OnDepositProgressChanged(DepositProgress);

    if (DepositProgress >= 1.0f)
    {
        CompleteDeposit(Survivor);
    }
}

void ATRGBag::CompleteDeposit(ABismillahSurvivor* Survivor)
{
    if (!Survivor)
    {
        StopDeposit(true);
        return;
    }

    const FCarriedSample Sample = Survivor->GetCarriedSample();
    const int32 Value = Sample.ResearchValue;
    const FName SampleID = Sample.SampleID;

    // Clear the sample on the survivor first.
    Survivor->ClearSample();

    // Then credit the game state.
    if (UWorld* World = GetWorld())
    {
        if (ABismillahGameState* GS = World->GetGameState<ABismillahGameState>())
        {
            GS->AddResearchValue(Value);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("TRGBag '%s': no ABismillahGameState found; research value lost. "
                    "Check that your GameMode's GameStateClass is set to ABismillahGameState."),
                *GetName());
        }
    }

    bBeingUsed = false;
    CurrentDepositor = nullptr;
    DepositProgress = 0.0f;

    UE_LOG(LogTemp, Warning,
        TEXT("TRGBag '%s': deposit COMPLETE by '%s' (sample=%s, value=%d)"),
        *GetName(), *Survivor->GetName(), *SampleID.ToString(), Value);

    OnDepositProgressChanged(DepositProgress);
    OnDepositCompleted(Value, SampleID);
}

void ATRGBag::OnRep_BeingUsed()
{
    if (bBeingUsed)
    {
        OnDepositStarted();
        OnDepositProgressChanged(DepositProgress);
    }
    else
    {
        OnDepositCancelled();
    }
}

void ATRGBag::OnRep_DepositProgress()
{
    OnDepositProgressChanged(DepositProgress);
}