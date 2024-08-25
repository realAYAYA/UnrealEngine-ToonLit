// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "ISceneOutlinerHierarchy.h"

class ITypedElementDataStorageUiInterface;
class ITypedElementDataStorageCompatibilityInterface;
struct FTypedElementWidgetConstructor;
class SWidget;

// This class is for any TEDS-Outliner related functionality that needs to be re-used between the new 
// FTypedElementOutlinerMode and FTEDSActorOutlinerMode, the latter of which exists for backwards compatibility with
// existing Outliner features while we build up the core tech for TEDS
class TEDSOUTLINER_API FBaseTEDSOutlinerMode
{
public:
	FBaseTEDSOutlinerMode();
	virtual ~FBaseTEDSOutlinerMode();
	TSharedRef<SWidget> CreateLabelWidgetForItem(TypedElementRowHandle InRowHandle);

	ITypedElementDataStorageInterface* GetStorage();
	ITypedElementDataStorageUiInterface* GetStorageUI();
	ITypedElementDataStorageCompatibilityInterface* GetStorageCompatibility();

protected:
	// TEDS Storage Constructs
	ITypedElementDataStorageInterface* Storage{ nullptr };
	ITypedElementDataStorageUiInterface* StorageUi{ nullptr };
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibility{ nullptr };

	TArray<TPair<TypedElementDataStorage::QueryHandle, TSharedPtr<FTypedElementWidgetConstructor>>> QueryToWidgetConstructorMap;
	TArray<FName, TFixedAllocator<3>> WidgetPurposes;
};
