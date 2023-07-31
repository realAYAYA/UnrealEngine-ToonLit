// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "Input/Reply.h"
#include "NiagaraStackErrorItem.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackErrorItem : public UNiagaraStackEntry
{
	GENERATED_BODY()
public:
	DECLARE_MULTICAST_DELEGATE(FOnIssueNotify);
public:
	void Initialize(FRequiredEntryData InRequiredEntryData, FStackIssue InStackIssue, FString InStackEditorDataKey);
	FStackIssue GetStackIssue() const { return StackIssue; }
	void SetStackIssue(const FStackIssue& InStackIssue);
	virtual FText GetDisplayName() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	FOnIssueNotify& OnIssueModified();
	virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;
	virtual EStackIssueSeverity GetIssueSeverity() const override;
	virtual bool IsExpandedByDefault() const override;
	virtual void SetIsExpandedByDefault(bool bIsExpanded);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

protected:
	FStackIssue StackIssue;
	FString EntryStackEditorDataKey;
	FOnIssueNotify IssueModifiedDelegate;
	bool bIsExpandedByDefault = true;

private:
	void IssueFixed();
}; 

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackErrorItemLongDescription : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, FStackIssue InStackIssue, FString InStackEditorDataKey);
	virtual FText GetDisplayName() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual EStackIssueSeverity GetIssueSeverity() const override;

protected:
	FStackIssue StackIssue;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackErrorItemFix : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, FStackIssue InStackIssue, FStackIssueFix InIssueFix, FString InStackEditorDataKey);

	FStackIssueFix GetStackIssueFix() const { return IssueFix; };
	FReply OnTryFixError();
	virtual FText GetDisplayName() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual FText GetFixButtonText() const;
	UNiagaraStackErrorItem::FOnIssueNotify& OnIssueFixed();
	void SetFixDelegate(const FStackIssueFixDelegate& InFixDelegate);
	virtual EStackIssueSeverity GetIssueSeverity() const override;

protected:
	FStackIssue StackIssue;
	FStackIssueFix IssueFix;
	UNiagaraStackErrorItem::FOnIssueNotify IssueFixedDelegate;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackErrorItemDismiss : public UNiagaraStackErrorItemFix
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStackEntry::FStackIssue InStackIssue, FString InStackEditorDataKey);

	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual FText GetFixButtonText() const override;

private:
	void DismissIssue();

};

