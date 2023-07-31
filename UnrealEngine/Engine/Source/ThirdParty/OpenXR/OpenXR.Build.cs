// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class OpenXR : ModuleRules
{
	protected string RootPath { get => Target.UEThirdPartySourceDirectory + "OpenXR"; }
	protected string LoaderPath { get => RootPath + "/loader"; }

	public OpenXR(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(RootPath + "/include");
		PublicDefinitions.Add("XR_NO_PROTOTYPES");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(LoaderPath + "/win64/openxr_loader.lib");

			PublicDelayLoadDLLs.Add("openxr_loader.dll");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/OpenXR/win64/openxr_loader.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string OpenXRPath = "$(EngineDir)/Binaries/ThirdParty/OpenXR/linux/x86_64-unknown-linux-gnu/libopenxr_loader.so";

			PublicAdditionalLibraries.Add(OpenXRPath);
			RuntimeDependencies.Add(OpenXRPath);
		}
	}
}
