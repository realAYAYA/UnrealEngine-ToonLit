// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PCGEditor : ModuleRules
	{
		public PCGEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Projects",
					"Engine",
					"CoreUObject",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{					
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]{
					"AppFramework",
					"ApplicationCore",
					"AssetTools",
					"BlueprintGraph",
					"DesktopWidgets",
					"DetailCustomizations",
					"EditorScriptingUtilities",
					"EditorStyle",
					"EditorSubsystem",
					"EditorWidgets",
					"GraphEditor",
					"InputCore",
					"Kismet",
					"KismetWidgets",
					"PCG",
					"PropertyEditor",
					"StructUtils",
					"StructUtilsEditor",
					"Slate",
					"SlateCore",
					"SourceControl",
					"ToolMenus",
					"UnrealEd",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
				});
		}
	}
}
