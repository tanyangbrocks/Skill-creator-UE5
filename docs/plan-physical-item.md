# 實體物品系統實作計畫

> 建立日期：2026-06-26
> 狀態：📋 計畫中
> 來源：`origin text setting/real object.txt`
> 關聯：`plan-gravity-system.md`（Phase 1-D 接口）、`plan-debris-fragment.md`（碎塊 IS 實體物品）

---

## 一、術語

| 術語 | 說明 |
|------|------|
| **實體物品（Physical Item）** | 存在世界中、有物理質量、可被拿取/投擲的可見物件。`APhysicalItemActor` 或 `ADebrisActor` |
| **拿取狀態（Carrying）** | 玩家手持實體物品期間的特殊狀態（`bool bIsCarrying`）。可移動、可施法，**不可基礎攻擊** |
| **掉落物（Dropped Item）** | `ADroppedItemActor`：靜態、無物理，由採掘/F 短按產生，靠近自動吸取 → 物品欄 |
| **碎塊（Debris）** | `ADebrisActor`：爆炸/斬擊/碾壓產生的物理飛行 Actor（D-1~D-5 已完成）。**本質上是實體物品**，未來整合至拿取系統（G-10）|
| **碎片（Fragment）** | `EItemId::FragmentXxx`：物品欄物品，撿起碎塊後換算而來 |
| **IPhysicalPickable** | 純 C++ 介面，讓未來的生物/投射物也能被玩家拿取，不寫死到 `APhysicalItemActor` |

### 系統邊界圖

```
ADroppedItemActor   採掘掉落物，靜態，靠近自動撿 ─── 無物理，獨立系統
APhysicalItemActor  實體物品，手動物理 tick ────────── 本計畫主角
ADebrisActor        碎塊，UE5 Chaos 物理 ──────────── 已有，G-10 整合撿取
```

---

## 二、現有架構盤點

### ✅ 已存在，可直接使用

| 項目 | 位置 | 說明 |
|------|------|------|
| `FItemData.Mass` | `ItemData.h:59` | `float Mass = 0.1f`，已存在於物品定義 |
| `FCharacterStats.Strength` | `CharacterStats.h:65` | 玩家力量，影響投擲速度 |
| `FCharacterStats.Mass` | `CharacterStats.h:231` | 角色質量（用於墜落傷害，不影響投擲） |
| `ADebrisActor` | `Public/ADebrisActor.h` | D-1~D-5 完成，Chaos 物理，`LaunchImpulse()` |
| `IInteractable` | `Public/IInteractable.h` | 右鍵互動介面（寶箱/工作臺），不同於撿取 |
| `WorldScale::GravityScaleMult` | `WorldScale.h:55` | `TileSizeCm / 30.f`，角色 CharacterMovement 用 |

### ❌ 缺口

| 項目 | 說明 |
|------|------|
| `WorldScale::GlobalGravityScale` | **尚未存在**（plan-gravity-system Phase 1-A 未做）；APhysicalItemActor 需要它 |
| `APhysicalItemActor` | 不存在，本計畫主角 |
| `IPhysicalPickable` | 不存在，需新建（給未來可持生物/投射物留擴充點） |
| G 鍵空出 | 目前 G = `OnOpenPlayerPanel`，需移到 T 鍵 |
| F 鍵長/短按分離 | 目前 F = 無條件短按丟出 `DroppedItem`，缺少長按投擲 `PhysicalItem` 路徑 |
| 拿取狀態 | `EPlayerMovementState` 沒有 Carrying，角色也無 `bIsCarrying` |
| 拿取提示 HUD | 近物體時畫面出現「G」按鍵圖示 |
| 投擲力量條 UI | `UPhysicalThrowWidget`：右側縱向力量條 + 循環指針 |

---

## 三、架構決策

### 3-A. 物理實作：手動 Tick 而非 Chaos SimulatePhysics

`ADebrisActor` 選用 `SimulatePhysics=true`（Chaos 剛體），是因為碎塊需要與幾何體碰撞的視覺擬真度。

`APhysicalItemActor` 改用**手動 FVector Velocity 積分**，原因：
- 直接讀 tile 格 → 支援在 CA 液體中浮沉、碰到材質彈跳（Restitution）
- 直接讀 `GlobalGravityScale` → 重力異常區效果自動生效
- 格解析度夠粗（6.25cm/tile），Chaos 的連續碰撞精度浪費

```
UE5 座標：Z = 上，重力 = -Z
Velocity.Z -= GlobalGravityScale * 980.f * WorldScale::GravityScaleMult * DeltaTime
```

### 3-B. 拿取狀態：bool 而非 EPlayerMovementState 新值

拿取狀態可疊加在現有所有移動狀態上（走路、蹲伏、飛行中都可拿東西），
用**獨立 bool `bIsCarrying`** 而非在 `EPlayerMovementState` 加新值，避免所有 switch-case 爆炸。

```cpp
// ASkillCreatorCharacter
bool                bIsCarrying = false;
TWeakObjectPtr<AActor> CarriedActor;  // 持有 IPhysicalPickable 的 Actor
```

### 3-C. IPhysicalPickable 純 C++ 介面

文件明確標注「未來會希望生物或投射物也可以被拿取，代碼不要寫太死」。
用純虛擬基類（非 UINTERFACE），保持低耦合：

```cpp
class IPhysicalPickable  // 純 C++，不 UINTERFACE
{
public:
    virtual ~IPhysicalPickable() = default;
    virtual float    GetMass()           const = 0;  // kg 等比
    virtual EItemId  GetInventoryItemId() const = 0;  // 存入物品欄的 ItemId（None = 不能存欄）
    virtual int32    GetInventoryCount() const = 0;  // 存入數量
    virtual FText    GetPickupHintText() const = 0;  // HUD 提示文字
    virtual void     OnCarried(ASkillCreatorCharacter* Carrier) = 0;
    virtual void     OnReleased(FVector ThrowVelocityCms) = 0;  // 0 = 放下（非投擲）
};
```

`APhysicalItemActor` 和（G-10 時的）`ADebrisActor` 各自實作這個介面。

### 3-D. G 鍵脈絡感知（Context-Sensitive）

G 鍵現在做三件事，依脈絡判斷：

| 當前狀態 | G 按下結果 |
|---------|-----------|
| 未攜帶，面前有可撿物 | 進入拿取狀態（撿起） |
| 攜帶中 | 將實體物品轉成物品欄（放入背包） |
| 未攜帶，面前無可撿物 | 開啟玩家個人面板（原本的 G 行為，已移至 T 鍵） |

實際上：把 G 的行為完全接管給 `OnPickupOrPanel()`，讓這個函式依脈絡決定。

### 3-E. F 鍵長/短按分離

```
F 短按（< 200ms）→ 短按路徑（現有）：
  ├─ 未攜帶 → DropCurrentItem（掉落物 ADroppedItemActor）
  └─ 攜帶中 → 同上（丟出攜帶物但力量=0？還是視為放下？→ 依短按定義，放下）

F 長按（≥ 200ms，持續按著）：
  ├─ 未攜帶 → 從熱鍵欄當前物品 spawn APhysicalItemActor → 進拿取狀態 → 繼續走長按流程
  └─ 攜帶中 → 直接進長按流程
  長按流程：顯示力量條（UPhysicalThrowWidget）→ 放開 F → 投擲
```

---

## 四、投擲速度公式

```
ThrowSpeed (cm/s) = BaseThrowSpeed × PowerPct × (Stats.Strength / FMath::Max(0.1f, ItemMass))

BaseThrowSpeed = 200.f × WorldScale::TileSizeCm   // ≈ 1250 cm/s @ Grain=16，可在設計時調整
PowerPct       = 0.01 ~ 1.0（力量條指針高度百分比）
Strength       = FCharacterStats::Strength（0~∞，玩家力量）
ItemMass       = IPhysicalPickable::GetMass()
```

**上限**：`MaxThrowSpeedCm = 5000.f`（防止超強玩家把物品打出地圖）

飛行中受重力影響（由 APhysicalItemActor 的 Tick 繼續積分），形成拋物線。

### 碎塊撿取特例

`ADebrisActor` 實作 `IPhysicalPickable`（G-10 步驟），其 `GetInventoryItemId()` 回傳
`FragmentItemId`，`GetInventoryCount()` 回傳 `FragmentCount`（已算好，存在 `ADebrisActor` 上）。

---

## 五、Phase 實作步驟

```
G-0   G→T 鍵位調整（前置）
G-1   GlobalGravityScale 前置（plan-gravity-system Phase 1-A）
G-2   IPhysicalPickable 純 C++ 介面
G-3   APhysicalItemActor 基礎 Actor（手動物理 tick）
G-4   拿取狀態（bIsCarrying + CarriedActor）
G-5   G 鍵脈絡感知撿取 + 存入物品欄
G-6   拿取提示 HUD
G-7   UPhysicalThrowWidget（力量條 UI）
G-8   投擲邏輯（F 長按 → 投擲 + 速度計算）
G-9   非攜帶狀態長按 F → 從物品欄投擲
G-10  ADebrisActor 整合（碎塊可撿取）
```

---

## 六、Phase 詳細設計

### G-0：G→T 鍵位調整

**修改 `ASkillCreatorPlayerController.cpp`**

```cpp
// 原：Bind(EKeys::G, &ASkillCreatorPlayerController::OnOpenPlayerPanel);
Bind(EKeys::T, &ASkillCreatorPlayerController::OnOpenPlayerPanel);
Bind(EKeys::G, &ASkillCreatorPlayerController::OnPickupOrPanel);
```

**修改 `ASkillCreatorPlayerController.h`**：加 `void OnPickupOrPanel();`（G-5 實作）

**修改 `ASkillCreatorHUD.h` 注釋**：`TogglePlayerPanel()` 改注解「T 鍵」

---

### G-1：GlobalGravityScale（plan-gravity-system Phase 1-A）

**修改 `WorldScale.h`**，在 `GravityScaleMult` 後面加：

```cpp
// 全域重力倍率（1.0=正常；0.5=月球；2.0=重力異常區）
// 未來 per-region 版本：AVoxelWorldActor 持有 TArray<FGravityZone>
inline float GlobalGravityScale = 1.0f;
```

**注意**：`inline` 變數在 C++17 可直接定義在 header（UE5 開 C++17）。

---

### G-2：IPhysicalPickable 純 C++ 介面

**新建 `Source/SkillCreatorRuntime/Public/IPhysicalPickable.h`**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "ItemId.h"

class ASkillCreatorCharacter;

// 純 C++ 介面（不 UINTERFACE）：實體物品撿取系統。
// 目前由 APhysicalItemActor 實作；未來 ADebrisActor / ABeastCharacter 也可實作（G-10+）。
class SKILLCREATORRUNTIME_API IPhysicalPickable
{
public:
    virtual ~IPhysicalPickable() = default;
    virtual float   GetMass()            const = 0;  // 投擲公式用
    virtual EItemId GetInventoryItemId() const = 0;  // EItemId::None = 無法存入物品欄
    virtual int32   GetInventoryCount()  const = 0;  // 存幾個
    virtual FText   GetPickupHintText()  const = 0;  // HUD 顯示「按 G 拿取 XXX」
    virtual void    OnCarried(ASkillCreatorCharacter* Carrier) = 0;  // 進入攜帶狀態
    virtual void    OnReleased(FVector ThrowVelocityCms) = 0;        // 放下/投擲
};
```

---

### G-3：APhysicalItemActor 基礎 Actor

**新建 `Source/SkillCreatorRuntime/Public/APhysicalItemActor.h`**

```cpp
UCLASS()
class SKILLCREATORRUNTIME_API APhysicalItemActor : public AActor, public IPhysicalPickable
{
    GENERATED_BODY()
public:
    APhysicalItemActor();

    // 初始化（由 UDroppedItemManager 或投擲邏輯呼叫）
    void Init(EItemId InId, int32 InCount, FVector InitialVelocityCms = FVector::ZeroVector);

    // IPhysicalPickable
    float   GetMass()            const override;
    EItemId GetInventoryItemId() const override { return ItemId; }
    int32   GetInventoryCount()  const override { return Count; }
    FText   GetPickupHintText()  const override;
    void    OnCarried(ASkillCreatorCharacter* Carrier) override;
    void    OnReleased(FVector ThrowVelocityCms) override;

    // 是否可被撿取（未在攜帶狀態中）
    bool IsPickable() const { return !bBeingCarried; }
    bool CanPickup(FVector PlayerWorldPos) const;

    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY() UStaticMeshComponent* MeshComp = nullptr;
    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    EItemId ItemId  = EItemId::None;
    int32   Count   = 1;
    bool    bBeingCarried = false;

    // 手動物理
    FVector Velocity = FVector::ZeroVector;  // cm/s
    bool    bLanded  = false;

    void PhysicsTick(float DeltaTime);  // 重力積分 + tile 碰撞
    void HandleTileCollision();         // 落地 Restitution 反彈
};
```

**物理 Tick 核心邏輯（`.cpp`）**：

```cpp
void APhysicalItemActor::PhysicsTick(float DeltaTime)
{
    if (bBeingCarried) return;

    // 重力（UE5 Z=上，-Z=向下）
    Velocity.Z -= WorldScale::GlobalGravityScale * 980.f * WorldScale::GravityScaleMult * DeltaTime;

    FVector NewPos = GetActorLocation() + Velocity * DeltaTime;
    FGridPos NewTile = WorldScale::WorldToTile(NewPos);

    // 查 tile 碰撞（用 CachedVoxelWorld->GetCell 或 TileWorld3D）
    if (IsSolidTile(NewTile))
    {
        HandleTileCollision();  // 依腳下材質 Restitution 計算反彈
    }
    else
    {
        SetActorLocation(NewPos);
    }
}

void APhysicalItemActor::HandleTileCollision()
{
    // 取腳下 tile 的 Restitution（來自 FMaterialRegistry::GetRestitution(Mat)）
    float R = GetBelowRestitution();  // 0~1
    Velocity.Z = -Velocity.Z * R;     // 反彈
    Velocity.X *= 0.8f;               // 水平摩擦
    Velocity.Y *= 0.8f;
    if (FMath::Abs(Velocity.Z) < 5.f) // 速度夠低 → 停止彈跳
    {
        Velocity = FVector::ZeroVector;
        bLanded  = true;
    }
}
```

**浮沉判斷**（簡化 Phase 1）：若腳下 tile 是 Liquid 類材質，施加向上浮力抵消重力。

---

### G-4：拿取狀態

**修改 `ASkillCreatorCharacter.h`**：

```cpp
// 拿取狀態（獨立 bool，可疊加在任意 MovementState 上）
bool bIsCarrying = false;
TWeakObjectPtr<AActor> CarriedActor;  // 實作 IPhysicalPickable

bool IsCarrying() const { return bIsCarrying && CarriedActor.IsValid(); }
```

**修改 `ASkillCreatorCharacter.cpp`**：

```cpp
// BeginCarry() — 由 G-5 的 OnPickupOrPanel 呼叫
void ASkillCreatorCharacter::BeginCarry(AActor* Target)
{
    IPhysicalPickable* P = Cast<IPhysicalPickable>(Target);
    if (!P) return;
    bIsCarrying = true;
    CarriedActor = Target;
    P->OnCarried(this);
}

// EndCarry() — 由 G-5 存入物品欄，或 G-8 投擲時呼叫
void ASkillCreatorCharacter::EndCarry(FVector ThrowVelocityCms)
{
    if (!CarriedActor.IsValid()) { bIsCarrying = false; return; }
    IPhysicalPickable* P = Cast<IPhysicalPickable>(CarriedActor.Get());
    if (P) P->OnReleased(ThrowVelocityCms);
    bIsCarrying  = false;
    CarriedActor = nullptr;
}
```

**攻擊阻斷（`OnAttackPressed`）**：

```cpp
// ASkillCreatorPlayerController::OnAttackPressed()
if (Char->bIsCarrying) return;  // 拿取中不可基礎攻擊
```

---

### G-5：G 鍵脈絡感知 + 存入物品欄

**新增 `OnPickupOrPanel()` 至 PlayerController**：

```cpp
void ASkillCreatorPlayerController::OnPickupOrPanel()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    if (Char->bIsCarrying)
    {
        // 攜帶中 G → 存入物品欄
        IPhysicalPickable* P = Cast<IPhysicalPickable>(Char->CarriedActor.Get());
        if (P && P->GetInventoryItemId() != EItemId::None)
            Char->InventoryComp->TryAdd(P->GetInventoryItemId(), P->GetInventoryCount());
        Char->EndCarry(FVector::ZeroVector);  // 速度=0 = 放下（不投擲）
        return;
    }

    // 搜尋附近可撿物
    if (AActor* Nearest = Char->FindNearestPickable())
    {
        Char->BeginCarry(Nearest);
        return;
    }

    // 附近無物 → 開玩家面板（原 G 行為）
    if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
        HUD->TogglePlayerPanel();
}
```

**`FindNearestPickable()`（角色端）**：

```cpp
AActor* ASkillCreatorCharacter::FindNearestPickable() const
{
    const float PickupRadiusCm = WorldScale::TileSizeCm * 3.f;  // 3 tile 撿取半徑
    TArray<AActor*> Overlaps;
    GetOverlappingActors(Overlaps);
    AActor* Best = nullptr; float BestDist = PickupRadiusCm;
    for (AActor* A : Overlaps)
    {
        if (!A->Implements<IPhysicalPickable>()) continue;
        IPhysicalPickable* P = Cast<IPhysicalPickable>(A);
        if (!P || /* APhysicalItemActor check */ !IsPickable(A)) continue;
        float D = FVector::Dist(GetActorLocation(), A->GetActorLocation());
        if (D < BestDist) { BestDist = D; Best = A; }
    }
    return Best;
}
```

> 實作注意：`GetOverlappingActors()` 需要 APhysicalItemActor 上有 sphere collision primitive。
> 加一個 `USphereComponent* PickupSphere`（半徑 = PickupRadiusCm），設為 `QueryOnly`，
> 不需要物理模擬。

---

### G-6：拿取提示 HUD

**修改 `ASkillCreatorHUD.cpp → DrawHUD()`**：

```cpp
// 每幀掃（開銷低，DrawHUD 裡輕量 Cast + 距離算法）
if (ASkillCreatorCharacter* Char = ...)
{
    if (!Char->bIsCarrying)
    {
        if (AActor* Near = Char->FindNearestPickable())
        {
            IPhysicalPickable* P = Cast<IPhysicalPickable>(Near);
            FText Hint = P ? P->GetPickupHintText() : FText::FromString(TEXT("G 拿取"));
            // DrawText() 顯示在準心附近或畫面左下角
            DrawText(Hint.ToString(), FColor::White, ScreenW * 0.5f, ScreenH * 0.7f, ...);
        }
    }
}
```

> 不需要新 Widget；HUD `DrawText()` 夠用。

---

### G-7：UPhysicalThrowWidget（力量條 UI）

**新建 `Source/SkillCreatorRuntime/Public/UPhysicalThrowWidget.h`**

外觀：
- 畫面右側固定位置，縱向長條（高 200px，寬 20px）
- 深色背景 + 亮色填充（從底部到指針位置）
- 指針（白色細橫線）以固定速度從下到上循環（3 秒一個來回）

```cpp
UCLASS()
class UPhysicalThrowWidget : public UUserWidget
{
public:
    // 啟動（開始循環），停止（固定指針位置，顯示最終力道%）
    void StartCharging();
    void StopCharging();
    float GetPowerPct() const;  // 0.0~1.0，當前指針位置
    void  Reset();              // 隱藏並重置

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geo, float Delta) override;
private:
    float CycleTime   = 3.f;    // 一個來回秒數
    float CurrentT    = 0.f;    // 目前位置計時（0~CycleTime）
    bool  bCharging   = false;
    float LockedPct   = 0.f;    // StopCharging 時鎖定的百分比

    TObjectPtr<UBorder>    BarBackground;
    TObjectPtr<UBorder>    BarFill;
    TObjectPtr<UBorder>    Pointer;  // 指針
    TObjectPtr<UCanvasPanel> Root;
};
```

指針位置計算（`NativeTick`）：

```cpp
if (bCharging)
{
    CurrentT = FMath::Fmod(CurrentT + Delta, CycleTime);
    // 0~CycleTime/2 → 向上；CycleTime/2~CycleTime → 向下
    float NormT = CurrentT / (CycleTime * 0.5f);
    float Pct   = NormT <= 1.f ? NormT : 2.f - NormT;  // 0→1→0
    // 更新 Pointer slot 的 Position.Y（UE5 Y 向下，所以 Pct 大 → 頂部 → Y 小）
    float BarH  = 200.f;
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Pointer->Slot))
        S->SetPosition(FVector2D(0.f, BarH * (1.f - Pct) - 2.f));  // -2 = 指針厚度修正
}
```

---

### G-8：投擲邏輯（攜帶中 F 長/短按）

**F 鍵改為 Pressed + Released（修改 PlayerController）**：

```cpp
// SetupInputComponent()
InputComponent->BindKey(EKeys::F, IE_Pressed,  this, &ASkillCreatorPlayerController::OnDropPressed);
InputComponent->BindKey(EKeys::F, IE_Released, this, &ASkillCreatorPlayerController::OnDropReleased);
// 移除原本：Bind(EKeys::F, &ASkillCreatorPlayerController::OnDropCurrentItem);
```

```cpp
void ASkillCreatorPlayerController::OnDropPressed()
{
    FHoldStart = GetWorld()->GetTimeSeconds();
    // 立刻啟動力量條（攜帶中才顯示；非攜帶狀態延遲確認長按再顯示）
    if (ASkillCreatorCharacter* Char = ... ; Char->bIsCarrying)
        if (ASkillCreatorHUD* HUD = ...) HUD->StartThrowCharge();
}

void ASkillCreatorPlayerController::OnDropReleased()
{
    const float HoldSec = GetWorld()->GetTimeSeconds() - FHoldStart;
    ASkillCreatorCharacter* Char = ...;
    if (!Char) return;

    if (HoldSec < 0.2f)
    {
        // 短按 → 放下 / 丟掉落物（原行為）
        if (Char->bIsCarrying)
            Char->EndCarry(FVector::ZeroVector);  // 放下，無初速
        else
            DropCurrentItemAsDropped();  // 原 OnDropCurrentItem 邏輯
    }
    else
    {
        // 長按 → 投擲
        if (ASkillCreatorHUD* HUD = ...) HUD->FinishThrowCharge(Char);
    }
    FHoldStart = -1.f;
}
```

**`ASkillCreatorHUD::FinishThrowCharge(ASkillCreatorCharacter* Char)`**：

```cpp
void ASkillCreatorHUD::FinishThrowCharge(ASkillCreatorCharacter* Char)
{
    if (!ThrowWidget || !Char) return;
    const float PowerPct = ThrowWidget->GetPowerPct();
    ThrowWidget->StopCharging();
    ThrowWidget->SetVisibility(ESlateVisibility::Collapsed);

    if (!Char->bIsCarrying) return;

    // 投擲速度計算
    APlayerController* PC = GetOwningPlayerController();
    FVector AimDir = PC ? PC->GetControlRotation().Vector() : Char->GetActorForwardVector();

    const float Strength  = Char->CharacterStateComp
        ? Char->CharacterStateComp->Stats.Strength : 50.f;
    IPhysicalPickable* P  = Cast<IPhysicalPickable>(Char->CarriedActor.Get());
    const float Mass      = P ? P->GetMass() : 1.f;

    constexpr float Base  = 200.f * WorldScale::TileSizeCm;
    const float Speed     = FMath::Clamp(Base * PowerPct * Strength / FMath::Max(0.1f, Mass),
                                          0.f, 5000.f);
    Char->EndCarry(AimDir * Speed);
}
```

---

### G-9：從物品欄長按 F 投擲（非攜帶狀態）

在 `OnDropPressed()` 裡，若非攜帶中且按住 > 0.3 秒（`OnDropReleased` 判定）：

```cpp
// OnDropReleased 長按路徑，Char->bIsCarrying==false
// 先 spawn APhysicalItemActor（從熱鍵欄）→ BeginCarry → 立刻投擲
if (HoldSec >= 0.2f && !Char->bIsCarrying)
{
    const int32 Idx    = Char->InventoryComp->ActiveHotbarIndex;
    const FItemStack S = Char->InventoryComp->Slots[Idx];
    if (!S.IsEmpty())
    {
        // 生成實體物品
        FVector SpawnPos = Char->GetActorLocation()
                         + Char->GetActorForwardVector() * WorldScale::TileSizeCm * 1.5f;
        APhysicalItemActor* PA = GetWorld()->SpawnActor<APhysicalItemActor>(SpawnPos, FRotator::ZeroRotator);
        if (PA)
        {
            PA->Init(S.ItemId, S.Count);
            Char->InventoryComp->Consume(Idx, S.Count);
            Char->BeginCarry(PA);
            // 立刻投擲（不需要再等力量條；力量條已在 OnDropPressed 開始計時）
            FinishThrowCharge(Char);
        }
    }
}
```

> ⚠️ **邊緣情況**：物品欄空格長按 F → 什麼都不做。

---

### G-10：ADebrisActor 整合（碎塊可撿取）

> **前置**：G-2 ~ G-5 完成後才做。

1. `ADebrisActor` 繼承 `IPhysicalPickable`
2. 實作 `GetMass()`（從材質密度或固定值推算）
3. `GetInventoryItemId()` → `FragmentItemId`
4. `GetInventoryCount()`  → `FragmentCount`
5. `GetPickupHintText()`  → `TEXT("G 拿取碎塊")`
6. `OnCarried(Carrier)` → 停止 Chaos 物理（`Mesh->SetSimulatePhysics(false)`）+ 跟隨角色
7. `OnReleased(Vel)` → 恢復 Chaos 物理（`SetSimulatePhysics(true)`）+ 給 `AddImpulse(Vel * Mass)`
8. 加 `USphereComponent* PickupSphere` 給 `FindNearestPickable()` 偵測

---

## 七、各 Phase 依賴關係

```
G-0 (G→T) ──────────────────────────────────────────────────► G-5 (OnPickupOrPanel 接管 G)
G-1 (GlobalGravityScale) ──────────────────────────────────► G-3 (PhysicsTick 讀取)
G-2 (IPhysicalPickable) ──► G-3 ──► G-4 ──► G-5 ──► G-6
                                              G-5 ──► G-8 ──► G-9
                                              G-7 (ThrowWidget) ──► G-8
G-3 + G-10 ─────────────────────────────────────────────────► 碎塊可撿
```

---

## 八、實作順序

```
[ ] G-0  G→T 鍵位（5 分鐘，一行改動）
[ ] G-1  WorldScale::GlobalGravityScale（同步完成 plan-gravity-system Phase 1-A）
[ ] G-2  IPhysicalPickable.h 純介面
[ ] G-3  APhysicalItemActor（手動物理 + PickupSphere）
[ ] G-4  bIsCarrying + BeginCarry/EndCarry + 攻擊阻斷
[ ] G-5  OnPickupOrPanel() 脈絡感知 G 鍵 + FindNearestPickable
[ ] G-6  拿取提示 HUD（DrawHUD DrawText）
[ ] G-7  UPhysicalThrowWidget 力量條 UI
[ ] G-8  F Pressed/Released + FinishThrowCharge（攜帶中投擲）
[ ] G-9  非攜帶狀態長按 F → 從物品欄投擲
──── 以下未來待辦 ────
[ ] G-10 ADebrisActor 實作 IPhysicalPickable（碎塊可撿）
[ ] G-11 浮沉判斷（液體材質浮力精確版）
[ ] G-12 爆炸飛散接入（炸到實體物品 → AddImpulse 而非無視）
[ ] G-13 生物/投射物可撿取（IPhysicalPickable 擴充）
```

---

## 九、與相關計畫的對照

| 計畫 | 對應點 |
|------|--------|
| `plan-gravity-system.md` | G-1 完成 Phase 1-A；G-3 Tick 完成 Phase 1-D |
| `plan-debris-fragment.md` | G-10 讓 ADebrisActor 也進拿取系統 |
| `plan-player-actions.md` | G-4 攻擊阻斷（`bIsCarrying` check in `OnAttackPressed`）|
| `plan-item-crafting-system.md` | G-9 長按 F 從物品欄生成實體物品，消耗物品欄 |

---

## 十、待確認事項（設計層）

| 事項 | 建議 | 說明 |
|------|------|------|
| 短按 F 時，攜帶中的行為 | 「放下（無初速）」 | 玩家可能想輕輕放下碎塊而不是丟遠 |
| 丟出鍵確認為 F | ✅ 確認（`OnDropCurrentItem` 綁 F）| real object.txt 問 F 是不是丟出鍵 |
| 力量條從底部到頂部一個來回 3 秒 | 可調 | 初始值，視手感微調 |
| `PickupRadius` 3 tile（~18.75cm @ Grain=16）| 可調 | 比 InteractRangeTiles 短，避免隔牆撿到 |
| 攜帶中玩家手部視覺（mesh 位置） | Phase 1 先跟隨 Actor 座標 | 完整 hand IK 留後續 |
