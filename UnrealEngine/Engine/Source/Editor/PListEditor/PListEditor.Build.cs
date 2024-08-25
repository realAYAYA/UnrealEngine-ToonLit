// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PListEditor : ModuleRules
{
	public PListEditor( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"InputCore",
				"Slate",
				"SlateCore",
				"DesktopPlatform",
				"XmlParser",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"WorkspaceMenuStructure",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
			}
		);
	}
}
