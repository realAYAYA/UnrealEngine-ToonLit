// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class BlackmagicSDK : ModuleRules
{
	public BlackmagicSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibraryName = "libBlackmagicLib";
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = Path.Combine(ThirdPartyPath, "Win64");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibraryName + ".lib"));

			string IncludeDir = Path.Combine(ModuleDirectory, "..", "Build", "Include");
			PublicSystemIncludePaths.Add(IncludeDir);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string LibPath = Path.Combine(ThirdPartyPath, "Linux");

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibraryName + ".so"));
			RuntimeDependencies.Add(Path.Combine(LibPath, LibraryName + ".so"));
			PrivateRuntimeLibraryPaths.Add(LibPath);


			string IncludeDir = Path.Combine(ModuleDirectory, "Include", "Linux");
			PublicSystemIncludePaths.Add(IncludeDir);
		}
	}
}
