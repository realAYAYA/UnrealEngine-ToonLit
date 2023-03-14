// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;


public class DisplayClusterMultiUser : ModuleRules
{
	public DisplayClusterMultiUser(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ConcertSyncClient",
				"DisplayClusterConfiguration",
			});
	}
}
