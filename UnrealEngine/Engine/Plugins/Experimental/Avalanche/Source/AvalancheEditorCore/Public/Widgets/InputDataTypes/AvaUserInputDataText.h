// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/InputDataTypes/AvaUserInputDataTypeBase.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"

namespace ETextCommit { enum Type : int; }

struct AVALANCHEEDITORCORE_API FAvaUserInputTextData : public FAvaUserInputDataTypeBase
{
	FAvaUserInputTextData(const FText& InValue, bool bInAllowMultiline = false, TOptional<int32> InMaxLength = TOptional<int32>());

	virtual ~FAvaUserInputTextData() override = default;

	const FText& GetValue() const;

	//~ Begin FAvaUserInputDataTypeBase
	virtual TSharedRef<SWidget> CreateInputWidget();
	//~ End FAvaUserInputDataTypeBase

protected:
	FText Value;
	bool bAllowMultiline;
	TOptional<int32> MaxLength;

	void OnTextChanged(const FText& InValue);

	void OnTextCommitted(const FText& InValue, ETextCommit::Type InCommitType);

	bool OnTextVerify(const FText& InValue, FText& OutErrorText);
};