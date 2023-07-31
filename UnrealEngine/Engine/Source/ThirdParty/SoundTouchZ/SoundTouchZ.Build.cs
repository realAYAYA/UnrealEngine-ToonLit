// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class SoundTouchZ : ModuleRules
{

	protected virtual bool bPlatformSupportsSoundTouchZ
	{
		get
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
				   Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
				   Target.Platform == UnrealTargetPlatform.Android ||
				   Target.Platform == UnrealTargetPlatform.IOS;
		}
	}

	protected virtual string LibraryRootDir
	{
        get
        {
			return ModuleDirectory;
		}
	}

	protected virtual string IncludeDir
	{
        get
        {
			return Path.Combine(ModuleDirectory, "include");
		}
	}


	public SoundTouchZ(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add(String.Format("WITH_SOUNDTOUCHZ={0}", bPlatformSupportsSoundTouchZ ? 1 : 0));
		PublicIncludePaths.Add(IncludeDir);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryRootDir, "lib", "Android", "arm64-v8a", "libSoundTouchZ-Android-Shipping.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryRootDir, "lib", "IOS", "libSoundTouchZ-IOS-Shipping.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			PublicAdditionalLibraries.Add(Path.Combine(LibraryRootDir, "lib", "Win64", "libSoundTouchZ-Win64-Shipping.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			//PublicAdditionalLibraries.Add(Path.Combine(LibraryRootDir, "lib", "Mac", "libSoundTouchZ-Mac-x86_64-Shipping.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
			PublicAdditionalLibraries.Add(Path.Combine(LibraryRootDir, "lib", "Linux", Target.Architecture, "libSoundTouchZ.a"));
        }
	}
}
