// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VCamWidgetFactory.h"
#include "VCamStateSwitcherWidgetFactory.generated.h"

UCLASS()
class VCAMCOREEDITOR_API UVCamStateSwitcherWidgetFactory : public UVCamWidgetFactory
{
	GENERATED_BODY()
public:
	
	UVCamStateSwitcherWidgetFactory();

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
	//~ End UFactory Interface
};