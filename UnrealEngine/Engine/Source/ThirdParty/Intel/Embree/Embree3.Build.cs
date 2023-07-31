// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Embree3 : ModuleRules
{
	public Embree3(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// EMBREE
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string SDKDir = Target.UEThirdPartySourceDirectory + "Intel/Embree/Embree3122/Win64/";

			PublicIncludePaths.Add(SDKDir + "include");
			PublicAdditionalLibraries.Add(SDKDir + "lib/embree3.lib");
			PublicAdditionalLibraries.Add(SDKDir + "lib/tbb.lib");
			RuntimeDependencies.Add("$(TargetOutputDir)/embree3.dll", SDKDir + "lib/embree3.dll");
			RuntimeDependencies.Add("$(TargetOutputDir)/tbb12.dll", SDKDir + "lib/tbb12.dll");
			PublicDelayLoadDLLs.Add("embree3.dll");
			PublicDelayLoadDLLs.Add("tbb12.dll");
			PublicDefinitions.Add("USE_EMBREE=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string SDKDir = Target.UEThirdPartySourceDirectory + "Intel/Embree/Embree3122/MacOSX/";
			string LibDir = Path.Combine(SDKDir, "lib");

			PublicIncludePaths.Add(Path.Combine(SDKDir, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libembree3.3.dylib"));
			RuntimeDependencies.Add("$(TargetOutputDir)/libembree3.3.dylib", Path.Combine(LibDir, "libembree3.3.dylib"));
			if (Target.Architecture.ToLower().Contains("x86"))
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libtbb.12.dylib"));
				RuntimeDependencies.Add("$(TargetOutputDir)/libtbb.12.dylib", Path.Combine(LibDir, "libtbb.12.dylib"));
			}
			PublicDefinitions.Add("USE_EMBREE=1");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture.StartsWith("x86_64")) // no support for arm64 yet
		{
			string IntelEmbreeLibs = Target.UEThirdPartyBinariesDirectory + "Intel/Embree/Embree3122";
			string IncludeDir = Target.UEThirdPartySourceDirectory + "Intel/Embree/Embree3122/Linux/x86_64-unknown-linux-gnu";
			string SDKDir = Path.Combine(IntelEmbreeLibs, "Linux/x86_64-unknown-linux-gnu/lib");

			PublicIncludePaths.Add(Path.Combine(IncludeDir, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(SDKDir, "libembree3.so"));
			RuntimeDependencies.Add(Path.Combine(SDKDir, "libembree3.so"));
			RuntimeDependencies.Add(Path.Combine(SDKDir, "libembree3.so.3"));
			PublicDefinitions.Add("USE_EMBREE=1");
		}
		else
		{
			PublicDefinitions.Add("USE_EMBREE=0");
		}
	}
}
