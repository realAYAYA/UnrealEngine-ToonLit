// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderFormatOpenGL : ModuleRules
{
	public ShaderFormatOpenGL(ReadOnlyTargetRules Target) : base(Target)
	{

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateIncludePaths.Add("Runtime/OpenGLDrv/Private");
		PrivateIncludePaths.Add("Runtime/OpenGLDrv/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"RenderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor",
				"RHI" // @todo platplug: this is caused by the DataDrivenShaderPlatformInfo stuff - maybe it should move to somewhere else, like RenderCore?
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"OpenGL",
			"HLSLCC"
			);

        if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
        }
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SPIRVReflect");
		}
	}
}
