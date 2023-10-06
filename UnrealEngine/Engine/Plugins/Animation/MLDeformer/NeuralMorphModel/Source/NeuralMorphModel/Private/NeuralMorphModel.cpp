// Copyright Epic Games, Inc. All Rights Reserved.
#include "NeuralMorphModel.h"
#include "NeuralMorphModelVizSettings.h"
#include "NeuralMorphModelInstance.h"
#include "NeuralMorphInputInfo.h"
#include "NeuralMorphNetwork.h"
#include "MLDeformerAsset.h"
#include "MLDeformerComponent.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NeuralMorphModel)

#define LOCTEXT_NAMESPACE "NeuralMorphModel"

// Implement our module.
namespace UE::NeuralMorphModel
{
	class NEURALMORPHMODEL_API FNeuralMorphModelModule
		: public IModuleInterface
	{
	};
}
IMPLEMENT_MODULE(UE::NeuralMorphModel::FNeuralMorphModelModule, NeuralMorphModel)

// Our log category for this model.
NEURALMORPHMODEL_API DEFINE_LOG_CATEGORY(LogNeuralMorphModel)

//////////////////////////////////////////////////////////////////////////////

UNeuralMorphModel::UNeuralMorphModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Create the visualization settings for this model.
	// Never directly create one of the frameworks base classes such as the FMLDeformerMorphModelVizSettings as
	// that can cause issues with detail customizations.
#if WITH_EDITORONLY_DATA
	SetVizSettings(ObjectInitializer.CreateEditorOnlyDefaultSubobject<UNeuralMorphModelVizSettings>(this, TEXT("VizSettings")));
#endif
}

UMLDeformerModelInstance* UNeuralMorphModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UNeuralMorphModelInstance>(Component);
}

UMLDeformerInputInfo* UNeuralMorphModel::CreateInputInfo()
{
	return NewObject<UNeuralMorphInputInfo>(this);
}

void UNeuralMorphModel::Serialize(FArchive& Archive)
{
	if (Archive.IsSaving())
	{
		UpdateMissingGroupNames();
	}

	if (Archive.IsSaving() && Archive.IsCooking())
	{
		// We haven't got a trained network, let's log a message about this, as it might be overlooked.
		if (NeuralMorphNetwork == nullptr)
		{
			UE_LOG(LogNeuralMorphModel, Display, TEXT("Neural Morph Model in MLD asset '%s' still needs to be trained."), *GetDeformerAsset()->GetName());
		}

		// Strip the mask data in the cooked asset.
		BoneMaskInfos.Empty();
		BoneGroupMaskInfos.Empty();
	}

	// Convert the UMLDeformerInputInfo object into a UNeuralMorphInputInfo object for backward compatiblity.
	UMLDeformerInputInfo* CurInputInfo = GetInputInfo();
	if (CurInputInfo)
	{
		if (!CurInputInfo->IsA<UNeuralMorphInputInfo>())
		{
			UNeuralMorphInputInfo* NeuralMorphInputInfo = Cast<UNeuralMorphInputInfo>(CreateInputInfo());
			NeuralMorphInputInfo->CopyMembersFrom(CurInputInfo);
			SetInputInfo(NeuralMorphInputInfo);
		}		
	}

	Super::Serialize(Archive);
}

void UNeuralMorphModel::UpdateMissingGroupNames()
{
	// Auto set names for the bone groups if they haven't been set yet.
	for (int32 Index = 0; Index < BoneGroups.Num(); ++Index)
	{
		FNeuralMorphBoneGroup& BoneGroup = BoneGroups[Index];
		if (!BoneGroup.GroupName.IsValid() || BoneGroup.GroupName.IsNone())
		{
			BoneGroup.GroupName = FName(FString::Format(TEXT("Bone Group #{0}"), {Index}));
		}
	}

	// Auto set names for the curve groups if they haven't been set yet.
	for (int32 Index = 0; Index < CurveGroups.Num(); ++Index)
	{
		FNeuralMorphCurveGroup& CurveGroup = CurveGroups[Index];
		if (!CurveGroup.GroupName.IsValid() || CurveGroup.GroupName.IsNone())
		{
			CurveGroup.GroupName = FName(FString::Format(TEXT("Curve Group #{0}"), {Index}));
		}
	}
}

void UNeuralMorphModel::PostLoad()
{
	Super::PostLoad();
	UpdateMissingGroupNames();
}

void UNeuralMorphModel::SetNeuralMorphNetwork(UNeuralMorphNetwork* Net)
{ 
	NeuralMorphNetwork = Net;
	GetReinitModelInstanceDelegate().Broadcast();
}

int32 UNeuralMorphModel::GetNumFloatsPerCurve() const
{
	if (NeuralMorphNetwork)
	{
		return NeuralMorphNetwork->GetNumFloatsPerCurve();
	}
	return (Mode == ENeuralMorphMode::Local) ? 6 : 1;
}

#undef LOCTEXT_NAMESPACE
