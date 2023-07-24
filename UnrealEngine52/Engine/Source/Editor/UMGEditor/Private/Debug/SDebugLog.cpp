// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDebugLog.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace UE::UMG
{

void SDebugLog::Construct(const FArguments& Args, FName LogName)
{
	ChildSlot
	[
		SNew(SListView<TSharedPtr<FTokenizedMessage>>)
		.OnGenerateRow(this, &SDebugLog::OnGenerateRow)
	];
}

TSharedRef<ITableRow> SDebugLog::OnGenerateRow(TSharedPtr<FTokenizedMessage> Message, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FTokenizedMessage>>, OwnerTable);
}

} // namespace UE::UMG
