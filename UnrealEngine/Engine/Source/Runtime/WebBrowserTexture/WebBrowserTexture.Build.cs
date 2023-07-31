// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebBrowserTexture : ModuleRules
{
	public WebBrowserTexture(ReadOnlyTargetRules Target) : base(Target)
	{
        // WebBrowserTexture objects are needed only on Android, but we also need to be able to
        // cook the asset so we must include it in editor builds
        if (Target.Platform == UnrealTargetPlatform.Android ||
            Target.Platform == UnrealTargetPlatform.IOS ||
            Target.Platform == UnrealTargetPlatform.TVOS ||
            Target.bBuildEditor == true)
		{			
			// Needed for external texture support
			PublicIncludePaths.AddRange(
				new string[] {
				"Runtime/MediaUtils/Public",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				"Runtime/MediaUtils/Private",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"RHI",
					"InputCore",
					"Slate",
					"SlateCore",
					"Serialization",
					"MediaUtils",
					"RenderCore",
					"Engine",
					"WebBrowser"
				}
			);
		}
		else
		{
			PrecompileForTargets = ModuleRules.PrecompileTargetsType.None;
		}
	}
}
