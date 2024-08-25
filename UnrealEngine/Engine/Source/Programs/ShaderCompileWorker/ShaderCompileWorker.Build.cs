// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ShaderCompileWorker : ModuleRules
{
	public ShaderCompileWorker(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Projects",
				"RenderCore",
				"SandboxFile",
				"TargetPlatform",
				"ApplicationCore",
				"TraceLog",
				"ShaderCompilerCommon",
				"Sockets",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Launch",
			});

		// Include D3D compiler binaries
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add(Path.Combine(Target.WindowsPlatform.DirectXDllDir, "d3dcompiler_47.dll"));
		}
	}
}

