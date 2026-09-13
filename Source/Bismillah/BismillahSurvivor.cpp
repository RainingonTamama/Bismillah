// BismillahSurvivor.cpp

#include "BismillahSurvivor.h"
#include "Net/UnrealNetwork.h"

void ABismillahSurvivor::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;

    UE_LOG(LogTemp, Warning, TEXT("Survivor spawned"));
}

void ABismillahSurvivor::ModifyHealth(float Amount)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);

    if (CurrentHealth <= 0.0f)
    {
        OnDeath();
    }
}

float ABismillahSurvivor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (!HasAuthority())
    {
        return 0.0f;
    }

    ModifyHealth(-DamageAmount);
    return DamageAmount;
}

void ABismillahSurvivor::OnRep_CurrentHealth()
{
}

void ABismillahSurvivor::SetSurvivorState(ESurvivorState NewState)
{
    if (!HasAuthority())
    {
        return;
    }

    if (SurvivorState == NewState)
    {
        return;
    }

    SurvivorState = NewState;
}

void ABismillahSurvivor::OnRep_SurvivorState()
{
}

void ABismillahSurvivor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABismillahSurvivor, CurrentHealth);
    DOREPLIFETIME(ABismillahSurvivor, SurvivorState);
}