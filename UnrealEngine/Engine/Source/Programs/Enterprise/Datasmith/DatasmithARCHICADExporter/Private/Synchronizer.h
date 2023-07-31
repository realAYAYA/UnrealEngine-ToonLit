// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"
#include "Utils/ViewState.h"

#include "SyncContext.h"
#include "Menus.h"

#include "Model.hpp"

class FDatasmithDirectLink;

BEGIN_NAMESPACE_UE_AC

#define DIRECTLINK_THREAD_UPDATE 1

// To do Direct Link snapshot update after scene updating
class FThreadUpdateSnapshot : GS::RunnableTask
{
  public:
	// Constructor
	FThreadUpdateSnapshot(FSynchronizer* InSynchronizer);

	// Destructor
	~FThreadUpdateSnapshot();

	// Return true if snapshot is done
	bool IsFinished() const { return Thread.GetState().IsFinished(); }

	// Show progression while current snapshot is done
	void Join(FProgression* IOProgression);

  private:
	// The snapshot update thread
	GS::Thread Thread;
};

// Class to synchronize 3D data thru  DirecLink
class FSynchronizer
{
	TUniquePtr< FDatasmithDirectLink >	DatasmithDirectLink;
	TUniquePtr< FSyncDatabase >			SyncDatabase;
	FViewState							ViewState;
	FSyncData::FProcessMetadata			ProcessMetadata;
	FSyncData::FAttachObservers			AttachObservers;
	TUniquePtr< FThreadUpdateSnapshot > ThreadUpdateSnapshot;

  public:
	// Register the Dynamic Link sync services
	static GSErrCode Register();

	// Enable handlers of the Dynamic Link sync services
	static GSErrCode Initialize();

	// Intra add-ons command handler
	static GSErrCode __ACENV_CALL SyncCommandHandler(GSHandle ParHdl, GSPtr ResultData, bool SilentMode) noexcept;

	// Process intra add-ons command
	static GSErrCode DoSyncCommand(GSHandle ParHdl);

	// Schedule a Auto Sync snapshot to be executed from the main thread event loop.
	static void PostDoSnapshot(const utf8_t* InReason);

	// Constructor
	FSynchronizer();

	// Destructor
	~FSynchronizer();

	// Return the synchronizer (create it if not already created)
	static FSynchronizer& Get();

	// Return the current synchronizer if any
	static FSynchronizer* GetCurrent();

	// FreeData is called, so we must free all our stuff
	static void DeleteSingleton();

	// Return true if a snapshot update is in progress
	bool UpdateSceneInProgress();

	// Delete the database (Usualy because document has changed)
	void Reset(const utf8_t* InReason);

	// Inform that a new project has been open
	void ProjectOpen();

	// Inform that current project has been save (maybe name changed)
	void ProjectSave();

	// Inform that the project has been closed
	void ProjectClosed();

	// Do a snapshot of the model 3D data
	void DoSnapshot(const ModelerAPI::Model& InModel);

	// Return the current sync database
	FSyncDatabase* GetSyncDatabase() { return SyncDatabase.Get(); };

	// Dump updated scene to a file
	void DumpAndValidate();

	// Update Direct Link snapshot
	void UpdateScene();

	// Process idle (To implement AutoSync)
	void DoIdle(int* IOCount);

	// Return true if view or at least one material changed
	bool NeedAutoSyncUpdate() const;

	// Return project's file info (if not a unsaved new project)
	static void GetProjectPathAndName(GS::UniString* OutPath, GS::UniString* OutName);

	// Dump the scene to a file
	static void DumpScene(const TSharedRef< IDatasmithScene >& InScene);
};

END_NAMESPACE_UE_AC
