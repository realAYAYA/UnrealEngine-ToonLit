// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReplicationSystemTest : ModuleRules
{
	public ReplicationSystemTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateIncludePathModuleNames.Add("DerivedDataCache");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
 				"ApplicationCore",
 				"AutomationController",
 				"AutomationWorker",
                "Core",
				"Projects",
				"Engine",
				"HeadMountedDisplay",
				"InstallBundleManager",
                "MediaUtils",
				"MRMesh",
				"MoviePlayer",
				"MoviePlayerProxy",
				"PreLoadScreen",
				"ProfilerService",
				"ReplicationSystemTestPlugin",
				"SessionServices",
				"SlateNullRenderer",
				"SlateRHIRenderer",
				"ProfileVisualizer",
 			}
		);
	}
}
