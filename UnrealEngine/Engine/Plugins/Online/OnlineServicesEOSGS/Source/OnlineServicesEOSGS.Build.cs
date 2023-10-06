// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineServicesEOSGS : ModuleRules
{
	public OnlineServicesEOSGS(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"EOSSDK",
				"OnlineServicesInterface",
				"OnlineServicesCommon",
				"OnlineServicesCommonEngineUtils",
				"OnlineBase",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreOnline",
				"CoreUObject",
				"EOSShared",
				"Sockets"
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"SocketSubsystemEOS"
				}
			);
		}
	}
}
