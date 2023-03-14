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
					"AudioExtensions",
					"AssetRegistry",
					"Core", 
					"CoreUObject",
					"DeveloperSettings",
					"EditorStyle",
					"Engine",
					"InputCore",
					"SignalProcessing",
					"UnrealEd", 
					"Slate",
					"SlateCore",
					"ToolMenus",
					"WaveformEditorWidgets",
					"WaveformTransformationsWidgets"
				}
			);
		}
	}
}