// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCacheComponent.h"

#include "GeometryCacheUSDComponent.generated.h"

/** GeometryCacheUSDComponent, encapsulates a transient GeometryCache asset instance that fetches its data from a USD file and implements functionality for rendering and playback */
UCLASS(HideDropDown, ClassGroup = (Rendering), meta = (DisplayName = "USD Geometry Cache"), Experimental, ClassGroup = Experimental)
class GEOMETRYCACHEUSD_API UGeometryCacheUsdComponent : public UGeometryCacheComponent
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	//~ End UObject Interface

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.
};
