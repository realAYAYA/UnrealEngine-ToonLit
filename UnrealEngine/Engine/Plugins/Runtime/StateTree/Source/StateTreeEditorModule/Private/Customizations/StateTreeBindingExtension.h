// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTypes.h"
#include "IDetailPropertyExtensionHandler.h"

class IPropertyHandle;
class IDetailLayoutBuilder;
struct FStateTreeEditorPropertyPath;
class IPropertyAccessEditor;
struct FStateTreeEditorPropertyPath;

namespace UE::StateTree::PropertyBinding
{
	extern const FName StateTreeNodeIDName;

	/**
	 * Get nearest Outer that implements IStateTreeEditorPropertyBindingsOwner.
	 * @param InObject Object where to start the search.
	 */
	UObject* FindEditorBindingsOwner(UObject* InObject);

	/**
	 * Returns property path for a specific property. The property's metadata "StateTreeItemID" is expected to specify the containing struct ID.
	 * @param InPropertyHandle Handle to the property to find path for.
	 * @param OutPath Resulting property path.
	 */
	void GetStructPropertyPath(TSharedPtr<IPropertyHandle> InPropertyHandle, FStateTreeEditorPropertyPath& OutPath);

	/**
	 * Parses property usage from a property handle.
	 * @param InPropertyHandle Handle to the property to find path for.
	 * @return Parsed property usage.
	 */
	EStateTreePropertyUsage ParsePropertyUsage(TSharedPtr<const IPropertyHandle> InPropertyHandle);
	
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateTreeBindingChanged, const FStateTreeEditorPropertyPath& /*SourcePath*/, const FStateTreeEditorPropertyPath& /*TargetPath*/);
	extern STATETREEEDITORMODULE_API FOnStateTreeBindingChanged OnStateTreeBindingChanged;

} // UE::StateTree::PropertyBinding

class FStateTreeBindingExtension : public IDetailPropertyExtensionHandler
{
public:
	// IDetailPropertyExtensionHandler interface
	virtual bool IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const override;
	virtual void ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> PropertyHandle) override;
};
