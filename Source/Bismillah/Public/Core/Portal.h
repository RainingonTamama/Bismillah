// Portal.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

class UStaticMeshComponent;
class ABismillahGameState;

/**
 * A portal that opens when the shared research threshold is reached.
 * Purely presentational — all timing lives on ABismillahGameState.
 * BP children handle the visual (mesh, niagara, materials).
 */
UCLASS()
class BISMILLAH_API APortal : public AActor
{
    GENERATED_BODY()

public:
    APortal();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Slot for the BP child to attach any mesh / effect. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    UStaticMeshComponent* MeshComponent;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portal")
    bool IsOpening() const;

    /** 0..1. 0 when portals aren't opening yet. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portal")
    float GetOpenProgress() const;

    UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
    void OnPortalStartedOpening();

    UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
    void OnPortalProgressChanged(float Progress);

    UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
    void OnPortalFullyOpened();

private:
    TWeakObjectPtr<ABismillahGameState> CachedGameState;
    bool bNotifiedStart = false;
    bool bNotifiedFullyOpen = false;
    float LastReportedProgress = -1.0f;
};