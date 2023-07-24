// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/APIEnvir.h"

#include <stddef.h>

#include "DatasmithSceneExporter.h"
#include "DatasmithSceneXmlWriter.h"

#include "Exporter.h"
#include "ResourcesIDs.h"
#include "Utils/TimeStat.h"

#ifdef TicksPerSecond
	#undef TicksPerSecond
#endif

#include "FileManager.h"
#include "Paths.h"

#include "FileSystem.hpp"
#include "DGFileDialog.hpp"

BEGIN_NAMESPACE_UE_AC

// Constructor
FExporter::FExporter() {}

// Destructor
FExporter::~FExporter() {}

// Export the AC model in the specified file
void FExporter::DoExport(const ModelerAPI::Model& InModel, const API_IOParams& IOParams)
{
	// Archicad, to secure document, do save in a scratch file. And exchange it on success
	// But Datasmith need to save in the real file.
	// We exchange for Datasmith, we do export, we exchange before returning to AC that will exchange again.

	// RealFileLocation
	bool		 bDoExchange = false;
	IO::Location RealFileLocation(*IOParams.fileLoc);
	if (IOParams.saveFileIOName != nullptr && !IOParams.saveFileIOName->IsEmpty())
	{
		bDoExchange = true;
		RealFileLocation.SetLastLocalName(*IOParams.saveFileIOName);
	}

	// Temporarly exchange files (Old vs Scratch)
	IO::Location FileLocation(*IOParams.fileLoc);
	FileLocation.DeleteLastLocalName();
	IO::Folder parentFolder(FileLocation);
	IO::Name   ScratchName;
	IOParams.fileLoc->GetLastLocalName(&ScratchName);
	if (bDoExchange)
	{
		GSErrCode GSErr = parentFolder.Exchange(ScratchName, *IOParams.saveFileIOName, IO::AccessDeniedIsError);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FExporter::DoExport - Exchange 1 returned error %s\n", GetErrorName(GSErr));
		}
	}

	DoExport(InModel, RealFileLocation);

	// Exchange files because Archicad will re-exchange just after.
	if (bDoExchange)
	{
		GSErrCode GSErr = parentFolder.Exchange(ScratchName, *IOParams.saveFileIOName, IO::AccessDeniedIsError);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FExporter::DoExport - Exchange 2 returned error %s\n", GetErrorName(GSErr));
		}
	}
}

// Export the AC model in the specified file
void FExporter::DoExport(const ModelerAPI::Model& InModel, const IO::Location& InDestFile)
{
	FTimeStat DoExportStart;

	// The exporter
	FDatasmithSceneExporter SceneExporter;
	SceneExporter.PreExport();

	// Get the name without extension
	IO::Name FileName;
	InDestFile.GetLastLocalName(&FileName);
	FString LabelString(GSStringToUE(FileName.GetBase()));

	SceneExporter.SetName(*LabelString);

	IO::Location FileLocation(InDestFile);
	FString		 FilePath(GSStringToUE(FileLocation.ToDisplayText()));
	FileLocation.DeleteLastLocalName();
	SceneExporter.SetOutputPath(GSStringToUE(FileLocation.ToDisplayText()));

	// Setup our progression
	bool		 OutUserCancelled = false;
	FProgression Progression(kStrListProgression, kExportTitle, kNbPhases, FProgression::kThrowOnCancel,
							 &OutUserCancelled);

	FSyncDatabase SyncDatabase(*FilePath, *LabelString, SceneExporter.GetAssetsOutputPath(),
							   FSyncDatabase::GetCachePath());

	FSyncContext SyncContext(false, InModel, SyncDatabase, &Progression);

	TSharedRef< IDatasmithScene > Scene = SyncDatabase.GetScene();

	SyncDatabase.SetSceneInfo();
	SyncDatabase.Synchronize(SyncContext);

	FTimeStat DoExportSyncEnd;

	SyncContext.NewPhase(kExportSaving);

	// Datasmith do the save
	SceneExporter.Export(Scene);
	SyncContext.Stats.Print();

	FTimeStat DoExportEnd;

	DoExportSyncEnd.PrintDiff("Synchronization", DoExportStart);
	DoExportEnd.PrintDiff("Scene Export", DoExportSyncEnd);
	DoExportEnd.PrintDiff("Total DoExport", DoExportStart);
	SyncDatabase.GetMeshIndexor().SaveToFile();
}

// Export the AC model in the specified file
GSErrCode FExporter::DoChooseDestination(IO::Location* OutDestFile)
{
	DG::FileDialog fileDialog(DG::FileDialog::Save);

	// IO::fileSystem.GetSpecialLocation(IO::FileSystem::CurrentFolder, OutDestFile);

	FTM::FileTypeManager templateFileFTM("TemplateFileFTM");
	FTM::TypeID			 datasmithTypeID = templateFileFTM.AddType(
		 FTM::FileType(GetStdName(kName_DatasmithFileTypeName), "udatasmith", 0, 0, kIconDSFile));

	fileDialog.SetTitle(GetGSName(kName_ExportToDatasmithFile));
	fileDialog.AddFilter(datasmithTypeID);
	fileDialog.AddFilter(FTM::RootGroup);
	fileDialog.SelectFilter(0);
	//	fileDialog.SetFolder(*OutDestFile);

	if (!fileDialog.Invoke())
		return Cancel;

	*OutDestFile = fileDialog.GetSelectedFile();

	return NoError;
}

END_NAMESPACE_UE_AC
