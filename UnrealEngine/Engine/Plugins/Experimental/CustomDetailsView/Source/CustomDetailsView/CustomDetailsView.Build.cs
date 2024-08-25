// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomDetailsView : ModuleRules
{
	public CustomDetailsView(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Slate",
				"SlateCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"PropertyEditor",
				"ToolMenus",
				"UnrealEd"
			}
		);
	}
}
