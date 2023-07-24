// Copyright Epic Games, Inc. All Rights Reserved.
#include "NeuralMorphModel.h"
#include "NeuralMorphModelVizSettings.h"
#include "NeuralMorphModelInstance.h"
#include "NeuralMorphInputInfo.h"
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
	if (Archive.IsSaving() && Archive.IsCooking())
	{
		if (NeuralMorphNetwork == nullptr)
		{
			UE_LOG(LogNeuralMorphModel, Display, TEXT("Neural Morph Model in MLD asset '%s' still needs to be trained."), *GetDeformerAsset()->GetName());
		}
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

#undef LOCTEXT_NAMESPACE
