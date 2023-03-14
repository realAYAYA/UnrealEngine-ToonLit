// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenXR_HoloLens : OpenXR
	{
		public OpenXR_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
			{
				PublicAdditionalLibraries.Add(LoaderPath + "/hololens/x64/openxr_loader.lib");

				PublicDelayLoadDLLs.Add("openxr_loader.dll");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/OpenXR/hololens/x64/openxr_loader.dll");
			}
			else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
			{
				PublicAdditionalLibraries.Add(LoaderPath + "/hololens/arm64/openxr_loader.lib");

				PublicDelayLoadDLLs.Add("openxr_loader.dll");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/OpenXR/hololens/arm64/openxr_loader.dll");
			}

		}
	}
}
