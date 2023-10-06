// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeReferenceDetails.h"

#include "StateTree.h"
#include "StateTreeDelegates.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "StateTreeReference.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TSharedRef<IPropertyTypeCustomization> FStateTreeReferenceDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeReferenceDetails);
}

void FStateTreeReferenceDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	PropUtils = InCustomizationUtils.GetPropertyUtilities();

	// Make sure parameters are synced when displaying StateTreeReference.
	// Associated StateTree asset might have been recompiled after the StateTreeReference was loaded.
	// Note: SyncParameters() will create an undoable transaction, do not try to sync when called during Undo/redo as it would overwrite the undo.   
	if (!GIsTransacting)
	{
		SyncParameters();
	}

	const TSharedPtr<IPropertyHandle> StateTreeProperty = StructPropertyHandle->GetChildHandle(FName(TEXT("StateTree")));
	check(StateTreeProperty.IsValid());

	const FString SchemaMetaDataValue = InStructPropertyHandle->GetMetaData(UE::StateTree::SchemaTag);

	InHeaderRow
	.NameContent()
	[
		StateTreeProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SObjectPropertyEntryBox)
		.PropertyHandle(StateTreeProperty)
		.AllowedClass(UStateTree::StaticClass())
		.ThumbnailPool(InCustomizationUtils.GetThumbnailPool())
		.OnShouldFilterAsset_Lambda([SchemaMetaDataValue](const FAssetData& InAssetData)
		{
			return !SchemaMetaDataValue.IsEmpty() && !InAssetData.TagsAndValues.ContainsKeyValue(UE::StateTree::SchemaTag, SchemaMetaDataValue);
		})
	]
	.ShouldAutoExpand(true);
	
	StateTreeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]()
		{
			SyncParameters();
		}));

	// Registers a delegate to be notified when the associated StateTree asset get successfully recompiled
	// to make sure that the parameters in the StateTreeReference are still valid.
	UE::StateTree::Delegates::OnPostCompile.AddSP(this, &FStateTreeReferenceDetails::OnTreeCompiled);
}

void FStateTreeReferenceDetails::CustomizeChildren(const TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildrenBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	uint32 NumChildren = 0;
	InStructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
	{
		const TSharedRef<IPropertyHandle> ChildPropertyHandle = InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

		// Skip StateTree that was used for the Header row
		if (ChildPropertyHandle->GetProperty()->GetFName() != FName(TEXT("StateTree")))
		{
			InChildrenBuilder.AddProperty(ChildPropertyHandle);
		}
	}
}

void FStateTreeReferenceDetails::OnTreeCompiled(const UStateTree& StateTree) const
{
	SyncParameters(&StateTree);
}

void FStateTreeReferenceDetails::SyncParameters(const UStateTree* StateTreeToSync) const
{
	check(StructPropertyHandle.IsValid());

	// This function will get called in 3 situations:
	// - auto update when the property customization is created
	// - auto update when a state tree is compiled.
	// - when the associate state tree asset is changed
	
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	bool bDidSync = false;
	
	for (int32 i = 0; i < RawData.Num(); i++)
	{
		if (FStateTreeReference* StateTreeReference = static_cast<FStateTreeReference*>(RawData[i]))
		{
			const UStateTree* StateTreeAsset = StateTreeReference->GetStateTree();

			if ((StateTreeToSync == nullptr || StateTreeAsset == StateTreeToSync) 
				&& StateTreeReference->RequiresParametersSync())
			{
				// Changing the data without Modify().
				// When called on property row creation, we don't expect to dirty the owner.
				// In other cases we expect the outer to already been modified.
				StateTreeReference->SyncParameters();
				bDidSync = true;
			}
		}
	}

	if (bDidSync && PropUtils)
	{
		PropUtils->RequestRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
