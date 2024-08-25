// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMaterialItemView;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UPrimitiveComponent;
class UDynamicMaterialInstance;

class SDMMaterialListExtensionWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMMaterialListExtensionWidget) {}
	SLATE_END_ARGS()

	virtual ~SDMMaterialListExtensionWidget() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<FMaterialItemView>& InMaterialItemView, UPrimitiveComponent* InCurrentComponent,
		IDetailLayoutBuilder& InDetailBuilder);

protected:
	TWeakPtr<FMaterialItemView> MaterialItemViewWeak;
	TWeakObjectPtr<UPrimitiveComponent> CurrentComponentWeak;

	UObject* GetAsset() const;
	void SetAsset(UObject* NewAsset);

	UDynamicMaterialInstance* GetDynamicMaterialInstance() const;
	void SetDynamicMaterialInstance(UDynamicMaterialInstance* NewInstance);

	FReply OnButtonClicked();
	FReply CreateDynamicMaterialInstance();
	FReply ClearDynamicMaterialInstance();
	FReply OpenDynamicMaterialInstanceTab();

	FText GetButtonText() const;
};
