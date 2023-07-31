// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelMetricsDiscovery : ModuleRules
{
	public IntelMetricsDiscovery(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IntelMetricsDiscoveryPath = Target.UEThirdPartySourceDirectory + "Intel/MetricsDiscovery/MetricsDiscoveryHelper/";
		bool bUseDebugBuild = false;
		if (Target.bCompileIntelMetricsDiscovery && Target.Platform == UnrealTargetPlatform.Win64)
		{
			string PlatformName = "x64";
			string BuildType = bUseDebugBuild ? "-md-debug" : "-md-release";

			PublicSystemIncludePaths.Add(IntelMetricsDiscoveryPath + "build/include/metrics_discovery/");

			string LibDir = IntelMetricsDiscoveryPath + "build/lib/" + PlatformName + BuildType + "/";
			PublicAdditionalLibraries.Add(LibDir + "metrics_discovery_helper.lib");

            PublicDefinitions.Add("INTEL_METRICSDISCOVERY=1");
        }
		else
        {
            PublicDefinitions.Add("INTEL_METRICSDISCOVERY=0");
        }
	}
}
