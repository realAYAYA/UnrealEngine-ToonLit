// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class WebMMedia : ModuleRules
	{
		public WebMMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"WebMMediaFactory",
					"Core",
					"Engine",
					"RenderCore",
					"RHI",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Media",
					"MediaUtils",
					"libOpus",
					"UEOgg",
					"Vorbis",
				});

			if (Target.Platform == UnrealTargetPlatform.Mac ||
				(Target.Platform == UnrealTargetPlatform.Win64 && Target.WindowsPlatform.Architecture != UnrealArch.Arm64) ||
				(Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.X64))
			{
				PublicDependencyModuleNames.Add("LibVpx");
				
				PublicDependencyModuleNames.AddRange(
					new string[] {
					"LibWebM",
				});

				PublicDefinitions.Add("WITH_WEBM_LIBS=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_WEBM_LIBS=0");
			}
		}
	}
}
