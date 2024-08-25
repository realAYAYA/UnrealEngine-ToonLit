// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Xml.Linq;
using UnrealBuildTool;

public class NNEOnnxEditor : ModuleRules
{
	public NNEOnnxEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Type != TargetType.Editor && Target.Type != TargetType.Program)
			return;

		string IncPath = Path.Combine(ModuleDirectory, "include");
		PublicSystemIncludePaths.Add(IncPath);

		string PlatformDir = Target.Platform.ToString();
		string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		
		string[] LibFileNames = new string[] {};
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibFileNames = new string[] { "onnx.lib", "onnx_proto.lib" };
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			LibFileNames = new string[] { "libonnx.a", "libonnx_proto.a" };
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibFileNames = new string[] { "libonnx.a", "libonnx_proto.a" };
		}

		foreach (string LibFileName in LibFileNames)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName));
		}

		PublicDefinitions.Add("ONNX_ML");
		PublicDefinitions.Add("ONNX_NAMESPACE = onnx");
		PublicDefinitions.Add("__ONNX_NO_DOC_STRINGS");
	}
}
