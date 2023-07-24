// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FleshAsset.cpp: UFleshAsset methods.
=============================================================================*/
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshCollection.h"
#include "GeometryCollection/TransformCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FleshAsset)

DEFINE_LOG_CATEGORY_STATIC(LogFleshAssetInternal, Log, All);


FFleshAssetEdit::FFleshAssetEdit(UFleshAsset* InAsset, FPostEditFunctionCallback InCallback)
	: PostEditCallback(InCallback)
	, Asset(InAsset)
{
}

FFleshAssetEdit::~FFleshAssetEdit()
{
	PostEditCallback();
}

FFleshCollection* FFleshAssetEdit::GetFleshCollection()
{
	if (Asset)
	{
		return Asset->FleshCollection.Get();
	}
	return nullptr;
}

UFleshAsset::UFleshAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FleshCollection(new FFleshCollection())
{
}

void UFleshAsset::SetCollection(FFleshCollection* InCollection)
{
	FleshCollection = TSharedPtr<FFleshCollection, ESPMode::ThreadSafe>(InCollection);
	Modify();
}


void UFleshAsset::PostEditCallback()
{
	//UE_LOG(LogFleshAssetInternal, Log, TEXT("UFleshAsset::PostEditCallback()"));
}

TManagedArray<FVector3f>& UFleshAsset::GetPositions()
{
	return FleshCollection->ModifyAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
}

const TManagedArray<FVector3f>* UFleshAsset::FindPositions() const
{
	return FleshCollection->FindAttributeTyped<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
}

/** Serialize */
void UFleshAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	bool bCreateSimulationData = false;
	Chaos::FChaosArchive ChaosAr(Ar);
	FleshCollection->Serialize(ChaosAr);
}

