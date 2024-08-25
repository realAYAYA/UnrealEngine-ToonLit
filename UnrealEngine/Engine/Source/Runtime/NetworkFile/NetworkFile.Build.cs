// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NetworkFile : ModuleRules
{
	public NetworkFile(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("DerivedDataCache");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Sockets",
				"CookOnTheFly"
			});

		PublicDefinitions.Add("ENABLE_HTTP_FOR_NETWORK_FILE=0");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
