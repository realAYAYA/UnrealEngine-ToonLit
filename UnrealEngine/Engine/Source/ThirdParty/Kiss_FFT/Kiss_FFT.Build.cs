// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Kiss_FFT : ModuleRules
{
	public Kiss_FFT(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_KISSFFT=1");

		// Compile and link with kissFFT
		string Kiss_FFTPath = Target.UEThirdPartySourceDirectory + "Kiss_FFT/kiss_fft129";

		PublicIncludePaths.Add(Kiss_FFTPath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibDir;
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibDir = Kiss_FFTPath + "/lib/x64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/Debug/";
			}
			else
			{
				LibDir = Kiss_FFTPath + "/lib/x64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/Release/";
			}

			PublicAdditionalLibraries.Add(LibDir + "KissFFT.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Mac/Debug/libKissFFT.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Mac/Release/libKissFFT.a");
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Linux/Release/" + Target.Architecture + "/libKissFFT.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Linux/Release/" + Target.Architecture + "/libKissFFT_fPIC.a");
			}

			if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
			{
				PrecompileForTargets = PrecompileTargetsType.None;
			}
		}
	}
}
