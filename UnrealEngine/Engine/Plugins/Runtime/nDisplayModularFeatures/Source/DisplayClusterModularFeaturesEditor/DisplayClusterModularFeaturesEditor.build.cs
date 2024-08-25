// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterModularFeaturesEditor : ModuleRules
{
	public DisplayClusterModularFeaturesEditor(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		ShortName = "NDCModFeaturesEd";

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd"
			});
	}
}
