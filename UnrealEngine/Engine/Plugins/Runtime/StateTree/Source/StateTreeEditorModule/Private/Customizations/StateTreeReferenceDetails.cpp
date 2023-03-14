// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeReferenceDetails.h"

#include "StateTree.h"
#include "StateTreeDelegates.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TSharedRef<IPropertyTypeCustomization> FStateTreeReferenceDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeReferenceDetails);
}

void FStateTreeReferenceDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	// Make sure parameters are synced when displaying StateTreeReference.
	// Associated StateTree asset might have been recompiled after the StateTreeReference was loaded.
	SyncParameters();

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
			TArray<void*> RawData;
			StructPropertyHandle->AccessRawData(RawData);

			const TSharedPtr<IPropertyHandle> ParametersProperty = StructPropertyHandle->GetChildHandle(FName(TEXT("Parameters")));

			if (ParametersProperty.IsValid() && !RawData.IsEmpty())
			{
				FStateTreeReference* StateTreeReference = static_cast<FStateTreeReference*>(RawData[0]);
				check(StateTreeReference);

				const UStateTree* StateTreeAsset = StateTreeReference->GetStateTree();
				
				FInstancedPropertyBag NewValue;
				bool bSetNewValue = true;

				if (RawData.Num() == 1)
				{
					bSetNewValue = StateTreeReference->RequiresParametersSync();

					// When only one element is selected we can migrate the Parameters in case we are able to preserve some matching parameters
					if (bSetNewValue)
					{
						StateTreeReference->SyncParametersToMatchStateTree(NewValue);
					}
				}
				else if (StateTreeAsset)
				{
					// In case where multiple elements are selected we use the default parameters from the StateTree since
					// the property handle will set value on all elements at once. In this scenario we can't "migrate" parameters since
					// they might be associated to different StateTree so the resulting parameters would have been different for each element. 
					NewValue = StateTreeAsset->GetDefaultParameters();
				}

				// Set new value through property handle to propagate changes to loaded instances
				if (bSetNewValue)
				{
					const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(ParametersProperty->GetProperty());

					FString TextValue;
					StructProperty->Struct->ExportText(TextValue, &NewValue, /*Defaults*/nullptr, /*OwnerObject*/nullptr, EPropertyPortFlags::PPF_None, /*ExportRootScope*/nullptr);
					ensure(ParametersProperty->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
				}
			}
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

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	for (int32 i = 0; i < RawData.Num(); i++)
	{
		if (FStateTreeReference* StateTreeReference = static_cast<FStateTreeReference*>(RawData[i]))
		{
			const UStateTree* StateTreeAsset = StateTreeReference->GetStateTree();

			if (StateTreeAsset
				&& (StateTreeToSync == nullptr || StateTreeAsset == StateTreeToSync) 
				&& StateTreeReference->RequiresParametersSync())
			{
				StateTreeReference->SyncParameters();

				// StateTree compilation is not a Transaction so we simply dirty outer object
				if (OuterObjects.Num() == RawData.Num())
				{
					if (const UObject* Outer = OuterObjects[i])
					{
						Outer->MarkPackageDirty();
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
