#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "SBlockEditorWidget.h"
#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"

static const FName BlockEditorTabName = TEXT("BlockSpellEditor");

static TSharedRef<SDockTab> SpawnBlockEditorTab(const FSpawnTabArgs& /*Args*/)
{
    UBlockEdGraph* Graph = NewObject<UBlockEdGraph>(
        GetTransientPackage(), UBlockEdGraph::StaticClass(),
        NAME_None, RF_Transient);
    Graph->Schema = UBlockEdGraphSchema::StaticClass();
    Graph->AddToRoot(); // keep alive while tab is open

    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(NSLOCTEXT("BlockEditor","TabLabel","Block Spell Editor"))
        [
            SNew(SBlockEditorWidget).GraphToEdit(Graph)
        ];
}

class FSkillCreatorUIModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            BlockEditorTabName,
            FOnSpawnTab::CreateStatic(&SpawnBlockEditorTab))
            .SetDisplayName(NSLOCTEXT("BlockEditor","TabTitle","Block Spell Editor"))
            .SetTooltipText(NSLOCTEXT("BlockEditor","TabTip","Open the spell block graph editor"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        // Add "Window > Block Spell Editor" menu entry so the user
        // doesn't need to use the console to open the tab.
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
            {
                UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(
                    TEXT("MainFrame.MainMenu.Window"));
                FToolMenuSection& Section =
                    Menu->FindOrAddSection(TEXT("SkillCreator"),
                        NSLOCTEXT("BlockEditor","MenuSection","Skill Creator"));
                Section.AddMenuEntry(
                    TEXT("BlockSpellEditor"),
                    NSLOCTEXT("BlockEditor","MenuEntry","Block Spell Editor"),
                    NSLOCTEXT("BlockEditor","MenuEntryTip","Open the spell block graph editor"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.OpenLevelBlueprint")),
                    FUIAction(FExecuteAction::CreateLambda([]()
                    {
                        FGlobalTabmanager::Get()->TryInvokeTab(FTabId(BlockEditorTabName));
                    }))
                );
            })
        );
    }

    virtual void ShutdownModule() override
    {
        // Startup callbacks are one-shot (already fired); no removal needed.
        // Tab spawner must be unregistered to avoid dangling handles.
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlockEditorTabName);
    }
};

IMPLEMENT_MODULE(FSkillCreatorUIModule, SkillCreatorUI)
