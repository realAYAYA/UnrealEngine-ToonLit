// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Misc/Guid.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeEditorPropertyBindings.generated.h"

/**
 * Editor representation of a property binding in StateTree
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeEditorPropertyBinding
{
	GENERATED_BODY()

	FStateTreeEditorPropertyBinding() = default;
	FStateTreeEditorPropertyBinding(const FStateTreeEditorPropertyPath& InSourcePath, const FStateTreeEditorPropertyPath& InTargetPath) : SourcePath(InSourcePath), TargetPath(InTargetPath) {}

	bool IsValid() const { return SourcePath.IsValid() && TargetPath.IsValid(); }

	/** Source property path of the binding */
	UPROPERTY()
	FStateTreeEditorPropertyPath SourcePath;

	/** Target property path of the binding */
	UPROPERTY()
	FStateTreeEditorPropertyPath TargetPath;
};


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
	void AddPropertyBinding(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath);
	
	/**
	 * Removes all bindings to target path.
	 * @param TargetPath Target property path.
	 */ 
	void RemovePropertyBindings(const FStateTreeEditorPropertyPath& TargetPath);
	
	/**
	 * @param TargetPath Target property path.
	 * @return True of the target path has any bindings.
	 */
	bool HasPropertyBinding(const FStateTreeEditorPropertyPath& TargetPath) const;
	
	/**
	 * @return Source path for given target path, or null if binding does not exists.
	 */
	const FStateTreeEditorPropertyPath* GetPropertyBindingSource(const FStateTreeEditorPropertyPath& TargetPath) const;
	
	/**
	 * Returns all bindings for a specified structs based in struct ID.
	 * @param StructID ID of the struct to find bindings for.
	 * @param OutBindings Bindings for specified struct.
	 */
	void GetPropertyBindingsFor(const FGuid StructID, TArray<FStateTreeEditorPropertyBinding>& OutBindings) const;
	
	/**
	 * Removes bindings which do not point to valid structs IDs.
	 * @param ValidStructs Set of struct IDs that are currently valid.
	 */
	void RemoveUnusedBindings(const TMap<FGuid, const UStruct*>& ValidStructs);

	/** @return array view to all bindings. */
	TConstArrayView<FStateTreeEditorPropertyBinding> GetBindings() const { return PropertyBindings; }

protected:

	UPROPERTY()
	TArray<FStateTreeEditorPropertyBinding> PropertyBindings;
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

	virtual const FStateTreeEditorPropertyPath* GetPropertyBindingSource(const FStateTreeEditorPropertyPath& InTargetPath) const override;
	virtual FText GetPropertyPathDisplayName(const FStateTreeEditorPropertyPath& InTargetPath) const override;
	virtual const FProperty* GetPropertyPathLeafProperty(const FStateTreeEditorPropertyPath& InPath) const override;

	IStateTreeEditorPropertyBindingsOwner* BindingOwner = nullptr;
};
