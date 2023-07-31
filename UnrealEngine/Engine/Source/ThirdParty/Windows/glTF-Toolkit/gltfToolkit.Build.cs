// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class gltfToolkit : ModuleRules
{
	public gltfToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.bBuildEditor)
		{
            RuntimeDependencies.Add(
				"$(TargetOutputDir)/WindowsMRAssetConverter.exe", 
				"$(EngineDir)/Source/ThirdParty/Windows/glTF-Toolkit/bin/WindowsMRAssetConverter.exe", 
				StagedFileType.NonUFS);
        }
    }
}