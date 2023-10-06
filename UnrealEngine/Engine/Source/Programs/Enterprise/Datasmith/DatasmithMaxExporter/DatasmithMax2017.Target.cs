// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public abstract class DatasmithMaxBaseTarget : TargetRules
{
	public DatasmithMaxBaseTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;
		bLegalToDistributeBinary = true;

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		WindowsPlatform.ModuleDefinitionFile = "Programs/Enterprise/Datasmith/DatasmithMaxExporter/DatasmithMaxExporterWithDirectLink.def";

		WindowsPlatform.bStrictConformanceMode = false;

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bCompileICU = false;
		bUsesSlate = false;

		bHasExports = true;
		bForceEnableExceptions = true;

		GlobalDefinitions.Add("UE_EXTERNAL_PROFILING_ENABLED=0"); // For DirectLinkUI (see FDatasmithExporterManager::FInitOptions)

		// Setting CppStandard to Cpp17 as new CppStandardVersion.Default is Cpp20 and this implies "/permissive-" for MSVC which
		// doesn't work well with 3ds Max includes(lots of non-conformance like const to non-const string conversion, temp to &, etc)
		// 
		// For some reason, adding following doesn't help 100%. Even though docs says that should override "/permissive-"'s settings like C2102:
		// AdditionalCompilerArguments = " /Zc:referenceBinding- /Zc:strictStrings- /Zc:rvalueCast- ";
		// 
		// 3dsMax uses MSVC extension which allows taking address of a class rvalue returned from a function:
		// class V
		// {
		// 	int dummy;
		// };
		//
		// class C {
		// public:
		// 	V get() const { return V(); }
		// };
		// void f(V* ){}
		// int main() {
		// 	C c;
		// 	f(&(c.get()));// In conformance mode C2102 '&' requires l-value, but MSVC allows this by default
		// } 
		// This behavior is disabled when c++20 enabled and is not repaired with any /Zc option. Seems like it only works with "/permissive" enabled. But...
		// Reverting permissive breaks Engine compilation because of operator resolution ambiguity(which doesn't happen with stricter checks) and other issues:
		// AdditionalCompilerArguments = "  /permissive ";
		CppStandard = CppStandardVersion.Cpp17;

		// todo: remove?
		// bSupportEditAndContinue = true;
	}

	protected void AddCopyPostBuildStep(TargetInfo Target)
	{
		// Add a post-build step that copies the output to a file with the .dle extension
		string OutputName = "$(TargetName)";
		if (Target.Configuration != UnrealTargetConfiguration.Development)
		{
			OutputName = string.Format("{0}-{1}-{2}", OutputName, Target.Platform, Target.Configuration);
		}

		string SrcOutputFileName = string.Format(@"$(EngineDir)\Binaries\Win64\{0}\{1}.dll", ExeBinariesSubFolder, OutputName);

		string DstOutputFileName;

		DstOutputFileName = string.Format(@"$(EngineDir)\Binaries\Win64\{0}\{1}.gup", ExeBinariesSubFolder, OutputName);

		PostBuildSteps.Add(string.Format("echo Copying {0} to {1}...", SrcOutputFileName, DstOutputFileName));
		PostBuildSteps.Add(string.Format("copy /Y \"{0}\" \"{1}\" 1>nul", SrcOutputFileName, DstOutputFileName));
	}
}

public class DatasmithMax2017Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2017Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithMax2017";
		ExeBinariesSubFolder = @"3DSMax\2017";

		AddCopyPostBuildStep(Target);
	}
}
