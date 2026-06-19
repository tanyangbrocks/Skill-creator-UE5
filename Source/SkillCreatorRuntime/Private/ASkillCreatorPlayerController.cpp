#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "USpellCaster.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#if WITH_EDITOR
#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"
#include "SBlockEditorWidget.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#endif

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
#if WITH_EDITOR
    ToggleBlockEditorOverlay();
#endif
}

#if WITH_EDITOR
void ASkillCreatorPlayerController::ToggleBlockEditorOverlay()
{
    if (!GEngine || !GEngine->GameViewport) return;

    if (!BlockEditorOverlay.IsValid())
    {
        UBlockEdGraph* Graph = NewObject<UBlockEdGraph>(GetTransientPackage());
        Graph->Schema = UBlockEdGraphSchema::StaticClass();

        TSharedRef<SBlockEditorWidget> EdWidget = SNew(SBlockEditorWidget).GraphToEdit(Graph);
        BlockEditorWidget = EdWidget;

        // 載入目前選中槽位既有的積木樹 + 名稱（若有）
        ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
        int32 ActiveSlot = 0;
        if (Char && Char->SpellCasterComp)
        {
            ActiveSlot = Char->SpellCasterComp->SpellGroups.GetActiveLoadout().ActiveIndex;
            const FSpellArray& Spell = Char->SpellCasterComp->SpellGroups.GetActiveLoadout().GetSlot(ActiveSlot);
            if (Spell.Blocks && !Spell.Blocks->IsEmpty())
                Graph->FromBlockNodes(*Spell.Blocks);
            EdWidget->SetSpellName(Spell.Name);
        }
        EdWidget->SetActiveSlot(ActiveSlot);
        EdWidget->OnSaveSpell.BindUObject(this, &ASkillCreatorPlayerController::OnBlockEditorSave);

        BlockEditorOverlay =
            SNew(SBox)
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.96f))
                .Padding(FMargin(0.f))
                [
                    EdWidget
                ]
            ];

        GEngine->GameViewport->AddViewportWidgetContent(
            BlockEditorOverlay.ToSharedRef(), 20);
        bBlockEditorOpen = true;
    }
    else
    {
        bBlockEditorOpen = !bBlockEditorOpen;
        BlockEditorOverlay->SetVisibility(
            bBlockEditorOpen ? EVisibility::Visible : EVisibility::Hidden);
    }

    if (bBlockEditorOpen)
    {
        SetShowMouseCursor(true);
        SetInputMode(FInputModeUIOnly());
    }
    else
    {
        SetShowMouseCursor(false);
        SetInputMode(FInputModeGameAndUI());
    }
}

void ASkillCreatorPlayerController::OnBlockEditorSave(const FString& SpellName, int32 SlotIndex)
{
    if (!BlockEditorWidget.IsValid()) return;

    ASkillCreatorCharacter* Char = GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (!Char || !Char->SpellCasterComp) return;

    FSpellLoadout& Loadout = Char->SpellCasterComp->SpellGroups.GetActiveLoadout();
    if (!Loadout.Slots.IsValidIndex(SlotIndex)) return;

    TArray<TUniquePtr<FBlockNode>> Nodes = BlockEditorWidget->GetBlockNodes();
    Loadout.Slots[SlotIndex].Name = SpellName;
    Loadout.Slots[SlotIndex].SetBlocks(MoveTemp(Nodes));

    // 寫入 in-memory SpellGroups 即可——FillSaveData() 在下一次定時/死亡/退出存檔時
    // 會把 SpellGroups 序列化進 SpellGroupJson（見 ASkillCreatorCharacter::FillSaveData）。
    UE_LOG(LogTemp, Log, TEXT("[儲存技能整構] 槽位 %d「%s」已寫入"), SlotIndex + 1, *SpellName);
}
#endif

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
