// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Virtualization : ModuleRules
{
	public Virtualization(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"DerivedDataCache",
				"MessageLog",
				"Projects",
				"SourceControl"
			});
	}
}
