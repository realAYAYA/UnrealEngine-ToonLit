// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FMenuBuilder;
class SComboButton;
class UDataprepObjectSelectionFilter;

template<class t>
class SComboBox;

struct FDataprepParametrizationActionData;

class SDataprepObjectSelectionFilter : public SCompoundWidget, public FGCObject
{
	SLATE_BEGIN_ARGS(SDataprepObjectSelectionFilter) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UDataprepObjectSelectionFilter& InFilter);

private:

	UDataprepObjectSelectionFilter* Filter;

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SDataprepObjectSelectionFilter");
	}
	//~ End FGCObject interface
};
