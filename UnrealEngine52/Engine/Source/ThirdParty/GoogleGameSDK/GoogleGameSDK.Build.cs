// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GoogleGameSDK : ModuleRules
{
	public GoogleGameSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string GoogleGameSDKPath = Target.UEThirdPartySourceDirectory + "GoogleGameSDK";

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string Arm64GameSDKPath = GoogleGameSDKPath + "/gamesdk/libs/arm64-v8a_API27_NDK23_cpp_static_Release/";
			string x86_64GameSDKPath = GoogleGameSDKPath + "/gamesdk/libs/x86_64_API27_NDK23_cpp_static_Release/";

			PublicAdditionalLibraries.Add(Arm64GameSDKPath + "libswappy_static.a");
			PublicAdditionalLibraries.Add(x86_64GameSDKPath + "libswappy_static.a");

			PublicAdditionalLibraries.Add(Arm64GameSDKPath + "libmemory_advice_static.a");
			PublicAdditionalLibraries.Add(x86_64GameSDKPath + "libmemory_advice_static.a");

			PublicSystemIncludePaths.Add(GoogleGameSDKPath + "/gamesdk/include");

            // Register Plugin Language
            string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "GoogleGameSDK_APL.xml"));
        }
    }
}
