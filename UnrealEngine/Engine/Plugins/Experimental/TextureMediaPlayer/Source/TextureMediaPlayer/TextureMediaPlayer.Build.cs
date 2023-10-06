// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class TextureMediaPlayer : ModuleRules
	{
		public TextureMediaPlayer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"ElectraBase",
				"ElectraSamples",
				"Engine",
				"HTTP",
				"MediaAssets",
				"MediaUtils",
				"RenderCore",
				"RHI",
			});

			DynamicallyLoadedModuleNames.AddRange(new string[]
			{
				"Media",
			});

			PrivateIncludePathModuleNames.AddRange(new string[]
			{
				"Media",
			});

			if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
			}
		}
	}
}
