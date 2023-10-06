// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using EpicGames.Core;

public class ArmlibGPUInfo : ModuleRules
{
	public ArmlibGPUInfo(ReadOnlyTargetRules Target) : base(Target)
	{
		//Type = ModuleType.CPlusPlus;
		//string ArmlibGPUInfoPath = Target.UEThirdPartySourceDirectory + "ARM/ArmlibGPUInfo";

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicSystemIncludePaths.Add(ModuleDirectory);
			//			PrivateIncludePathModuleNames.Add(ArmlibGPUInfoPath + "/ArmlibGPUInfo");

			//             // Register Plugin Language
			//             string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			//             AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "ArmlibGPUInfo_APL.xml"));
		}
	}
}
