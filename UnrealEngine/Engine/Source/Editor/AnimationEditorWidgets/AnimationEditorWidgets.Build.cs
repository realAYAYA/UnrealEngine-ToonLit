// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationEditorWidgets : ModuleRules
{
	public AnimationEditorWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		/** NOTE: THIS MODULE SHOULD NOT EVER DEPEND ON UNREALED. 
		 * Please refer to ToolWidgets.Build.cs for more information.
		 */
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
				"InputCore",
				"ToolWidgets",
				"AnimationCore",
				"ApplicationCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"CoreUObject",
				"UnrealEd",
				"GraphEditor",
				"PropertyEditor"
			}
		);
	}
}
