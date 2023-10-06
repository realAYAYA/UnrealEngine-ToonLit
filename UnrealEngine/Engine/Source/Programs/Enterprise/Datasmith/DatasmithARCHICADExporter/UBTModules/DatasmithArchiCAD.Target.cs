// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public class DatasmithArchiCADTarget : TargetRules
{
	public DatasmithArchiCADTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;

		ExeBinariesSubFolder = Path.Combine("DatasmithArchiCADExporter", "ArchiCAD");
		LaunchModuleName = "DatasmithArchiCAD";

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bCompileICU = false;
		bUsesSlate = false;

		bHasExports = true;
		bForceEnableExceptions = true;

		// Define post-build step
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ArchiCADExporterPath = @"$(EngineDir)\Source\Programs\Enterprise\Datasmith\DatasmithARCHICADExporter";
			string SolutionPath = Path.Combine(ArchiCADExporterPath, @"Build\DatasmithARCHICADExporter.sln");
			string BuildCommand = string.Format(@"$(EngineDir)\Build\BatchFiles\MSBuild.bat /t:Build /p:Configuration=ReleaseUE;Platform=x64 {0}", SolutionPath);
			PreBuildSteps.Add(BuildCommand);
		}
	}
}
