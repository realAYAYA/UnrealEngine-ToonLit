// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ToolWidgets : ModuleRules
{
	public ToolWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		/** NOTE: THIS MODULE SHOULD NOT EVER DEPEND ON UNREALED. 
		 * If you are adding a reusable widget that depends on UnrealEd, add it to EditorWidgets instead
		 */
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"ToolMenus",
				"CoreUObject"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework",
				"ApplicationCore"
			}
		);
	}
}
