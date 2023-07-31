// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineServicesOSSAdapter : ModuleRules
{
	public OnlineServicesOSSAdapter(ReadOnlyTargetRules Target) : base(Target)
    {
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"OnlineServicesInterface",
				"OnlineSubsystem",
				"Json"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreOnline",
				"OnlineServicesCommon"
			}
		);
	}
}
