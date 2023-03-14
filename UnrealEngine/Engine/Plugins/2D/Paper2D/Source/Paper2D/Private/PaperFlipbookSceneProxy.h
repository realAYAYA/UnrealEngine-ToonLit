// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperRenderSceneProxy.h"

//////////////////////////////////////////////////////////////////////////
// FPaperFlipbookSceneProxy

class FPaperFlipbookSceneProxy final : public FPaperRenderSceneProxy_SpriteBase
{
public:
	SIZE_T GetTypeHash() const override;

	FPaperFlipbookSceneProxy(const class UPaperFlipbookComponent* InComponent);
};
