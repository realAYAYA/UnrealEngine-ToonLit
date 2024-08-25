// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SblCore : ModuleRules
{
	public SblCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"OpenSSL",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore", // for LaunchEngineLoop.cpp dependency
				"Projects", // for LaunchEngineLoop.cpp dependency
				"Core",
				"CoreUObject",
				"Networking",
				"MsQuic",
				"SwitchboardCommon",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Json",
				"JsonUtilities",
				"JWT",
				"MsQuicRuntime",
				"TraceLog",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Launch",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"NVAPI",
				}
			);

			PublicSystemLibraries.Add("Pdh.lib");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Sockets",
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
