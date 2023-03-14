// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GpgCppSDK : ModuleRules
{
	public GpgCppSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string GPGAndroidPath = Path.Combine(Target.UEThirdPartySourceDirectory, "GooglePlay/gpg-cpp-sdk.v3.0.1/gpg-cpp-sdk/android/");

			PublicIncludePaths.Add(Path.Combine(GPGAndroidPath, "include/"));

			PublicAdditionalLibraries.Add(Path.Combine(GPGAndroidPath, "lib/c++/arm64-v8a/libgpg.a"));
		}
	}
}
