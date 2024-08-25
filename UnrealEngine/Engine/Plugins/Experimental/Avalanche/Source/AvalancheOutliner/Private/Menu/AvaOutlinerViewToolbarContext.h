// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "AvaOutlinerViewToolbarContext.generated.h"

class FAvaOutlinerView;

UCLASS()
class UAvaOutlinerViewToolbarContext : public UObject
{
	GENERATED_BODY()

	friend FAvaOutlinerView;

public:
	UAvaOutlinerViewToolbarContext() = default;

	TSharedPtr<FAvaOutlinerView> GetOutlinerView() const { return OutlinerViewWeak.Pin(); }

private:
	TWeakPtr<FAvaOutlinerView> OutlinerViewWeak;
};
