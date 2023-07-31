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
				"ShaderCompilerCommon"
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Launch",
				"TargetPlatform",
			});

		PrivateIncludePaths.Add("Runtime/Launch/Private");      // For LaunchEngineLoop.cpp include

		// Include D3D compiler binaries
		string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add(EngineDir + "Binaries/ThirdParty/Windows/DirectX/x64/d3dcompiler_47.dll");
		}
	}
}

