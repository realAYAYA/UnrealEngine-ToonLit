// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterLightCardEditorShaders : ModuleRules
{
	public DisplayClusterLightCardEditorShaders(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"Renderer",
				"RHI",
				"Projects"
			});

		PrivateIncludePaths.AddRange(
			new string[] {
			});

		ShortName = "DCLCEShaders";
	}
}
