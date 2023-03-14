// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamOutputProviderBase.h"

#include "VCamOutputViewport.generated.h"

UCLASS(meta = (DisplayName = "Viewport Output Provider"))
class VCAMCORE_API UVCamOutputViewport : public UVCamOutputProviderBase
{
	GENERATED_BODY()

public:

protected:
	virtual void CreateUMG() override;

private:
};
