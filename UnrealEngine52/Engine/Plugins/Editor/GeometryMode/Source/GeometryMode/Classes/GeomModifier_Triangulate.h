// Copyright Epic Games, Inc. All Rights Reserved.


/**
 * Triangulates selected objects.
 */

#pragma once

#include "GeomModifier_Edit.h"
#include "GeomModifier_Triangulate.generated.h"

UCLASS()
class UGeomModifier_Triangulate : public UGeomModifier_Edit
{
	GENERATED_UCLASS_BODY()


	//~ Begin UGeomModifier Interface
	virtual bool SupportsCurrentSelection() override;
protected:
	virtual bool OnApply() override;
	//~ End UGeomModifier Interface
};




#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
