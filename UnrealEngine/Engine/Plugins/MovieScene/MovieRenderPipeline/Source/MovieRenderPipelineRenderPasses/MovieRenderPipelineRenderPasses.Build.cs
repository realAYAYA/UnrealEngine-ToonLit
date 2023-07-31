// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieRenderPipelineRenderPasses : ModuleRules
{
	public MovieRenderPipelineRenderPasses(ReadOnlyTargetRules Target) : base(Target)
	{
		bEnableExceptions = true;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"ImageWriteQueue",
                "SignalProcessing", // Needed for wave writer.
				"AudioMixer",
				"Imath",
				"UEOpenExr", // Needed for multilayer EXRs
				"UEOpenExrRTTI", // Needed for EXR metadata
				"ImageWrapper",				
				"CinematicCamera", // For metadata
				"MovieRenderPipelineSettings", // For settings
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"MovieRenderPipelineCore",
				"Renderer",
				"RenderCore",
				"RHI",
				"OpenColorIO",
				"ActorLayerUtilities", // For Layering
			}
        );

		// Required for UEOpenExr
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
	}
}
