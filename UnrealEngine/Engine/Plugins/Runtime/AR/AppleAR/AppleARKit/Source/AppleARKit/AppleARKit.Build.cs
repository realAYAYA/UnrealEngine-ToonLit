// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AppleARKit : ModuleRules
{
	public AppleARKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[]
		{
			System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
		});
			
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"MRMesh",
			"EyeTracker",
		});
			
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Slate",
			"SlateCore",
			"RHI",
			"Renderer",
			"RenderCore",
			"HeadMountedDisplay",
			"AugmentedReality",
			"AppleImageUtils",
			"Projects",
			"ARUtilities",
		});
		
		
		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
		});
		
		
		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
    		PrivateDependencyModuleNames.Add("IOSRuntimeSettings");
			
			PublicFrameworks.AddRange(new string[]
			{
				"ARKit",
				"MetalPerformanceShaders",
				"CoreLocation",
			});
			
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("IOSPlugin", Path.Combine(PluginPath, "AppleARKit_IOS_UPL.xml"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("MetalPerformanceShaders");
		}
	}
}
