// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnalyticsVisualEditing : ModuleRules
	{
        public AnalyticsVisualEditing(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"Analytics",
    				"Core",
	    			"CoreUObject",
					"DeveloperSettings",
					"Engine"
				}
			);
		}
	}
}
