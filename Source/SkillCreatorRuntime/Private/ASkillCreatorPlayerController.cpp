#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "USpellCaster.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "UBlockEditorWidget.h"
#include "SpellGroup.h"

void ASkillCreatorPlayerController::BeginPlay()
{
    Super::BeginPlay();
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

    // 面板開關
    Bind(EKeys::E, &ASkillCreatorPlayerController::OnOpenEditor);
    Bind(EKeys::B, &ASkillCreatorPlayerController::OnOpenSettings);
    Bind(EKeys::N, &ASkillCreatorPlayerController::OnOpenShapeMenu);
    Bind(EKeys::V, &ASkillCreatorPlayerController::OnSpellGroupSwitch);
    Bind(EKeys::Z, &ASkillCreatorPlayerController::OnOpenInventory);
    Bind(EKeys::X, &ASkillCreatorPlayerController::OnOpenEquipment);
    Bind(EKeys::C, &ASkillCreatorPlayerController::OnOpenStats);
    Bind(EKeys::Q, &ASkillCreatorPlayerController::OnEquipItem);

    // Tab / U / I / O / P：由 ASkillCreatorCharacter Enhanced Input 負責，不重複綁定
}

// ── 物品熱鍵欄 ───────────────────────────────────────────────────────────

void ASkillCreatorPlayerController::SetActiveHotbarIndex(int32 Idx)
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
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
    ToggleBlockEditorOverlay();
}

void ASkillCreatorPlayerController::ToggleBlockEditorOverlay()
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
    }

    if (!bBlockEditorOpen)
    {
        // 載入目前選中槽位既有的積木樹 + 名稱（UBlockEditorWidget::SetEditingSpell 直接指向
        // Loadout.Slots[ActiveSlot] 本體，不像舊版 Slate 需要 FromBlockNodes() 額外轉換）
        FSpellGroup& Groups = Char->SpellCasterComp->SpellGroups;
        const int32 ActiveSlot = Groups.GetActiveLoadout().ActiveIndex;
        BlockEditorWidget->SetSpellGroups(&Groups);
        BlockEditorWidget->SetActiveSlot(ActiveSlot);
        BlockEditorWidget->SetEditingSpell(&Groups.GetActiveLoadout().Slots[ActiveSlot]);

        BlockEditorWidget->SetVisibility(ESlateVisibility::Visible);
        bBlockEditorOpen = true;
        SetShowMouseCursor(true);
        SetInputMode(FInputModeUIOnly());
    }
    else
    {
        // 對應 UI 上「← 返回」按鈕的同一條未儲存確認流程（TryExitEditor），E 鍵不開後門
        // 繞過確認；實際隱藏/還原輸入模式交給 OnBlockEditorClosed（OnCloseRequested 觸發時才做）。
        BlockEditorWidget->RequestClose();
    }
}

void ASkillCreatorPlayerController::OnBlockEditorClosed()
{
    if (BlockEditorWidget)
        BlockEditorWidget->SetVisibility(ESlateVisibility::Hidden);
    bBlockEditorOpen = false;
    SetShowMouseCursor(false);
    SetInputMode(FInputModeGameAndUI());
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

void ASkillCreatorPlayerController::OnSpellGroupSwitch()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleSpellGroup();
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

// ── Q：裝備/使用熱鍵格物品 ───────────────────────────────────────────────

void ASkillCreatorPlayerController::OnEquipItem()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char || !Char->InventoryComp || !Char->EquipmentComp) return;

    int32 ActiveIdx = Char->InventoryComp->ActiveHotbarIndex;
    if (!Char->InventoryComp->Slots.IsValidIndex(ActiveIdx)) return;
    if (Char->InventoryComp->Slots[ActiveIdx].IsEmpty()) return;

    Char->EquipmentComp->TryEquip(Char->InventoryComp, ActiveIdx);
}
