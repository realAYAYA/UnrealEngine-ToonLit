// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterEditor : ModuleRules
{
	public DisplayClusterEditor(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DisplayCluster",

				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd"
			});
	}
}
