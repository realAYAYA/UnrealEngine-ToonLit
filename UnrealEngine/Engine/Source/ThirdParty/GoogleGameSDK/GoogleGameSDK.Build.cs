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
			PublicAdditionalLibraries.Add(GoogleGameSDKPath + "/gamesdk/libs/arm64-v8a_API24_NDK21_cpp_shared_Release/libgamesdk.a");
			PublicAdditionalLibraries.Add(GoogleGameSDKPath + "/gamesdk/libs/x86_64_API24_NDK21_cpp_shared_Release/libgamesdk.a");
			PublicIncludePaths.Add(GoogleGameSDKPath + "/gamesdk/include");

            // Register Plugin Language
            string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "GoogleGameSDK_APL.xml"));
        }
    }
}
