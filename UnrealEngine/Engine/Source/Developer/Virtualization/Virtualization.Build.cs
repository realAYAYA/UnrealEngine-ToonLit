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
			new string[] {
				"Core",
				"CoreUObject",
				"DerivedDataCache",
				"Projects",
				"SourceControl"
			}
		);

		if (Target.bUsesSlate)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MessageLog",
					"Slate",
					"SlateCore"
				}
			);

			PublicDefinitions.Add("UE_VA_WITH_SLATE=1");
		}
		else
		{
			PublicDefinitions.Add("UE_VA_WITH_SLATE=0");
		}
	}
}
