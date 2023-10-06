// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosFlesh/Asset/FleshAssetFactory.h"

#include "ChaosFlesh/FleshComponent.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FleshAssetFactory)


#define LOCTEXT_NAMESPACE "ChaosFlesh"

/////////////////////////////////////////////////////
// FleshFactory

UFleshAssetFactory::UFleshAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UFleshAsset::StaticClass();
}

UObject* UFleshAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	FTransform LastTransform = FTransform::Identity;
	TArray< FleshTuple > FleshList;

	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetAsset()->IsA<UFleshAsset>())
		{
			UFleshComponent *DummyValue(NULL);
			FleshList.Add(FleshTuple(static_cast<const UFleshAsset *>(AssetData.GetAsset()), DummyValue, FTransform()));
		}

	}

	UFleshAsset* NewFlesh = NewObject<UFleshAsset>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone);

	NewFlesh->Modify();
	return NewFlesh;
}

#undef LOCTEXT_NAMESPACE




