// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AudioFormatBink : ModuleRules
{
	public AudioFormatBink(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine"
			}
		);

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "..", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "..", "..", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "binka_ue_encode_win64_static.lib"));
		}
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "..", "..", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "libbinka_ue_encode_lnx64_static.a"));
		}
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "..", "..", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "libbinka_ue_encode_osx_static.a"));
		}
	}
}
