// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using Microsoft.Win32;

namespace UnrealBuildTool.Rules
{
	// The IDE is called 10X, we add the N to make it compiler friendly
	public class N10XSourceCodeAccess : ModuleRules
	{
        public N10XSourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SourceCodeAccess",
					"DesktopPlatform"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("HotReload");
			}

			bBuildLocallyWithSNDBS = true;

			ShortName = "10XSCA";
		}
	}
}
