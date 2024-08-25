// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementExportedTextWidget.generated.h"

UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementExportedTextWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementExportedTextWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;

	TSet<TWeakObjectPtr<const UScriptStruct>> RegisteredTypes;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementExportedTextWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementExportedTextWidgetConstructor();
	~FTypedElementExportedTextWidgetConstructor() override = default;

	TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row,
		const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT(meta = (DisplayName = "Exported text widget"))
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementExportedTextWidgetTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};