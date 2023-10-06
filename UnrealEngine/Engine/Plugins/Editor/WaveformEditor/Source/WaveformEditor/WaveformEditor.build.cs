// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class WaveformEditor : ModuleRules
	{
		public WaveformEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AssetRegistry",
					"AudioExtensions",
					"AudioWidgets",
					"Core", 
					"CoreUObject",
					"ContentBrowser",
					"DeveloperSettings",
					"EditorStyle",
					"Engine",
					"InputCore",
					"SignalProcessing",
					"Slate",
					"SlateCore",
					"ToolMenus",
					"UnrealEd", 
					"WaveformEditorWidgets",
					"WaveformTransformationsWidgets"
				}
			);
		}
	}
}