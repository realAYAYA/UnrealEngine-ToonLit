// Copyright Epic Games, Inc. All Rights Reserved.

#include "VertexDeltaModel.h"
#include "VertexDeltaModelVizSettings.h"

#define LOCTEXT_NAMESPACE "VertexDeltaModel"

VERTEXDELTAMODEL_API DEFINE_LOG_CATEGORY(LogVertexDeltaModel)

// Implement the module.
namespace UE::VertexDeltaModel
{
	class VERTEXDELTAMODEL_API FVertexDeltaModelModule
		: public IModuleInterface
	{
	};
}
IMPLEMENT_MODULE(UE::VertexDeltaModel::FVertexDeltaModelModule, VertexDeltaModel)


UVertexDeltaModel::UVertexDeltaModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	SetVizSettings(ObjectInitializer.CreateEditorOnlyDefaultSubobject<UVertexDeltaModelVizSettings>(this, TEXT("VizSettings")));
#endif
}

FString UVertexDeltaModel::GetDefaultDeformerGraphAssetPath() const 
{ 
	return FString(TEXT("/MLDeformerFramework/Deformers/DG_MLDeformerModel.DG_MLDeformerModel"));
}

#undef LOCTEXT_NAMESPACE
