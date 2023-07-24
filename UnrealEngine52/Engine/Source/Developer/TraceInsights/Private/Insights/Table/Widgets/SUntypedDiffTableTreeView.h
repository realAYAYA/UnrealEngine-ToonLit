// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "SUntypedTableTreeView.h"
#include "Insights/Table/ViewModels/UntypedTable.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class SUntypedDiffTableTreeView : public SUntypedTableTreeView
{
public:
	void UpdateSourceTableA(const FString& Name, TSharedPtr<TraceServices::IUntypedTable> SourceTable);
	void UpdateSourceTableB(const FString& Name, TSharedPtr<TraceServices::IUntypedTable> SourceTable);

protected:
	FReply SwapTables_OnClicked();
	FText GetSwapButtonText() const;

	virtual TSharedPtr<SWidget> ConstructToolbar() override;

	void RequestMergeTables();

private:
	TSharedPtr<TraceServices::IUntypedTable> SourceTableA;
	TSharedPtr<TraceServices::IUntypedTable> SourceTableB;
	FString TableNameA;
	FString TableNameB;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
