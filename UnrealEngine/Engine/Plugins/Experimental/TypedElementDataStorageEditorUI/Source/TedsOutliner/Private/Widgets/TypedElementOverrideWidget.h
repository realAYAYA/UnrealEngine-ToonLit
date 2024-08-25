// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "UObject/OverridableManager.h"
#include "Widgets/TypedElementTypeInfoWidget.h"

#include "TypedElementOverrideWidget.generated.h"

/*
 * Widget for the Outliner that shows the icon showing the type of the object alongside the override status as a badg
 */
UCLASS()
class UTypedElementOverrideWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementOverrideWidgetFactory() override = default;

	TEDSOUTLINER_API void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;

	TEDSOUTLINER_API void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

};

USTRUCT(meta = (DisplayName = "Override Widget"))
struct FTypedElementOverrideWidgetTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FTypedElementOverrideWidgetConstructor : public FTypedElementTypeInfoWidgetConstructor
{
	GENERATED_BODY()

public:
	FTypedElementOverrideWidgetConstructor();
	~FTypedElementOverrideWidgetConstructor() override = default;

	virtual TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;
	
	static void AddOverrideBadge(const TWeakPtr<SWidget>& Widget, EOverriddenState OverriddenState);
	static void RemoveOverrideBadge(const TWeakPtr<SWidget>& Widget);

	static void UpdateOverrideWidget(const TWeakPtr<SWidget>& Widget, const TypedElementRowHandle TargetRow);

protected:
	TEDSOUTLINER_API TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	TEDSOUTLINER_API bool FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};