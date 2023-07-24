// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class PixelCapture : ModuleRules
	{
		public PixelCapture(ReadOnlyTargetRules Target) : base(Target)
		{
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			// This is so for game projects using our public headers don't have to include extra modules they might not know about.
			PublicDependencyModuleNames.AddRange(new string[] {
				"RHI",
			});

			
			// NOTE: General rule is not to access the private folder of another module
			string WebRtcIncludeDir = Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/4664/Include");
			PrivateIncludePaths.AddRange(new string[] {
				Path.Combine(WebRtcIncludeDir, "third_party/libyuv/include"),
				Path.Combine(EngineDir, "Source/Runtime/Renderer/Private"),
			});

			PrivateDependencyModuleNames.AddRange(new string[] {
				"Core",
				"Engine",
				"PixelCaptureShaders",
				"RenderCore",
				"Renderer",
				"WebRTC",
			});
		}
	}
}
