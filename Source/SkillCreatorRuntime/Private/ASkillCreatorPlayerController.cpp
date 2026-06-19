#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "USpellCaster.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "GameFramework/Pawn.h"
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

    Bind(EKeys::MouseScrollUp,   &ASkillCreatorPlayerController::OnScrollUp);
    Bind(EKeys::MouseScrollDown, &ASkillCreatorPlayerController::OnScrollDown);
    Bind(EKeys::E,               &ASkillCreatorPlayerController::OnOpenEditor);
    Bind(EKeys::One,             &ASkillCreatorPlayerController::OnHotbar1);
    Bind(EKeys::Two,             &ASkillCreatorPlayerController::OnHotbar2);
    Bind(EKeys::Three,           &ASkillCreatorPlayerController::OnHotbar3);
    Bind(EKeys::Four,            &ASkillCreatorPlayerController::OnHotbar4);
    Bind(EKeys::Five,            &ASkillCreatorPlayerController::OnHotbar5);
    Bind(EKeys::B,               &ASkillCreatorPlayerController::OnOpenSettings);
    Bind(EKeys::N,               &ASkillCreatorPlayerController::OnOpenShapeMenu);
    Bind(EKeys::V,               &ASkillCreatorPlayerController::OnSpellGroupSwitch);
    Bind(EKeys::Z,               &ASkillCreatorPlayerController::OnOpenInventory);
    Bind(EKeys::X,               &ASkillCreatorPlayerController::OnOpenEquipment);
    Bind(EKeys::C,               &ASkillCreatorPlayerController::OnOpenStats);
    Bind(EKeys::I,               &ASkillCreatorPlayerController::OnOpenInputSettings);
    Bind(EKeys::L,               &ASkillCreatorPlayerController::OnOpenSpellList);
    Bind(EKeys::Q,               &ASkillCreatorPlayerController::OnEquipItem);
    Bind(EKeys::Tab,             &ASkillCreatorPlayerController::OnToggleXray);
}

// ── Spell hotbar ─────────────────────────────────────────────────────────

void ASkillCreatorPlayerController::SetActiveSpellSlot(int32 Idx)
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->SwitchSlot(Idx);
}

void ASkillCreatorPlayerController::OnScrollUp()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->CycleSlot(+1);
}

void ASkillCreatorPlayerController::OnScrollDown()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->CycleSlot(-1);
}

void ASkillCreatorPlayerController::OnHotbar1() { SetActiveSpellSlot(0); }
void ASkillCreatorPlayerController::OnHotbar2() { SetActiveSpellSlot(1); }
void ASkillCreatorPlayerController::OnHotbar3() { SetActiveSpellSlot(2); }
void ASkillCreatorPlayerController::OnHotbar4() { SetActiveSpellSlot(3); }
void ASkillCreatorPlayerController::OnHotbar5() { SetActiveSpellSlot(4); }

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
        // 第一次開啟：建立 Graph + Widget，加入 viewport overlay
        UBlockEdGraph* Graph = NewObject<UBlockEdGraph>(GetTransientPackage());
        Graph->Schema = UBlockEdGraphSchema::StaticClass();

        BlockEditorOverlay =
            SNew(SBox)
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.96f))
                .Padding(FMargin(0.f))
                [
                    SNew(SBlockEditorWidget)
                    .GraphToEdit(Graph)
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

    // 開啟時釋放滑鼠給 Slate；關閉時還給遊戲
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
#endif

// ── HUD 面板開關（每個都取 HUD 並呼叫對應 Toggle）───────────────────────

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

void ASkillCreatorPlayerController::OnOpenInputSettings()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleInputSettings();
}

void ASkillCreatorPlayerController::OnOpenSpellList()
{
    if (auto* H = GetHUD<ASkillCreatorHUD>()) H->ToggleSpellList();
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

// ── Tab：X-ray 切換（顯示牆後敵人）─────────────────────────────────────
// GPU Compute 渲染模式（M-10+），目前無實作。

void ASkillCreatorPlayerController::OnToggleXray()
{
}
