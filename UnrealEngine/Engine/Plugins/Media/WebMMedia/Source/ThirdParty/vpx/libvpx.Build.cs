// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LibVpx : ModuleRules
{
	public LibVpx(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string RootPath = ModuleDirectory;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string LibPath = RootPath + "/lib/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			string LibFileName = "vpxmd.lib";
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, LibFileName));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(RootPath + "/lib/Unix/" + Target.Architecture + ((Target.LinkType == TargetLinkType.Monolithic) ? "/libvpx" : "/libvpx_fPIC") + ".a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(RootPath + "/lib/Mac" + ((Target.LinkType == TargetLinkType.Monolithic) ? "/libvpx" : "/libvpx_fPIC") + ".a");
		}

		string IncludePath = RootPath + "/include";
		PublicIncludePaths.Add(IncludePath);
	}
}
