// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MetalRHI : ModuleRules
{	
	public MetalRHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Engine",
				"RHI",
				"RHICore",
				"RenderCore"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"MetalCPP"
		);

        AddEngineThirdPartyPrivateStaticDependencies(Target,
            "MetalShaderConverter"
        );   
		
		PublicWeakFrameworks.Add("Metal");

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("QuartzCore");
		}
	}
}
