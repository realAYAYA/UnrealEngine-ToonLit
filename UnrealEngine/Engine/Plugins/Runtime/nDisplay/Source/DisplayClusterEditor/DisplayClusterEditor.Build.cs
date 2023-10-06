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
				"DisplayClusterConfiguration",

				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd"
			});

		// TODO: Should not be including private headers from other modules
		PrivateIncludePaths.Add(Path.Combine(GetModuleDirectory("DisplayCluster"), "Private")); // For IPDisplayCluster.h
	}
}
