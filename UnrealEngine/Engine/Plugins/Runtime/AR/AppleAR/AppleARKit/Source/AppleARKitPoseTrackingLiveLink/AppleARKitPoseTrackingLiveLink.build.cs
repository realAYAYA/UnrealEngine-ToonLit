// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AppleARKitPoseTrackingLiveLink : ModuleRules
{
	public AppleARKitPoseTrackingLiveLink(ReadOnlyTargetRules Target) : base(Target)
	{
        //OptimizeCode = CodeOptimization.Never;	
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
		});
			
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"HeadMountedDisplay",
			"XRBase",
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
