// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public abstract class DatasmithNavisworksBaseTarget : TargetRules
{
	public DatasmithNavisworksBaseTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;

		string NavisworksVersionString = GetVersion();

		ExeBinariesSubFolder = Path.Combine("Navisworks", NavisworksVersionString);
		LaunchModuleName = "DatasmithNavisworks" + NavisworksVersionString;

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
		bCompileICU = false;
		bUsesSlate = false;

		// Define post-build step
		string NavisworksExporterPath = @"$(EngineDir)\Source\Programs\Enterprise\Datasmith\DatasmithNavisworksExporter";
		string SolutionPath = Path.Combine(NavisworksExporterPath, @"DatasmithNavisworksPlugin\DatasmithNavisworks.sln");
		string BuildCommand = string.Format(@"$(EngineDir)\Build\BatchFiles\MSBuild.bat /t:Build /p:Configuration=Release{1} {0}", SolutionPath, NavisworksVersionString);
		PostBuildSteps.Add(BuildCommand);
	}

	public abstract string GetVersion();
}

public class DatasmithNavisworks2020Target : DatasmithNavisworksBaseTarget
{
	public DatasmithNavisworks2020Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersion() { return "2020"; }
}
