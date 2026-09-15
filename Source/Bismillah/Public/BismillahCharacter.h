#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BismillahCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UInputComponent;
struct FInputActionValue;

UCLASS()
class BISMILLAH_API ABismillahCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABismillahCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FollowCamera;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JumpAction;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

public:
    UFUNCTION(BlueprintCallable, Category = "Input")
    void DoMove(float Right, float Forward);

    UFUNCTION(BlueprintCallable, Category = "Input")
    void DoLook(float Yaw, float Pitch);

    UFUNCTION(BlueprintCallable, Category = "Input")
    void DoJumpStart();

    UFUNCTION(BlueprintCallable, Category = "Input")
    void DoJumpEnd();

    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};