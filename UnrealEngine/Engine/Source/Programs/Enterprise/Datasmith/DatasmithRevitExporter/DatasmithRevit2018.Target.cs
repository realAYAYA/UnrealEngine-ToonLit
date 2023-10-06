// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public abstract class DatasmithRevitBaseTarget : TargetRules
{
	public DatasmithRevitBaseTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;

		string RevitVersionString = GetVersion();
		string ProjectName = "DatasmithRevit" + RevitVersionString;
		string DynamoNodeProjectName = "DatasmithDynamoNode";

		ExeBinariesSubFolder = Path.Combine("Revit", RevitVersionString);
		LaunchModuleName = ProjectName;

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

		string RevitSDKLocation = "";
		string RevitSDKEnvVar = "Revit_" + RevitVersionString + "_API";

		// Try with custom setup
		string Location = System.Environment.GetEnvironmentVariable(RevitSDKEnvVar);
		if (Location != null && Location != "")
		{
			RevitSDKLocation = Location;
		}

		if (!Directory.Exists(RevitSDKLocation))
		{
			// Try with build machine setup
			string SDKRootEnvVar = System.Environment.GetEnvironmentVariable("UE_SDKS_ROOT");
			if (SDKRootEnvVar != null && SDKRootEnvVar != "")
			{
				RevitSDKLocation = Path.Combine(SDKRootEnvVar, "HostWin64", "Win64", "Revit", RevitVersionString);
			}
		}

		string RevitExporterPath = @"$(EngineDir)\Source\Programs\Enterprise\Datasmith\DatasmithRevitExporter";
		string ProjectFile = Path.Combine(RevitExporterPath, ProjectName, ProjectName+".csproj");
		string DynamoNodeProjectFile = Path.Combine(RevitExporterPath, ProjectName, DynamoNodeProjectName+".csproj");
		string Config = "Release";
		string BuildCommand = string.Format(@"$(EngineDir)\Build\BatchFiles\MSBuild.bat /t:Build /p:Configuration={2} /p:{1}=%{1}% {0}", ProjectFile, RevitSDKEnvVar, Config);
		string BuildCommandDynamoNode = string.Format(@"$(EngineDir)\Build\BatchFiles\MSBuild.bat /t:Build /p:Configuration={2} /p:{1}=%{1}% {0}", DynamoNodeProjectFile, RevitSDKEnvVar, Config);
		string ErrorMsg = string.Format("Cannot build {0}: Environment variable {1} is not defined.", ProjectName, RevitSDKEnvVar);

		// Since the Datasmith Revit Exporter is a C# project, build in batch the release configuration of the Visual Studio C# project file.
		// Outside of Epic Games, environment variable <RevitSDKEnvVar> (Revit_<year>_API) must be set to the Revit API directory on the developer's workstation.
		PostBuildSteps.Add("setlocal enableextensions");
		PostBuildSteps.Add(string.Format(@"if not defined {0} (if exist {1} (set {0}={1}) else ((echo {2}) & (exit /b 1)))", RevitSDKEnvVar, RevitSDKLocation, ErrorMsg));
		PostBuildSteps.Add(string.Format(@"echo {0}", BuildCommandDynamoNode));
		PostBuildSteps.Add(BuildCommandDynamoNode);
		PostBuildSteps.Add(string.Format(@"echo {0}", BuildCommand));
		PostBuildSteps.Add(BuildCommand);
	}

	public abstract string GetVersion();
}

public class DatasmithRevit2018Target : DatasmithRevitBaseTarget
{
	public DatasmithRevit2018Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "2018"; }
}
