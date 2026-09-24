// BismillahGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BismillahGameState.generated.h"

/**
 * Match-scoped state: shared research progress and portal opening.
 * All gameplay state mutated only under HasAuthority(); clients react via OnRep_*.
 */
UCLASS()
class BISMILLAH_API ABismillahGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ABismillahGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ---- Research tracking --------------------------------------------------

    /** Total research value deposited so far by all survivors. */
    UPROPERTY(ReplicatedUsing = OnRep_DepositedResearchValue, BlueprintReadOnly, Category = "Research")
    int32 DepositedResearchValue = 0;

    /** Threshold that triggers portal opening. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Research")
    int32 RequiredResearchValue = 500;

    /** Server-only. Adds research value and triggers portal opening when the threshold is reached. */
    UFUNCTION(BlueprintCallable, Category = "Research")
    void AddResearchValue(int32 Amount);

    // ---- Portal state -------------------------------------------------------

    /** True once the threshold has been crossed and the portals are opening. */
    UPROPERTY(ReplicatedUsing = OnRep_PortalsOpening, BlueprintReadOnly, Category = "Portal")
    bool bPortalsOpening = false;

    /** Server world time at which portals began opening. 0 when not opening. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Portal")
    float PortalOpenStartTime = 0.0f;

    /** How long the portals take to fully open. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal")
    float PortalOpenDuration = 50.0f;

    /** 0..1. Returns 0 when portals aren't opening. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portal")
    float GetPortalOpenProgress() const;

    /** Seconds left until portals are fully open. 0 when not opening. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portal")
    float GetPortalTimeRemaining() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portal")
    bool IsPortalsOpening() const { return bPortalsOpening; }

    // ---- BP hooks -----------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Research")
    void OnResearchValueChanged(int32 NewValue, int32 RequiredValue);

    UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
    void OnPortalsStartedOpening();

protected:
    UFUNCTION()
    void OnRep_DepositedResearchValue();

    UFUNCTION()
    void OnRep_PortalsOpening();

private:
    /** Server world time, preferring GameState's own time if available. */
    float GetServerWorldTime() const;
};