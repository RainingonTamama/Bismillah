// BismillahKiller.h

#pragma once

#include "CoreMinimal.h"
#include "BismillahCharacter.h"
#include "BismillahKiller.generated.h"

// Forward declarations
class UCameraComponent;
class UInputAction;

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

    /** Input Action for the melee attack (assign IA_Attack in the Blueprint defaults) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* AttackAction;

    /** How far forward the attack trace reaches (in cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float AttackRange = 250.0f;

    /** Radius of the attack trace sphere (in cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float AttackRadius = 50.0f;

    /** Minimum time between attacks, in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float AttackCooldownDuration = 1.0f;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called after all components are initialized (used to attach camera to head socket)
    virtual void PostInitializeComponents() override;

    // Binds the AttackAction input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /** Input entry point — calls the server RPC */
    void PerformAttack();

    /** Server-authoritative attack logic (trace, cooldown, state advance) */
    UFUNCTION(Server, Reliable)
    void Server_PerformAttack();

private:
    /** Timestamp of the last successful attack, used for cooldown */
    float LastAttackTime = -1000.0f;
};