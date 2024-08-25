// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"

class SNiagaraSimTargetToggle: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimTargetToggle) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);

private:
	void ToggleSimTarget() const;
	FReply ToggleSimTargetInUI() const;

	const FSlateBrush* GetSimTargetImage() const;
	FText GetSimTargetToggleTooltip() const;

private:
	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;
};
