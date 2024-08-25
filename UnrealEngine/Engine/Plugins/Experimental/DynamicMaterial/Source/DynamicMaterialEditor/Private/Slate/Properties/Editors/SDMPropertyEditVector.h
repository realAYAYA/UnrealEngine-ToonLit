// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;

class SDMPropertyEditVector : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditVector)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	SDMPropertyEditVector() = default;
	virtual ~SDMPropertyEditVector() override = default;

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle, int32 InComponentCount);

protected:
	int32 ComponentCount;

	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;
	virtual float GetMaxWidthForWidget(int32 InIndex) const override;

	float GetVectorValue(int32 InComponent) const;
	virtual void OnValueChanged(float InNewValue, int32 InComponent) const;
};
