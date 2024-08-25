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
				"Engine",
				"GameplayTags",
				"SmartObjectsModule",
				"SourceControl",
				"StructUtils",
				"UnrealEd",
				"WorldConditions",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"AssetDefinition",
				"BlueprintGraph",
				"ComponentVisualizers",
				"InputCore",
				"PropertyAccessEditor",
				"PropertyBindingUtils",
				"PropertyEditor",
				"RenderCore",
				"Slate",
				"SlateCore",
				"StructUtilsEditor",
				"ToolWidgets",
			}
			);
		}

	}
}
