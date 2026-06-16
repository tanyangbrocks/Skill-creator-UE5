#include "AMobSpawnController.h"
#include "AEnemyManager.h"
#include "AEnemy.h"
#include "AVoxelWorldActor.h"
#include "MaterialType.h"
#include "EngineUtils.h"
#include "Math/UnrealMathUtility.h"

AMobSpawnController::AMobSpawnController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMobSpawnController::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AEnemyManager>    It(GetWorld()); It; ++It) { CachedEnemyMgr    = *It; break; }
    for (TActorIterator<AVoxelWorldActor> It(GetWorld()); It; ++It) { CachedVoxelWorld  = *It; break; }
}

void AMobSpawnController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedEnemyMgr) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;

    FVector PawnLoc = PC->GetPawn()->GetActorLocation();
    FGridPos PlayerPos(FMath::RoundToInt(PawnLoc.X), FMath::RoundToInt(PawnLoc.Y), FMath::RoundToInt(PawnLoc.Z));

    HandleDespawns(PlayerPos, DeltaTime);

    SpawnTimer -= DeltaTime * FMath::Max(0.01f, SpawnRateMultiplier);
    if (SpawnTimer > 0.f) return;
    SpawnTimer = BaseInterval / FMath::Max(0.01f, SpawnRateMultiplier);

    if (CachedEnemyMgr->GetDynamicActiveCount() >= MaxCommonActive) return;

    const FMobTableEntry* Entry = PickEntry();
    if (!Entry) return;

    FGridPos SpawnPos;
    if (TryFindSpawnPos(PlayerPos, *Entry, SpawnPos))
        CachedEnemyMgr->Spawn(SpawnPos, Entry->Type, -1.f, Entry->Category);
}

void AMobSpawnController::HandleDespawns(const FGridPos& PlayerPos, float DeltaTime)
{
    for (AEnemy* E : CachedEnemyMgr->GetEnemies())
    {
        if (!IsValid(E) || !E->IsAlive()) continue;
        if (E->Category != ESpawnCategory::Common && E->Category != ESpawnCategory::Area) continue;

        float D = HorizDist(E->GridPosition, PlayerPos);
        bool bHardDespawn = D > DespawnHardDist;
        bool bSoftDespawn = !bHardDespawn && D > DespawnSoftDist
                         && FMath::FRand() < SoftDespawnRate * DeltaTime;

        if (bHardDespawn || bSoftDespawn)
            E->ForceDespawn();
    }
}

const FMobTableEntry* AMobSpawnController::PickEntry() const
{
    // 只從 Common / Area 條目加權抽選
    float Total = 0.f;
    for (const FMobTableEntry& E : MobTable)
        if (E.Category == ESpawnCategory::Common || E.Category == ESpawnCategory::Area)
            Total += E.Weight;

    if (Total <= 0.f) return nullptr;

    float Roll = FMath::FRand() * Total;
    for (const FMobTableEntry& E : MobTable)
    {
        if (E.Category != ESpawnCategory::Common && E.Category != ESpawnCategory::Area) continue;
        Roll -= E.Weight;
        if (Roll <= 0.f) return &E;
    }
    return MobTable.Num() > 0 ? &MobTable.Last() : nullptr;
}

bool AMobSpawnController::TryFindSpawnPos(const FGridPos& Player,
                                          const FMobTableEntry& Entry, FGridPos& OutPos) const
{
    if (!CachedVoxelWorld) return false;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return false;

    const int32 W = TW->Width, H = TW->Height, D = TW->Depth;

    for (int32 Attempt = 0; Attempt < SpawnTriesPerAttempt; ++Attempt)
    {
        float Angle = FMath::FRand() * 2.f * PI;
        int32 HDist = MinSpawnDist + FMath::RandRange(0, MaxSpawnDist - MinSpawnDist);
        int32 TX = FMath::Clamp(Player.X + FMath::RoundToInt(FMath::Cos(Angle) * HDist), 1, W - 2);
        int32 TZ = FMath::Clamp(Player.Z + FMath::RoundToInt(FMath::Sin(Angle) * HDist), 1, D - 2);

        // 從玩家高度往下找第一個 Air tile 緊接固體的位置
        int32 TY = FMath::Clamp(Player.Y + 2, 0, H - 2);

        // Area 怪物：需在指定半徑內
        if (Entry.Category == ESpawnCategory::Area && Entry.AreaRadius > 0)
        {
            int64 ADX = TX - Entry.AreaCenter.X;
            int64 ADZ = TZ - Entry.AreaCenter.Z;
            if (ADX * ADX + ADZ * ADZ > (int64)Entry.AreaRadius * Entry.AreaRadius)
                continue;
        }

        // 找可站立位置：腳下 Air，腳下一格固體
        if (TW->GetTile(TX, TY, TZ) == EMaterialType::Air
         && TW->GetTile(TX, TY + 1, TZ) != EMaterialType::Air)
        {
            OutPos = FGridPos(TX, TY, TZ);
            return true;
        }
    }
    return false;
}

float AMobSpawnController::HorizDist(const FGridPos& A, const FGridPos& B)
{
    float DX = (float)(A.X - B.X), DZ = (float)(A.Z - B.Z);
    return FMath::Sqrt(DX * DX + DZ * DZ);
}
