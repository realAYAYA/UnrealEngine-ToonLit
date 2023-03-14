// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreeItem.h"

#include "Internationalization/Text.h"
#include "SSkeletonTreeRow.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class ITableRow;
class STableViewBase;

TSharedRef<ITableRow> FSkeletonTreeItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, const TAttribute<FText>& InFilterText)
{
	return SNew(SSkeletonTreeRow, InOwnerTable)
		.FilterText(InFilterText)
		.Item(SharedThis(this))
		.OnDraggingItem(this, &FSkeletonTreeItem::OnDragDetected);
}
