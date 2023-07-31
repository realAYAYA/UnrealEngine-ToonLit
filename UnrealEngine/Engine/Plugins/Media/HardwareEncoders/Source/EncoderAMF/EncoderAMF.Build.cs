// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class EncoderAMF : ModuleRules
{
	public EncoderAMF(ReadOnlyTargetRules Target) : base(Target)
	{
		// Without these two compilation fails on VS2017 with D8049: command line is too long to fit in debug record.
		bLegacyPublicIncludePaths = false;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		// PCHUsage = PCHUsageMode.NoPCHs;

		// PrecompileForTargets = PrecompileTargetsType.None;

		PublicIncludePaths.AddRange(new string[] {
			// ... add public include paths required here ...
		});

		PrivateIncludePaths.AddRange(new string[] {
			// ... add other private include paths required here ...
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Engine",
			"AVEncoder",
			"VulkanRHI",
			"Amf",
			"RenderCore",
			"Core",
			"RHI"
		});

		string EngineSourceDirectory = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineSourceDirectory, "Source/Runtime/AVEncoder/Private"));

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
	}
}
