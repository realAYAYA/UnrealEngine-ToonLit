// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyPath.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
struct FCapturableProperty;

class SCapturedPropertiesWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCapturedPropertiesWidget) {}
	SLATE_ARGUMENT(TArray<TSharedPtr<FCapturableProperty>>*, PropertyPaths)
	SLATE_END_ARGS()

	SCapturedPropertiesWidget()
	{
	}

	~SCapturedPropertiesWidget()
	{
	}

	void Construct(const FArguments& InArgs);
	TArray<TSharedPtr<FCapturableProperty>> GetCurrentCheckedProperties();
	void FilterPropertyPaths(const FText& Filter);

private:

	TSharedRef<ITableRow> MakeCapturedPropertyWidget(TSharedPtr<FCapturableProperty> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<TSharedPtr<FCapturableProperty>> CapturedProperties;

	TSharedPtr<SListView<TSharedPtr<FCapturableProperty>>> PropListView;
	TArray<TSharedPtr<FCapturableProperty>> FilteredCapturedProperties;
};
