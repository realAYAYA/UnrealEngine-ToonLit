// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpriteDrawCall.h"
#include "PaperRenderSceneProxy.h"

class FMeshElementCollector;
class UBodySetup;
class UPaperSpriteComponent;

//////////////////////////////////////////////////////////////////////////
// FPaperSpriteSceneProxy

class FPaperSpriteSceneProxy final : public FPaperRenderSceneProxy_SpriteBase
{
public:
	SIZE_T GetTypeHash() const override;

	FPaperSpriteSceneProxy(UPaperSpriteComponent* InComponent);

	// FPrimitiveSceneProxy interface
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	// End of FPrimitiveSceneProxy interface

protected:
	const UBodySetup* BodySetup;
};
