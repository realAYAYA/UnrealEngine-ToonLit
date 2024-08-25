// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGMEImageItem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FGMEResourceItemViewModel;
class IGMETreeNodeViewModel;
class ITableRow;
class STableViewBase;

class SGMEResourceItem
	: public SGMEImageItem<FGMEResourceItemViewModel>
{
public:
	SLATE_BEGIN_ARGS(SGMEResourceItem) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<FGMEResourceItemViewModel>& InViewModel);

protected:
	FText GetLabel() const;
	virtual FOptionalSize GetAspectRatio() override;
	
private:
	TSharedPtr<FGMEResourceItemViewModel> ViewModel;

	/** The Slate brush that renders the texture. */
	TSharedPtr<FSlateBrush> TextureBrush;
};
