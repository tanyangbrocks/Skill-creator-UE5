#include "AMobSpawnController.h"
#include "AEnemyManager.h"
#include "AEnemy.h"
#include "AVoxelWorldActor.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "EngineUtils.h"
#include "Math/UnrealMathUtility.h"
#include "MapGenerator3D.h"
#include "HAL/PlatformProcess.h"

AMobSpawnController::AMobSpawnController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMobSpawnController::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AEnemyManager>    It(GetWorld()); It; ++It) { CachedEnemyMgr    = *It; break; }
    for (TActorIterator<AVoxelWorldActor> It(GetWorld()); It; ++It) { CachedVoxelWorld  = *It; break; }

    if (MobTable.IsEmpty())
        PopulateDefaultTable();
}

void AMobSpawnController::PopulateDefaultTable()
{
    // 預設生成表；在 Editor 的 Details 面板覆寫 MobTable 可完全取代此設定
    MobTable.Add({ EEnemyType::Melee,  ESpawnCategory::Common, 3.f });
    MobTable.Add({ EEnemyType::Ranged, ESpawnCategory::Common, 2.f });
    MobTable.Add({ EEnemyType::Patrol, ESpawnCategory::Common, 2.f });
    MobTable.Add({ EEnemyType::Heavy,  ESpawnCategory::Common, 1.f });
}

void AMobSpawnController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedEnemyMgr) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;

    FVector PawnLoc = PC->GetPawn()->GetActorLocation();
    const int32 WorldH = CachedVoxelWorld ? CachedVoxelWorld->WorldHeight : WorldScale::DefaultWorldHeight;
    FGridPos PlayerPos = WorldScale::WorldToTile(PawnLoc, WorldH);

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

        // Area 怪物：需在指定半徑內
        if (Entry.Category == ESpawnCategory::Area && Entry.AreaRadius > 0)
        {
            int64 ADX = TX - Entry.AreaCenter.X;
            int64 ADZ = TZ - Entry.AreaCenter.Z;
            if (ADX * ADX + ADZ * ADZ > (int64)Entry.AreaRadius * Entry.AreaRadius)
                continue;
        }

        // 2026-06-22 修復（對應 Godot MobSpawnController.cs:124 GetTerrainY?.Invoke(tx,tz)
        // → Main.cs:413 spawner.GetTerrainY = (wx,wz) => _mapGen.GetHeightAt(wx,wz)）：
        // 原先用 TW->HeightEstimator(TX,TZ) 掃描「實際已載入的 tile 資料」找地表，但生成點
        // 距玩家 MinSpawnDist~MaxSpawnDist（32~128 tile）外，幾乎必定還沒被 streaming 載入
        // chunk（GetTile 對未載入 chunk 一律回傳 Air），導致 HeightEstimator 永遠掃到
        // Height-1（找不到任何非 Air 格）→ 後面 SurfaceY>=H-1 檢查必定失敗 → continue。
        // 24 次嘗試全部失敗 → TryFindSpawnPos 永遠回傳 false → 敵人從未真正生成。
        // Godot 版用 MapGenerator 的純雜訊函數 GetHeightAt() 算地表高度，不依賴任何 chunk
        // 是否已載入；UE5 對應的 FMapGenerator3D::GetHeightAt() 也是同樣的純函數
        // （MapGenerator3D.cpp:31-42，只用 WorldSeed+WorldH 算 FastNoiseLite，不查 tile），
        // 改用它就能在生成點 chunk 還沒載入時也正確算出地表高度。
        int32 SurfaceY = CachedVoxelWorld->GetMapGenerator().GetHeightAt(TX, TZ);
        if (SurfaceY <= 0 || SurfaceY >= H - 1) continue;

        // 確保生成點所在 chunk 真的有地形資料（不是只建立空 chunk）——對應 Godot
        // EnsureChunkSync()（同步生成或讀檔），這裡用「排程背景生成＋忙等套用」模擬同步：
        // chunk 數量固定（最多腳下+地表 2 個），生成成本與一般 streaming 單個 chunk 相同，
        // 此函式呼叫頻率受 BaseInterval（預設 8 秒/SpawnRateMultiplier）節流，可接受短暫等待。
        FMapGenerator3D& Gen = CachedVoxelWorld->GetMapGenerator();
        const int32 CCX = FMath::FloorToInt(static_cast<float>(TX) / WorldScale::ChunkSize);
        const int32 CCY = FMath::FloorToInt(static_cast<float>(SurfaceY - 1) / WorldScale::ChunkSize);
        const int32 CCZ = FMath::FloorToInt(static_cast<float>(TZ) / WorldScale::ChunkSize);
        if (!Gen.IsChunkGenerated(FIntVector(CCX, CCY, CCZ)))
        {
            Gen.EnsureChunksAround(*TW, CCX, CCY, CCZ, /*Radius=*/0, /*MaxPerCall=*/1);
            int32 WaitGuard = 0;
            while (Gen.HasPendingChunks() && WaitGuard++ < 200)
            {
                FPlatformProcess::Sleep(0.005f);
                Gen.ApplyPendingChunks(*TW, /*MaxPerFrame=*/999);
            }
        }

        // 找可站立位置：腳下 Air（SurfaceY-1），腳下一格固體（SurfaceY）
        int32 TY = SurfaceY - 1;
        if (TW->GetTile(TX, TY, TZ) == EMaterialType::Air
         && TW->GetTile(TX, SurfaceY, TZ) != EMaterialType::Air)
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
