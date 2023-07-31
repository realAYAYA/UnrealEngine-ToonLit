// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;

namespace UnrealBuildTool.Rules
{
	public class PixelStreamingVCam : ModuleRules
	{
		public PixelStreamingVCam(ReadOnlyTargetRules Target) : base(Target)
		{
			// This is so for game projects using our public headers don't have to include extra modules they might not know about.
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"InputDevice",
				"MediaIOCore",
				"VCamCore",
				"PixelStreaming",
				"PixelStreamingServers"
			});

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"UMG",
				"RHI",
				"CinematicCamera",
				"LiveLinkInterface",
				"PixelCapture",
				"PixelStreamingEditor",
				"SlateCore",
				"Slate",
				"VPUtilities",
				"InputCore"
			});

			// Can't package non-editor targets (e.g. games) with UnrealEd, so this dependency should only be added in editor.
			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

		}
	}
}
