// Copyright Epic Games, Inc. All Rights Reserved.


/**
 * Optimizes selected objects by attempting to merge their polygons back together.
 */

#pragma once

#include "GeomModifier_Triangulate.h"
#include "GeomModifier_Optimize.generated.h"

UCLASS()
class UGeomModifier_Optimize : public UGeomModifier_Triangulate
{
	GENERATED_UCLASS_BODY()


	//~ Begin UGeomModifier Interface
protected:
	virtual bool OnApply() override;
	//~ End UGeomModifier Interface
};




#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
