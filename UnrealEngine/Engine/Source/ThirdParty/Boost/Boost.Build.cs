// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Boost : ModuleRules
{
	public Boost(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string BoostVersion = "1_82_0";
		string[] BoostLibraries = { "atomic", "chrono", "filesystem", "iostreams", "program_options", "python311", "regex", "system", "thread" };

		string BoostVersionDir = "boost-" + BoostVersion;
		string BoostPath = Path.Combine(Target.UEThirdPartySourceDirectory, "Boost", BoostVersionDir);
		string BoostIncludePath = Path.Combine(BoostPath, "include");
		PublicSystemIncludePaths.Add(BoostIncludePath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string BoostLibPath = Path.Combine(BoostPath, "lib", "Win64");

			foreach (string BoostLib in BoostLibraries)
			{
				string BoostLibName = "boost_" + BoostLib + "-mt-x64";
				PublicAdditionalLibraries.Add(Path.Combine(BoostLibPath, BoostLibName + ".lib"));
				RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", BoostLibName + ".dll"), Path.Combine(BoostLibPath, BoostLibName + ".dll"));
			}

			PublicDefinitions.Add("BOOST_ALL_NO_LIB");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string BoostLibPath = Path.Combine(BoostPath, "lib", "Mac");

			foreach (string BoostLib in BoostLibraries)
			{
				// Note that these file names identify the universal binaries
				// that support both x86_64 and arm64.
				string BoostLibName = "libboost_" + BoostLib + "-mt-a64";
				PublicAdditionalLibraries.Add(Path.Combine(BoostLibPath, BoostLibName + ".a"));
				RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", BoostLibName + ".dylib"), Path.Combine(BoostLibPath, BoostLibName + ".dylib"));
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string BoostLibPath = Path.Combine(BoostPath, "lib", "Unix", Target.Architecture.LinuxName);

			string BoostLibArchSuffix = "x64";
			if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
			{
				BoostLibArchSuffix = "a64";
			}

			foreach (string BoostLib in BoostLibraries)
			{
				string BoostLibName = "libboost_" + BoostLib + "-mt-" + BoostLibArchSuffix;
				PublicAdditionalLibraries.Add(Path.Combine(BoostLibPath, BoostLibName + ".a"));

				// Declare all version variations of the shared libraries as
				// runtime dependencies.
				foreach (string BoostSharedLibPath in Directory.EnumerateFiles(BoostLibPath, BoostLibName + ".so*"))
				{
					RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", Path.GetFileName(BoostSharedLibPath)), BoostSharedLibPath);
				}
			}
		}
	}
}
