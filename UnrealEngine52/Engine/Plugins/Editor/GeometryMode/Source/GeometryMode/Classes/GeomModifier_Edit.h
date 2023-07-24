// Copyright Epic Games, Inc. All Rights Reserved.


/**
 *
 */


//~=============================================================================
// GeomModifier_Edit: Maniupalating selected objects with the widget
//~=============================================================================

#pragma once

#include "GeomModifier.h"
#include "GeomModifier_Edit.generated.h"

class FViewport;

UCLASS(autoexpandcategories=Settings)
class UGeomModifier_Edit : public UGeomModifier
{
	GENERATED_UCLASS_BODY()

	virtual bool SupportsCurrentSelection() override;

	//~ Begin UGeomModifier Interface
	virtual bool InputDelta(class FEditorViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale) override;	
	//~ End UGeomModifier Interface
};




#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
