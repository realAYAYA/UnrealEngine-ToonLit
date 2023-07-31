// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheEditorModel.h"

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
		// ~END FMLDeformerEditorModel overrides.
	};
}	// namespace UE::VertexDeltaModel
