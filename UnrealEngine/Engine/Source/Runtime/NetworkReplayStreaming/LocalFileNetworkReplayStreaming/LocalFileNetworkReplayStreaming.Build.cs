// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LocalFileNetworkReplayStreaming : ModuleRules
    {
        public LocalFileNetworkReplayStreaming(ReadOnlyTargetRules Target) : base(Target)
        {
			ShortName = "LFNRS";
			PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
					"NetworkReplayStreaming",
					"Json"
				} );
        }
    }
}
