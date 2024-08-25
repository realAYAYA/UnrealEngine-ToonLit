// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Draco : ModuleRules
{
	public Draco(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DracoLibsDir = Path.Combine(ModuleDirectory, "lib");
		string DracoIncDir = Path.Combine(ModuleDirectory, "include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string Win64DracoLibsDir = Path.Combine(DracoLibsDir, "Win64");

			PublicSystemIncludePaths.Add(DracoIncDir);
			PublicSystemLibraryPaths.Add(Win64DracoLibsDir);

			foreach (string DracoLib in Directory.EnumerateFiles(Win64DracoLibsDir, "*.lib", SearchOption.AllDirectories))
			{
				PublicAdditionalLibraries.Add(DracoLib);
			}

			PublicDefinitions.Add("USE_DRACO_LIBRARY=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string MacOSDracoLibsDir = Path.Combine(DracoLibsDir, "Linux");

			PublicSystemIncludePaths.Add(DracoIncDir);
			PublicSystemLibraryPaths.Add(MacOSDracoLibsDir);

			foreach (string DracoLib in Directory.EnumerateFiles(MacOSDracoLibsDir, "*.a", SearchOption.AllDirectories))
			{
				PublicAdditionalLibraries.Add(DracoLib);
			}

			PublicDefinitions.Add("USE_DRACO_LIBRARY=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string MacOSDracoLibsDir = Path.Combine(DracoLibsDir, "MacOS");

			PublicSystemIncludePaths.Add(DracoIncDir);
			PublicSystemLibraryPaths.Add(MacOSDracoLibsDir);

			foreach (string DracoLib in Directory.EnumerateFiles(MacOSDracoLibsDir, "*.a", SearchOption.AllDirectories))
			{
				PublicAdditionalLibraries.Add(DracoLib);
			}

			PublicDefinitions.Add("USE_DRACO_LIBRARY=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			string iOSDracoLibsDir = Path.Combine(DracoLibsDir, "iOS");

			PublicSystemIncludePaths.Add(DracoIncDir);
			PublicSystemLibraryPaths.Add(iOSDracoLibsDir);

			foreach (string DracoLib in Directory.EnumerateFiles(iOSDracoLibsDir, "*.a", SearchOption.AllDirectories))
			{
				PublicAdditionalLibraries.Add(DracoLib);
			}

			PublicDefinitions.Add("USE_DRACO_LIBRARY=1");
		}
		else
		{
			PublicDefinitions.Add("USE_DRACO_LIBRARY=0");
		}
	}
}
