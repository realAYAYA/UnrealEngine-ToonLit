// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementPackagePathWidget.generated.h"

UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementPackagePathWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementPackagePathWidgetFactory() override = default;

	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementPackagePathWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementPackagePathWidgetConstructor();
	~FTypedElementPackagePathWidgetConstructor() override = default;

protected:
	explicit FTypedElementPackagePathWidgetConstructor(const UScriptStruct* InTypeInfo);

	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementLoadedPackagePathWidgetConstructor : public FTypedElementPackagePathWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementLoadedPackagePathWidgetConstructor();
	~FTypedElementLoadedPackagePathWidgetConstructor() override = default;

protected:
	bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};
