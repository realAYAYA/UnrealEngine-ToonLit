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

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				// Engine does not currently ship with Mac or Stadia binaries for WebRTC so use fallback
				
				PrivateDependencyModuleNames.Add("LibVpx");
				
				PublicDependencyModuleNames.AddRange(
					new string[] {
					"LibWebM",
				});

				PublicDefinitions.Add("WITH_WEBM_LIBS=1");
			} 
			else if ((Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture.StartsWith("x86_64")) || Target.Platform == UnrealTargetPlatform.Win64)
			{
				// libVPX is linked inside WebRTC so just use those headers and binaries where avaliable
				PrivateIncludePaths.Add(Path.Join(EngineDirectory, "Source", "ThirdParty", WebRTC.WebRtcVersionPath, "Include", "third_party", "libvpx", "source", "libvpx"));

				PublicDependencyModuleNames.Add("WebRTC");

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
