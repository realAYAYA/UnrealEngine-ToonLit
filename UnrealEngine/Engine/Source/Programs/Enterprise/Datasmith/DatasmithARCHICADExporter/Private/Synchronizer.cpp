// Copyright Epic Games, Inc. All Rights Reserved.

#include "Synchronizer.h"
#include "MaterialsDatabase.h"
#include "Commander.h"
#ifdef DEBUG
#include "ISceneValidator.h"
#endif
#include "Utils/TimeStat.h"
#include "Utils/Error.h"
#include "Utils/CurrentOS.h"

#include "DatasmithDirectLink.h"
#include "DatasmithSceneExporter.h"
#include "DatasmithSceneXmlWriter.h"

#ifdef TicksPerSecond
	#undef TicksPerSecond
#endif

#include "FileManager.h"
#include "Paths.h"
#include "Version.h"

BEGIN_NAMESPACE_UE_AC

// Do Direct Link snapshot update on a thread
class FThreadUpdateSnapshotRunner : public GS::Runnable
{
  public:
	// Constructor
	FThreadUpdateSnapshotRunner(FSynchronizer* InSynchronizer)
		: Synchronizer(InSynchronizer)
	{
	}

	// The task code
	void Run() override
	{
		TryFunctionCatchAndLog("FThreadUpdateSnapshotRunner::Run", [this]() -> GSErrCode {
#if PLATFORM_WINDOWS
			SetThreadName("UpdateSceneRunner");
#else
			pthread_setname_np("UpdateSceneRunner");
#endif
			Synchronizer->DumpAndValidate();
			Synchronizer->UpdateScene();
			return NoError;
		});
	}

  private:
	// The synchronizer we update
	FSynchronizer* Synchronizer;
};

// Constructor
FThreadUpdateSnapshot::FThreadUpdateSnapshot(FSynchronizer* InSynchronizer)
	: RunnableTask(new FThreadUpdateSnapshotRunner(InSynchronizer))
	, Thread(*this, GS::UniString("FThreadUpdateSnapshot"))
{
	Thread.Start();
}

// Destructor
FThreadUpdateSnapshot::~FThreadUpdateSnapshot()
{
	Thread.Join();
}

// Show progression while current snapshot is done
void FThreadUpdateSnapshot::Join(FProgression* IOProgression)
{
	GS::UInt32 TimeOut = 10; // miliseconds
	while (!Thread.Join(TimeOut))
	{
		if (IOProgression)
		{
			IOProgression->Update();
		}
	}
}

// Class to process metadata as idle task (Only for Direct Link synchronization)
class FCountNeededMetadata : public FSyncData::FInterator
{
  public:
	GS::Int32 Count = 0;

	FCountNeededMetadata(FSyncData* Root)
	{
		Start(Root);
		ProcessAll();
	}

	// Call ProcessMetaData for the sync data
	virtual EProcessControl Process(FSyncData* InCurrent) override
	{
		if (InCurrent == nullptr)
		{
			return kDone;
		}
		if (InCurrent->NeedTagsAndMetaDataUpdate())
		{
			++Count;
		}
		return kContinue;
	}
};

#define UE_AC_FULL_TRACE 0

enum : GSType
{
	DatasmithDynamicLink = 'DsDL'
}; // Can be called by another Add-on

// Add menu to the menu bar and also add an item to palette menu
GSErrCode FSynchronizer::Register()
{
	return ACAPI_Register_SupportedService(DatasmithDynamicLink, 1L);
}

// Enable handlers of menu items
GSErrCode FSynchronizer::Initialize()
{
	GSErrCode GSErr = ACAPI_Install_ModulCommandHandler(DatasmithDynamicLink, 1L, SyncCommandHandler);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FSynchronizer::Initialize - ACAPI_Install_ModulCommandHandler error=%s\n", GetErrorName(GSErr));
	}
	return GSErr;
}

// Intra add-ons command handler
GSErrCode __ACENV_CALL FSynchronizer::SyncCommandHandler(GSHandle ParHdl, GSPtr /* ResultData */,
														 bool /* SilentMod */) noexcept
{
	return TryFunctionCatchAndAlert("FSynchronizer::DoSyncCommand",
									[ParHdl]() -> GSErrCode { return FSynchronizer::DoSyncCommand(ParHdl); });
}

static bool bPostSent = false;

// Process intra add-ons command
GSErrCode FSynchronizer::DoSyncCommand(GSHandle ParHdl)
{
	GSErrCode GSErr = NoError;

	if (ParHdl == nullptr)
	{
		return APIERR_GENERAL;
	}

	Int32 NbPars = 0;
	GSErr = ACAPI_Goodies(APIAny_GetMDCLParameterNumID, ParHdl, &NbPars);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FSynchronizer::DoSyncCommand - APIAny_GetMDCLParameterNumID error %s\n", GetErrorName(GSErr));
		return GSErr;
	}

	if (NbPars != 1)
	{
		UE_AC_DebugF("FSynchronizer::DoSyncCommand - Invalid number of parameters %d\n", NbPars);
		return APIERR_BADPARS;
	}

	API_MDCLParameter Param = {};
	Param.index = 1;
	GSErr = ACAPI_Goodies(APIAny_GetMDCLParameterID, ParHdl, &Param);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FSynchronizer::DoSyncCommand - APIAny_GetMDCLParameterID 1 error %s\n", GetErrorName(GSErr));
		return GSErr;
	}
	if (CHCompareCStrings(Param.name, "Reason", CS_CaseSensitive) != 0 || Param.type != MDCLPar_string)
	{
		UE_AC_DebugF("FSynchronizer::DoSyncCommand - Invalid parameters (type=%d) %s\n", Param.type, Param.name);
		return APIERR_BADPARS;
	}

	if (bPostSent == true)
	{
		bPostSent = false;
		if (Is3DCurrenWindow() && (GetCurrent() == nullptr || !GetCurrent()->UpdateSceneInProgress()))
		{
			UE_AC_ReportF("Auto Sync for %s\n", Param.string_par);
			FCommander::DoSnapshot();
		}
		else
		{
			PostDoSnapshot(Param.string_par);
		}
	}

	return GSErr;
}

// Schedule a Auto Sync snapshot to be executed from the main thread event loop.
void FSynchronizer::PostDoSnapshot(const utf8_t* InReason)
{
	if (bPostSent == false)
	{
		GSHandle  ParHdl = nullptr;
		GSErrCode GSErr = ACAPI_Goodies(APIAny_InitMDCLParameterListID, &ParHdl);
		if (GSErr == NoError)
		{
			API_MDCLParameter Param;
			Zap(&Param);
			Param.name = "Reason";
			Param.type = MDCLPar_string;
			Param.string_par = InReason;
			GSErr = ACAPI_Goodies(APIAny_AddMDCLParameterID, ParHdl, &Param);
			if (GSErr == NoError)
			{
				API_ModulID mdid;
				Zap(&mdid);
				mdid.developerID = kEpicGamesDevId;
				mdid.localID = kDatasmithExporterId;
				GSErr = ACAPI_Command_CallFromEventLoop(&mdid, DatasmithDynamicLink, 1, ParHdl, false, nullptr);
				if (GSErr == NoError)
				{
					ParHdl = nullptr;
					bPostSent = true; // Only one post at a time
				}
				else
				{
					UE_AC_DebugF("FSynchronizer::PostDoSnapshot - ACAPI_Command_CallFromEventLoop error %s\n",
								 GetErrorName(GSErr));
				}
			}
			else
			{
				UE_AC_DebugF("FSynchronizer::PostDoSnapshot - APIAny_AddMDCLParameterID error %s\n",
							 GetErrorName(GSErr));
			}

			if (ParHdl != nullptr)
			{
				GSErr = ACAPI_Goodies(APIAny_FreeMDCLParameterListID, &ParHdl);
				if (GSErr != NoError)
				{
					UE_AC_DebugF("FSynchronizer::PostDoSnapshot - APIAny_FreeMDCLParameterListID error %s\n",
								 GetErrorName(GSErr));
				}
			}
		}
		else
		{
			UE_AC_DebugF("FSynchronizer::PostDoSnapshot - APIAny_InitMDCLParameterListID error %s\n",
						 GetErrorName(GSErr));
		}
	}
}

static FSynchronizer* CurrentSynchonizer = nullptr;

// Return the synchronizer (create it if not already created)
FSynchronizer& FSynchronizer::Get()
{
	if (CurrentSynchonizer == nullptr)
	{
		CurrentSynchonizer = new FSynchronizer();
	}

	return *CurrentSynchonizer;
}

// Return the current synchronizer if any
FSynchronizer* FSynchronizer::GetCurrent()
{
	return CurrentSynchonizer;
}

// FreeData is called, so we must free all our stuff
void FSynchronizer::DeleteSingleton()
{
	if (CurrentSynchonizer)
	{
		delete CurrentSynchonizer;
		CurrentSynchonizer = nullptr;
	}
}

// Constructor
FSynchronizer::FSynchronizer()
	: DatasmithDirectLink(new FDatasmithDirectLink)
	, ProcessMetadata(this)
{
}

// Destructor
FSynchronizer::~FSynchronizer()
{
	Reset("Synchronizer deleted");
	ThreadUpdateSnapshot.Reset();
	DatasmithDirectLink.Reset();
}

// Return true if a snapshot update is in progress
bool FSynchronizer::UpdateSceneInProgress()
{
	if (ThreadUpdateSnapshot.IsValid())
	{
		if (!ThreadUpdateSnapshot->IsFinished())
		{
			return true;
		}
		ThreadUpdateSnapshot.Reset();
	}
	return false;
}

// Delete the database (Usualy because document has changed)
void FSynchronizer::Reset(const utf8_t* InReason)
{
	if (FCommander::IsAutoSyncEnabled())
	{
		FCommander::ToggleAutoSync();
	}
	ProcessMetadata.Stop();
	AttachObservers.Stop();

	UE_AC_TraceF("FSynchronizer::Reset - %s\n", InReason);
	SyncDatabase.Reset();
}

// Delete the database (Usualy because document has changed)
void FSynchronizer::ProjectOpen()
{
	if (SyncDatabase.IsValid())
	{
		UE_AC_DebugF("FSynchronizer::ProjectOpen - Previous project hasn't been closed before ???");
		Reset("Project Open");
	}

	// Create a new synchronization database
	GS::UniString ProjectPath;
	GS::UniString ProjectName;
	GetProjectPathAndName(&ProjectPath, &ProjectName);
	SyncDatabase.Reset(new FSyncDatabase(GSStringToUE(ProjectPath), GSStringToUE(ProjectName),
										 GSStringToUE(FSyncDatabase::GetCachePath()), FSyncDatabase::GetCachePath()));

	// Announce it to potential receivers
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 26
	TSharedRef< IDatasmithScene > ToBuildWith_4_26(SyncDatabase->GetScene());
	DatasmithDirectLink->InitializeForScene(ToBuildWith_4_26);
#else
	DatasmithDirectLink->InitializeForScene(SyncDatabase->GetScene());
#endif
}

// Inform that current project has been save (maybe name changed)
void FSynchronizer::ProjectSave()
{
	if (SyncDatabase.IsValid())
	{
		GS::UniString ProjectPath;
		GetProjectPathAndName(&ProjectPath, nullptr);

		FString SanitizedName(FDatasmithUtils::SanitizeObjectName(GSStringToUE(ProjectPath)));
		if (FCString::Strcmp(*SanitizedName, SyncDatabase->GetScene()->GetName()) == 0)
		{
			// Name is the same
			return;
		}

		UE_AC_TraceF("FSynchronizer::ProjectSave - Project saved under a new name");
		Reset("Project Renamed"); // There's no way to change to rename DirecLink connection
	}
	else
	{
		UE_AC_DebugF("FSynchronizer::ProjectSave - Project hasn't been open before ???");
	}

	ProjectOpen();
}

// Inform that the project has been closed
void FSynchronizer::ProjectClosed()
{
	Reset("Project Closed");
}

// Do a snapshot of the model 3D data
void FSynchronizer::DoSnapshot(const ModelerAPI::Model& InModel)
{
	// Setup our progression
	bool OutUserCancelled = false;
	int	 NbPhases = kSyncWaitPreviousSync - kCommonProjectInfos + 1;
#if defined(DEBUG)
	++NbPhases;
#endif
	FProgression Progression(kStrListProgression, kSyncTitle, NbPhases, FProgression::kSetFlags, &OutUserCancelled);

	GS::UniString ExportPath = FSyncDatabase::GetCachePath();

	// If we have a sync database validate it use the ExportPath
	if (SyncDatabase.IsValid())
	{
		if (FCString::Strcmp(GSStringToUE(ExportPath), SyncDatabase->GetAssetsFolderPath()) != 0)
		{
			Reset("ExportPath changed");
		}
	}

	// Insure we have a sync database and a snapshot scene
	if (!SyncDatabase.IsValid())
	{
		GS::UniString ProjectPath;
		GS::UniString ProjectName;
		GetProjectPathAndName(&ProjectPath, &ProjectName);

		SyncDatabase.Reset(new FSyncDatabase(GSStringToUE(ProjectPath), GSStringToUE(ProjectName),
											 GSStringToUE(ExportPath), ExportPath));

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 26
		TSharedRef< IDatasmithScene > ToBuildWith_4_26(SyncDatabase->GetScene());
		DatasmithDirectLink->InitializeForScene(ToBuildWith_4_26);
#else
		DatasmithDirectLink->InitializeForScene(SyncDatabase->GetScene());
#endif
	}
	// Synchronisation context
	FSyncContext SyncContext(true, InModel, *SyncDatabase, &Progression);

	// If there a pending update
	if (ThreadUpdateSnapshot.IsValid())
	{
		SyncContext.NewPhase(kSyncWaitPreviousSync);
		ThreadUpdateSnapshot->Join(&Progression);
		ThreadUpdateSnapshot.Reset();
		if (OutUserCancelled)
		{
			return;
		}
	}

	FTimeStat DoSnapshotStart;

	ViewState = FViewState();

	SyncDatabase->SetSceneInfo();

	SyncDatabase->Synchronize(SyncContext);

	SyncDatabase->GetMaterialsDatabase().UpdateModified(SyncContext);

	FTimeStat DoSynchronizeEnd;

	// Try to process meta data now, so if it take less than 10 seconds we can have only one sync
	SyncContext.NewPhase(kCommonCollectMetaDatas, FCountNeededMetadata(&SyncDatabase->GetSceneSyncData()).Count);
	ProcessMetadata.Start(&SyncDatabase->GetSceneSyncData());
	double EndSyncData = FTimeStat::RealTimeClock() + 10; // seconds
	while (FTimeStat::RealTimeClock() < EndSyncData &&
		   ProcessMetadata.ProcessUntil(FTimeStat::RealTimeClock() + 1.0 / 3.0) == FSyncData::FInterator::kContinue)
	{
		// Update progression
		SyncContext.NewCurrentValue(ProcessMetadata.GetProcessedCount());
	}
	ProcessMetadata.CleardMetadataUpdated();
	FTimeStat DoMetadataEnd;

#if DIRECTLINK_THREAD_UPDATE
	UE_AC_Assert(!ThreadUpdateSnapshot.IsValid());
	ThreadUpdateSnapshot.Reset(new FThreadUpdateSnapshot(this));
#else
	SyncContext.NewPhase(kDebugSaveScene);
	DumpAndValidate();
	SyncContext.NewPhase(kSyncSnapshot);
	UpdateScene();
#endif

	SyncContext.Stats.Print();
	FTimeStat DoSnapshotEnd;
	DoSynchronizeEnd.PrintDiff("Synchronization", DoSnapshotStart);
	DoMetadataEnd.PrintDiff("Metadata", DoSynchronizeEnd);
	DoSnapshotEnd.PrintDiff("Total DoSnapshot", DoSnapshotStart);

	AttachObservers.Start(&SyncDatabase->GetSceneSyncData());
	SyncDatabase->GetMeshIndexor().SaveToFile();
}

// Dump updated scene to a file
void FSynchronizer::DumpAndValidate()
{
#ifdef DEBUG
	if (!FCommander::IsAutoSyncEnabled()) // In Auto Sync mode we don't do scene dump or validation
	{
		FTimeStat DumpAndValidateStart;
		DumpScene(SyncDatabase->GetScene());
        TSharedRef< Validator::ISceneValidator > Validator = Validator::ISceneValidator::CreateForScene(SyncDatabase->GetScene());
		Validator->CheckElementsName();
		Validator->CheckDependances();
        Validator->CheckTexturesFiles();
        Validator->CheckMeshFiles();
		FString Reports = Validator->GetReports(Validator::ISceneValidator::kVerbose);
        if (!Reports.IsEmpty())
        {
            UE_AC_TraceF("%s", TCHAR_TO_UTF8(*Reports));
        }
		FTimeStat DumpAndValidateEnd;
		DumpAndValidateEnd.PrintDiff("FSynchronizer::DumpAndValidate", DumpAndValidateStart);
	}
#endif
}

// Update Direct Link snapshot
void FSynchronizer::UpdateScene()
{
	FTimeStat UpdateSceneStart;
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 26
	TSharedRef< IDatasmithScene > ToBuildWith_4_26(SyncDatabase->GetScene());
	DatasmithDirectLink->UpdateScene(ToBuildWith_4_26);
#else
	DatasmithDirectLink->UpdateScene(SyncDatabase->GetScene());
#endif
	FTimeStat UpdateSceneEnd;
	UpdateSceneEnd.PrintDiff("DirectLink UpdateScene", UpdateSceneStart);
}

// Process idle (To implement AutoSync)
void FSynchronizer::DoIdle(int* IOCount)
{
	// If we wait for a snapshoot to be processed
	if (bPostSent)
	{
		// We do nothing until we have processed the pending request
		return;
	}

	// If we need to schedule an Auto Sync
	if (FCommander::IsAutoSyncEnabled() && NeedAutoSyncUpdate())
	{
		PostDoSnapshot("View or material modified");
		return;
	}

	// Process meta data in priority and attach observers after
	if (ProcessMetadata.NeedProcess())
	{
		// While matadata processing isn't finish
		if (ProcessMetadata.ProcessUntil(FTimeStat::RealTimeClock() + 1.0 / 3.0) == FSyncData::FInterator::kContinue)
		{
			*IOCount = 2;
			return;
		}
		// If we must have meta data to sync ?
		if (ProcessMetadata.HasMetadataUpdated())
		{
			UE_AC_ReportF("Metadata update completed in %.2lgs\n", ProcessMetadata.GetProcessedTime());
			PostDoSnapshot("Update MetaData");
			return;
		}
	}
	else
	{
		// If we need to schedule an Auto Sync
		if (FCommander::IsAutoSyncEnabled() &&
			AttachObservers.ProcessAttachUntil(FTimeStat::RealTimeClock() + 1.0 / 3.0))
		{
			if (FCommander::IsAutoSyncEnabled())
			{
				PostDoSnapshot("Process detect modification");
				return;
			}
		}
	}

	// If we need to process more
	if (FCommander::IsAutoSyncEnabled() && AttachObservers.NeedProcess())
	{
		*IOCount = 2;
	}
}

// Auto Sync related: If view changed shedule an update
bool FSynchronizer::NeedAutoSyncUpdate() const
{
	FViewState CurrentViewState;
	if (!(ViewState == CurrentViewState))
	{
		return true;
	};
	if (SyncDatabase.IsValid() && SyncDatabase->GetMaterialsDatabase().CheckModify())
	{
		return true;
	}
	return false;
}

// Return project's file info (if not a unsaved new project)
void FSynchronizer::GetProjectPathAndName(GS::UniString* OutPath, GS::UniString* OutName)
{
	API_ProjectInfo ProjectInfo;
#if AC_VERSION < 26
	Zap(&ProjectInfo);
#endif
	GSErrCode GSErr = ACAPI_Environment(APIEnv_ProjectID, &ProjectInfo);
	if (GSErr == NoError)
	{
		if (ProjectInfo.location != nullptr)
		{
			if (OutPath != nullptr)
			{
				ProjectInfo.location->ToPath(OutPath);
			}
			if (OutName != nullptr)
			{
				IO::Name ProjectName;
				ProjectInfo.location->GetLastLocalName(&ProjectName);
				ProjectName.DeleteExtension();
				*OutName = ProjectName.ToString();
			}
			return;
		}
		else
		{
			// Maybe ArchiCAD is running as demo
			UE_AC_DebugF("CIdentity::GetFromProjectInfo - No project locations\n");
		}
	}
	else
	{
		UE_AC_DebugF("CIdentity::GetFromProjectInfo - Error(%d) when accessing project info\n", GSErr);
	}

	if (OutPath != nullptr)
	{
		*OutPath = "Nameless";
	}

	if (OutName != nullptr)
	{
		*OutName = "Nameless";
	}
}

// Dump the scene to a file
void FSynchronizer::DumpScene(const TSharedRef< IDatasmithScene >& InScene)
{
	static bool bDoDump = false;
	if (!bDoDump) // To active dump with recompiling, set sDoDump to true with the debugger
	{
		return;
	}

	// Define a directory same name as scene.
	FString sceneName(InScene->GetName());
	if (sceneName.IsEmpty())
	{
		sceneName = TEXT("Unnamed");
	}
	FString FolderPath(FPaths::Combine(GSStringToUE(GetAddonDataDirectory()), *(FString("Dumps ") + sceneName)));

	// If we change scene, we delete and recreate the folder
	static int	   NbDumps = 0;
	static FString PreviousFolderPath;
	if (!FolderPath.Equals(PreviousFolderPath))
	{
		NbDumps = 0;
		PreviousFolderPath = FolderPath;
		IFileManager::Get().DeleteDirectory(*FolderPath, false, true);
		IFileManager::Get().MakeDirectory(*FolderPath);
	}

	// Create dump file (starting from 0)
	FString ArchiveName = FPaths::Combine(*FolderPath, *FString::Printf(TEXT("Dump %d.xml"), NbDumps++));
	UE_AC_TraceF("Dump scene ---> %s\n", TCHAR_TO_UTF8(*ArchiveName));
	TUniquePtr< FArchive > archive(IFileManager::Get().CreateFileWriter(*ArchiveName));
	if (archive.IsValid())
	{
		FDatasmithSceneXmlWriter().Serialize(InScene, *archive);
	}
	else
	{
		UE_AC_DebugF("Dump scene Error can create archive file %s\n", TCHAR_TO_UTF8(*ArchiveName));
	}
}

END_NAMESPACE_UE_AC
