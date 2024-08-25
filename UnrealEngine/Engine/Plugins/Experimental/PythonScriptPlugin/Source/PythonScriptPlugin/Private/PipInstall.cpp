// Copyright Epic Games, Inc. All Rights Reserved.

#include "PipInstall.h"

#include "PyUtil.h"
#include "PythonScriptPluginSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FeedbackContextMarkup.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "PipInstall"

#if WITH_PYTHON

// Simple interface for parsing cmd output to update slowtask progress
// Similar to FFeedbackContextMarkup, but supports arbitrary line parsing
class IProgressParser
{
public:
	// Get a total work estimate
	virtual int GetTotalWork() = 0;
	// Parse line and update status/progress (return true to eat the output and not log)
	virtual bool UpdateStatus(const FString& ChkLine, FSlowTask& Task) = 0;
};

class FPipProgressParser : public IProgressParser
{
public:
	FPipProgressParser(int GuessRequirementsCount)
	: RequirementsDone(0)
	, RequirementsCount(FMath::Max(GuessRequirementsCount,1.0f))
	{}

	virtual int GetTotalWork() override
	{
		return RequirementsCount;
	}

	virtual bool UpdateStatus(const FString& ChkLine, FSlowTask& Task) override
	{
		FString TrimLine = ChkLine.TrimStartAndEnd();
		// Just log if it's not a status update line
		if (!CheckUpdateMatch(TrimLine))
		{
			return false;
		}

		// Exponentially approach 100% if steps goes above estimate
		float ProgLeft = RequirementsCount - RequirementsDone;
		float NextWork = FMath::Clamp(0.9*ProgLeft, 0.0f, 1.0f);

		// TODO: Pass in specific requirements to update status lines more accurately
		FString StatusStr = ReplaceUpdateStrs(TrimLine);
		Task.EnterProgressFrame(NextWork, FText::FromString(StatusStr));

		RequirementsDone += NextWork;

		return false;
	}

private:
	static bool CheckUpdateMatch(const FString& Line)
	{
		for (const FString& ChkMatch : MatchStatusStrs)
		{
			if (Line.StartsWith(ChkMatch))
			{
				return true;
			}
		}

		return false;
	}

	static FString ReplaceUpdateStrs(const FString& Line)
	{
		FString RepLine = Line;
		for (const TPair<FString, FString>& ReplaceMap : LogReplaceStrs)
		{
			RepLine = RepLine.Replace(*ReplaceMap.Key, *ReplaceMap.Value, ESearchCase::CaseSensitive);
		}

		return RepLine;
	}

	float RequirementsDone;
	float RequirementsCount;

	static const TArray<FString> MatchStatusStrs;
	static const TMap<FString,FString> LogReplaceStrs;
};

const TArray<FString> FPipProgressParser::MatchStatusStrs = {TEXT("Requirement"), TEXT("Downloading"), TEXT("Using"), TEXT("Installing")};
const TMap<FString,FString> FPipProgressParser::LogReplaceStrs = {{TEXT("Installing collected packages:"), TEXT("Installing collected python package dependencies:")}};


// In order to keep editor startup time fast, check directly for this utils version (make sure to match with wheel version in PythonScriptPlugin/Content/Python/Lib/wheels)
// NOTE: This version must also be changed in PipInstallMode.cs in order to support UBT functionality
const FString FPipInstall::PipInstallUtilsVer = TEXT("0.1.5");

const FString FPipInstall::PluginsListingFilename = TEXT("pyreqs_plugins.list");
const FString FPipInstall::PluginsSitePackageFilename = TEXT("plugin_site_package.pth");
const FString FPipInstall::RequirementsInputFilename = TEXT("merged_requirements.in");
const FString FPipInstall::ExtraUrlsFilename = TEXT("extra_urls.txt");
const FString FPipInstall::ParsedRequirementsFilename = TEXT("merged_requirements.txt");


FPipInstall& FPipInstall::Get()
{
	static FPipInstall Instance;
	return Instance;
}


bool FPipInstall::IsEnabled() const
{
	return bRunOnStartup && !bCmdLineDisable;
}

bool FPipInstall::IsCmdLineDisabled() const
{
	return bCmdLineDisable;
}

FString FPipInstall::WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::WritePluginsListing);

	OutPythonPlugins.Empty();

	// List of plugins with pip dependencies
	TArray<FString> PipPluginPaths;
	for ( const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins() )
	{
		const FPluginDescriptor& PluginDesc = Plugin->GetDescriptor();
		if (PluginDesc.CachedJson->HasTypedField(TEXT("PythonRequirements"), EJson::Array))
		{
			const FString PluginDescFile = FPaths::ConvertRelativePathToFull(Plugin->GetDescriptorFileName());
			PipPluginPaths.Add(PluginDescFile);
			OutPythonPlugins.Add(Plugin);
		}
	}

	// Create list of plugins that may require pip install dependencies
	const FString PyPluginsListingFile = PipInstallPath / PluginsListingFilename;
	FFileHelper::SaveStringArrayToFile(PipPluginPaths, *PyPluginsListingFile);

	// Create .pth file in site-packages dir to account for plugins with packaged dependencies
	WriteSitePackagePthFile();

    return PyPluginsListingFile;
}

FString FPipInstall::WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::WritePluginDependencies);

	OutRequirements.Empty();
	OutExtraUrls.Empty();

	for (const TSharedRef<IPlugin>& Plugin : PythonPlugins)
	{
		const FPluginDescriptor& PluginDesc = Plugin->GetDescriptor();
		for (const TSharedPtr<FJsonValue>& JsonVal : PluginDesc.CachedJson->GetArrayField(TEXT("PythonRequirements")))
		{
			const TSharedPtr<FJsonObject>& JsonObj = JsonVal->AsObject();
			if (!CheckCompatiblePlatform(JsonObj, FPlatformMisc::GetUBTPlatform()))
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* PyReqs;
			if (JsonObj->TryGetArrayField(TEXT("Requirements"), PyReqs))
			{
				for (const TSharedPtr<FJsonValue>& JsonReqVal : *PyReqs)
				{
					OutRequirements.Add(JsonReqVal->AsString());
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* PyUrls;
			if (JsonObj->TryGetArrayField(TEXT("ExtraIndexUrls"), PyUrls))
			{
				for (const TSharedPtr<FJsonValue>& JsonUrlVal : *PyUrls)
				{
					OutExtraUrls.Add(JsonUrlVal->AsString());
				}
			}
		}
	}

	const FString MergedReqsFile = FPaths::ConvertRelativePathToFull(PipInstallPath / RequirementsInputFilename);
	const FString ExtraUrlsFile = FPaths::ConvertRelativePathToFull(PipInstallPath / ExtraUrlsFilename);

	FFileHelper::SaveStringArrayToFile(OutRequirements, *MergedReqsFile);
	FFileHelper::SaveStringArrayToFile(OutExtraUrls, *ExtraUrlsFile);

	return MergedReqsFile;
}

class FCheckOrphanDirVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	FCheckOrphanDirVisitor()
	: IPlatformFile::FDirectoryVisitor()
	, bOrphan(true)
	{}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDir) override
	{
		if (!bIsDir)
		{
			bOrphan = false;
			return true;
		}

		//// Short circuit recursion if we don't allow deleting sub-hierarchies and this traversal level is already non-orphan
		//if (!bAllowDeleteSubdirs && !bOrphan)
		//{
		//	return true;
		//}

		// Always treat __pycache__ dir as orphan but don't directly delete them unless full parent is also orphan (nothing but empty or __pycache__ dirs)
		const FString DirPath(FilenameOrDirectory);
		if (DirPath.EndsWith(TEXT("__pycache__")))
		{
			return true;
		}

		FCheckOrphanDirVisitor SubDirVisit;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		bool res = PlatformFile.IterateDirectory(FilenameOrDirectory, SubDirVisit);

		bOrphan = bOrphan && SubDirVisit.bOrphan;
		if (SubDirVisit.bOrphan)
		{
			Orphans.Add(FilenameOrDirectory);
		}
		//else if (bAllowDeleteSubdirs)
		//{
		//	Orphans.Append(SubDirVisit.Orphans);
		//}

		return res;
	}

public:
	bool bOrphan;
	TArray<FString> Orphans;
};

// Remove orphan path hierarchies (hierarchies with only __pycache__ or empty dirs)
// Only runs for <PluginDir>/Content/Python/Lib/* subdirectories for plugins with
// Pip PythonRequirements uplugin section
void FPipInstall::CheckRemoveOrphanedPackages(const FString& SitePackagesPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::CheckRemoveOrphanedPackages);

	if (!FPaths::DirectoryExists(SitePackagesPath))
	{
		return;
	}
	
	// NOTE: FCheckOrphanDirVisitor should only return top-level orphan hierarchies for removal (all or nothing)
	FCheckOrphanDirVisitor DirVisit;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.IterateDirectory(*SitePackagesPath, DirVisit))
	{
		return;
	}

	if (DirVisit.bOrphan)
	{
		// Remove the entire site-packages dir if everything beneath is orphaned
		UE_LOG(LogPython, Log, TEXT("PipInstall found orphan plugin site-package directory: %s (removing)"), *SitePackagesPath);
		PlatformFile.DeleteDirectoryRecursively(*SitePackagesPath);
	}
	else
	{
		// Only remove specifically orphaned subdirs if there are some valid hierarchies in site-packages
		for (const FString& OrphanDir : DirVisit.Orphans)
		{
			UE_LOG(LogPython, Log, TEXT("PipInstall found orphan plugin site-package directory: %s (removing)"), *OrphanDir);
			PlatformFile.DeleteDirectoryRecursively(*OrphanDir);
		}
	}
}

void FPipInstall::CheckInvalidPipEnv() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::CheckInvalidPipEnv);

	if (!FPaths::DirectoryExists(PipInstallPath))
	{
		return;
	}

	// If not a venv directory don't delete in case offline packages were added before editor run
	FString VenvConfig = PipInstallPath / TEXT("pyvenv.cfg");
	if (!FPaths::FileExists(VenvConfig))
	{
		return;
	}

	const FString VenvVersion = ParseVenvVersion();
	if (VenvVersion == TEXT(PY_VERSION))
	{
		return;
	}

	UE_LOG(LogPython, Display, TEXT("Engine python version (%s) incompatible with venv (%s), recreating..."), TEXT(PY_VERSION), *VenvVersion);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteDirectoryRecursively(*PipInstallPath);
}

void FPipInstall::SetupPipEnv(FFeedbackContext* Context, bool bForceRebuild /* = false */) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::SetupPipEnv);

	const FString EngineInterp = PyUtil::GetInterpreterExecutablePath();
#ifdef PYTHON_CHECK_SYSEXEC
	// HACK: Set this compiler variable to check what sys.executable python subproceses get (should match python executable unreal was built against)
	RunPythonCmd(LOCTEXT("PipInstall.DebugInterpWeirdness", "Check Python sys.executable..."), EngineInterp, TEXT("-c \"import sys; print(f'sys.executable: {sys.executable}')\""), Context);
#endif //PYTHON_CHECK_SYSEXEC

	if (!bForceRebuild && FPaths::FileExists(VenvInterp))
	{
		SetupPipInstallUtils(Context);
		return;
	}

	if (bForceRebuild && FPaths::DirectoryExists(PipInstallPath))
	{
		// TODO: Need to cache generated files before deleting the directory (or only delete subdirs)
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteDirectoryRecursively(*PipInstallPath);
	}

	const FString VenvCmd = FString::Printf(TEXT("-m venv \"%s\""), *FPaths::ConvertRelativePathToFull(PipInstallPath));
	int32 Res = RunPythonCmd(LOCTEXT("PipInstall.SetupVenv", "Setting up pip install environment..."), EngineInterp, VenvCmd, Context);
	if (Res != 0)
	{
		UE_LOG(LogPython, Error, TEXT("Unable to create pip install environment (%d)"), Res);
		return;
	}

	SetupPipInstallUtils(Context);
}

void FPipInstall::RemoveParsedDependencyFiles() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::ParsePluginDependencies);

	const FString ParsedReqsFile = PipInstallPath / ParsedRequirementsFilename;
	if (FPaths::FileExists(ParsedReqsFile))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteFile(*ParsedReqsFile);
	}
}

FString FPipInstall::ParsePluginDependencies(const FString& MergedInRequirementsFile, FFeedbackContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::ParsePluginDependencies);

	const FString ParsedReqsFile = PipInstallPath / ParsedRequirementsFilename;

	// NOTE: Hashes are all-or-nothing so if we are ignoring, just remove them all with the parser
	FString DisableHashes = TEXT("");
	if (!GetDefault<UPythonScriptPluginSettings>()->bPipStrictHashCheck)
	{
		DisableHashes = TEXT("--disable-hashes");
	}

	const FString Cmd = FString::Printf(TEXT("-m ue_parse_plugin_reqs %s -vv \"%s\" \"%s\""), *DisableHashes, *MergedInRequirementsFile, *ParsedReqsFile);
	RunPythonCmd(LOCTEXT("PipInstall.ParseRequirements", "Parsing pip requirements..."), VenvInterp, Cmd, Context);

	return FPaths::ConvertRelativePathToFull(ParsedReqsFile);
}

bool FPipInstall::RunPipInstall(FFeedbackContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::RunPipInstall);

	const FString ParsedReqsFile = PipInstallPath / ParsedRequirementsFilename;
	const FString ExtraUrlsFile = PipInstallPath / ExtraUrlsFilename;

	if (!FPaths::FileExists(ParsedReqsFile))
	{
		return true;
	}

	TArray<FString> ParsedReqLines;
	if ( !FFileHelper::LoadFileToStringArray(ParsedReqLines, *ParsedReqsFile) )
	{
		return false;
	}

	int ReqCount = CountInstallLines(ParsedReqLines);
	if (ReqCount < 1)
	{
		return true;
	}

	TArray<FString> ExtraUrls;
	if (FPaths::FileExists(ExtraUrlsFile))
	{
		FFileHelper::LoadFileToStringArray(ExtraUrls, *ExtraUrlsFile);
	}

	const FString Cmd = SetupPipInstallCmd(ParsedReqsFile, ExtraUrls);

	TSharedPtr<IProgressParser> ProgParser = MakeShared<FPipProgressParser>(ReqCount);
	int32 Result = RunPythonCmd(LOCTEXT("PipInstall.InstallRequirements", "Installing pip requirements..."), VenvInterp, Cmd, Context, ProgParser);
	return (Result == 0);
}

int FPipInstall::NumPackagesToInstall() const
{
	const FString ParsedReqsFile = FPaths::ConvertRelativePathToFull(PipInstallPath / ParsedRequirementsFilename);

	TArray<FString> ParsedReqLines;
	if (!FPaths::FileExists(ParsedReqsFile) || !FFileHelper::LoadFileToStringArray(ParsedReqLines, *ParsedReqsFile))
	{
		return 0;
	}

	return FPipInstall::CountInstallLines(ParsedReqLines);
}

int FPipInstall::CountInstallLines(const TArray<FString>& RequirementLines)
{
	int Count = 0;
	for (const FStringView Line : RequirementLines)
	{
		bool bCommentLine = Line.TrimStart().StartsWith(TCHAR('#'));
		if (!bCommentLine && !Line.Contains(TEXT("# [pkg:check]")))
		{
			Count += 1;
		}
	}

	return Count;
}

FString FPipInstall::GetPipInstallPath() const
{
	return PipInstallPath;
}

FString FPipInstall::GetPipSitePackagesPath() const
{
	const FString VenvPath = GetPipInstallPath();
#if PLATFORM_WINDOWS
	return VenvPath / TEXT("Lib") / TEXT("site-packages");
#elif PLATFORM_MAC || PLATFORM_LINUX
	return VenvPath / TEXT("lib") / FString::Printf(TEXT("python%d.%d"), PY_MAJOR_VERSION, PY_MINOR_VERSION) / TEXT("site-packages");
#else
	static_assert(false, "Python not supported on this platform!");
#endif
}


FPipInstall::FPipInstall()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::FPipInstall);

	// Check settings/cmd-line for whether pip installer is enabled
	bRunOnStartup = GetDefault<UPythonScriptPluginSettings>()->bRunPipInstallOnStartup;
	bCmdLineDisable = FParse::Param(FCommandLine::Get(), TEXT("DisablePipInstall"));

	// Default install path: <ProjectDir>/PipInstall
	PipInstallPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() / TEXT("PipInstall"));

	// Check for UE_PIPINSTALL_PATH install path override
	const FString EnvInstallPath = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_PIPINSTALL_PATH"));
	if (!EnvInstallPath.IsEmpty())
	{
		FText ErrReason;
		if (FPaths::ValidatePath(EnvInstallPath, &ErrReason))
		{
			PipInstallPath = FPaths::ConvertRelativePathToFull(EnvInstallPath);
		}
		else
		{
			UE_LOG(LogPython, Warning, TEXT("UE_PIPINSTALL_PATH: Invalid path specified: %s"), *ErrReason.ToString());
		}
	}

	VenvInterp = GetVenvInterpreter(PipInstallPath);
}


void FPipInstall::WriteSitePackagePthFile() const
{
	// Write all paths from script-plugin
	// TODO: Should we directly use PyUtil::GetSystemPaths instead?
	// TArray<FString> PluginSitePackagePaths = PyUtil::GetSystemPaths();

	// List of enabled plugins' site-packages folders
	TArray<FString> PluginSitePackagePaths;
	for ( const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins() )
	{
		const FString PythonContentPath = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir() / TEXT("Python"));
		const FString PluginPlatformSitePackagesPath = PythonContentPath / TEXT("Lib") / FPlatformMisc::GetUBTPlatform() / TEXT("site-packages");
		const FString PluginGeneralSitePackagesPath = PythonContentPath / TEXT("Lib") / TEXT("site-packages");

		// Write platform/general site-packages paths per-plugin to .pth file to account for packaged python dependencies during pip install
		if (FPaths::DirectoryExists(PluginPlatformSitePackagesPath))
		{
			PluginSitePackagePaths.Add(PluginPlatformSitePackagesPath);
		}

		if (FPaths::DirectoryExists(PluginGeneralSitePackagesPath))
		{
			PluginSitePackagePaths.Add(PluginGeneralSitePackagesPath);
		}
	}

	// Additional paths
	for (const FDirectoryPath& AdditionalPath : GetDefault<UPythonScriptPluginSettings>()->AdditionalPaths)
	{
		const FString AddPath = FPaths::ConvertRelativePathToFull(AdditionalPath.Path);
		if (FPaths::DirectoryExists(AddPath))
		{
			PluginSitePackagePaths.Add(AddPath);
		}
	}

	// UE_PYTHONPATH
	TArray<FString> SystemEnvPaths;
	FPlatformMisc::GetEnvironmentVariable(TEXT("UE_PYTHONPATH")).ParseIntoArray(SystemEnvPaths, FPlatformMisc::GetPathVarDelimiter());
	for (const FString& SystemEnvPath : SystemEnvPaths)
	{
		if (FPaths::DirectoryExists(SystemEnvPath))
		{
			PluginSitePackagePaths.Add(SystemEnvPath);
		}
	}

	// Create .pth file in PipInstall/Lib/site-packages to account for plugins with packaged dependencies
	const FString PyPluginsSitePackageFile = GetPipSitePackagesPath() / PluginsSitePackageFilename;
	FFileHelper::SaveStringArrayToFile(PluginSitePackagePaths, *PyPluginsSitePackageFile);
}


void FPipInstall::SetupPipInstallUtils(FFeedbackContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::SetupPipInstallUtils);

	if (CheckPipInstallUtils(Context))
	{
		return;
	}

	const FString PythonScriptDir = GetPythonScriptPluginPath();
	if (PythonScriptDir.IsEmpty())
	{
		return;
	}

	const FString PipWheelsDir = PythonScriptDir / TEXT("Content/Python/Lib/wheels");
	const FString InstallRequirements = PythonScriptDir / TEXT("Content/Python/PipInstallUtils/requirements.txt");

	const FString PipInstallReq = TEXT("ue-pipinstall-utils==") + PipInstallUtilsVer;
	const FString Cmd = FString::Printf(TEXT("-m pip install --upgrade --no-index --find-links \"%s\" -r \"%s\" %s"), *PipWheelsDir, *InstallRequirements, *PipInstallReq);

	RunPythonCmd(LOCTEXT("PipInstall.SetupPipInstallUtils", "Setting up pip install utils"), VenvInterp, Cmd, Context);
}


bool FPipInstall::CheckPipInstallUtils(FFeedbackContext* Context) const
{
	// Verify that correct version of pip install utils is already available
	const FString Cmd = FString::Printf(TEXT("-c \"import pkg_resources;dist=pkg_resources.working_set.find(pkg_resources.Requirement.parse('ue-pipinstall-utils'));exit(dist.version!='%s' if dist is not None else 1)\""), *PipInstallUtilsVer);
	return (RunPythonCmd(LOCTEXT("PipInstall.CheckPipInstallUtils", "Check pip install utils installed"), VenvInterp, Cmd, Context) == 0);
}

FString FPipInstall::SetupPipInstallCmd(const FString& ParsedReqsFile, const TArray<FString>& ExtraUrls) const
{
	const UPythonScriptPluginSettings* ScriptSettings = GetDefault<UPythonScriptPluginSettings>();

	FString Cmd = TEXT("-m pip install --disable-pip-version-check --only-binary=:all:");

	// Force require hashes in requirements lines by default
	if (ScriptSettings->bPipStrictHashCheck)
	{
		Cmd += TEXT(" --require-hashes");
	}

	if (ScriptSettings->bOfflineOnly)
	{
		Cmd += TEXT(" --no-index");
	}
	else if (!ScriptSettings->OverrideIndexURL.IsEmpty())
	{
		Cmd += TEXT(" --index-url ") + ScriptSettings->OverrideIndexURL;
	}
	else if (!ExtraUrls.IsEmpty())
	{
		for (const FString& Url: ExtraUrls)
		{
			Cmd += TEXT(" --extra-index-url ") + Url;
		}
	}

	Cmd += " -r \"" + ParsedReqsFile + "\"";
	
	return Cmd;
}

int32 FPipInstall::RunPythonCmd(const FText& Description, const FString& PythonInterp, const FString& Cmd, FFeedbackContext* Context, TSharedPtr<IProgressParser> CmdParser)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPipInstall::RunPythonCmd);

	UE_LOG(LogPython, Log, TEXT("Running python command: \"%s\" %s"), *PythonInterp, *Cmd);

	int32 Result = 0;
	RunLoggedSubprocess(&Result, Description, PythonInterp, Cmd, Context, CmdParser);

	return Result;
}

bool FPipInstall::RunLoggedSubprocess(int32* OutExitCode, const FText& Description, const FString& URL, const FString& Params, FFeedbackContext* Context, TSharedPtr<IProgressParser> CmdParser)
{
	int AmountOfWork = (CmdParser.IsValid()) ? CmdParser->GetTotalWork() : 0;

	FScopedSlowTask SubprocessTask(AmountOfWork, Description, true, *Context);
	SubprocessTask.MakeDialog();

	// Create a read and write pipe for the child process
	void* StdOutPipeRead = nullptr;
	void* StdOutPipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(StdOutPipeRead, StdOutPipeWrite));

	ON_SCOPE_EXIT
	{
		FPlatformProcess::ClosePipe(StdOutPipeRead, StdOutPipeWrite);
	};

	// Create the process
	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*URL, *Params, false, true, true, nullptr, 0, nullptr, StdOutPipeWrite, nullptr);
	if (ProcessHandle.IsValid())
	{
		FString BufferedText;
		for (bool bProcessFinished = false; !bProcessFinished; )
		{
			bProcessFinished = FPlatformProcess::GetProcReturnCode(ProcessHandle, OutExitCode);
			BufferedText += FPlatformProcess::ReadPipe(StdOutPipeRead);

			int32 EndOfLineIdx;
			while (BufferedText.FindChar(TEXT('\n'), EndOfLineIdx))
			{
				FString Line = BufferedText.Left(EndOfLineIdx);
				Line.RemoveFromEnd(TEXT("\r"), ESearchCase::CaseSensitive);

				// Always log if no output parser, also log if UpdateStatus returns false
				if (!CmdParser.IsValid() || !CmdParser->UpdateStatus(Line, SubprocessTask))
				{
					Context->Log(LogPython.GetCategoryName(), ELogVerbosity::Log, Line);
				}

				BufferedText.MidInline(EndOfLineIdx + 1, MAX_int32, EAllowShrinking::No);
			}

			FPlatformProcess::Sleep(0.1f);
		}
		ProcessHandle.Reset();
		return true;
	}
	else
	{
		Context->CategorizedLogf(LogPython.GetCategoryName(), ELogVerbosity::Warning, TEXT("Couldn't create process '%s'"), *URL);
	}

	return false;
}


FString FPipInstall::GetPythonScriptPluginPath()
{
	TSharedPtr<IPlugin> PythonPlugin = IPluginManager::Get().FindPlugin("PythonScriptPlugin");
	if (!PythonPlugin)
	{
		return TEXT("");
	}

	return PythonPlugin->GetBaseDir();
}

FString FPipInstall::ParseVenvVersion() const
{
	FString VenvConfig = PipInstallPath / TEXT("pyvenv.cfg");
	if (!FPaths::FileExists(VenvConfig))
	{
		return TEXT("");
	}

	TArray<FString> ConfigLines;
	if (!FFileHelper::LoadFileToStringArray(ConfigLines, *VenvConfig))
	{
		return TEXT("");
	}

	for (FStringView Line : ConfigLines)
	{
		FStringView ChkLine = Line.TrimStartAndEnd();
		if (!ChkLine.StartsWith(TEXT("version =")))
		{
			continue;
		}

		FStringView Version = ChkLine.RightChop(9).TrimStart();
		return FString(Version);
	}

	return TEXT("");
}

FString FPipInstall::GetVenvInterpreter(const FString& InstallPath)
{
#if PLATFORM_WINDOWS
	return InstallPath / TEXT("Scripts/python.exe");
#elif PLATFORM_MAC || PLATFORM_LINUX
	return InstallPath / TEXT("bin/python3");
#else
	static_assert(false, "Python not supported on this platform!");
#endif
}

bool FPipInstall::CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName)
{
	FString JsonPlatform;

	return !JsonObject->TryGetStringField(TEXT("Platform"), JsonPlatform) || JsonPlatform.Equals(TEXT("All"), ESearchCase::IgnoreCase) || JsonPlatform.Equals(PlatformName, ESearchCase::IgnoreCase);
}

#endif //WITH_PYTHON

#undef LOCTEXT_NAMESPACE
