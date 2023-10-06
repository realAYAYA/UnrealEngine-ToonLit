// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public abstract class DatasmithSketchUpRubyBaseTarget : TargetRules
{
	public DatasmithSketchUpRubyBaseTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		WindowsPlatform.ModuleDefinitionFile = "Programs/Enterprise/Datasmith/DatasmithSketchUpRubyExporter/DatasmithSketchUpRubyExporter.def";

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bCompileICU = false;
		bUsesSlate = false;

		bHasExports = true;
		bForceEnableExceptions = true;

		GlobalDefinitions.Add("UE_EXTERNAL_PROFILING_ENABLED=0"); // For DirectLinkUI (see FDatasmithExporterManager::FInitOptions)
	}

	protected void AddCopyPostBuildStep(TargetInfo Target)
	{
		string OutputName = "$(TargetName)";

		PostBuildSteps.Add("echo on");

		// Copy Ruby scripts
		PostBuildSteps.Add(string.Format("echo D|xcopy /Y /R /F /S \"{0}\" \"{1}\"",
			string.Format(@"$(EngineDir)/Source/Programs/Enterprise/Datasmith/DatasmithSketchUpRubyExporter/Plugin/*.rb"),
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/Plugin/", ExeBinariesSubFolder)
			));

		ReadOnlyBuildVersion BuildVersion = Target.Version;
		string VersionString = string.Format("{0}.{1}.{2}", BuildVersion.MajorVersion, BuildVersion.MinorVersion, BuildVersion.PatchVersion);

		PostBuildSteps.Add(string.Format("echo {1}> \"{0}\"",
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/Plugin/UnrealDatasmithSketchUp/version", ExeBinariesSubFolder), VersionString));

		// Copy plugin dll
		PostBuildSteps.Add(string.Format("echo F|xcopy /Y /R /F \"{0}\" \"{1}\"",
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/{1}.dll", ExeBinariesSubFolder, OutputName),
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/Plugin/UnrealDatasmithSketchUp/DatasmithSketchUp.so", ExeBinariesSubFolder, OutputName)
			));

		// Copy support dlls
		PostBuildSteps.Add(string.Format("echo F|xcopy /Y /R /F \"{0}\" \"{1}\"",
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/tbbmalloc.dll", ExeBinariesSubFolder),
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/Plugin/UnrealDatasmithSketchUp", ExeBinariesSubFolder)
		));


		// Copy resources
		PostBuildSteps.Add(string.Format("echo D|xcopy /Y /R /F /S \"{0}\" \"{1}\"",
			string.Format(@"$(EngineDir)/Source/Programs/Enterprise/Datasmith/DatasmithSketchUpRubyExporter/Resources/Windows"),
			string.Format(@"$(EngineDir)/Binaries/Win64/{0}/Plugin/UnrealDatasmithSketchUp/Resources", ExeBinariesSubFolder)
			));
	}

}

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2020Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2020Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2020";
		ExeBinariesSubFolder = @"SketchUpRuby/2020";

		AddCopyPostBuildStep(Target);
	}
}
