// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineServicesInterface : ModuleRules
{
	public OnlineServicesInterface(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"CoreOnline",
				"OnlineBase"
			}
		);

		PublicIncludePaths.Add(ModuleDirectory);

		// OnlineService cannot depend on Engine!
		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
			}
		);
	}
}
