// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackItemFooter.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackModuleItemOutputCollection;
class UNiagaraNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItemFooter : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnToggleShowAdvanced);

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		FString InOwnerStackItemEditorDataKey);

	virtual bool GetCanExpand() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;

	virtual bool GetIsEnabled() const override;
	void SetIsEnabled(bool bInIsEnabled);

	bool GetHasAdvancedContent() const;
	void SetHasAdvancedContent(bool bHInHasAdvancedRows, bool bHasChangedAdvancedContent);

	void SetOnToggleShowAdvanced(FOnToggleShowAdvanced OnToggleShowAdvanced);

	bool GetShowAdvanced() const;

	void ToggleShowAdvanced();

	bool HasChangedAdvancedContent() const { return bHasChangedAdvancedContent; }

private:
	FString OwnerStackItemEditorDataKey;

	FOnToggleShowAdvanced ToggleShowAdvancedDelegate;

	bool bIsEnabled;

	bool bHasAdvancedContent;

	bool bHasChangedAdvancedContent;
};
