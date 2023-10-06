// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/Interface.h"
#include "ClothEditorToolBuilder.generated.h"

namespace UE::Chaos::ClothAsset
{
	enum class EClothPatternVertexType : uint8;
}


UINTERFACE(MinimalAPI)
class UChaosClothAssetEditorToolBuilder : public UInterface
{
	GENERATED_BODY()
};


class IChaosClothAssetEditorToolBuilder
{
	GENERATED_BODY()

public:

	/** Returns all Construction View modes that this tool can operate in. The first element should be the preferred mode to switch to if necessary. */
	virtual void GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const = 0;

	/** Returns whether or not view can be set to wireframe when this tool is active.. */
	virtual bool CanSetConstructionViewWireframeActive() const { return true; }
};

