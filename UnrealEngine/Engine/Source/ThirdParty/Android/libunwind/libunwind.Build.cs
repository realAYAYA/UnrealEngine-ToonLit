// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class libunwind : ModuleRules
{
	public libunwind(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string libunwindLibraryPath = Target.UEThirdPartySourceDirectory + "Android/libunwind/Android/Release/";
			string libunwindIncludePath = Target.UEThirdPartySourceDirectory + "Android/libunwind/libunwind/include/";
			PublicIncludePaths.Add(libunwindIncludePath);

			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(libunwindLibraryPath, "arm64-v8a", "libunwind.a"),
				Path.Combine(libunwindLibraryPath, "arm64-v8a", "libunwindbacktrace.a"),
				/*
				Path.Combine(libunwindLibraryPath, "x64", "libunwind.a"),
				Path.Combine(libunwindLibraryPath, "x64", "libunwindbacktrace.a"), */
			});
		}
    }
}
