// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;
enum class ECheckBoxState : uint8;

class SDMPropertyEditBool : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditBool)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	SDMPropertyEditBool() = default;
	virtual ~SDMPropertyEditBool() override = default;

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle);

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;

	ECheckBoxState IsChecked() const;
	void OnValueToggled(ECheckBoxState InNewState);
};
