// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UDataprepBoolFilter;

class SDataprepBoolFilter : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDataprepBoolFilter) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UDataprepBoolFilter& InFilter);
};
