// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class DNxUncompressed : ModuleRules
{
	public DNxUncompressed(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string IncPath = Path.Combine(ModuleDirectory, "include");
			PublicSystemIncludePaths.Add(IncPath);

			string LibPath = Path.Combine(ModuleDirectory, "lib64");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "DNxUncompressedSDK.lib"));

			PublicDelayLoadDLLs.Add("DNxUncompressedSDK.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/Win64/DNxUncompressedSDK.dll");
		}
	}
}
