// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Virtualization : ModuleRules
{
	public Virtualization(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateIncludePathModuleNames.Add("Analytics");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DerivedDataCache",
				"MessageLog",
				"Projects",
				"SourceControl"
			});

		if (Target.bUsesSlate)
		{
			// NOTE: Slate is being included via SourceControl anyway
			PrivateDependencyModuleNames.Add("Slate");
			PrivateDependencyModuleNames.Add("SlateCore");

			PublicDefinitions.Add("UE_VA_WITH_SLATE=1");
		}
		else
		{
			PublicDefinitions.Add("UE_VA_WITH_SLATE=0");
		}
	}
}
