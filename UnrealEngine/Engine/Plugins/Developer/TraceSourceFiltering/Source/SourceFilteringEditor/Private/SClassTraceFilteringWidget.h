// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class ISessionSourceFilterService;
class FClassFilterObject;
class SWrapBox;
class SComboButton;

class SClassTraceFilteringWidget : public SCompoundWidget
{
public:
	/** Default constructor. */
	SClassTraceFilteringWidget() {}
	virtual ~SClassTraceFilteringWidget() {}

	SLATE_BEGIN_ARGS(SClassTraceFilteringWidget) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);

	void SetSessionFilterService(TSharedPtr<ISessionSourceFilterService> InSessionFilterService);
protected:
	void ConstructClassFilterPickerButton();
	void RefreshClassFilterData();
protected:
	/** Session service used to retrieve state and request filtering changes */
	TSharedPtr<ISessionSourceFilterService> SessionFilterService;

	/** Latest set of retrieved Class Filter Objects from SessionFilterService */
	TArray<TSharedPtr<FClassFilterObject>> ClassFilterObjects;

	/** Wrap box that will contain the class filter object widgets */
	TSharedPtr<SWrapBox> ClassFiltersWrapBox;

	TSharedPtr<SComboButton> AddClassFilterButton;
};