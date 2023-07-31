// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class WaveformTransformationsWidgets : ModuleRules
	{
		public WaveformTransformationsWidgets(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AudioExtensions",
					"ContentBrowser",
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"EditorStyle",
					"Engine",
					"InputCore",
					"Slate",
					"SignalProcessing",
					"SlateCore", 
					"ToolMenus", 
					"WaveformTransformations",
					"WaveformEditorWidgets",
				}
			);
		}
	}
}