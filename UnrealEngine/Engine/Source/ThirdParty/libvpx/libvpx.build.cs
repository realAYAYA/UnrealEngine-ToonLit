// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LibVpx : ModuleRules
{
	protected virtual string LibVpxVersion { get { return Target.IsInPlatformGroup(UnrealPlatformGroup.Unix)? "libvpx-1.6.1":"libvpx-1.10.0"; } }
	protected virtual string RootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string LibvpxIncludePath { get { return Path.Combine(RootDirectory, "libvpx", LibVpxVersion, "include"); } }
	protected virtual string LibvpxLibraryPath { get { return Path.Combine(RootDirectory, "libvpx", LibVpxVersion, "lib"); } }

	public LibVpx(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibraryPath = LibvpxLibraryPath + "/";
		string Config = Target.Configuration == UnrealTargetConfiguration.Debug ? "Debug" : "Release";

		if ((Target.Platform == UnrealTargetPlatform.Win64))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibvpxLibraryPath, "Win64", Config, "libvpx.lib"));
		} 
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibvpxLibraryPath, "Unix", Target.Architecture.LinuxName, ((Target.LinkType == TargetLinkType.Monolithic) ? "libvpx.a" : "libvpx_fPIC.a")));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibvpxLibraryPath, "Mac", ((Target.LinkType == TargetLinkType.Monolithic) ? "libvpx.a" : "libvpx_fPIC.a")));
		}
		PublicSystemIncludePaths.Add(LibvpxIncludePath);
	}
}
