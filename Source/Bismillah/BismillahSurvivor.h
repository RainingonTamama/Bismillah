// BismillahSurvivor.h

#pragma once

#include "CoreMinimal.h"
#include "BismillahCharacter.h"
#include "BismillahSurvivor.generated.h"

UENUM(BlueprintType)
enum class ESurvivorState : uint8
{
    Healthy         UMETA(DisplayName = "Healthy"),
    Injured         UMETA(DisplayName = "Injured"),
    Downed          UMETA(DisplayName = "Downed"),
    BeingDragged    UMETA(DisplayName = "Being Dragged"),
    Captured        UMETA(DisplayName = "Captured")
};

UCLASS()
class BISMILLAH_API ABismillahSurvivor : public ABismillahCharacter
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Health")
    float CurrentHealth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "Health")
    void ModifyHealth(float Amount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Health")
    void OnDeath();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_SurvivorState, Category = "State")
    ESurvivorState SurvivorState = ESurvivorState::Healthy;

    UFUNCTION(BlueprintCallable, Category = "State")
    void SetSurvivorState(ESurvivorState NewState);

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_CurrentHealth();

    UFUNCTION()
    void OnRep_SurvivorState();
};