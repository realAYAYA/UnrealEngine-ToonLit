// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinitionReferenceDetails.h"

#include "SmartObjectDefinition.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyBagDetails.h"
#include "PropertyCustomizationHelpers.h"
#include "SmartObjectDefinitionReference.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

class FSmartObjectDefinitionRefParametersDetails : public FPropertyBagInstanceDataDetails
{
public:
	FSmartObjectDefinitionRefParametersDetails(const TSharedPtr<IPropertyHandle> InSmartObjectDefinitionRefStructProperty, const TSharedPtr<IPropertyHandle> InParametersStructProperty, const TSharedPtr<IPropertyUtilities>& InPropUtils)
		: FPropertyBagInstanceDataDetails(InParametersStructProperty, InPropUtils, /*bInFixedLayout*/true)
		, SmartObjectDefinitionRefStructProperty(InSmartObjectDefinitionRefStructProperty)
	{
	}

protected:
	struct FSmartObjectDefinitionReferenceOverrideProvider : public IPropertyBagOverrideProvider
	{
		FSmartObjectDefinitionReferenceOverrideProvider(FSmartObjectDefinitionReference& InSmartObjectDefinitionRef)
			: SmartObjectDefinitionRef(InSmartObjectDefinitionRef)
		{
		}
		
		virtual bool IsPropertyOverridden(const FGuid PropertyID) const override
		{
			return SmartObjectDefinitionRef.IsPropertyOverridden(PropertyID);
		}
		
		virtual void SetPropertyOverride(const FGuid PropertyID, const bool bIsOverridden) const override
		{
			SmartObjectDefinitionRef.SetPropertyOverridden(PropertyID, bIsOverridden);
		}

	private:
		FSmartObjectDefinitionReference& SmartObjectDefinitionRef;
	};

	virtual bool HasPropertyOverrides() const override
	{
		return true;
	}

	virtual void PreChangeOverrides() override
	{
		check(SmartObjectDefinitionRefStructProperty);
		SmartObjectDefinitionRefStructProperty->NotifyPreChange();
	}

	virtual void PostChangeOverrides() override
	{
		check(SmartObjectDefinitionRefStructProperty);
		SmartObjectDefinitionRefStructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		SmartObjectDefinitionRefStructProperty->NotifyFinishedChangingProperties();
	}

	virtual void EnumeratePropertyBags(TSharedPtr<IPropertyHandle> PropertyBagHandle, const EnumeratePropertyBagFuncRef& Func) const override
	{
		check(SmartObjectDefinitionRefStructProperty);
		SmartObjectDefinitionRefStructProperty->EnumerateRawData([Func](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FSmartObjectDefinitionReference* SmartObjectDefinitionRef = static_cast<FSmartObjectDefinitionReference*>(RawData))
			{
				if (const USmartObjectDefinition* Definition = SmartObjectDefinitionRef->GetSmartObjectDefinition())
				{
					const FInstancedPropertyBag& DefaultParameters = Definition->GetDefaultParameters();
					FInstancedPropertyBag& Parameters = SmartObjectDefinitionRef->GetMutableParameters();
					FSmartObjectDefinitionReferenceOverrideProvider OverrideProvider(*SmartObjectDefinitionRef);
					if (!Func(DefaultParameters, Parameters, OverrideProvider))
					{
						return false;
					}
				}
			}
			return true;
		});
	}

private:
	
	TSharedPtr<IPropertyHandle> SmartObjectDefinitionRefStructProperty;
};


TSharedRef<IPropertyTypeCustomization> FSmartObjectDefinitionReferenceDetails::MakeInstance()
{
	return MakeShareable(new FSmartObjectDefinitionReferenceDetails);
}

void FSmartObjectDefinitionReferenceDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	PropUtils = InCustomizationUtils.GetPropertyUtilities();

	ParametersHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSmartObjectDefinitionReference, Parameters));
	check(ParametersHandle);
	
	// Make sure parameters are synced when displaying SmartObjectDefinitionReference.
	// Associated StateTree asset might have been recompiled after the SmartObjectDefinitionReference was loaded.
	// Note: SyncParameters() will create an undoable transaction, do not try to sync when called during Undo/redo as it would overwrite the undo.   
	if (!GIsTransacting)
	{
		SyncParameters();
	}

	const TSharedPtr<IPropertyHandle> SmartObjectDefinitionProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSmartObjectDefinitionReference, SmartObjectDefinition));
	check(SmartObjectDefinitionProperty.IsValid());

	InHeaderRow
	.NameContent()
	[
		SmartObjectDefinitionProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SObjectPropertyEntryBox)
		.PropertyHandle(SmartObjectDefinitionProperty)
		.AllowedClass(USmartObjectDefinition::StaticClass())
		.ThumbnailPool(InCustomizationUtils.GetThumbnailPool())
	]
	.ShouldAutoExpand(true);
	
	SmartObjectDefinitionProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]()
		{
			SyncParameters();
		}));

	// Registers a delegate to be notified when the associated SmartObject definition parameters change
	// to make sure that the parameters in the SmartObjectDefinitionReference are still valid.
	UE::SmartObject::Delegates::OnParametersChanged.AddSP(this, &FSmartObjectDefinitionReferenceDetails::OnParametersChanged);
}

void FSmartObjectDefinitionReferenceDetails::CustomizeChildren(const TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildrenBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// Show parameter values.
	const TSharedRef<FSmartObjectDefinitionRefParametersDetails> ParameterInstanceDetails = MakeShareable(new FSmartObjectDefinitionRefParametersDetails(StructPropertyHandle, ParametersHandle, PropUtils));
	InChildrenBuilder.AddCustomBuilder(ParameterInstanceDetails);
}

void FSmartObjectDefinitionReferenceDetails::OnParametersChanged(const USmartObjectDefinition& SmartObjectDefinition) const
{
	SyncParameters(&SmartObjectDefinition);
}

void FSmartObjectDefinitionReferenceDetails::SyncParameters(const USmartObjectDefinition* DefinitionToSync) const
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
		if (FSmartObjectDefinitionReference* SmartObjectDefinitionReference = static_cast<FSmartObjectDefinitionReference*>(RawData[i]))
		{
			const USmartObjectDefinition* Definition = SmartObjectDefinitionReference->GetSmartObjectDefinition();

			if ((DefinitionToSync == nullptr || Definition == DefinitionToSync)
				&& SmartObjectDefinitionReference->RequiresParametersSync())
			{
				// Changing the data without Modify().
				// When called on property row creation, we don't expect to dirty the owner.
				// In other cases we expect the outer to already been modified.
				SmartObjectDefinitionReference->SyncParameters();
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
