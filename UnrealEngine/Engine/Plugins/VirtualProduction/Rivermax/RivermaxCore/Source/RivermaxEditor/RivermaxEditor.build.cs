// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RivermaxEditor : ModuleRules
{
	public RivermaxEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"PropertyEditor",
				"RivermaxCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"EditorStyle",
				"InputCore",
				"Slate",
				"SlateCore",
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				}
			);
		}
	}
}
