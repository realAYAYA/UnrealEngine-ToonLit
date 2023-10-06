// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeEditorPropertyBindings.generated.h"

/**
 * Editor representation of a all property bindings in a StateTree
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeEditorPropertyBindings
{
	GENERATED_BODY()

	/**
	 * Adds binding between source and destination paths. Removes any bindings to TargetPath before adding the new one.
	 * @param SourcePath Binding source property path.
	 * @param TargetPath Binding target property path.
	 */
	void AddPropertyBinding(const FStateTreePropertyPath& SourcePath, const FStateTreePropertyPath& TargetPath);
	
	/**
	 * Removes all bindings to target path.
	 * @param TargetPath Target property path.
	 */ 
	void RemovePropertyBindings(const FStateTreePropertyPath& TargetPath);
	
	/**
	 * @param TargetPath Target property path.
	 * @return True of the target path has any bindings.
	 */
	bool HasPropertyBinding(const FStateTreePropertyPath& TargetPath) const;

	/**
	 * Copies property bindings from an existing struct to another.
	 * Overrides a binding to a specific property if it already exists in ToStructID.
	 * @param FromStructID ID of the struct to copy from.
	 * @param ToStructID ID of the struct to copy to.
	 */
	void CopyBindings(const FGuid FromStructID, const FGuid ToStructID);
	
	/**
	 * @return Source path for given target path, or null if binding does not exists.
	 */
	const FStateTreePropertyPath* GetPropertyBindingSource(const FStateTreePropertyPath& TargetPath) const;
	
	/**
	 * Returns all bindings for a specified structs based in struct ID.
	 * @param StructID ID of the struct to find bindings for.
	 * @param OutBindings Bindings for specified struct.
	 */
	void GetPropertyBindingsFor(const FGuid StructID, TArray<FStateTreePropertyPathBinding>& OutBindings) const;
	
	/**
	 * Removes bindings which do not point to valid structs IDs.
	 * @param ValidStructs Set of struct IDs that are currently valid.
	 */
	void RemoveUnusedBindings(const TMap<FGuid, const FStateTreeDataView>& ValidStructs);

	/** @return true if any of the bindings references any of the Structs. */
	bool ContainsAnyStruct(const TSet<const UStruct*>& Structs);

	/** @return array view to all bindings. */
	TConstArrayView<FStateTreePropertyPathBinding> GetBindings() const { return PropertyBindings; }

	TArrayView<FStateTreePropertyPathBinding> GetMutableBindings() { return PropertyBindings; }

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	void AddPropertyBinding(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath);

	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	void RemovePropertyBindings(const FStateTreeEditorPropertyPath& TargetPath);

	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	bool HasPropertyBinding(const FStateTreeEditorPropertyPath& TargetPath) const;

	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	const FStateTreeEditorPropertyPath* GetPropertyBindingSource(const FStateTreeEditorPropertyPath& TargetPath) const;

	UE_DEPRECATED(5.3, "Use RemoveUnusedBindings with values instead.")
	void RemoveUnusedBindings(const TMap<FGuid, const UStruct*>& ValidStructs);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
private:

	UPROPERTY()
	TArray<FStateTreePropertyPathBinding> PropertyBindings;
};


UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UStateTreeEditorPropertyBindingsOwner : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class STATETREEEDITORMODULE_API IStateTreeEditorPropertyBindingsOwner
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * Returns structs within the owner that are visible for target struct.
	 * @param TargetStructID Target struct ID
	 * @param OutStructDescs Result descriptors of the visible structs.
	 */
	virtual void GetAccessibleStructs(const FGuid TargetStructID, TArray<FStateTreeBindableStructDesc>& OutStructDescs) const PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::GetAccessibleStructs, return; );

	/**
	 * Returns struct descriptor based on struct ID.
	 * @param StructID Target struct ID
	 * @param OutStructDesc Result descriptor.
	 * @return True if struct found.
	 */
	virtual bool GetStructByID(const FGuid StructID, FStateTreeBindableStructDesc& OutStructDesc) const PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::GetStructByID, return false; );

	/**
	 * Returns data view based on struct ID.
	 * @param StructID Target struct ID
	 * @param OutDataView Result data view.
	 * @return True if struct found.
	 */
	virtual bool GetDataViewByID(const FGuid StructID, FStateTreeDataView& OutDataView) const PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::GetDataViewByID, return false; );

	/** @return Pointer to editor property bindings. */
	virtual FStateTreeEditorPropertyBindings* GetPropertyEditorBindings() PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::GetPropertyEditorBindings, return nullptr; );
};

// TODO: We should merge this with IStateTreeEditorPropertyBindingsOwner and FStateTreeEditorPropertyBindings.
// Currently FStateTreeEditorPropertyBindings is meant to be used as a member for just to store things,
// IStateTreeEditorPropertyBindingsOwner is meant return model specific stuff,
// and IStateTreeBindingLookup is used in non-editor code and it cannot be in FStateTreeEditorPropertyBindings because bindings don't know about the owner.
struct STATETREEEDITORMODULE_API FStateTreeBindingLookup : public IStateTreeBindingLookup
{
	FStateTreeBindingLookup(IStateTreeEditorPropertyBindingsOwner* InBindingOwner);

	IStateTreeEditorPropertyBindingsOwner* BindingOwner = nullptr;

protected:
	virtual const FStateTreePropertyPath* GetPropertyBindingSource(const FStateTreePropertyPath& InTargetPath) const override;
	virtual FText GetPropertyPathDisplayName(const FStateTreePropertyPath& InTargetPath) const override;
	virtual const FProperty* GetPropertyPathLeafProperty(const FStateTreePropertyPath& InPath) const override;

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
