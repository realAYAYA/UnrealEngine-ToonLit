// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithUE4ArchiCADTarget : TargetRules
{
	public DatasmithUE4ArchiCADTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		LaunchModuleName = "DatasmithUE4ArchiCAD";
		ExeBinariesSubFolder = "DatasmithUE4ArchiCAD";

		ExtraModuleNames.AddRange( new string[] { "DatasmithCore", "DatasmithExporter"} );

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
		}

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bCompileICU = false;
		bUsesSlate = true;

		bUsePDBFiles = true;
		bHasExports = true;

		GlobalDefinitions.Add("UE_EXTERNAL_PROFILING_ENABLED=0"); // For DirectLinkUI (see FDatasmithExporterManager::FInitOptions)
	}
}
