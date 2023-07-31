// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataValidation : ModuleRules
{
	public DataValidation(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MessageLog",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"TargetPlatform",
				"EditorSubsystem"
			}
		);
//		goto through these one by one and remove extra ones
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"AssetRegistry",
				"Json",
				"CollectionManager",
				"ContentBrowser",
				"WorkspaceMenuStructure",
				"AssetTools",
				"PropertyEditor",
				"GraphEditor",
				"BlueprintGraph",
				"KismetCompiler",
				"SandboxFile",
				"Blutility",
				"ToolMenus",
				"SourceControl",
				"UncontrolledChangelists",
			}
		);
	}
}
