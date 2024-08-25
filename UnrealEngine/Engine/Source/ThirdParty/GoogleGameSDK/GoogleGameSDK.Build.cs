// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using EpicGames.Core;

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

			bool bEnableGameSDKMemAdvisor = false;
			DirectoryReference ProjectDir = Target.ProjectFile == null ? (DirectoryReference)null : Target.ProjectFile.Directory;
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectDir, Target.Platform);
			if (EngineIni != null)
			{
				EngineIni.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGameSDKMemAdvisor", out bEnableGameSDKMemAdvisor);
			}
			
			if (bEnableGameSDKMemAdvisor)
			{
				PublicAdditionalLibraries.Add(Arm64GameSDKPath + "libmemory_advice_static.a");
				PublicAdditionalLibraries.Add(x86_64GameSDKPath + "libmemory_advice_static.a");
				PublicDefinitions.Add("HAS_ANDROID_MEMORY_ADVICE=1");
			}
			else
			{
				PublicDefinitions.Add("HAS_ANDROID_MEMORY_ADVICE=0");
			}

			PublicSystemIncludePaths.Add(GoogleGameSDKPath + "/gamesdk/include");

            // Register Plugin Language
            string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "GoogleGameSDK_APL.xml"));
        }
    }
}
