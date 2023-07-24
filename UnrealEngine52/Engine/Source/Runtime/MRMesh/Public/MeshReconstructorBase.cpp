// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshReconstructorBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshReconstructorBase)


void UMeshReconstructorBase::StartReconstruction()
{
}

void UMeshReconstructorBase::StopReconstruction()
{
}

void UMeshReconstructorBase::PauseReconstruction()
{
}

bool UMeshReconstructorBase::IsReconstructionStarted() const
{
	return false;
}

bool UMeshReconstructorBase::IsReconstructionPaused() const
{
	return false;
}

void UMeshReconstructorBase::ConnectMRMesh(class UMRMeshComponent* Mesh)
{
}

void UMeshReconstructorBase::DisconnectMRMesh()
{
}
