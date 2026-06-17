#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"
#include "SBlockEditorWidget.h"

#define LOCTEXT_NAMESPACE "SkillCreatorEditor"

static const FName BlockEditorTabName("BlockSpellEditor");

// ──────────────────────────────────────────────────────────────────────────────
// 模組類別（取代 FDefaultModuleImpl）
// StartupModule：向 UE Tab 系統登錄「BlockSpellEditor」Tab
// ShutdownModule：反登錄，避免 Editor 關閉時出現懸空指標
// ──────────────────────────────────────────────────────────────────────────────
class FSkillCreatorEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            BlockEditorTabName,
            FOnSpawnTab::CreateRaw(this, &FSkillCreatorEditorModule::SpawnBlockEditorTab))
            .SetDisplayName(LOCTEXT("BlockEditorTabTitle", "積木技能編輯器"))
            .SetTooltipText(LOCTEXT("BlockEditorTabTooltip", "設計技能法陣（積木拼接）"))
            .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
    }

    virtual void ShutdownModule() override
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlockEditorTabName);
    }

private:
    TSharedRef<SDockTab> SpawnBlockEditorTab(const FSpawnTabArgs& /*Args*/)
    {
        // 建立空白 Graph；SpellCompiler 之後從 GetBlockNodes() 取節點樹
        UBlockEdGraph* Graph = NewObject<UBlockEdGraph>(GetTransientPackage());
        Graph->Schema = UBlockEdGraphSchema::StaticClass();

        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(LOCTEXT("BlockEditorTabLabel", "積木技能編輯器"))
            [
                SNew(SBlockEditorWidget)
                    .GraphToEdit(Graph)
            ];
    }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSkillCreatorEditorModule, SkillCreatorEditor)
