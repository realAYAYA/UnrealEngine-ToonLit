// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ImgMedia : ModuleRules
	{
		public ImgMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"ImageWrapper",
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"ImgMediaEngine",
					"ImgMediaFactory",
					"MediaUtils",
					"MovieScene",
					"RenderCore",
					"Renderer",
					"RHI",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"ImageWrapper",
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"ImgMedia/Private",
					"ImgMedia/Private/Assets",
					"ImgMedia/Private/GlobalCache",
					"ImgMedia/Private/Loader",
					"ImgMedia/Private/Player",
					"ImgMedia/Private/Readers",
					"ImgMedia/Private/Scheduler",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
					"TimeManagement",
				});

			bool bLinuxEnabled = Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64");

			if ((Target.Platform == UnrealTargetPlatform.Mac) ||
				Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
				bLinuxEnabled)
			{
				PrivateDependencyModuleNames.Add("OpenExrWrapper");
				PrivateDependencyModuleNames.Add("ExrReaderGpu");
			}

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
				PrivateDependencyModuleNames.Add("D3D12RHI");
			}
			
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source", "Runtime", "Renderer", "Private"));

			// Is this the editor?
			if (Target.bBuildEditor == true)
			{
				PublicDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}
