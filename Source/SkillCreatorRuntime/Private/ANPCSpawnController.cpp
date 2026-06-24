#include "ANPCSpawnController.h"
#include "ANPCCharacter.h"
#include "AVoxelWorldActor.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "EngineUtils.h"
#include "Math/UnrealMathUtility.h"
#include "MapGenerator3D.h"
#include "HAL/PlatformProcess.h"

ANPCSpawnController::ANPCSpawnController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ANPCSpawnController::BeginPlay()
{
    Super::BeginPlay();
    CachedVoxelWorld = AVoxelWorldActor::FindInWorld(GetWorld());

    if (NPCTable.IsEmpty())
        PopulateDefaultTable();
}

void ANPCSpawnController::PopulateDefaultTable()
{
    // 規格目前只有「游蕩詩人」一個已實作的子類（docs/plan-base-npc-system.md §六）
    FNPCTableEntry Bard;
    Bard.SubtypeId = TEXT("WanderingBard");
    Bard.Weight    = 1.f;
    NPCTable.Add(Bard);
}

void ANPCSpawnController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedVoxelWorld) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;

    const FVector PawnLoc = PC->GetPawn()->GetActorLocation();
    const int32 WorldH = CachedVoxelWorld->WorldHeight;
    const FGridPos PlayerPos = WorldScale::WorldToTile(PawnLoc, WorldH);

    HandleDespawns(PlayerPos, DeltaTime);

    SpawnTimer -= DeltaTime * FMath::Max(0.01f, SpawnRateMultiplier);
    if (SpawnTimer > 0.f) return;
    SpawnTimer = BaseInterval / FMath::Max(0.01f, SpawnRateMultiplier);

    if (SpawnedNPCs.Num() >= MaxActiveNPCs) return;

    const FNPCTableEntry* Entry = PickEntry();
    if (!Entry) return;

    FGridPos SpawnPos;
    if (!TryFindSpawnPos(PlayerPos, SpawnPos)) return;

    const FVector WorldLoc = WorldScale::TileToWorld(SpawnPos, WorldH);
    ANPCCharacter* NPC = GetWorld()->SpawnActor<ANPCCharacter>(ANPCCharacter::StaticClass(), FTransform(WorldLoc));
    if (!NPC) return;

    NPC->CachedVoxelWorld = CachedVoxelWorld;
    NPC->GridPosition = NPC->SpawnGridPos = SpawnPos;

    const FName NewNPCId(*FString::Printf(TEXT("NPC_%d"), NextNPCSerial++));
    NPC->InitializeIdentity(NewNPCId, Entry->SubtypeId);

    SpawnedNPCs.Add(NPC);
}

void ANPCSpawnController::HandleDespawns(const FGridPos& PlayerPos, float DeltaTime)
{
    for (int32 i = SpawnedNPCs.Num() - 1; i >= 0; --i)
    {
        ANPCCharacter* NPC = SpawnedNPCs[i];
        if (!IsValid(NPC) || !NPC->IsAlive())
        {
            SpawnedNPCs.RemoveAt(i);
            continue;
        }

        const float D = HorizDist(NPC->GetPosition(), PlayerPos);
        const bool bHardDespawn = D > DespawnHardDist;
        const bool bSoftDespawn = !bHardDespawn && D > DespawnSoftDist
                               && FMath::FRand() < SoftDespawnRate * DeltaTime;

        if (bHardDespawn || bSoftDespawn)
        {
            NPC->Destroy();
            SpawnedNPCs.RemoveAt(i);
        }
    }
}

const FNPCTableEntry* ANPCSpawnController::PickEntry() const
{
    float Total = 0.f;
    for (const FNPCTableEntry& E : NPCTable) Total += E.Weight;
    if (Total <= 0.f) return nullptr;

    float Roll = FMath::FRand() * Total;
    for (const FNPCTableEntry& E : NPCTable)
    {
        Roll -= E.Weight;
        if (Roll <= 0.f) return &E;
    }
    return NPCTable.Num() > 0 ? &NPCTable.Last() : nullptr;
}

bool ANPCSpawnController::TryFindSpawnPos(const FGridPos& Player, FGridPos& OutPos) const
{
    if (!CachedVoxelWorld) return false;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return false;

    const int32 W = TW->Width, H = TW->Height, D = TW->Depth;

    for (int32 Attempt = 0; Attempt < SpawnTriesPerAttempt; ++Attempt)
    {
        const float Angle = FMath::FRand() * 2.f * PI;
        const int32 HDist = MinSpawnDist + FMath::RandRange(0, MaxSpawnDist - MinSpawnDist);
        const int32 TX = FMath::Clamp(Player.X + FMath::RoundToInt(FMath::Cos(Angle) * HDist), 1, W - 2);
        const int32 TZ = FMath::Clamp(Player.Z + FMath::RoundToInt(FMath::Sin(Angle) * HDist), 1, D - 2);

        // 同 AMobSpawnController::TryFindSpawnPos 的 chunk 載入修復（2026-06-22）：
        // 用 FMapGenerator3D::GetHeightAt() 純函數算地表高度，不依賴 chunk 是否已 streaming 載入。
        const int32 SurfaceY = CachedVoxelWorld->GetMapGenerator().GetHeightAt(TX, TZ);
        if (SurfaceY <= 0 || SurfaceY >= H - 1) continue;

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

        const int32 TY = SurfaceY - 1;
        if (TW->GetTile(TX, TY, TZ) == EMaterialType::Air
         && TW->GetTile(TX, SurfaceY, TZ) != EMaterialType::Air)
        {
            OutPos = FGridPos(TX, TY, TZ);
            return true;
        }
    }
    return false;
}

float ANPCSpawnController::HorizDist(const FGridPos& A, const FGridPos& B)
{
    const float DX = (float)(A.X - B.X), DZ = (float)(A.Z - B.Z);
    return FMath::Sqrt(DX * DX + DZ * DZ);
}
