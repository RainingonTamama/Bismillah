#include "BismillahKiller.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"

ABismillahKiller::ABismillahKiller()
{
    // Disable the inherited third-person camera boom (not a camera itself, doesn't affect view target selection, but keep it inactive for cleanliness)
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