// BismillahKiller.cpp

#include "BismillahKiller.h"
#include "BismillahSurvivor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

ABismillahKiller::ABismillahKiller()
{
    // Disable the inherited third-person camera boom
    if (CameraBoom)
    {
        CameraBoom->SetActive(false);
    }

    // Critical fix: bAutoActivate defaults to true on UCameraComponent, which re-activates it
    // at BeginPlay regardless of SetActive(false). Must explicitly disable auto-activation too.
    if (FollowCamera)
    {
        FollowCamera->bAutoActivate = false;
        FollowCamera->Deactivate();
    }

    // Create first-person camera
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetMesh());
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->bAutoActivate = true;

    // Required so the Server_PerformAttack RPC can actually reach the server.
    // Without this, the RPC is silently dropped.
    bReplicates = true;
}

void ABismillahKiller::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("Killer spawned"));
}

void ABismillahKiller::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (FirstPersonCamera && GetMesh())
    {
        if (GetMesh()->DoesSocketExist(FName("head")))
        {
            FirstPersonCamera->AttachToComponent(
                GetMesh(),
                FAttachmentTransformRules::SnapToTargetIncludingScale,
                FName("head"));
        }
        else
        {
            FirstPersonCamera->AttachToComponent(
                GetMesh(),
                FAttachmentTransformRules::SnapToTargetIncludingScale);
        }
    }
}

void ABismillahKiller::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (AttackAction)
        {
            EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ABismillahKiller::PerformAttack);
        }
    }
}

void ABismillahKiller::PerformAttack()
{
    // Input fires on the owning client. Route to the server so the hit
    // resolution and state changes stay server-authoritative.
    Server_PerformAttack();
}

void ABismillahKiller::Server_PerformAttack_Implementation()
{
    // --- Cooldown check ---
    const float Now = GetWorld()->GetTimeSeconds();
    const float TimeSinceLastAttack = Now - LastAttackTime;
    if (TimeSinceLastAttack < AttackCooldownDuration)
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: on cooldown (%.2fs remaining)"),
            AttackCooldownDuration - TimeSinceLastAttack);
        return;
    }
    LastAttackTime = Now;

    UE_LOG(LogTemp, Warning, TEXT("PerformAttack: fired"));

    // --- Sphere trace forward ---
    const FVector Start = GetActorLocation();
    const FVector Forward = GetControlRotation().Vector();
    const FVector End = Start + Forward * AttackRange;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    FHitResult Hit;
    const bool bHit = GetWorld()->SweepSingleByObjectType(
        Hit,
        Start,
        End,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(AttackRadius),
        QueryParams
    );

    if (!bHit)
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: no hit"));
        return;
    }

    AActor* HitActor = Hit.GetActor();
    UE_LOG(LogTemp, Warning, TEXT("PerformAttack: hit %s"), *GetNameSafe(HitActor));

    // --- Advance survivor state if it's a survivor ---
    ABismillahSurvivor* Survivor = Cast<ABismillahSurvivor>(HitActor);
    if (!Survivor)
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: hit actor is not a survivor"));
        return;
    }

    // --- Cancel any in-progress collection FIRST, before the state-advance switch.
    //     Getting hit always interrupts collection, regardless of whether the melee
    //     also advances the survivor's state (which it does not for Downed/Captured).
    if (Survivor->CancelCollection(TEXT("hit by killer melee")))
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: interrupted collection on '%s'"), *Survivor->GetName());
    }

    ESurvivorState NextState = Survivor->SurvivorState;
    switch (Survivor->SurvivorState)
    {
    case ESurvivorState::Healthy:
        NextState = ESurvivorState::Injured;
        break;
    case ESurvivorState::Injured:
        NextState = ESurvivorState::Downed;
        break;
    default:
        // Already Downed / BeingDragged / Captured — melee doesn't advance further
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: survivor already past Injured, no state change"));
        return;
    }

    Survivor->SetSurvivorState(NextState);
    UE_LOG(LogTemp, Warning, TEXT("PerformAttack: advanced survivor to state %s"),
        *UEnum::GetValueAsString(NextState));
}