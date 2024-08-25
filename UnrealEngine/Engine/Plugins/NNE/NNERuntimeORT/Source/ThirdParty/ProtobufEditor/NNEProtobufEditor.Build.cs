// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NNEProtobufEditor : ModuleRules
{
	public NNEProtobufEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Type != TargetType.Editor && Target.Type != TargetType.Program)
			return;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

		string LibraryName = "libprotobuf-lite";
		string PlatformDir = Target.Platform.ToString();
		string LibraryPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		
		string LibPlatformExtension = "";
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibPlatformExtension = ".lib";
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			LibPlatformExtension = ".a";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibPlatformExtension = ".a";
		}

		PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, LibraryName + LibPlatformExtension));
	}
}
