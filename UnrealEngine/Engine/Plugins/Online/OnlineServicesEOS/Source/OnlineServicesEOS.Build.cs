// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineServicesEOS : ModuleRules
{
	public OnlineServicesEOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"OnlineServicesInterface",
				"OnlineServicesCommon",
				"OnlineServicesEOSGS"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreOnline",
				"CoreUObject",
				"EOSSDK",
				"EOSShared",
			}
		);
	}
}
