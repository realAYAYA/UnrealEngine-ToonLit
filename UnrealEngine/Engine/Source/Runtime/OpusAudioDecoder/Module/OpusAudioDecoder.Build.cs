// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpusAudioDecoder : ModuleRules
{
    public OpusAudioDecoder(ReadOnlyTargetRules Target) : base(Target)
    {
        ShortName = "OpusAudioDecoder";

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );
        
        AddEngineThirdPartyPrivateStaticDependencies(Target,
	        "UEOgg",
	        "libOpus"
        );        

	    PublicDefinitions.Add("WITH_OPUS=1");
    }
}
