// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementAlertQueries.generated.h"

namespace TypedElementDataStorage
{
	struct IQueryContext;
}
struct FTypedElementAlertColumn;
struct FTypedElementChildAlertColumn;
struct FTypedElementParentColumn;

/**
 * Calls to manage alerts, in particular child alerts.
 */
UCLASS()
class UTypedElementAlertQueriesFactory final : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementAlertQueriesFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterSubQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterUpdateAlertsQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterOnAddQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterOnAlertRemoveQueries(ITypedElementDataStorageInterface& DataStorage);
	void RegisterOnParentRemoveQueries(ITypedElementDataStorageInterface& DataStorage);

	template<bool bHasAlert, bool bHasChildAlert>
	static void Update(TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row,
		FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert, const FTypedElementParentColumn& Parent,
		int32 ChildAlertColumnReadWriteSubquery, int32 ParentReadOnlySubquery);
	
	template<bool bHasAlert, bool bHasChildAlert>
	static void NeedsUpdating(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
		const FTypedElementParentColumn& Parent, bool& bAlertNeedsUpdate, bool& bChildAlertsNeedUpdate);
	
	template<bool bHasAlert, bool bHasChildAlert>
	static void NeedsDecrementing(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
		const FTypedElementParentColumn& Parent, bool& bAlertNeedsDecrementing, bool& bChildAlertsNeedDecrementing);
	
	template<bool bHasAlert, bool bHasChildAlert>
	static TypedElementDataStorage::RowHandle GetParent(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert);
	
	template<bool bHasAlert, bool bHasChildAlert>
	static void GetTotalCounts(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
		uint16& ErrorCount, uint16& WarningCount, bool IncludeAlert, bool IncludeChildAlerts);
	
	template<bool bHasAlert, bool bHasChildAlert>
	static void MarkDecremented(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert);
	
	static void GuaranteeUpdateReentry(TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row);
	
	template<bool bHasAlert, bool bHasChildAlert>
	static void UpdateParent(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
		TypedElementDataStorage::RowHandle NewParent);

	static void AddChildAlertsToHierarchy(
		TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Parent, int32 ParentQueryIndex);

	static void IncrementParents(
		TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle ParentRow,
		uint16 ErrorIncrement, uint16 WarningIncrement,
		int32 ChildAlertQueryIndex);
	static void DecrementParents(
		TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle ParentRow,
		uint16 ErrorDecrement, uint16 WarningDecrement,
		int32 ChildAlertQueryIndex);

	// Find the next parent to the current parent. If there's no parent this function return false. If there is a parent, it will return
	// true and Parent will be updated to the new parent.
	static bool MoveToNextParent(
		TypedElementDataStorage::RowHandle& Parent, TypedElementDataStorage::IQueryContext& Context, int32 SubQueryIndex);

	TypedElementDataStorage::QueryHandle ChildAlertColumnReadWriteQuery;
	TypedElementDataStorage::QueryHandle ParentReadOnlyQuery;
};

/**
 * Tag used during the update cycle as this can take between 1 and 3 cycles to complete.
 */
USTRUCT(meta = (DisplayName = "Alert update"))
struct FTypedElementAlertUpdateTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

/**
 * Enable AlertColumn to be mementoized
 */
UCLASS()
class UTypedElementAlertColumnMementoTranslator final : public UTypedElementDefaultMementoTranslator
{
	GENERATED_BODY()
public:
	const UScriptStruct* GetColumnType() const override;
};
