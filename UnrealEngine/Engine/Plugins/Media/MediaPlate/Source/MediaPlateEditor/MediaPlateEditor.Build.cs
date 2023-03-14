// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaPlateEditor : ModuleRules
{
	public MediaPlateEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CinematicCamera",
				"CoreUObject",
				"DesktopWidgets",
				"EditorStyle",
				"Engine",
				"GeometryCore",
				"InputCore",
				"MediaAssets",
				"MediaCompositing",
				"MediaCompositingEditor",
				"MediaPlate",
				"MediaPlayerEditor",
				"ModelingComponentsEditorOnly",
				"ModelingComponents",
				"MovieScene",
				"PropertyEditor",
				"SceneOutliner",
				"Sequencer",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
			);
	}
}
