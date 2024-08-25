// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetalShaderFormat : ModuleRules
{
	public MetalShaderFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePathModuleNames.Add("MetalRHI");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"RenderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor",
				"FileUtilities",
				"RHI"
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SPIRVReflect");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "MetalShaderConverter");

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string SCBinariesDir = Path.Combine(Target.UEThirdPartyBinariesDirectory, "ShaderConductor", "Mac");
				PublicAdditionalLibraries.Add(SCBinariesDir + "/libdxcompiler.dylib");
				RuntimeDependencies.Add(SCBinariesDir + "/libdxcompiler.dylib");
			}
			
            PublicSystemIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "ShaderConductor", "ShaderConductor", "External", "DirectXShaderCompiler", "include"));
		}
	}
}
