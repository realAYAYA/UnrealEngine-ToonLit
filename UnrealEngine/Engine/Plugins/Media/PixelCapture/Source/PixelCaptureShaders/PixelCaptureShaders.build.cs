// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class PixelCaptureShaders : ModuleRules
	{
		public PixelCaptureShaders(ReadOnlyTargetRules Target) : base(Target)
		{
			// This is so for game projects using our public headers don't have to include extra modules they might not know about.
			PublicDependencyModuleNames.AddRange(new string[] {});

			// NOTE: General rule is not to access the private folder of another module
			PrivateIncludePaths.AddRange(new string[] {});

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"Engine",
				"Projects",
				"RenderCore",
				"RHI",
			});

			// required for casting UE4 BackBuffer to Vulkan Texture2D for NvEnc
			PrivateDependencyModuleNames.AddRange(new string[] {});
		}
	}
}
