// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectSkeletalMesh.h"

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectMeshUpdate.h"


UCustomizableObjectSkeletalMesh* UCustomizableObjectSkeletalMesh::CreateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& OperationData, const UCustomizableObjectInstance& Instance, const int32 InComponentIndex)
{
	UCustomizableObject* CustomizableObject = Instance.GetCustomizableObject();
	check(CustomizableObject);

	UCustomizableObjectSkeletalMesh* OutSkeletalMesh = NewObject<UCustomizableObjectSkeletalMesh>();
	
	// Debug info
	OutSkeletalMesh->CustomizableObjectPathName = GetNameSafe(CustomizableObject);
	OutSkeletalMesh->InstancePathName = Instance.GetName();


	// Init properties
	OutSkeletalMesh->Model = CustomizableObject->GetPrivate()->GetModel();

	OutSkeletalMesh->Parameters = OperationData->Parameters;
	OutSkeletalMesh->State = OperationData->GetCapturedDescriptor().GetState();
	
	OutSkeletalMesh->MeshIDs.Init(MAX_uint64, MAX_MESH_LOD_COUNT);
	
	for (int32 LODIndex = OperationData->FirstLODAvailable; LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			if (Component.Id == InComponentIndex)
			{
				OutSkeletalMesh->MeshIDs[LODIndex] = Component.MeshID;
			}
		}
	}

	return OutSkeletalMesh;
}


bool UCustomizableObjectSkeletalMesh::StreamIn(int32 NewMipCount, bool bHighPrio)
{
	check(IsInGameThread());
	if (!HasPendingInitOrStreaming() && CachedSRRState.StreamIn(NewMipCount))
	{
		PendingUpdate = new FCustomizableObjectMeshStreamIn(this, bHighPrio, !GRHISupportsAsyncTextureCreation);

		return !PendingUpdate->IsCancelled();
	}
	return false;
}
