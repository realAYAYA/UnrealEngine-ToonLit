// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using UnrealBuildTool;

[SupportedPlatforms("Win64", "Mac", "Linux", "LinuxArm64")]
[SupportedConfigurations(UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Shipping)]
public class CrashReportClientTarget : TargetRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "TelemetryUrl")]
	public string TelemetryUrl;

	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "TelemetryKey_Dev")]
	public string TelemetryKey_Dev;

	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "TelemetryKey_Release")]
	public string TelemetryKey_Release;

	public CrashReportClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		UndecoratedConfiguration = UnrealTargetConfiguration.Shipping;

		LaunchModuleName = "CrashReportClient";

		if (bBuildEditor == true && Target.Platform != UnrealTargetPlatform.Linux)
		{
			ExtraModuleNames.Add("EditorStyle");
		}

		bLegalToDistributeBinary = true;

		bBuildDeveloperTools = false;

		// CrashReportClient doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bUseLoggingInShipping = true;

		// CrashReportClient.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		bHasExports = false;

		bUseChecksInShipping = true;

		// Epic Games Launcher needs to run on OS X 10.9, so CrashReportClient needs this as well
		bEnableOSX109Support = true;

		// Need to disable the bundled version of dbghelp so that CrashDebugHelper can load dbgeng.dll.
		WindowsPlatform.bUseBundledDbgHelp = false;

		// Add the definitions from config files
		if(!string.IsNullOrWhiteSpace(TelemetryUrl))
		{
			AddConfigMacro("CRC_TELEMETRY_URL=", string.Format("\"{0}\"", TelemetryUrl));
		}
		AddConfigMacro("CRC_TELEMETRY_KEY_DEV=", string.Format("\"{0}\"", TelemetryKey_Dev));
		AddConfigMacro("CRC_TELEMETRY_KEY_RELEASE=", string.Format("\"{0}\"", TelemetryKey_Release));

		GlobalDefinitions.Add("NOINITCRASHREPORTER=1");
	}

	void AddConfigMacro(string Prefix, string Value)
	{
		if (!string.IsNullOrEmpty(Value) && !GlobalDefinitions.Any(x => x.StartsWith(Prefix, StringComparison.Ordinal)))
		{
			GlobalDefinitions.Add(Prefix + Value);
		}
	}
}
