// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Editor)]
public class ShaderCompileWorkerTarget : TargetRules
{
	public ShaderCompileWorkerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		LaunchModuleName = "ShaderCompileWorker";

        if (bUseXGEController && (Target.Platform == UnrealTargetPlatform.Win64) && Configuration == UnrealTargetConfiguration.Development)
        {
            // The interception interface in XGE requires that the parent and child processes have different filenames on disk.
            // To avoid building an entire separate worker just for this, we duplicate the ShaderCompileWorker in a post build step.
            const string SrcPath  = "$(EngineDir)\\Binaries\\$(TargetPlatform)\\$(TargetName).exe";
            const string DestPath = "$(EngineDir)\\Binaries\\$(TargetPlatform)\\XGEControlWorker.exe";

            PostBuildSteps.Add(string.Format("echo Copying {0} to {1}", SrcPath, DestPath));
            PostBuildSteps.Add(string.Format("copy /Y /B \"{0}\" /B \"{1}\" >nul:", SrcPath, DestPath));

			AdditionalBuildProducts.Add(DestPath);
        }

		// Turn off various third party features we don't need

		// Currently we force Lean and Mean mode
		bBuildDeveloperTools = false;

		// ShaderCompileWorker isn't localized, so doesn't need ICU
		bCompileICU = false;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
		bBuildWithEditorOnlyData = true;
		bCompileCEF3 = false;

		// Force all shader formats to be built and included.
		bForceBuildShaderFormats = true;

		// ShaderCompileWorker is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		bIsBuildingConsoleApplication = true;

		// Disable logging, as the workers are spawned often and logging will just slow them down
		GlobalDefinitions.Add("ALLOW_LOG_FILE=0");

		// Linking against wer.lib/wer.dll causes XGE to bail when the worker is run on a Windows 8 machine, so turn this off.
		GlobalDefinitions.Add("ALLOW_WINDOWS_ERROR_REPORT_LIB=0");

		// Disable external profiling in ShaderCompiler to improve startup time
		GlobalDefinitions.Add("UE_EXTERNAL_PROFILING_ENABLED=0");

		// This removes another thread being created even when -nocrashreports is specified
		GlobalDefinitions.Add("NOINITCRASHREPORTER=1");

		if (bShaderCompilerWorkerTrace)
        {
			GlobalDefinitions.Add("LLM_ENABLED_IN_CONFIG=1");
			GlobalDefinitions.Add("UE_MEMORY_TAGS_TRACE_ENABLED=1");
			bEnableTrace = true;
		}
	}
}
