// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorModifierCoreEditor : ModuleRules
{
	public ActorModifierCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ActorModifierCore",
				"Core",
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ActorModifierCore",
				"ApplicationCore",
				"CoreUObject",
				"CustomDetailsView",
				"EditorSubsystem",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"OperatorStackEditor",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
				"WorkspaceMenuStructure",
			}
		);
	}
}
