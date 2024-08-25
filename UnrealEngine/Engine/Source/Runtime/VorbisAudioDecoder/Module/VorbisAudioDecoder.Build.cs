// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VorbisAudioDecoder : ModuleRules
{
    public VorbisAudioDecoder(ReadOnlyTargetRules Target) : base(Target)
    {
        ShortName = "VorbisAudioDecoder";

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );
        
        AddEngineThirdPartyPrivateStaticDependencies(
	        Target,
		        "UEOgg",
		        "Vorbis",
		        "VorbisFile"
        );        

	    PublicDefinitions.Add("WITH_OGGVORBIS=1");
    }
}
