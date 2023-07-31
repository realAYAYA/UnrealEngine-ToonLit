// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;
using System.Collections.Generic;
using Microsoft.Win32;
using System.Diagnostics;


namespace UnrealBuildTool.Rules
{
	public class AzureSpatialAnchorsForARKit : ModuleRules
	{
		public AzureSpatialAnchorsForARKit(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnableExceptions = true;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AugmentedReality"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AzureSpatialAnchors",
                    "AppleARKit"
				}
			);
   
            PublicFrameworks.Add("ARKit");
                
            PublicAdditionalFrameworks.Add(
                new Framework(
                "AzureSpatialAnchors",
                "ThirdParty/AzureSpatialAnchors.embeddedframework.zip",
                null,
                true)
            );

			PublicAdditionalFrameworks.Add(
                new Framework(
                "CoarseReloc",
                "ThirdParty/CoarseReloc.embeddedframework.zip",
                null,
                true)
            );
		}
	}
}
