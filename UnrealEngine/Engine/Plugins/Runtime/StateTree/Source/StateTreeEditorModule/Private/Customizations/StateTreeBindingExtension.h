// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailPropertyExtensionHandler.h"

enum class EStateTreePropertyUsage : uint8;

class IPropertyHandle;
class IDetailLayoutBuilder;
class IPropertyAccessEditor;
struct FStateTreeEditorPropertyPath;
struct FStateTreePropertyPath;

namespace UE::StateTree::PropertyBinding
{
	extern const FName StateTreeNodeIDName;

	/**
	 * Get nearest Outer that implements IStateTreeEditorPropertyBindingsOwner.
	 * @param InObject Object where to start the search.
	 */
	UObject* FindEditorBindingsOwner(UObject* InObject);

	/**
	 * Returns property path for a specific property.
	 * Walks towards root up until a property with metadata  "StateTreeItemID" is found. 
	 * The property's metadata "StateTreeItemID" is expected to specify the containing struct ID.
	 * @param InPropertyHandle Handle to the property to find path for.
	 * @param OutPath Resulting property path.
	 * @return usage type extracted from the root-most property.
	 */
	EStateTreePropertyUsage MakeStructPropertyPathFromPropertyHandle(TSharedPtr<const IPropertyHandle> InPropertyHandle, FStateTreePropertyPath& OutPath);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateTreePropertyBindingChanged, const FStateTreePropertyPath& /*SourcePath*/, const FStateTreePropertyPath& /*TargetPath*/);
	extern STATETREEEDITORMODULE_API FOnStateTreePropertyBindingChanged OnStateTreePropertyBindingChanged;
} // UE::StateTree::PropertyBinding

class FStateTreeBindingExtension : public IDetailPropertyExtensionHandler
{
public:
	// IDetailPropertyExtensionHandler interface
	virtual bool IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const override;
	virtual void ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> PropertyHandle) override;
};
