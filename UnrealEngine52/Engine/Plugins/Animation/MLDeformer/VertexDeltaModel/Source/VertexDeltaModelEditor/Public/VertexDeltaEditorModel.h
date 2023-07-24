// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheEditorModel.h"

class UNeuralNetwork;
class UVertexDeltaModel;

namespace UE::VertexDeltaModel
{
	using namespace UE::MLDeformer;

	/** 
	 * The editor model for a UVertexDeltaModel.
	 */
	class VERTEXDELTAMODELEDITOR_API FVertexDeltaEditorModel 
		: public UE::MLDeformer::FMLDeformerGeomCacheEditorModel
	{
	public:
		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override	{ return TEXT("FVertexDeltaEditorModel"); }
		// ~END FGCObject overrides.
	
		// FMLDeformerEditorModel overrides.
		virtual ETrainingResult Train() override;
		virtual bool LoadTrainedNetwork() const override;
		virtual bool IsTrained() const override;
		virtual FString GetHeatMapDeformerGraphPath() const override;
		// ~END FMLDeformerEditorModel overrides.

		UNeuralNetwork* LoadNeuralNetworkFromOnnx(const FString& Filename) const;
		UVertexDeltaModel* GetVertexDeltaModel() const;
	};
}	// namespace UE::VertexDeltaModel
