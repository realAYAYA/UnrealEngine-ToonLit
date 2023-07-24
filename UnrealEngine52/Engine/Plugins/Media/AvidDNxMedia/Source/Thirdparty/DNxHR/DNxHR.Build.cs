// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class DNxHR : ModuleRules
{
	public DNxHR(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string IncPath = Path.Combine(ModuleDirectory, "include");
			PublicSystemIncludePaths.Add(IncPath);

			string LibPath = Path.Combine(ModuleDirectory, "lib64");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "DNxHR.lib"));

			PublicDelayLoadDLLs.Add("DNxHR.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/Win64/DNxHR.dll");
		}
	}
}
