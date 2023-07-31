// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMultiUserServer : ModuleRules
{
	public UnrealMultiUserServer(ReadOnlyTargetRules Target) : base(Target)
	{
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Concert",
				"TraceInsights",
				"ApplicationCore",					// for LaunchEngineLoop.cpp dependency
				"Projects",							// for LaunchEngineLoop.cpp dependency
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Launch",
				"ConcertSyncCore",
				"ConcertSyncServer"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Launch/Private",           // for LaunchEngineLoop.cpp include
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ConcertSyncServer"
			}
		);
	}
}
