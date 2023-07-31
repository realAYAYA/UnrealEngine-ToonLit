// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MaterialX : ModuleRules
{
	public MaterialX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "MaterialX-1.38.5");

		PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

		string[] MaterialXLibraries = {
			"MaterialXCore",
			"MaterialXFormat",
			"MaterialXGenGlsl",
			"MaterialXGenMdl",
			"MaterialXGenOsl",
			"MaterialXGenShader",
			"MaterialXRender",
			"MaterialXRenderGlsl",
			"MaterialXRenderHw",
			"MaterialXRenderOsl"
		};

		string LibPostfix = bDebug ? "_d" : "";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
				Target.WindowsPlatform.GetArchitectureSubpath(),
				"lib");

			foreach (string MaterialXLibrary in MaterialXLibraries)
			{
				string StaticLibName = MaterialXLibrary + LibPostfix + ".lib";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, StaticLibName));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Mac",
				"lib");

			foreach (string MaterialXLibrary in MaterialXLibraries)
			{
				string StaticLibName = "lib" + MaterialXLibrary + LibPostfix + ".a";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, StaticLibName));
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			// Note that since we no longer support OpenGL on
			// Linux, we do not build the MaterialXRender
			// libraries, since MaterialX does not offer a way to
			// disable only MaterialXRenderGlsl, which requires
			// linking against OpenGL.
			MaterialXLibraries = new string[] {
				"MaterialXCore",
				"MaterialXFormat",
				"MaterialXGenGlsl",
				"MaterialXGenMdl",
				"MaterialXGenOsl",
				"MaterialXGenShader"
			};

			string LibDirectory = Path.Combine(
				DeploymentDirectory,
				"Unix",
				Target.Architecture,
				"lib");

			foreach (string MaterialXLibrary in MaterialXLibraries)
			{
				string StaticLibName = "lib" + MaterialXLibrary + LibPostfix + ".a";
				PublicAdditionalLibraries.Add(
					Path.Combine(LibDirectory, StaticLibName));
			}
		}

		// Add the MaterialX standard data libraries as runtime dependencies.
		RuntimeDependencies.Add(
			Path.Combine(Target.UEThirdPartyBinariesDirectory, "MaterialX", "libraries", "..."),
			StagedFileType.NonUFS);
	}
}
