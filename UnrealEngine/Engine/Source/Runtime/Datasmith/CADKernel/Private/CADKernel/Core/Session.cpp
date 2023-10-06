// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Core/Session.h"

#include "CADKernel/Topo/Model.h"
#include "CADKernel/UI/Message.h"

namespace UE::CADKernel
{

#if defined(CADKERNEL_DEV) || defined(CADKERNEL_STDA)
FSession FSession::Session(0.01);
#endif

void FSession::SaveDatabase(const TCHAR* FileName)
{
	TSharedPtr<FCADKernelArchive> Archive = FCADKernelArchive::CreateArchiveWriter(*this, FileName);
	if (!Archive.IsValid())
	{
		FMessage::Printf(Log, TEXT("The archive file %s is corrupted\n"), FileName);
	}

	Database.Serialize(*Archive.Get());
	Archive->Close();
}

FModel& FSession::GetModel()
{
	return Database.GetModel();
}

void FSession::SaveDatabase(const TCHAR* FileName, const TArray<FEntity*>& SelectedEntities)
{
	TArray<FIdent> EntityIds;
	EntityIds.Reserve(SelectedEntities.Num());

	SpawnEntityIdents(SelectedEntities, true);

	for (const FEntity* Entity : SelectedEntities)
	{
		EntityIds.Add(Entity->GetId());
	}

	TSharedPtr<FCADKernelArchive> Archive = FCADKernelArchive::CreateArchiveWriter(*this, FileName);

	Database.SerializeSelection(*Archive.Get(), EntityIds);
	Archive->Close();
}

void FSession::SaveDatabase(const TCHAR* FileName, const TArray<TSharedPtr<FEntity>>& SelectedEntities)
{
	TArray<FIdent> EntityIds;
	EntityIds.Reserve(SelectedEntities.Num());

	SpawnEntityIdents(SelectedEntities, true);

	for (const TSharedPtr<FEntity>& Entity : SelectedEntities)
	{
		EntityIds.Add(Entity->GetId());
	}

	TSharedPtr<FCADKernelArchive> Archive = FCADKernelArchive::CreateArchiveWriter(*this, FileName);

	Database.SerializeSelection(*Archive.Get(), EntityIds);
	Archive->Close();
}

void FSession::LoadDatabase(const TCHAR* FilePath)
{
	TSharedPtr<FCADKernelArchive> Archive = FCADKernelArchive::CreateArchiveReader(*this, FilePath);
	if (!Archive.IsValid())
	{
		FMessage::Printf(Log, TEXT("The archive file %s is corrupted\n"), FilePath);
		return;
	}

	FModel& Model = GetModel();

	Database.Deserialize(*Archive.Get());
	FModel* ArchiveModel = Archive->ArchiveModel;

	if(ArchiveModel != nullptr)
	{
		ArchiveModel->Empty();
		Database.RemoveEntity(*ArchiveModel);
	}
	Archive->Close();
}

void FSession::AddDatabase(const TArray<uint8>& InRawData)
{
	FCADKernelArchive Archive = FCADKernelArchive(*this, InRawData);
	Database.Deserialize(Archive);
}

void FSession::SetGeometricTolerance(double NewTolerance)
{
	ensure(Database.GetModel().EntityCount() == 0);
	GeometricTolerance = NewTolerance;
	IntersectionTool::SetTolerance(NewTolerance);
}
} // namespace UE::CADKernel
