// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FString;
class IPropertyHandle;
class SWidget;
class UClass;
struct FAssetData;

class SDMPropertyEditObject : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditObject)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	SDMPropertyEditObject() = default;
	virtual ~SDMPropertyEditObject() override = default;

	void Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle, UClass* InAllowedClass);

protected:
	TWeakObjectPtr<UClass> AllowedClass;

	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;

	FString GetObjectPath() const;
	void OnValueChanged(const FAssetData& InAssetData);
};
