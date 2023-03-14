// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashDebugHelper : ModuleRules
{
	public CrashDebugHelper( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateIncludePaths.AddRange(
		new string[] {
				"Developer/CrashDebugHelper/Private/",
				"Developer/CrashDebugHelper/Private/Linux",
				"Developer/CrashDebugHelper/Private/Mac",
				"Developer/CrashDebugHelper/Private/Windows",
				"Developer/CrashDebugHelper/Private/IOS",
			}
		);

		PrivateIncludePaths.Add( "ThirdParty/PLCrashReporter/PLCrashReporter/Source" );

		if (Target.Type != TargetType.Game && Target.Type != TargetType.Client)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				}
			);
		}
		else
		{
			IsRedistributableOverride = true;
			PublicDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				}
			);
		}

		if(Target.Platform == UnrealTargetPlatform.Win64 && Target.WindowsPlatform.bUseBundledDbgHelp)
		{
			throw new System.Exception("CrashDebugHelper uses DBGENG.DLL at runtime, which depends on a matching version of DBGHELP.DLL but cannot be redistributed. Please set WindowsPlatform.bUseBundledDbgHelp = false for this target.");
		}

		// Note: PLCrashReporter is not supported on tvOS.
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"PLCrashReporter"
			});
		}
    }
}
