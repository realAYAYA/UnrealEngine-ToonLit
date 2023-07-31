// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterMedia : ModuleRules
{
	public DisplayClusterMedia(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		// [temporary] We need this to be able to use some private data types. This should
		// be removed once we move the nD rendering pipeline to RDG.
		string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source", "Runtime", "Renderer", "Private"));

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"DisplayClusterShaders",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DisplayCluster",
				"DisplayClusterConfiguration",
				"Engine",
				"MediaAssets",
				"MediaIOCore",
				"Renderer",
				"RenderCore",
				"RHI",
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
