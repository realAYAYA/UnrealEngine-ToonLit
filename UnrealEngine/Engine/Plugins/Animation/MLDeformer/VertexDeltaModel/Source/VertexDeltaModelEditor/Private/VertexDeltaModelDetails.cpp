// Copyright Epic Games, Inc. All Rights Reserved.

#include "VertexDeltaModelDetails.h"
#include "VertexDeltaModel.h"
#include "DetailCategoryBuilder.h"

#define LOCTEXT_NAMESPACE "VertexDeltaModelDetails"

namespace UE::VertexDeltaModel
{
	using namespace UE::MLDeformer;

	TSharedRef<IDetailCustomization> FVertexDeltaModelDetails::MakeInstance()
	{
		return MakeShareable(new FVertexDeltaModelDetails());
	}

	void FVertexDeltaModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		// Create all the detail categories and add the properties of the base class.
		FMLDeformerGeomCacheModelDetails::CustomizeDetails(DetailBuilder);

		// Training settings.
		TrainingSettingsCategoryBuilder->AddProperty(GET_MEMBER_NAME_CHECKED(UVertexDeltaModel, NumHiddenLayers));
		TrainingSettingsCategoryBuilder->AddProperty(GET_MEMBER_NAME_CHECKED(UVertexDeltaModel, NumNeuronsPerLayer));
		TrainingSettingsCategoryBuilder->AddProperty(GET_MEMBER_NAME_CHECKED(UVertexDeltaModel, NumIterations));
		TrainingSettingsCategoryBuilder->AddProperty(GET_MEMBER_NAME_CHECKED(UVertexDeltaModel, BatchSize));
		TrainingSettingsCategoryBuilder->AddProperty(GET_MEMBER_NAME_CHECKED(UVertexDeltaModel, LearningRate));
	}
}	// namespace UE::VertexDeltaModel

#undef LOCTEXT_NAMESPACE
