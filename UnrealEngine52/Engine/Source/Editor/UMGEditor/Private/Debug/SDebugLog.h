// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FTokenizedMessage;
class ITableRow;
class STableViewBase;

namespace UE::UMG
{

class SDebugLog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDebugLog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, FName LogName);

private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FTokenizedMessage> Message, const TSharedRef<STableViewBase>& OwnerTable) const;
};

} // namespace UE::UMG
