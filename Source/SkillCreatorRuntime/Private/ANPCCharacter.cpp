#include "ANPCCharacter.h"
#include "FCombatResolver.h"
#include "UCombatantRegistrySubsystem.h"
#include "UElementalAuraComponent.h"
#include "USpecialStatusComponent.h"
#include "AVoxelWorldActor.h"
#include "AEnemyManager.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "AFloatingDamageActor.h"
#include "UNPCMemoryComponent.h"
#include "UNPCPerceptionComponent.h"
#include "UNPCIdentityGeneratorSubsystem.h"
#include "FWorldInterfaceAdapter.h"
#include "UMobMeshRegistry.h"
#include "FPromptBuilder.h"
#include "FNPCResponseParser.h"
#include "UNPCBrainSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ANPCAIController.h"
int32 ANPCCharacter::NextId = 1;

ANPCCharacter::ANPCCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComp->SetCollisionObjectType(ECC_Pawn);
    MeshComp->SetCastShadow(true);

    MemoryComp        = CreateDefaultSubobject<UNPCMemoryComponent>(TEXT("MemoryComp"));
    PerceptionComp    = CreateDefaultSubobject<UNPCPerceptionComponent>(TEXT("PerceptionComp"));
    AuraComp          = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    SpecialStatusComp = CreateDefaultSubobject<USpecialStatusComponent>(TEXT("SpecialStatusComp"));

    // BP_NPCAIController.uasset 尚未建立（需在 Editor 手動建立後賦值），
    // Cook 時 ConstructorHelpers 找不到資產會觸發 CDO Error 使打包失敗。
    // 預設指向純 C++ 版，等 BP 子類建立後可在 Editor 覆寫 AIControllerClass。
    AIControllerClass = ANPCAIController::StaticClass();

    UniqueId = NextId++;
}

void ANPCCharacter::BeginPlay()
{
    Super::BeginPlay();

    CachedVoxelWorld = AVoxelWorldActor::FindInWorld(GetWorld());
    for (TActorIterator<AEnemyManager> It(GetWorld()); It; ++It) { CachedEnemyMgr = *It; break; }

    // IWorldInterface 接通（docs/plan-base-npc-system.md §四，解開 M-NPC-3 遺留缺口）
    WorldAdapter = new FWorldInterfaceAdapter(CachedVoxelWorld, CachedEnemyMgr);
    if (PerceptionComp) PerceptionComp->SetWorldInterface(WorldAdapter);

    if (CachedVoxelWorld)
        SpawnGridPos = GridPosition = WorldScale::WorldToTile(GetActorLocation(), CachedVoxelWorld->WorldHeight);

    if (UCombatantRegistrySubsystem* Reg = GetWorld()->GetSubsystem<UCombatantRegistrySubsystem>())
        Reg->Register(this);
}

void ANPCCharacter::EndPlay(EEndPlayReason::Type Reason)
{
    if (UCombatantRegistrySubsystem* Reg = GetWorld() ? GetWorld()->GetSubsystem<UCombatantRegistrySubsystem>() : nullptr)
        Reg->Unregister(this);
    if (PerceptionComp) PerceptionComp->SetWorldInterface(nullptr);
    delete WorldAdapter;
    WorldAdapter = nullptr;
    Super::EndPlay(Reason);
}

void ANPCCharacter::InitializeIdentity(FName InNPCId, FName SubtypeId)
{
    Identity.NPCId = InNPCId;

    UNPCIdentityGeneratorSubsystem* Gen = UNPCIdentityGeneratorSubsystem::Get(this);
    if (!Gen) return;

    FOnIdentityReady OnReady;
    OnReady.BindLambda([this](const FNPCIdentity& Loaded)
    {
        Identity = Loaded;
        ApplyMeshFromRegistry();
    });
    Gen->LoadOrGenerate(InNPCId, SubtypeId, OnReady);
}

void ANPCCharacter::ApplyMeshFromRegistry()
{
    // 用 RaceId 查表；找不到（素材還沒準備好）時保持目前狀態（裸 SkeletalMeshComponent，
    // 沒有指定 Mesh 不會顯示任何東西，但不會 crash），跟 AVoxelWorldActor 的
    // 「Registry 找不到 fallback 到 VoxelMaterial」同一個容錯精神——這裡沒有預設骨架網格
    // 可以 fallback（專案目前完全沒有任何骨骼模型資產，見 docs/plan-base-npc-system.md
    // §四「附帶發現」），所以只能保持不可見，等真正的模型資產進來再生效。
    const FMobMeshResolved Resolved = FMobMeshRegistry::Find(Identity.RaceId);
    if (!Resolved.IsValid() || !MeshComp) return;

    MeshComp->SetSkeletalMesh(Resolved.Mesh);
    if (Resolved.Material) MeshComp->SetMaterial(0, Resolved.Material);
    if (Resolved.AnimBPClass) MeshComp->SetAnimInstanceClass(Resolved.AnimBPClass);
}

// ── ICombatant 方法 ─────────────────────────────────────────────────────────

UElementalAuraComponent* ANPCCharacter::GetAuraComp() const { return AuraComp; }

// B-3 管線：FCombatResolver 統一公式 → ApplyFinalDamage → TakeDamageAmount
void ANPCCharacter::TakePhysicalDamage(float PhysAtk, const FCharacterStats* Atk, AActor* /*Attacker*/)
{
    FCombatResolver::TakePhysicalDamage(*this, PhysAtk, Atk);
}

void ANPCCharacter::TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* Atk)
{
    FCombatResolver::TakeEnergyDamage(*this, EnergyAtk, ManaTypeKey, Atk);
}

void ANPCCharacter::TakeElementalDamage(float ElemAtk, ESkillElementType Element, bool bEnergyDefenseApplies, const FCharacterStats* Atk)
{
    FCombatResolver::TakeElementalDamage(*this, ElemAtk, Element, bEnergyDefenseApplies, Atk);
}

void ANPCCharacter::TakeDamageAmount(float Amount)
{
    if (!IsAlive()) return;
    Amount = ActionBus.DispatchPlayerDamage(Amount);  // 傷害護盾/DeathGuard 攔截
    Hp = FMath::Max(0.f, Hp - Amount);
    if (Amount <= 0.f) return;

    AFloatingDamageActor::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 50.f), Amount);

    // 被玩家攻擊 → 轉為敵對（docs/plan-base-npc-system.md §五「反擊後的恢復」）
    Disposition = ENPCDisposition::Hostile;
    HostileCooldownRemaining = HostileCooldownDuration;
    if (MemoryComp)
        MemoryComp->AddMemoryEvent(ENPCMemoryCategory::Observe, TEXT("被玩家攻擊"), /*bPermanent=*/false);
}

void ANPCCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (PerceptionComp) PerceptionComp->TickPerception(DeltaTime, GridPosition);

    if (Disposition == ENPCDisposition::Hostile)
    {
        HostileCooldownRemaining -= DeltaTime;
        if (HostileCooldownRemaining <= 0.f)
            Disposition = ENPCDisposition::Neutral;
    }
}

FEntitySnapshot ANPCCharacter::TakeSnapshot() const
{
    FEntitySnapshot Snap;
    Snap.EntityId       = UniqueId;
    Snap.Position       = GridPosition;
    Snap.Hp             = Hp;
    Snap.Mp             = 0.f;
    Snap.bWasAlive      = IsAlive();
    Snap.bHasDisposition = true;
    Snap.DispositionByte = static_cast<uint8>(Disposition);
    return Snap;
}

void ANPCCharacter::TriggerDialogue(const FString& PlayerInput)
{
    UNPCBrainSubsystem* Brain = UNPCBrainSubsystem::Get(this);
    if (!Brain || !Brain->IsReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("ANPCCharacter '%s': TriggerDialogue — Brain not ready"), *Identity.Name);
        return;
    }

    FNPCWorldSnapshot Snapshot;
    if (PerceptionComp)
        Snapshot = PerceptionComp->GetLastSnapshot();

    const TArray<FNPCMessage> Messages = FPromptBuilder::Build(Identity, MemoryComp, Snapshot, PlayerInput);

    TWeakObjectPtr<ANPCCharacter> WeakSelf(this);

    FOnInferenceComplete OnDone;
    OnDone.BindLambda([WeakSelf](const FString& Raw)
    {
        if (ANPCCharacter* Self = WeakSelf.Get())
            Self->HandleBrainResponse(FNPCResponseParser::Parse(Raw));
    });

    FOnInferenceError OnErr;
    OnErr.BindLambda([WeakSelf](int32 Code, const FString& Msg)
    {
        if (const ANPCCharacter* Self = WeakSelf.Get())
            UE_LOG(LogTemp, Warning,
                TEXT("ANPCCharacter '%s': inference error %d: %s"), *Self->Identity.Name, Code, *Msg);
    });

    Brain->SendMessages(Messages, OnDone, OnErr);
}

void ANPCCharacter::HandleBrainResponse(const FNPCBrainResponse& Response)
{
    if (!Response.bValid) return;

    LastDialogue = Response.Dialogue;
    LastEmotion  = Response.Emotion;

    if (!Response.MemoryNote.IsEmpty() && MemoryComp)
        MemoryComp->AddMemoryEvent(ENPCMemoryCategory::Observe, Response.MemoryNote);

    DispatchBrainAction(Response.Action);
}

void ANPCCharacter::DispatchBrainAction(ENPCAction Action)
{
    switch (Action)
    {
    case ENPCAction::Idle:
        break;

    case ENPCAction::Attack:
        Disposition              = ENPCDisposition::Hostile;
        HostileCooldownRemaining = HostileCooldownDuration;
        break;

    case ENPCAction::Flee:
        bFleeRequested = true;
        FollowTarget   = nullptr;
        break;

    case ENPCAction::Follow:
        bFleeRequested = false;
        FollowTarget   = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
        break;

    case ENPCAction::Trade:
        UE_LOG(LogTemp, Log,
            TEXT("ANPCCharacter '%s': Trade — stub (no trade system yet)"), *Identity.Name);
        break;

    case ENPCAction::CastSpell:
        UE_LOG(LogTemp, Log,
            TEXT("ANPCCharacter '%s': CastSpell — stub (no NPC spell slots yet)"), *Identity.Name);
        break;

    case ENPCAction::PlaceTile:
        UE_LOG(LogTemp, Log,
            TEXT("ANPCCharacter '%s': PlaceTile — stub (needs target tile)"), *Identity.Name);
        break;

    case ENPCAction::BreakTile:
        UE_LOG(LogTemp, Log,
            TEXT("ANPCCharacter '%s': BreakTile — stub (needs target tile)"), *Identity.Name);
        break;

    case ENPCAction::GiveItem:
        UE_LOG(LogTemp, Log,
            TEXT("ANPCCharacter '%s': GiveItem — stub (no NPC inventory yet)"), *Identity.Name);
        break;
    }
}

void ANPCCharacter::RestoreFromSnapshot(const FEntitySnapshot& Snap)
{
    GridPosition = Snap.Position;
    Hp           = Snap.Hp;
    if (Snap.bHasDisposition)
        Disposition = static_cast<ENPCDisposition>(Snap.DispositionByte);
    SetActorLocation(WorldScale::TileToWorld(GridPosition,
        CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld()->Height : WorldScale::DefaultWorldHeight));
}
