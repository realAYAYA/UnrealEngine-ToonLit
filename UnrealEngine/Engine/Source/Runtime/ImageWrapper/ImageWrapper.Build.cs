// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageWrapper : ModuleRules
{
	public ImageWrapper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.AddRange(new string[] {
			"ImageCore"
			}
		);

		PublicDefinitions.Add("WITH_UNREALPNG=1");
		PublicDefinitions.Add("WITH_UNREALJPEG=1");

		PrivateDependencyModuleNames.Add("Core");

		PublicDependencyModuleNames.Add("LibTiff");
		PublicDependencyModuleNames.Add("ImageCore");

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"zlib",
			"UElibPNG",
			"LibTiff"
		);

		// Jpeg Decoding
		// LibJpegTurbo is much faster than UElibJPG but has not been compiled or tested for all platforms
		// Note that currently this module is included at runtime, so consider the increase in exe size before
		// enabling for any of the console/phone platforms!

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows)
			|| Target.Platform == UnrealTargetPlatform.Mac
			|| Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicDefinitions.Add("WITH_LIBJPEGTURBO=1");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "LibJpegTurbo");
		}
		else
		{
			PublicDefinitions.Add("WITH_LIBJPEGTURBO=0");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "UElibJPG");
		}

		// Add openEXR lib for windows builds.
		if ((Target.Platform.IsInGroup(UnrealPlatformGroup.Windows)) ||
			(Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture.StartsWith("x86_64")))
		{
			PublicDefinitions.Add("WITH_UNREALEXR=1");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Imath");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "UEOpenExr");
		}
		else
		{
			PublicDefinitions.Add("WITH_UNREALEXR=0");
		}

		// Enable exceptions to allow error handling
		bEnableExceptions = true;
	}
}
