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
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			PublicIncludePaths.AddRange(
				new string[] {
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
