#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
#include "IPhysicalPickable.h"
#include "APhysicalItemActor.h"
#include "ADebrisActor.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "UPotionBagComponent.h"
#include "UMapComponent.h"
#include "UAfterimageFXComponent.h"
#include "ASkillCreatorHUD.h"
#include "USpellCaster.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "UDroppedItemManager.h"
#include "WorldScale.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UBlockEditorWidget.h"
#include "USpellListWidget.h"
#include "SpellGroup.h"

void ASkillCreatorPlayerController::BeginPlay()
{
    Super::BeginPlay();
}

void ASkillCreatorPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // K-23：套用存檔的自訂鍵位（對應 Godot InputBindings.cs，開機時從 input_bindings.json 讀入）
    // UInputSettingsWidget::SaveBindings() 寫入，這裡在玩家 possess pawn 後立刻讀回並套用，
    // 確保不需要打開設定面板就能生效（不同於 widget 的 LoadAndApplyBindings 只在開啟 UI 時跑）
    ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(InPawn);
    if (!Char) return;

    UInputMappingContext* IMC = Char->GetDefaultMappingContext();
    if (!IMC) return;

    const FString CfgPath = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("PlayerInputBindings.ini");
    if (!FPaths::FileExists(CfgPath)) return;

    static const TCHAR* Section = TEXT("PlayerInputBindings");
    const TArray<FEnhancedActionKeyMapping> Snapshot = IMC->GetMappings();

    bool bChanged = false;
    for (const FEnhancedActionKeyMapping& M : Snapshot)
    {
        if (!M.Action) continue;
        FString KeyStr;
        if (!GConfig->GetString(Section, *M.Action->GetName(), KeyStr, CfgPath)) continue;

        FKey LoadedKey(*KeyStr);
        if (!LoadedKey.IsValid() || LoadedKey == M.Key) continue;

        UInputAction* Action = const_cast<UInputAction*>(M.Action.Get());
        IMC->UnmapKey(Action, M.Key);
        IMC->MapKey(Action, LoadedKey);
        bChanged = true;
    }

    if (bChanged)
        if (ULocalPlayer* LP = GetLocalPlayer())
            if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Sub->RemoveMappingContext(IMC);
                Sub->AddMappingContext(IMC, 0);
            }
}

void ASkillCreatorPlayerController::SpawnDefaultHUD()
{
    // 不呼叫 Super::SpawnDefaultHUD()——它會用 GetWorld()->GetAuthGameMode()->HUDClass，
    // 而那個值可能來自 BP_SkillCreatorGameMode_C（World Settings 覆寫）的 CDO，
    // 進而連到舊版 WBP_PlayerHUD_C，導致準心/物品熱鍵欄消失（見標頭檔註解）。
    // 這裡複製引擎原始邏輯（NetMode 檢查 + 已有 HUD 則跳過），但 HUD 類別永遠強制
    // 用純 C++ ASkillCreatorHUD，不管 GameMode CDO 設了什麼。
    // 2026-06-23 診斷：確認這個覆寫真的有跑到，而不是被別的路徑搶先設了 MyHUD
    UE_LOG(LogTemp, Warning, TEXT("ASkillCreatorPlayerController::SpawnDefaultHUD called, MyHUD already=%s"),
        MyHUD ? *MyHUD->GetClass()->GetName() : TEXT("nullptr"));
    if (MyHUD != nullptr || GetNetMode() == NM_DedicatedServer)
        return;

    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Owner            = this;
    SpawnInfo.Instigator       = GetInstigator();
    SpawnInfo.ObjectFlags     |= RF_Transient;
    SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    MyHUD = GetWorld()->SpawnActor<AHUD>(ASkillCreatorHUD::StaticClass(), SpawnInfo);
    UE_LOG(LogTemp, Warning, TEXT("ASkillCreatorPlayerController::SpawnDefaultHUD spawned class=%s"),
        MyHUD ? *MyHUD->GetClass()->GetName() : TEXT("FAILED"));
}

void ASkillCreatorPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    auto Bind = [&](FKey Key, auto Fn)
    {
        InputComponent->BindKey(Key, IE_Pressed, this, Fn);
    };

    // 滾輪：Ctrl+滾 = 縮放；否則切換熱鍵欄（對應 Godot Main.cs scroll logic）
    Bind(EKeys::MouseScrollUp,   &ASkillCreatorPlayerController::OnScrollUp);
    Bind(EKeys::MouseScrollDown, &ASkillCreatorPlayerController::OnScrollDown);

    // 數字鍵 → 熱鍵欄（對應 Godot Key1~Key9/Key0 → Inventory.ActiveHotbarIndex）
    Bind(EKeys::One,   &ASkillCreatorPlayerController::OnHotbar1);
    Bind(EKeys::Two,   &ASkillCreatorPlayerController::OnHotbar2);
    Bind(EKeys::Three, &ASkillCreatorPlayerController::OnHotbar3);
    Bind(EKeys::Four,  &ASkillCreatorPlayerController::OnHotbar4);
    Bind(EKeys::Five,  &ASkillCreatorPlayerController::OnHotbar5);
    Bind(EKeys::Six,   &ASkillCreatorPlayerController::OnHotbar6);
    Bind(EKeys::Seven, &ASkillCreatorPlayerController::OnHotbar7);
    Bind(EKeys::Eight, &ASkillCreatorPlayerController::OnHotbar8);
    Bind(EKeys::Nine,  &ASkillCreatorPlayerController::OnHotbar9);
    Bind(EKeys::Zero,  &ASkillCreatorPlayerController::OnHotbar0);

    // 面板開關（2026-06-25 玩家面板整合；2026-06-26 G→T 見 plan-physical-item.md G-0）
    // T → 玩家個人面板（含 Stats / 職業能力 / 技能創建空間 / 內部空間 四 Tab）
    Bind(EKeys::T, &ASkillCreatorPlayerController::OnOpenPlayerPanel);
    // G → 撿取實體物品 / 攜帶中存入物品欄 / 無物時退回開玩家面板（plan-physical-item.md G-5 填實作）
    Bind(EKeys::G, &ASkillCreatorPlayerController::OnPickupOrPanel);
    // R → 物品欄 + 裝備欄同時開關
    Bind(EKeys::R, &ASkillCreatorPlayerController::OnOpenInventoryAndEquipment);
    // N → 形狀選單
    Bind(EKeys::N, &ASkillCreatorPlayerController::OnOpenShapeMenu);
    // V / B 已無預設鍵位（移除綁定）；G = OnPickupOrPanel；T = OnOpenPlayerPanel

    // 動作快捷鍵
    Bind(EKeys::Q,   &ASkillCreatorPlayerController::OnUsePotion);        // S-6 服用藥水袋
    Bind(EKeys::E,   &ASkillCreatorPlayerController::OnToggleLockTarget);  // S-3 鎖敵切換
    Bind(EKeys::Tab, &ASkillCreatorPlayerController::OnSwitchLockTarget);  // S-3 循環切換目標
    // F 鍵改為 Pressed+Released 雙軸（短按=丟掉落物，長按=投擲；G-8）
    InputComponent->BindKey(EKeys::F, IE_Pressed,  this, &ASkillCreatorPlayerController::OnDropPressed);
    InputComponent->BindKey(EKeys::F, IE_Released, this, &ASkillCreatorPlayerController::OnDropReleased);
    Bind(EKeys::H,   &ASkillCreatorPlayerController::OnCancelAction);
    // Z：toggle 疾跑/超速（再按一次取消），計時 1s 升超速（無須一直按住）
    Bind(EKeys::Z, &ASkillCreatorPlayerController::OnSprintPressed);
    // K：空中=飛行；地面對防禦目標=破防 stub；K+L=前衝 stub
    Bind(EKeys::K,   &ASkillCreatorPlayerController::OnGuardBreakOrFly);
    // X：按下=飛行/空中/蹲/翻滾/滑鏟 情境分流；放開=解除防禦/滑鏟
    Bind(EKeys::X, &ASkillCreatorPlayerController::OnXPressed);
    InputComponent->BindKey(EKeys::X, IE_Released, this, &ASkillCreatorPlayerController::OnXReleased);
    // J：按下=蓄力開始（X 同時按住=彈反）；放開=輕攻 or 蓄力攻
    Bind(EKeys::J, &ASkillCreatorPlayerController::OnAttackPressed);
    InputComponent->BindKey(EKeys::J, IE_Released, this, &ASkillCreatorPlayerController::OnAttackReleased);
    Bind(EKeys::L,   &ASkillCreatorPlayerController::OnBackDash);          // S-8 後撤衝量（4×走速向後衝量+殘影 FX stub）
    Bind(EKeys::Y,   &ASkillCreatorPlayerController::OnOpenPotionPanel);   // S-6 藥水袋面板 stub
    Bind(EKeys::M,   &ASkillCreatorPlayerController::OnOpenMap);           // S-7 地圖 stub

    // Shift：展開/收起加工選單面板
    Bind(EKeys::LeftShift, &ASkillCreatorPlayerController::ToggleCraftingPanel);
    // V：展開/收起技能組切換面板（對應 Godot InputBindings.cs:76 spell_group_switch = Key.V）
    Bind(EKeys::V,         &ASkillCreatorPlayerController::OnOpenSpellGroupPanel);

    // Ctrl / U / I / O / P：由 ASkillCreatorCharacter Enhanced Input 負責，不重複綁定
    // Ctrl（短按 Tap ≤0.2s）→ CycleCameraMode（長按不觸發，解決 Ctrl+滾輪縮放衝突）
}

// ── 物品熱鍵欄 ───────────────────────────────────────────────────────────

void ASkillCreatorPlayerController::SetActiveHotbarIndex(int32 Idx)
{
    // 2026-06-23 診斷：使用者回報數字鍵切不了熱鍵欄，確認 BindKey 事件到底有沒有觸發到這裡，
    // 還是觸發了但 Char/InventoryComp 是 null。
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    UE_LOG(LogTemp, Warning, TEXT("ASkillCreatorPlayerController::SetActiveHotbarIndex(%d) Char=%s InventoryComp=%s"),
        Idx, Char ? TEXT("valid") : TEXT("NULL"),
        (Char && Char->InventoryComp) ? TEXT("valid") : TEXT("NULL"));
    if (Char && Char->InventoryComp)
        Char->InventoryComp->SetActiveHotbarIndex(Idx);
}

void ASkillCreatorPlayerController::OnScrollUp()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
    {
        // Ctrl + 滾上 = 鏡頭拉近（縮短彈簧臂）
        if (Char->SpringArm)
            Char->SpringArm->TargetArmLength =
                FMath::Max(50.f, Char->SpringArm->TargetArmLength - 100.f);
        return;
    }
    // 切換熱鍵欄：往前一格（wraps around）
    if (Char->InventoryComp)
    {
        int32 Next = (Char->InventoryComp->ActiveHotbarIndex - 1 + UInventoryComponent::HotbarSize)
                     % UInventoryComponent::HotbarSize;
        Char->InventoryComp->SetActiveHotbarIndex(Next);
    }
}

void ASkillCreatorPlayerController::OnScrollDown()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
    {
        // Ctrl + 滾下 = 鏡頭拉遠（延長彈簧臂）
        if (Char->SpringArm)
            Char->SpringArm->TargetArmLength =
                FMath::Min(2000.f, Char->SpringArm->TargetArmLength + 100.f);
        return;
    }
    // 切換熱鍵欄：往後一格（wraps around）
    if (Char->InventoryComp)
    {
        int32 Next = (Char->InventoryComp->ActiveHotbarIndex + 1)
                     % UInventoryComponent::HotbarSize;
        Char->InventoryComp->SetActiveHotbarIndex(Next);
    }
}

void ASkillCreatorPlayerController::OnHotbar1() { SetActiveHotbarIndex(0); }
void ASkillCreatorPlayerController::OnHotbar2() { SetActiveHotbarIndex(1); }
void ASkillCreatorPlayerController::OnHotbar3() { SetActiveHotbarIndex(2); }
void ASkillCreatorPlayerController::OnHotbar4() { SetActiveHotbarIndex(3); }
void ASkillCreatorPlayerController::OnHotbar5() { SetActiveHotbarIndex(4); }
void ASkillCreatorPlayerController::OnHotbar6() { SetActiveHotbarIndex(5); }
void ASkillCreatorPlayerController::OnHotbar7() { SetActiveHotbarIndex(6); }
void ASkillCreatorPlayerController::OnHotbar8() { SetActiveHotbarIndex(7); }
void ASkillCreatorPlayerController::OnHotbar9() { SetActiveHotbarIndex(8); }
void ASkillCreatorPlayerController::OnHotbar0() { SetActiveHotbarIndex(9); }

// ── 技能編輯器 overlay ────────────────────────────────────────────────────

void ASkillCreatorPlayerController::OnOpenEditor()
{
    // E 鍵已改綁 OnToggleLockTarget（S-3 鎖敵）；此函式為死碼保留，不再呼叫。
    // SpellList 現在是 PlayerPanel（G 鍵）的 SpellEditor Tab，
    // 由 OnOpenPlayerPanel() 的 OnSlotClicked delegate 處理開啟積木編輯器。
    if (bBlockEditorOpen && BlockEditorWidget)
        BlockEditorWidget->RequestClose();
}

// 對應 Godot Main.cs:1794-1799 OnListActiveSpellClicked：圓球列表裡點擊主動技能槽位
// → 隱藏 PlayerPanel、開啟積木編輯器編輯該槽位。
void ASkillCreatorPlayerController::OnSpellListSlotClicked(int32 SlotIndex)
{
    if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
    {
        bPlayerPanelWasOpenBefore =
            HUD->PlayerPanel && HUD->PlayerPanel->GetVisibility() == ESlateVisibility::Visible;
        if (HUD->PlayerPanel) HUD->PlayerPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    OpenBlockEditorForSlot(SlotIndex);
}

void ASkillCreatorPlayerController::OpenBlockEditorForSlot(int32 SlotIndex)
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char || !Char->SpellCasterComp) return;

    if (!BlockEditorWidget)
    {
        BlockEditorWidget = CreateWidget<UBlockEditorWidget>(this, UBlockEditorWidget::StaticClass());
        if (!BlockEditorWidget) return;
        BlockEditorWidget->OnSaveSpell.BindUObject(this, &ASkillCreatorPlayerController::OnBlockEditorSave);
        BlockEditorWidget->OnCloseRequested.BindUObject(this, &ASkillCreatorPlayerController::OnBlockEditorClosed);
        BlockEditorWidget->AddToViewport(20);
        BlockEditorWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
    }

    // 載入指定槽位既有的積木樹 + 名稱（UBlockEditorWidget::SetEditingSpell 直接指向
    // Loadout.Slots[SlotIndex] 本體，不像舊版 Slate 需要 FromBlockNodes() 額外轉換）
    FSpellGroup& Groups = Char->SpellCasterComp->SpellGroups;
    FSpellLoadout& Loadout = Groups.GetActiveLoadout();
    const int32 ClampedSlot = FMath::Clamp(SlotIndex, 0, FSpellLoadout::MaxSlots - 1);
    Loadout.ActiveIndex = ClampedSlot; // 對應 Godot SelectEditorSlot/OpenSlot：_activeEditorSlot = i
    BlockEditorWidget->SetSpellGroups(&Groups);
    BlockEditorWidget->SetActiveSlot(ClampedSlot);
    BlockEditorWidget->SetEditingSpell(&Loadout.Slots[ClampedSlot]);

    BlockEditorWidget->SetVisibility(ESlateVisibility::Visible);
    bBlockEditorOpen = true;
    SetShowMouseCursor(true);
    SetInputMode(FInputModeGameAndUI());
}

void ASkillCreatorPlayerController::OnBlockEditorClosed()
{
    if (BlockEditorWidget)
        BlockEditorWidget->SetVisibility(ESlateVisibility::Hidden);
    bBlockEditorOpen = false;

    // 若進入 BlockEditor 前 PlayerPanel 是開著的，關閉編輯器後恢復（SpellEditor Tab 仍選中）
    if (bPlayerPanelWasOpenBefore)
    {
        bPlayerPanelWasOpenBefore = false;
        if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
            if (HUD->PlayerPanel)
                HUD->PlayerPanel->SetVisibility(ESlateVisibility::Visible);
    }

    SetShowMouseCursor(false);
    SetInputMode(FInputModeGameAndUI());
}

void ASkillCreatorPlayerController::ToggleCraftingPanel()
{
    if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
        HUD->ToggleCraftingPanel();
}

void ASkillCreatorPlayerController::OnPickupOrPanel()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    if (Char->bIsCarrying)
    {
        // 攜帶中 G → 存入物品欄
        IPhysicalPickable* P = Cast<APhysicalItemActor>(Char->CarriedActor.Get());
        if (!P) P = Cast<ADebrisActor>(Char->CarriedActor.Get());
        if (P && Char->InventoryComp && P->GetInventoryItemId() != EItemId::None)
            Char->InventoryComp->TryAdd(P->GetInventoryItemId(), P->GetInventoryCount());
        Char->EndCarry(FVector::ZeroVector);
        return;
    }

    // 搜尋附近可撿物
    if (AActor* Nearest = Char->FindNearestPickable())
    {
        Char->BeginCarry(Nearest);
        return;
    }

    // 附近無物 → 開玩家面板（原 G 鍵行為）
    OnOpenPlayerPanel();
}

void ASkillCreatorPlayerController::OnOpenSpellGroupPanel()
{
    // 對應 Godot Main.cs:1430：!_editorOpen 才允許開技能組面板
    if (bBlockEditorOpen) return;
    if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
        HUD->ToggleSpellGroup();
}

void ASkillCreatorPlayerController::OnBlockEditorSave(const FString& SpellName, int32 SlotIndex)
{
    // UMG 版本的 UBlockEditorWidget::SetEditingSpell 一開始就指向 Loadout.Slots[SlotIndex]
    // 本體（裸指標直接編輯），驗證通過時資料早已經是最新的，不需要像舊版 Slate
    // 那樣額外 GetBlockNodes()+SetBlocks() 寫回。
    // 寫入 in-memory SpellGroups 即可——FillSaveData() 在下一次定時/死亡/退出存檔時
    // 會把 SpellGroups 序列化進 SpellGroupJson（見 ASkillCreatorCharacter::FillSaveData）。
    UE_LOG(LogTemp, Log, TEXT("[儲存技能整構] 槽位 %d「%s」已存"), SlotIndex + 1, *SpellName);
}

// ── HUD 面板開關 ──────────────────────────────────────────────────────────

void ASkillCreatorPlayerController::OnOpenPlayerPanel()
{
    ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>();
    if (!HUD || !HUD->PlayerPanel) return;

    // SpellList delegate 一次性綁定（移自舊 OnOpenEditor()）
    if (!bSpellListSlotClickBound)
    {
        if (USpellListWidget* SpellList = HUD->PlayerPanel->GetSpellListWidget())
        {
            SpellList->OnSlotClicked.BindUObject(
                this, &ASkillCreatorPlayerController::OnSpellListSlotClicked);

            SpellList->OnAddSpellRequested.BindLambda([this]()
            {
                ASkillCreatorCharacter* Char = GetPawn()
                    ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
                if (!Char || !Char->SpellCasterComp) return;
                const FSpellLoadout& Loadout =
                    Char->SpellCasterComp->SpellGroups.GetActiveLoadout();
                int32 EmptySlot = -1;
                for (int32 i = 0; i < FSpellLoadout::MaxSlots; ++i)
                {
                    if (!Loadout.Slots.IsValidIndex(i) || !Loadout.Slots[i].IsValid())
                    { EmptySlot = i; break; }
                }
                if (EmptySlot < 0) return;
                // 開 BlockEditor 前先隱藏 PlayerPanel
                if (ASkillCreatorHUD* H = GetHUD<ASkillCreatorHUD>())
                {
                    bPlayerPanelWasOpenBefore =
                        H->PlayerPanel && H->PlayerPanel->GetVisibility() == ESlateVisibility::Visible;
                    if (H->PlayerPanel) H->PlayerPanel->SetVisibility(ESlateVisibility::Collapsed);
                }
                OpenBlockEditorForSlot(EmptySlot);
            });
            bSpellListSlotClickBound = true;
        }
    }

    HUD->TogglePlayerPanel();
}

void ASkillCreatorPlayerController::OnOpenInventoryAndEquipment()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleInventoryAndEquipment();
}

void ASkillCreatorPlayerController::OnOpenShapeMenu()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleShapeMenu();
}

// ── 動作快捷鍵 ───────────────────────────────────────────────────────────

void ASkillCreatorPlayerController::OnUsePotion()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->PotionBagComp)
        Char->PotionBagComp->UseAllBags();
}

void ASkillCreatorPlayerController::OnToggleLockTarget()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char) Char->TryToggleLockTarget();
}

void ASkillCreatorPlayerController::OnSwitchLockTarget()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char) Char->SwitchToNextLockTarget();
}

// ── G-8 F 長/短按分離 ────────────────────────────────────────────────────

void ASkillCreatorPlayerController::OnDropPressed()
{
    FHoldStart = GetWorld()->GetTimeSeconds();

    // 攜帶中或持有物品欄物品時都預先啟動力量條，讓長按路徑（含 G-9 非攜帶投擲）能拿到正確 PowerPct
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;
    const bool bHasHotbarItem = Char->InventoryComp
        && Char->InventoryComp->Slots.IsValidIndex(Char->InventoryComp->ActiveHotbarIndex)
        && !Char->InventoryComp->Slots[Char->InventoryComp->ActiveHotbarIndex].IsEmpty();
    if (Char->bIsCarrying || bHasHotbarItem)
        if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
            HUD->StartThrowCharge();
}

void ASkillCreatorPlayerController::OnDropReleased()
{
    if (FHoldStart < 0.f) return;
    const float HoldSec = GetWorld()->GetTimeSeconds() - FHoldStart;
    FHoldStart = -1.f;

    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    if (HoldSec < 0.2f)
    {
        // 短按路徑
        if (Char->bIsCarrying)
            Char->EndCarry(FVector::ZeroVector);  // 放下，無初速
        else
            OnDropCurrentItem();
    }
    else
    {
        // 長按路徑
        if (Char->bIsCarrying)
        {
            if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
                HUD->FinishThrowCharge(Char);
        }
        else
        {
            // G-9：從物品欄生成實體物品後投擲
            OnThrowFromInventory(HoldSec);
        }
    }
}

void ASkillCreatorPlayerController::OnDropCurrentItem()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char || !Char->InventoryComp) return;

    const int32 Idx = Char->InventoryComp->ActiveHotbarIndex;
    if (!Char->InventoryComp->Slots.IsValidIndex(Idx)) return;
    const FItemStack Stack = Char->InventoryComp->Slots[Idx];
    if (Stack.IsEmpty()) return;

    if (auto* DropMgr = GetWorld()->GetSubsystem<UDroppedItemManager>())
    {
        const FGridPos DropPos = WorldScale::WorldToTile(Char->GetActorLocation());
        DropMgr->SpawnDrop(Stack.ItemId, Stack.Count, DropPos);
    }
    Char->InventoryComp->Consume(Idx, Stack.Count);
}

// G-9：非攜帶狀態長按 F → 從物品欄生成實體物品 → 立刻投擲
void ASkillCreatorPlayerController::OnThrowFromInventory(float /*HoldSec*/)
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char || !Char->InventoryComp) return;

    const int32 Idx = Char->InventoryComp->ActiveHotbarIndex;
    if (!Char->InventoryComp->Slots.IsValidIndex(Idx)) return;
    const FItemStack S = Char->InventoryComp->Slots[Idx];
    if (S.IsEmpty()) return;

    // 在角色前方 1.5 tile 生成實體物品
    FVector SpawnPos = Char->GetActorLocation()
        + Char->GetActorForwardVector() * WorldScale::TileSizeCm * 1.5f;
    APhysicalItemActor* PA = GetWorld()->SpawnActor<APhysicalItemActor>(
        APhysicalItemActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
    if (!PA) return;

    PA->Init(S.ItemId, S.Count);
    Char->InventoryComp->Consume(Idx, S.Count);
    Char->BeginCarry(PA);

    // 力量條已在 OnDropPressed 開始計時，直接計算投擲
    if (ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>())
        HUD->FinishThrowCharge(Char);
}

void ASkillCreatorPlayerController::OnCancelAction()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->CancelAll();
}

void ASkillCreatorPlayerController::OnSprintPressed()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;
    // Z = toggle：疾跑/超速中再按一次取消；其他狀態按下則進入疾跑
    if (Char->MovementState == EPlayerMovementState::Sprinting
     || Char->MovementState == EPlayerMovementState::SuperSprinting)
        Char->EndSprint();
    else
        Char->StartSprint();
}

void ASkillCreatorPlayerController::OnGuardBreakOrFly()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    // K+L 同時按下 → 前衝 stub（S-8 待實作）
    if (IsInputKeyDown(EKeys::L))
    {
        UE_LOG(LogTemp, Log, TEXT("[S-8] K+L 前衝 — stub, pending S-8 dash implementation"));
        return;
    }

    // 空中（含下落）→ 飛行切換
    if (Char->GetCharacterMovement()->IsFalling() || Char->IsFlying())
    {
        Char->ToggleFlight();
        return;
    }

    // 地面有鎖定目標 → 破防 stub（S-2 攻擊框架 + 敵人防禦狀態完成後接上真正破防邏輯）
    if (Char->LockedTarget)
    {
        UE_LOG(LogTemp, Log, TEXT("[S-2] K 破防 — stub, pending enemy guard state + guard break implementation"));
        return;
    }

    // 地面無目標：無動作
    UE_LOG(LogTemp, Verbose, TEXT("[K] 地面按下，無鎖定目標，忽略"));
}

void ASkillCreatorPlayerController::OnXPressed()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;

    // 飛行中 → 取消飛行 + 向下衝量
    if (Char->IsFlying())
    {
        Char->FlyDown();
        return;
    }
    // 空中下落 → 快速墜落衝量
    if (Char->GetCharacterMovement()->IsFalling())
    {
        Char->StartFastFall();
        return;
    }
    // 疾跑/超速 → 滑鏟
    if (Char->MovementState == EPlayerMovementState::Sprinting
     || Char->MovementState == EPlayerMovementState::SuperSprinting)
    {
        Char->PerformSlide();
        return;
    }
    // 地面蹲狀態 → 解除蹲（toggle off）
    if (Char->IsCrouching())
    {
        Char->EndCrouch();
        return;
    }
    // 地面有水平速度 → 翻滾
    if (Char->GetVelocity().Size2D() > WorldScale::WalkSpeedCm * 0.15f)
    {
        Char->PerformRoll();
        return;
    }
    // 地面靜止 → 蹲
    Char->PerformCrouch();
}

void ASkillCreatorPlayerController::OnXReleased()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;
    if (Char->IsGuarding())  Char->EndGuard();
    else if (Char->IsSliding()) Char->EndSlide();
}

void ASkillCreatorPlayerController::OnAttackPressed()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char) return;
    // X 同時按住（地面）→ 進入彈反窗口（S-4）
    if (!Char->IsFlying() && IsInputKeyDown(EKeys::X))
        Char->PerformGuard();
    else
        Char->StartChargingAttack();
}

void ASkillCreatorPlayerController::OnAttackReleased()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char) Char->ReleaseAttack();
}

void ASkillCreatorPlayerController::OnBackDash()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char) Char->PerformBackDash();
}

void ASkillCreatorPlayerController::OnOpenPotionPanel()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->PotionBagComp)
        Char->PotionBagComp->TogglePanel();
}

void ASkillCreatorPlayerController::OnOpenMap()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->MapComp)
        Char->MapComp->ToggleMap();
}
