// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;


public class DisplayClusterRemoteControlInterceptor : ModuleRules
{
	public DisplayClusterRemoteControlInterceptor(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		ShortName = "RCInterceptor";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DisplayCluster",
				"Engine",
				"RemoteControlInterception",
				"Serialization",
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
