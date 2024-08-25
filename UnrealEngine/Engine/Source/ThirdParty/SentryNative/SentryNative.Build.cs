// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Linq;
using System.IO;

[SupportedPlatforms("Win64")]
public class SentryNative : ModuleRules
{
	readonly string Version = "0.6.6";

	readonly string[] Libraries = new string[] {
		"crashpad_client.lib",
		"crashpad_compat.lib",
		"crashpad_getopt.lib",
		"crashpad_handler_lib.lib",
		"crashpad_minidump.lib",
		"crashpad_snapshot.lib",
		"crashpad_tools.lib",
		"crashpad_util.lib",
		"mini_chromium.lib",
		"sentry.lib",
	};

	public SentryNative(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (!Target.Architecture.bIsX64)
		{
			throw new BuildException("SentryNative v{0} is not currently supported for Platform '{1}' Architecture '{2}'", Version, Target.Platform, Target.Architecture);
		}
		string LibDir = $"lib{(Target.bUseStaticCRT ? string.Empty : "-md")}";

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, Version, "include"));
		PublicAdditionalLibraries.AddRange(Libraries.Select(library => Path.Combine(ModuleDirectory, Version, LibDir, "Win64", "Release", library)));
	}
}
