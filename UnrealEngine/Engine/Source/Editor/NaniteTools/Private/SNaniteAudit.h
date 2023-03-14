// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SCompoundWidget.h"
#include "NaniteAuditRegistry.h"
#include "Misc/NotifyHook.h"

class ITableRow;
class STableViewBase;
class FUICommandList;
class UNaniteToolsArguments;
class UNaniteAuditErrorArguments;
class UNaniteAuditOptimizeArguments;
class IDetailsView;
class SNaniteTools;

struct FNaniteAuditRow : TSharedFromThis<FNaniteAuditRow>
{
public:
	explicit FNaniteAuditRow(TSharedPtr<FNaniteAuditRecord> InRecord)
	: Record(InRecord)
	, SelectionState(ECheckBoxState::Checked)
	{
	}

	TSharedPtr<FNaniteAuditRecord> Record;
	ECheckBoxState SelectionState;
};

class SNaniteAudit : public SCompoundWidget, public FNotifyHook
{
public:
	enum class AuditMode : uint8
	{
		Errors,
		Optimize,
	};

public:
	SLATE_BEGIN_ARGS(SNaniteAudit)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& Args, AuditMode Mode, SNaniteTools* Parent);
	~SNaniteAudit();

	void PreAudit();
	void PostAudit(TSharedPtr<FNaniteAuditRegistry> AuditRegistry);

	inline uint32 GetRowCount() const
	{
		return NaniteAuditRows.Num();
	}

private:
	// FNotifyHook Interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

private:
	TSharedRef<ITableRow>	OnGenerateRow(TSharedPtr<FNaniteAuditRow> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SWidget>		OnConstructContextMenu();

	void OnShowInContentBrowser();
	void OnEnableNanite();
	void OnDisableNanite();

	FReply OnBatchEnableNanite();
	FReply OnBatchDisableNanite();

	inline const TArray<TSharedPtr<FNaniteAuditRow>>* GetNaniteAuditRows() const { return &NaniteAuditRows; };

private:
	TSharedPtr<class FUICommandList> CommandList;
	TSharedPtr<SListView<TSharedPtr<FNaniteAuditRow>>> NaniteAuditList;
	TArray<TSharedPtr<FNaniteAuditRow>> NaniteAuditRows;
	TSharedPtr<IDetailsView> AuditArgumentsDetailsView = nullptr;

	TStrongObjectPtr<UNaniteAuditErrorArguments> AuditErrorArguments = nullptr;
	TStrongObjectPtr<UNaniteAuditOptimizeArguments> AuditOptimizeArguments = nullptr;

	SNaniteTools* Parent = nullptr;
	AuditMode Mode = AuditMode::Errors;
};
