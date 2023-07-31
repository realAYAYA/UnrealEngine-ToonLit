// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class nvTextureTools : ModuleRules
{
	public nvTextureTools(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string nvttPath = Target.UEThirdPartySourceDirectory + "nvTextureTools/nvTextureTools-2.0.8/";

		string nvttLibPath = nvttPath + "lib";

		PublicIncludePaths.Add(nvttPath + "src/src");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			nvttLibPath += ("/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/");

			PublicAdditionalLibraries.Add(nvttLibPath + "nvtt_64.lib");

			PublicDelayLoadDLLs.Add("nvtt_64.dll");

			// note this does not stage it to a place where DelayLoad will find it, you must manually load :
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/nvTextureTools/Win64/nvtt_64.dll");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/nvTextureTools/Win64/AVX2/nvtt_64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string NVCorePath = Target.UEThirdPartyBinariesDirectory + "nvTextureTools/Mac/libnvcore.dylib";
			string NVImagePath = Target.UEThirdPartyBinariesDirectory + "nvTextureTools/Mac/libnvimage.dylib";
			string NVMathPath = Target.UEThirdPartyBinariesDirectory + "nvTextureTools/Mac/libnvmath.dylib";
			string NVTTPath = Target.UEThirdPartyBinariesDirectory + "nvTextureTools/Mac/libnvtt.dylib";

			PublicDelayLoadDLLs.Add(NVCorePath);
			PublicDelayLoadDLLs.Add(NVImagePath);
			PublicDelayLoadDLLs.Add(NVMathPath);
			PublicDelayLoadDLLs.Add(NVTTPath);

			RuntimeDependencies.Add(NVCorePath);
			RuntimeDependencies.Add(NVImagePath);
			RuntimeDependencies.Add(NVMathPath);
			RuntimeDependencies.Add(NVTTPath);

			PublicAdditionalLibraries.Add(nvttPath + "lib/Mac/libsquish.a");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string NvBinariesDir = Target.UEThirdPartyBinariesDirectory + "nvTextureTools/Linux/" + Target.Architecture;
			PrivateRuntimeLibraryPaths.Add(NvBinariesDir);

			PublicAdditionalLibraries.Add(NvBinariesDir + "/libnvcore.so");
			PublicAdditionalLibraries.Add(NvBinariesDir + "/libnvimage.so");
			PublicAdditionalLibraries.Add(NvBinariesDir + "/libnvmath.so");
			PublicAdditionalLibraries.Add(NvBinariesDir + "/libnvtt.so");

			RuntimeDependencies.Add(NvBinariesDir + "/libnvcore.so");
			RuntimeDependencies.Add(NvBinariesDir + "/libnvimage.so");
			RuntimeDependencies.Add(NvBinariesDir + "/libnvmath.so");
			RuntimeDependencies.Add(NvBinariesDir + "/libnvtt.so");
		}
	}
}

