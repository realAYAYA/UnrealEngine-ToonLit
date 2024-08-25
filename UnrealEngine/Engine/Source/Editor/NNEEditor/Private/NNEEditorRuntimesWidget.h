// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::NNEEditor::Private
{

	class SRuntimesWidget : public SCompoundWidget
	{

	public:
		DECLARE_DELEGATE_OneParam(FOnTargetRuntimesChanged, TArrayView<const FString> /*NewTargetRuntimes*/)

		SLATE_BEGIN_ARGS(SRuntimesWidget)
			{}
			SLATE_ATTRIBUTE(TArrayView<const FString>, TargetRuntimes)
			SLATE_EVENT(FOnTargetRuntimesChanged, OnTargetRuntimesChanged)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:
		void OnEntryChanged(ECheckBoxState CheckType, int32 Index);
		ECheckBoxState IsEntryChecked(int32 Index) const;

		void InitCheckBoxNames();

		TArray<FString> CheckBoxNames;
		TAttribute<TArrayView<const FString>> TargetRuntimes;
		FOnTargetRuntimesChanged OnTargetRuntimesChanged;
	};

} // UE::NNEEditor::Private