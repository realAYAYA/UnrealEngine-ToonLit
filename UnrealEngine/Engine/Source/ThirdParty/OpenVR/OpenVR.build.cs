// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class OpenVR : ModuleRules
{
	public OpenVR(ReadOnlyTargetRules Target) : base(Target)
	{
		/** Mark the current version of the OpenVR SDK */
		string OpenVRVersion = "v1_5_17";
		Type = ModuleType.External;

		string SdkBase = Target.UEThirdPartySourceDirectory + "OpenVR/OpenVR" + OpenVRVersion;
		if (!Directory.Exists(SdkBase))
		{
			string Err = string.Format("OpenVR SDK not found in {0}", SdkBase);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}

		PublicIncludePaths.Add(SdkBase + "/headers");

		string LibraryPath = SdkBase + "/lib/";

		if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibraryPath += "win64/";
			PublicAdditionalLibraries.Add(LibraryPath + "openvr_api.lib");
			PublicDelayLoadDLLs.Add("openvr_api.dll");

			string OpenVRBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/OpenVR/OpenVR{0}/Win64/", OpenVRVersion);
			RuntimeDependencies.Add(OpenVRBinariesDir + "openvr_api.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string DylibPath = Target.UEThirdPartyBinariesDirectory + "OpenVR/OpenVR" + OpenVRVersion + "/osx32/libopenvr_api.dylib";
			PublicDelayLoadDLLs.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
		{
			LibraryPath += "linux64/";
			PublicAdditionalLibraries.Add(LibraryPath + "libopenvr_api.so");

			string DylibDir = Target.UEThirdPartyBinariesDirectory + "OpenVR/OpenVR" + OpenVRVersion + "/linux64";
			PrivateRuntimeLibraryPaths.Add(DylibDir);

			string DylibPath = DylibDir + "/libopenvr_api.so";
			PublicDelayLoadDLLs.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
	}
}
