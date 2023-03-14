// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SmartObjectsEditorModule : ModuleRules
	{
		public SmartObjectsEditorModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"AdvancedPreviewScene",
				"Core",
				"CoreUObject",
				"ComponentVisualizers",
				"EditorInteractiveToolsFramework",
				"Engine",
				"InputCore",
				"InteractiveToolsFramework",
				"MassEntity",
				"PropertyEditor",
				"SlateCore",
				"Slate",
				"SmartObjectsModule",
				"SourceControl",
				"UnrealEd"
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RenderCore",
			}
			);
		}

	}
}
