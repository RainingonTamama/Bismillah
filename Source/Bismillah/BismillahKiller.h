// BismillahKiller.h

#pragma once

#include "CoreMinimal.h"
#include "BismillahCharacter.h"
#include "BismillahKiller.generated.h"

// Forward declaration for the first-person camera
class UCameraComponent;

UCLASS()
class BISMILLAH_API ABismillahKiller : public ABismillahCharacter
{
    GENERATED_BODY()

public:
    // Sets default values for this character's properties
    ABismillahKiller();

    /** First-person camera for the Killer */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FirstPersonCamera;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called after all components are initialized (used to attach camera to head socket)
    virtual void PostInitializeComponents() override;
};