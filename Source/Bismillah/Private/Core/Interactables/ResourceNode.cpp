// ResourceNode.cpp

#include "Core/Interactables/ResourceNode.h"
#include "Engine/DataTable.h"

AResourceNode::AResourceNode()
{
    PrimaryActorTick.bCanEverTick = false; // flip to true in Milestone 2 when the timer ticks

    // Ensure replication for multiplayer. All future state changes (collection progress,
    // depleted flag, current collector) must be made under HasAuthority() so this stays
    // consistent with the existing server-authoritative pattern used by BismillahSurvivor
    // and BismillahKiller.
    bReplicates = true;
}

void AResourceNode::BeginPlay()
{
    Super::BeginPlay();

    // Resolve the DataTable row once and cache it. This runs on server AND clients,
    // each of which has the DataTable asset locally. No replication of FSampleData needed.
    RefreshSampleData();

    if (!bHasValidSampleData)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode '%s': BeginPlay could not resolve FSampleData (SampleID='%s', SampleDataTable=%s)."),
            *GetName(),
            *SampleID.ToString(),
            SampleDataTable ? *SampleDataTable->GetName() : TEXT("<null>"));
    }
}

bool AResourceNode::RefreshSampleData()
{
    bHasValidSampleData = false;
    CachedSampleData = FSampleData();

    if (!SampleDataTable || SampleID.IsNone())
    {
        return false;
    }

    // DataTable::FindRow looks up by *row name*, not by the SampleID field.
    // We treat the SampleID field as the identity (row name stays cosmetic), so we
    // iterate rows and match on the field. This is the ONLY place that walks the
    // table — everything else reads CachedSampleData.
    static const FString ContextString(TEXT("AResourceNode::RefreshSampleData"));

    TArray<FSampleData*> AllRows;
    SampleDataTable->GetAllRows<FSampleData>(ContextString, AllRows);

    for (const FSampleData* Row : AllRows)
    {
        if (Row && Row->SampleID == SampleID)
        {
            CachedSampleData = *Row;
            bHasValidSampleData = true;
            return true;
        }
    }

    return false;
}

bool AResourceNode::GetSampleData(FSampleData& OutSampleData) const
{
    if (!bHasValidSampleData)
    {
        return false;
    }

    OutSampleData = CachedSampleData;
    return true;
}

void AResourceNode::OnInteract_Implementation(APawn* InstigatorPawn)
{
    // Server-only. The RPC caller (ABismillahSurvivor::Server_Interact_Implementation)
    // already runs on the authority, but we guard defensively so nothing that
    // eventually mutates replicated state slips through on a client.
    if (!HasAuthority())
    {
        return;
    }

    const FString PawnName = InstigatorPawn ? InstigatorPawn->GetName() : FString(TEXT("<null>"));

    // NOTE (Milestone 1): interaction shell only.
    // Future milestones will add: collection timer, disturbance prompt,
    // interruption mini-game hook, and ARC bag deposit — none of that is here.

    if (bHasValidSampleData)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode::OnInteract: node '%s' interacted by '%s' | SampleID=%s DisplayName=%s Rarity=%s ResearchValue=%d BaseCollectionTime=%.2f InterruptionOddsMultiplier=%.2f"),
            *GetName(),
            *PawnName,
            *CachedSampleData.SampleID.ToString(),
            *CachedSampleData.DisplayName.ToString(),
            *UEnum::GetValueAsString(CachedSampleData.Rarity),
            CachedSampleData.ResearchValue,
            CachedSampleData.BaseCollectionTime,
            CachedSampleData.InterruptionOddsMultiplier);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResourceNode::OnInteract: node '%s' interacted by '%s' | WARNING: no FSampleData cached (SampleID='%s', SampleDataTable=%s)"),
            *GetName(),
            *PawnName,
            *SampleID.ToString(),
            SampleDataTable ? *SampleDataTable->GetName() : TEXT("<null>"));
    }
}