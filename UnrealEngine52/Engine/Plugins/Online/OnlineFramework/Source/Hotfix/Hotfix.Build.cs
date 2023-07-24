// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Hotfix : ModuleRules
{
	public Hotfix(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
                "HTTP",
				"MoviePlayerProxy",
				"OnlineSubsystemUtils"
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"PatchCheck",
				"InstallBundleManager",
				"OnlineSubsystem",
			}
			);

		PublicDefinitions.Add("UPDATEMANAGER_PLATFORM_ENVIRONMENT_DETECTION=" + (bPlatformEnvironmentDetection ? "1" : "0"));
	}

	protected virtual bool bPlatformEnvironmentDetection { get { return false; } }
}
