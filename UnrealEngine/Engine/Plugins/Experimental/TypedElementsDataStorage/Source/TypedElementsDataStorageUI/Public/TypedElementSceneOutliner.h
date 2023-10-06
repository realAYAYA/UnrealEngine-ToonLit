// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSceneOutliner.generated.h"

class ISceneOutliner;
class ITypedElementDataStorageInterface;
class ITypedElementDataStorageUiInterface;
class ITypedElementDataStorageCompatibilityInterface;

class FTypedElementSceneOutliner
{
public:
	~FTypedElementSceneOutliner();

	void Initialize(
		ITypedElementDataStorageInterface& InStorage,
		ITypedElementDataStorageUiInterface& InStorageUi,
		ITypedElementDataStorageCompatibilityInterface& InStorageCompatibility,
		const TSharedPtr<ISceneOutliner>& InOutliner);

	void AssignQuery(TypedElementQueryHandle Query);

private:
	static FName FindLongestMatchingName(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes, int32 DefaultNameIndex);
	static TArray<TWeakObjectPtr<const UScriptStruct>> CreateVerifiedColumnTypeAray(
		TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes);
	static TSharedPtr<FTypedElementWidgetConstructor> CreateHeaderWidgetConstructor(
		ITypedElementDataStorageUiInterface& StorageUI, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes);
	void ClearColumns(ISceneOutliner& InOutliner);

	TArray<FName> AddedColumns;
	TWeakPtr<ISceneOutliner> Outliner;
	ITypedElementDataStorageInterface* Storage{ nullptr };
	ITypedElementDataStorageUiInterface* StorageUi{ nullptr };
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibility{ nullptr };
};

/**
 * Utility class to bind Typed Elements Data Storage queries to a Scene Outliner. The provided query is expected to be a select query
 * and will be used to populate the Scene Outliner in addition to already existing data.
 */
class FTypedElementSceneOutlinerQueryBinder
{
public:
	static const FName CellWidgetTableName;
	static const FName HeaderWidgetPurpose;
	static const FName DefaultHeaderWidgetPurpose;
	static const FName CellWidgetPurpose;
	static const FName DefaultCellWidgetPurpose;

	static FTypedElementSceneOutlinerQueryBinder& GetInstance();

	void AssignQuery(TypedElementQueryHandle Query, const TSharedPtr<ISceneOutliner>& Widget);

private:
	FTypedElementSceneOutlinerQueryBinder();

	TMap<TWeakPtr<ISceneOutliner>, FTypedElementSceneOutliner> SceneOutliners;

	ITypedElementDataStorageInterface* Storage{ nullptr };
	ITypedElementDataStorageUiInterface* StorageUi{ nullptr };
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibility{ nullptr };
};

UCLASS()
class UTypedElementSceneOutlinerFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementSceneOutlinerFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) const override;
	
	void RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};
