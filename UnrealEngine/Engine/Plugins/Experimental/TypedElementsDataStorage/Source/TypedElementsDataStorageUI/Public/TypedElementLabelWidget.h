// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementLabelWidget.generated.h"

UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementLabelWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementLabelWidgetFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const override;
	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementLabelWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementLabelWidgetConstructor();
	~FTypedElementLabelWidgetConstructor() override = default;

	TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;
	bool CanBeReused() const override;

protected:
	explicit FTypedElementLabelWidgetConstructor(const UScriptStruct* InTypeInfo);
	TSharedPtr<SWidget> CreateWidget() override;
	bool SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementLabelWithHashTooltipWidgetConstructor : public FTypedElementLabelWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementLabelWithHashTooltipWidgetConstructor();
	~FTypedElementLabelWithHashTooltipWidgetConstructor() = default;

protected:
	bool SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT(meta = (DisplayName = "Label widget"))
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementLabelWidgetColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	bool bShowHashInTooltip{ false };
};