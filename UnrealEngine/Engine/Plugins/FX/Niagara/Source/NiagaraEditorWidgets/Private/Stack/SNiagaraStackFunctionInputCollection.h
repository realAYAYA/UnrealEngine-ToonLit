// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorCommon.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SExpanderArrow.h"

class UNiagaraStackFunctionInputCollectionBase;
class SWrapBox;
enum class ECheckBoxState : uint8;

class SNiagaraStackFunctionInputCollection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackFunctionInputCollection) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackValueCollection* PropertyCollectionBase);

private:
	EVisibility GetLabelVisibility() const;

	void ConstructSectionButtons();

	void InputCollectionStructureChanged(ENiagaraStructureChangedFlags StructureChangedFlags);

	ECheckBoxState GetSectionCheckState(FText Section) const;

	void OnSectionChecked(ECheckBoxState CheckState, FText Section);

	FText GetTooltipText(FText Section) const;

private:
	UNiagaraStackValueCollection* PropertyCollection;

	TSharedPtr<SWrapBox> SectionSelectorBox;
};
