// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class NNEOnnxruntimeEditor : ModuleRules
{
	public NNEOnnxruntimeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Type != TargetType.Editor && Target.Type != TargetType.Program)
			return;

		string PlatformDir = Target.Platform.ToString();
		string IncDirPath = Path.Combine(ModuleDirectory, "include");
		string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		string OrtPlatformRelativePath = Path.Combine("Binaries", "ThirdParty", "OnnxruntimeEditor", PlatformDir);
		string OrtPlatformPath = Path.Combine(PluginDirectory, OrtPlatformRelativePath);
		string SharedLibFileName = "UNSUPPORTED_PLATFORM";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			SharedLibFileName = "onnxruntime.dll";
			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, "onnxruntime.lib"));
			PublicDelayLoadDLLs.Add("onnxruntime.dll");
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, "onnxruntime.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			SharedLibFileName = "libonnxruntime.so.1.14.1";
			PublicAdditionalLibraries.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.so"));
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.so"));
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.so.1.14.1"));
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.so"));
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.so.1.14.1"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			SharedLibFileName = "libonnxruntime.1.14.1.dylib";
			PublicAdditionalLibraries.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.dylib"));
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.dylib"));
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.1.14.1.dylib"));
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.dylib"));
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, "libonnxruntime.1.14.1.dylib"));
		}

		string SharedLibRelativePath = Path.Combine(OrtPlatformRelativePath, SharedLibFileName);

		PublicIncludePaths.Add(IncDirPath);
		PublicDefinitions.Add("ORT_API_MANUAL_INIT");
		PublicDefinitions.Add("ONNXRUNTIME_SHAREDLIB_PATH=" + SharedLibRelativePath.Replace('\\', '/'));
	}
}