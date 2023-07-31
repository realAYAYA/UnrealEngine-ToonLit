// Copyright Epic Games, Inc. All Rights Reserved.

#include "Changes/MeshChange.h"
#include "DynamicMesh/DynamicMesh3.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshChange)

using namespace UE::Geometry;

FMeshChange::FMeshChange()
{
}

FMeshChange::FMeshChange(TUniquePtr<FDynamicMeshChange> DynamicMeshChangeIn)
{
	DynamicMeshChange = MoveTemp(DynamicMeshChangeIn);
}

void FMeshChange::Apply(UObject* Object)
{
	IMeshCommandChangeTarget* ChangeTarget = CastChecked<IMeshCommandChangeTarget>(Object);
	ChangeTarget->ApplyChange(this, false);

	if (OnChangeAppliedFunc)
	{
		OnChangeAppliedFunc(this, Object, true);
	}
}

void FMeshChange::Revert(UObject* Object)
{
	IMeshCommandChangeTarget* ChangeTarget = CastChecked<IMeshCommandChangeTarget>(Object);
	ChangeTarget->ApplyChange(this, true);

	if (OnChangeAppliedFunc)
	{
		OnChangeAppliedFunc(this, Object, false);
	}
}


FString FMeshChange::ToString() const
{
	return FString(TEXT("Mesh Change"));
}


void FMeshChange::ApplyChangeToMesh(FDynamicMesh3* Mesh, bool bRevert) const
{
	DynamicMeshChange->Apply(Mesh, bRevert);
}



