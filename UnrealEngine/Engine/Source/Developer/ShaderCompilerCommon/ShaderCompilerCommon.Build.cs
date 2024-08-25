// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ShaderCompilerCommon : ModuleRules
{
	public ShaderCompilerCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"RenderCore",
				"ShaderPreprocessor",
				"TargetPlatform",
			}
			);
		if (Target.bCompileAgainstEngine)
		{
			PublicDependencyModuleNames.AddRange(
			new string[] {
				"FileUtilities",
			}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "ShaderConductor");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SPIRVReflect");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("ImageHlp.lib");
		}

		// We only need a header containing definitions
		PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "hlslcc/hlslcc/src/hlslcc_lib"));
		PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "SPIRV-Reflect/SPIRV-Reflect"));
		PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "ShaderConductor/ShaderConductor/External/SPIRV-Headers/include"));
	}
}
