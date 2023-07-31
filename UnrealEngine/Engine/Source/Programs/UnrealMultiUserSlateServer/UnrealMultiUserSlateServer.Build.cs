// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMultiUserSlateServer : ModuleRules
{
	public UnrealMultiUserSlateServer(ReadOnlyTargetRules Target) : base(Target)
	{
		//ShortName = "MU-SlateServer";
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
				"ConcertSyncServer",
				"MultiUserServer",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Launch/Private",           // for LaunchEngineLoop.cpp include
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ConcertSyncServer",
				"MultiUserServer"
			}
		);
		
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnixCommonStartup"
				}
			);
		}
	}
}
