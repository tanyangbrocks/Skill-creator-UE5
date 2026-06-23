#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
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

    // 面板開關（2026-06-23 鍵位重整）
    Bind(EKeys::V, &ASkillCreatorPlayerController::OnOpenEditor);   // was E
    Bind(EKeys::B, &ASkillCreatorPlayerController::OnOpenSettings);
    Bind(EKeys::N, &ASkillCreatorPlayerController::OnOpenShapeMenu);
    Bind(EKeys::R, &ASkillCreatorPlayerController::OnOpenInventory); // was Z
    Bind(EKeys::T, &ASkillCreatorPlayerController::OnOpenEquipment); // was X
    Bind(EKeys::G, &ASkillCreatorPlayerController::OnOpenStats);     // was C
    // V（SpellGroupSwitch）已移除：技能組切換改為積木編輯器面板內 UI 操作

    // 動作快捷鍵
    Bind(EKeys::Q,   &ASkillCreatorPlayerController::OnUsePotion);        // S-6 服用藥水袋
    Bind(EKeys::E,   &ASkillCreatorPlayerController::OnToggleLockTarget);  // S-3 鎖敵切換
    Bind(EKeys::Tab, &ASkillCreatorPlayerController::OnSwitchLockTarget);  // S-3 循環切換目標
    Bind(EKeys::F,   &ASkillCreatorPlayerController::OnDropCurrentItem);
    Bind(EKeys::H,   &ASkillCreatorPlayerController::OnCancelAction);
    // Z：按住疾跑，長按 1s 超速，放開恢復
    Bind(EKeys::Z, &ASkillCreatorPlayerController::OnSprintPressed);
    InputComponent->BindKey(EKeys::Z, IE_Released, this, &ASkillCreatorPlayerController::OnSprintReleased);
    // K：空中=飛行；地面對防禦目標=破防 stub；K+L=前衝 stub
    Bind(EKeys::K,   &ASkillCreatorPlayerController::OnGuardBreakOrFly);
    // X：按下=飛行/空中/蹲/翻滾/滑鏟 情境分流；放開=解除防禦/滑鏟
    Bind(EKeys::X, &ASkillCreatorPlayerController::OnXPressed);
    InputComponent->BindKey(EKeys::X, IE_Released, this, &ASkillCreatorPlayerController::OnXReleased);
    // J：按下=蓄力開始（X 同時按住=彈反）；放開=輕攻 or 蓄力攻
    Bind(EKeys::J, &ASkillCreatorPlayerController::OnAttackPressed);
    InputComponent->BindKey(EKeys::J, IE_Released, this, &ASkillCreatorPlayerController::OnAttackReleased);
    Bind(EKeys::L,   &ASkillCreatorPlayerController::OnBackDash);          // S-8 後撤衝量 stub
    Bind(EKeys::Y,   &ASkillCreatorPlayerController::OnOpenPotionPanel);   // S-6 藥水袋面板 stub
    Bind(EKeys::M,   &ASkillCreatorPlayerController::OnOpenMap);           // S-7 地圖 stub

    // Shift 游標模式（按一下顯示系統游標且鏡頭凍結，再按切回準心操控）
    Bind(EKeys::LeftShift, &ASkillCreatorPlayerController::ToggleCursorMode);

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
    // 對應 Godot Main.cs:1772-1791 ToggleEditor()：E 鍵切換的是「圓球列表」首頁
    // （_spellList.Visible = !_editorOpen），不是直接開積木編輯器本體（_editor 只在
    // 列表裡點擊某個圓球後才顯示，見 Main.cs:1794-1829 OnListActiveSpellClicked 等）。
    // 2026-06-22 修復：之前這裡直接呼叫 ToggleBlockEditorOverlay()，等同把 Godot 兩層
    // 畫面（列表首頁 → 點擊進入編輯器）疊成一層，玩家按 E 會直接跳進積木編輯器本體，
    // 跳過原本「先看到整組技能、決定要編輯哪一格或切換哪個技能組」的首頁。
    if (bBlockEditorOpen)
    {
        // 已經在編輯器本體裡：E 鍵走「← 返回」同一條未儲存確認流程關閉整個編輯流程
        // （對應 Godot _backBtn.Pressed 在 _navStack 空的狀況下呼叫 TryExitEditor()）。
        if (BlockEditorWidget) BlockEditorWidget->RequestClose();
        return;
    }

    ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>();
    if (!HUD || !HUD->SpellListPanel) return;

    // 只需要綁一次：圓球列表裡點擊任一槽位 → 隱藏列表、開啟積木編輯器編輯該槽位
    // （對應 Godot Main.cs:436 _spellList.ActiveSpellClicked += OnListActiveSpellClicked）
    if (!bSpellListSlotClickBound)
    {
        HUD->SpellListPanel->OnSlotClicked.BindUObject(this, &ASkillCreatorPlayerController::OnSpellListSlotClicked);
        // 對應 Godot Main.cs:1801-1820 OnListAddSpellClicked：點「+」→ 找第一個空槽位開積木編輯器
        HUD->SpellListPanel->OnAddSpellRequested.BindLambda([this]()
        {
            ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
            if (!Char || !Char->SpellCasterComp) return;
            const FSpellLoadout& Loadout = Char->SpellCasterComp->SpellGroups.GetActiveLoadout();
            int32 EmptySlot = -1;
            for (int32 i = 0; i < FSpellLoadout::MaxSlots; ++i)
            {
                if (!Loadout.Slots.IsValidIndex(i) || !Loadout.Slots[i].IsValid())
                { EmptySlot = i; break; }
            }
            if (EmptySlot < 0) return;
            if (ASkillCreatorHUD* H = GetHUD<ASkillCreatorHUD>())
                if (H->SpellListPanel)
                    H->SpellListPanel->SetVisibility(ESlateVisibility::Collapsed);
            OpenBlockEditorForSlot(EmptySlot);
        });
        bSpellListSlotClickBound = true;
    }

    const bool bListVisible = HUD->SpellListPanel->GetVisibility() == ESlateVisibility::Visible;
    if (!bListVisible)
    {
        HUD->SpellListPanel->RefreshSpellList(); // 對應 Godot Main.cs:1777 _spellList.Refresh()
        HUD->SpellListPanel->SetVisibility(ESlateVisibility::Visible);
        SetShowMouseCursor(true);
        // 使用 GameAndUI 而非 UIOnly：UIOnly 會封鎖所有 InputComponent->BindKey 事件，
        // 導致再按 E 無法觸發 OnOpenEditor() 來關閉面板。GameAndUI 保留遊戲輸入同時
        // 允許滑鼠事件傳給 UMG，讓 E 鍵和圓球點擊都能正常運作。
        SetInputMode(FInputModeGameAndUI());
    }
    else
    {
        HUD->SpellListPanel->SetVisibility(ESlateVisibility::Collapsed);
        SetShowMouseCursor(bCursorMode);  // 面板關閉後恢復 Shift 游標模式的狀態
        SetInputMode(FInputModeGameAndUI());
    }
}

// 對應 Godot Main.cs:1794-1799 OnListActiveSpellClicked：圓球列表裡點擊主動技能槽位
// → 隱藏列表、開啟積木編輯器編輯該槽位。
void ASkillCreatorPlayerController::OnSpellListSlotClicked(int32 SlotIndex)
{
    ASkillCreatorHUD* HUD = GetHUD<ASkillCreatorHUD>();
    if (HUD && HUD->SpellListPanel)
        HUD->SpellListPanel->SetVisibility(ESlateVisibility::Collapsed);
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
    SetShowMouseCursor(bCursorMode);  // 面板關閉後恢復 Shift 游標模式的狀態
    SetInputMode(FInputModeGameAndUI());
}

void ASkillCreatorPlayerController::ToggleCursorMode()
{
    bCursorMode = !bCursorMode;
    SetShowMouseCursor(bCursorMode);
    // SetIgnoreLookInput 是計數器（+1/-1）而非布林值：
    // true → counter++（鏡頭凍結），false → counter--（鏡頭恢復）
    SetIgnoreLookInput(bCursorMode);
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

void ASkillCreatorPlayerController::OnOpenSettings()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleSettings();
}

void ASkillCreatorPlayerController::OnOpenShapeMenu()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleShapeMenu();
}

void ASkillCreatorPlayerController::OnOpenInventory()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleInventory();
}

void ASkillCreatorPlayerController::OnOpenEquipment()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleEquipment();
}

void ASkillCreatorPlayerController::OnOpenStats()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleStats();
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
    if (Char) Char->StartSprint();
}

void ASkillCreatorPlayerController::OnSprintReleased()
{
    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char) Char->EndSprint();
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
