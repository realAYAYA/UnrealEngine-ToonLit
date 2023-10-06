// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UNiagaraStackEntry;

class SNiagaraStackInheritanceIcon : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackInheritanceIcon) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UNiagaraStackEntry* InStackEntry);

private:
	EVisibility IsVisible() const;

private:
	UNiagaraStackEntry* StackEntry;
};