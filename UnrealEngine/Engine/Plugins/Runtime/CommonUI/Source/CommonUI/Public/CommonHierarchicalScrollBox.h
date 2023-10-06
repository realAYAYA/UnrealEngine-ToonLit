// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ScrollBox.h"
#include "CommonHierarchicalScrollBox.generated.h"

/**
 * An arbitrary scrollable collection of widgets.  Great for presenting 10-100 widgets in a list.  Doesn't support virtualization.
 */
UCLASS()
class COMMONUI_API UCommonHierarchicalScrollBox : public UScrollBox
{
	GENERATED_UCLASS_BODY()

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
