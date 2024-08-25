// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Nanosvg : ModuleRules
{
	public Nanosvg(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;

		PublicDefinitions.Add("NSVG_USE_BGRA=1");

		PublicSystemIncludePaths.Add(Path.Join(ModuleDirectory, "src"));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
	}
}
