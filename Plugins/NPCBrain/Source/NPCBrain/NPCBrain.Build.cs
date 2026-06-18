using UnrealBuildTool;

public class NPCBrain : ModuleRules
{
	public NPCBrain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			// FGridPos / EMaterialType / ICreature / IWorldInterface — zero gameplay
			// dependencies, lets perception (M-NPC-3) stay decoupled from
			// SkillCreatorRuntime/VoxelWorld concrete actor types.
			"SkillCreatorCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"Json",
			"JsonUtilities",
		});
	}
}
