// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AppleARKitPoseTrackingLiveLink : ModuleRules
{
	public AppleARKitPoseTrackingLiveLink(ReadOnlyTargetRules Target) : base(Target)
	{
        //OptimizeCode = CodeOptimization.Never;
		
		PrivateIncludePaths.AddRange(new string[]
		{
			System.IO.Path.Combine(GetModuleDirectory("AppleARKit"), "Private"),
		});
			
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
		});
			
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"HeadMountedDisplay",
			"LiveLinkAnimationCore",
			"LiveLinkInterface",
			"AppleARKit",
			"AppleImageUtils",
			"ARUtilities",
		});
		
		
		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicFrameworks.Add( "ARKit" );
		}
	}
}
