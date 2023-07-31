// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

using System;
using System.IO;
using System.Collections.Generic;

public class GeoReferencingEditor : ModuleRules
{
	public GeoReferencingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"GeoReferencing",
				"Projects",
				"UnrealEd"
			}
		);
	}
}
