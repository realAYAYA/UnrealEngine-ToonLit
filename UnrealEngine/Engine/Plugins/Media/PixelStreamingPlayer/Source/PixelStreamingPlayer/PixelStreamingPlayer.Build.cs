// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class PixelStreamingPlayer : ModuleRules
	{
		public PixelStreamingPlayer(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			PrivatePCHHeaderFile = "Private/PCH.h";

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			PublicIncludePaths.AddRange(
				new string[] {
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					Path.Combine(EngineDir, "Plugins/Media/PixelStreaming/Source/PixelStreaming/Private")
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"WebRTC",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"MediaAssets",
					"PixelStreaming",
					"RenderCore",
					"RHI",
					"WebSockets",
				});
		}
	}
}
