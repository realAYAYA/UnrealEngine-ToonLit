// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorCommon.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SExpanderArrow.h"

class UNiagaraStackFunctionInputCollectionBase;
class SWrapBox;

class SNiagaraStackFunctionInputCollection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackFunctionInputCollection) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackFunctionInputCollectionBase* InInputCollection);

private:
	EVisibility GetLabelVisibility() const;

	void ConstructSectionButtons();

	void InputCollectionStructureChanged(ENiagaraStructureChangedFlags StructureChangedFlags);

	ECheckBoxState GetSectionCheckState(FText Section) const;

	void OnSectionChecked(ECheckBoxState CheckState, FText Section);

private:
	UNiagaraStackFunctionInputCollectionBase* InputCollection;

	TSharedPtr<SWrapBox> SectionSelectorBox;
};
