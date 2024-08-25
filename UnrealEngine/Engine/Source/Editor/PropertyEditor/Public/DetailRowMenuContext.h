// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "DetailRowMenuContext.generated.h"

class IDetailsView;
class IPropertyHandle;

UCLASS()
class PROPERTYEDITOR_API UDetailRowMenuContext : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UDetailRowMenuContext() override
	{
		DetailsView = nullptr;		
	}
	
	/** Optionally invoke to refresh the widget. */
	TMulticastDelegate<void()>& ForceRefreshWidget() { return ForceRefreshWidgetDelegate; } 
	
	/** PropertyHandles associated with the Row. */
	TArray<TSharedPtr<IPropertyHandle>> PropertyHandles;

	/** Containing DetailsView. */
	IDetailsView* DetailsView = nullptr;
	
private:
	/** Optionally invoke to refresh the widget. */
	TMulticastDelegate<void()> ForceRefreshWidgetDelegate;
};
