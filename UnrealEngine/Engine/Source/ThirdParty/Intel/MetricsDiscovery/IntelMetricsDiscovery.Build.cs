// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

[Obsolete("Deprecated in UE5.4 - No longer used.")]
public class IntelMetricsDiscovery : ModuleRules
{
	public IntelMetricsDiscovery(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.bCompileIntelMetricsDiscovery && Target.Platform == UnrealTargetPlatform.Win64 && Target.Architecture.bIsX64)
		{
			string BuildType = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "-md-debug" : "-md-release";

			string ThirdPartyDir = Path.Combine(Target.UEThirdPartySourceDirectory, "Intel", "MetricsDiscovery", "MetricsDiscoveryHelper");
			string IncludeDir = Path.Combine(ThirdPartyDir, "build", "include", "metrics_discovery");
			string LibrariesDir = Path.Combine(ThirdPartyDir, "build", "lib", "x64" + BuildType);

			PublicDefinitions.Add("INTEL_METRICSDISCOVERY=1");

			PublicSystemIncludePaths.Add(IncludeDir);
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesDir, "metrics_discovery_helper.lib"));
		}
		else
		{
			PublicDefinitions.Add("INTEL_METRICSDISCOVERY=0");
		}
	}
}
