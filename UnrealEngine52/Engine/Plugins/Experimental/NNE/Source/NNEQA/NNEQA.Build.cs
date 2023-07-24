// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NNEQA : ModuleRules
{
	public NNEQA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Projects",
				"Json",
				"CoreUObject",
				"NNECore",
				"NNERuntimeRDG",
				"NNEUtils",
				"RHI",
				"RenderCore"
			}
		);

		// RuntimeDependencies
		foreach (string JsonFilePath in Directory.EnumerateFiles(Path.Combine(ModuleDirectory, "Resources"), "*.json"))
		{
			RuntimeDependencies.Add(JsonFilePath);
		}
	}
}
