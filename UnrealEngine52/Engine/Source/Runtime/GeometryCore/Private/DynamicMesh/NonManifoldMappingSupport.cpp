// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMesh/NonManifoldMappingSupport.h"

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"


FName UE::Geometry::FNonManifoldMappingSupport::NonManifoldMeshVIDsAttrName = FName(TEXT("NonManifoldVIDAttr"));


UE::Geometry::FNonManifoldMappingSupport::FNonManifoldMappingSupport(const FDynamicMesh3& MeshIn)
{
	Reset(MeshIn);
}

void UE::Geometry::FNonManifoldMappingSupport::Reset(const FDynamicMesh3& MeshIn)
{
	
	NonManifoldSrcVIDsAttribute = nullptr;
	DynamicMesh = &MeshIn;

	const FDynamicMeshAttributeSet* Attributes = DynamicMesh->Attributes();

	if (Attributes && Attributes->HasAttachedAttribute(NonManifoldMeshVIDsAttrName))
	{
		NonManifoldSrcVIDsAttribute = static_cast<const FDynamicMeshVertexInt32Attribute* >(Attributes->GetAttachedAttribute(NonManifoldMeshVIDsAttrName));
	}
	
}

bool  UE::Geometry::FNonManifoldMappingSupport::IsNonManifoldVertexInSource() const
{
	return (NonManifoldSrcVIDsAttribute != nullptr);
}

int32 UE::Geometry::FNonManifoldMappingSupport::GetOriginalNonManifoldVertexID(const int32 vid) const
{
	checkSlow(DynamicMesh->IsVertex(vid));

	int32 Result = vid;
	if (NonManifoldSrcVIDsAttribute)
	{
		NonManifoldSrcVIDsAttribute->GetValue(vid, &Result);
	}
	return Result;
}

bool UE::Geometry::FNonManifoldMappingSupport::AttachNonManifoldVertexMappingData(const TArray<int32>& VertexToNonManifoldVertexIDMap, FDynamicMesh3& MeshInOut)
{
	
	const int32 MaxVID = MeshInOut.MaxVertexID();
	FDynamicMeshAttributeSet* Attributes = MeshInOut.Attributes();

	if (VertexToNonManifoldVertexIDMap.Num() < MaxVID || !Attributes)
	{
		return false;
	}

	// create and attach a vertex ID buffer, removing any pre-exising one.
	FDynamicMeshVertexInt32Attribute* VIDAttr = [&] {
													// replace existing buffer.  we *should* be able to re-use it
													if (Attributes->HasAttachedAttribute(NonManifoldMeshVIDsAttrName))
													{
														Attributes->RemoveAttribute(NonManifoldMeshVIDsAttrName);
													}
													FDynamicMeshVertexInt32Attribute* SrcMeshVIDAttr = new FDynamicMeshVertexInt32Attribute(&MeshInOut);
													SrcMeshVIDAttr->SetName(NonManifoldMeshVIDsAttrName);
													MeshInOut.Attributes()->AttachAttribute(NonManifoldMeshVIDsAttrName, SrcMeshVIDAttr);
													return SrcMeshVIDAttr;
												}();
	
	
	for (int32 vid : MeshInOut.VertexIndicesItr())
	{
		const int32 SrcVID = VertexToNonManifoldVertexIDMap[vid];
		VIDAttr->SetValue(vid, &SrcVID);
	}
	return true;
}

void UE::Geometry::FNonManifoldMappingSupport::RemoveNonManifoldVertexMappingData(FDynamicMesh3& MeshInOut)
{

	FDynamicMeshAttributeSet* Attributes = MeshInOut.Attributes();

	if (Attributes && Attributes->HasAttachedAttribute(NonManifoldMeshVIDsAttrName))
	{
		 Attributes->RemoveAttribute(NonManifoldMeshVIDsAttrName);
	}

}