// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using System;
using UnrealBuildTool;

public class DirectShow : ModuleRules
{
	public DirectShow(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{

            string DirectShowLibPath = Target.UEThirdPartySourceDirectory
                + "DirectShow/DirectShow-1.0.0/Lib/Win64/vs" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				DirectShowLibPath = Path.Combine(DirectShowLibPath, Target.Architecture.WindowsLibDir);
			}

			PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "DirectShow/DirectShow-1.0.0/src/Public");

			string LibraryName = "DirectShow";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibraryName += "d";
			}
			LibraryName += "_64.lib";
			PublicAdditionalLibraries.Add(DirectShowLibPath + "/" + LibraryName);
		}
	}
}

