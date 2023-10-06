// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEHlslShaders : ModuleRules
{
	public NNEHlslShaders( ReadOnlyTargetRules Target ) : base( Target )
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"Projects",
				"RenderCore",
				"RHI",
				"NNE"
			}
		);
	}
}
