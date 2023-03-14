// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheVizSettingsDetails.h"

namespace UE::VertexDeltaModel
{
	/**
	 * The detail customization for the vertex delta model visualization settings.
	 * We inherit from a base class as that already takes care of some nice grouping of properties, some error reporting
	 * in case of issues, and some nice grouping of settings.
	 */
	class VERTEXDELTAMODELEDITOR_API FVertexDeltaModelVizSettingsDetails
		: public UE::MLDeformer::FMLDeformerGeomCacheVizSettingsDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShareable(new FVertexDeltaModelVizSettingsDetails());
		}
	};
}	// namespace UE::VertexDeltaModel
