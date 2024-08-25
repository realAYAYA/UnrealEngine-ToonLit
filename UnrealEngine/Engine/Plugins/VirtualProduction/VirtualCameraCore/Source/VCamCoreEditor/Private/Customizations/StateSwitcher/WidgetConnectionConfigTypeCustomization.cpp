// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetConnectionConfigTypeCustomization.h"

#include "BaseWidgetBlueprint.h"
#include "ConnectionTargetNodeBuilder.h"
#include "Customizations/StateSwitcher/SStringSelectionComboBox.h"
#include "LogVCamEditor.h"
#include "UI/Switcher/VCamStateSwitcherWidget.h"
#include "UI/Switcher/WidgetConnectionConfig.h"
#include "UI/VCamWidget.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/PropertyIterator.h"
#include "Util/WidgetTreeUtils.h"

#define LOCTEXT_NAMESPACE "FWidgetConnectionConfigCustomization"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IPropertyTypeCustomization> FWidgetConnectionConfigTypeCustomization::MakeInstance()
	{
		return MakeShared<FWidgetConnectionConfigTypeCustomization>();
	}

	void FWidgetConnectionConfigTypeCustomization::CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		HeaderRow.
			NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				PropertyHandle->CreatePropertyValueWidget()
			];
	}

	void FWidgetConnectionConfigTypeCustomization::CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		// Retrieve structure's child properties
		uint32 NumChildren;
		StructPropertyHandle->GetNumChildren( NumChildren );	
		const FName WidgetProperty = GET_MEMBER_NAME_CHECKED(FWidgetConnectionConfig, Widget);
		const FName ConnectionTargetsProperty = GET_MEMBER_NAME_CHECKED(FWidgetConnectionConfig, ConnectionTargets);

		TSharedPtr<IPropertyHandle> WidgetReferencePropertyHandle;
		TSharedPtr<IPropertyHandle> ConnectionTargetsPropertyHandle;
		
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			TSharedPtr<IPropertyHandle> ChildProperty = StructPropertyHandle->GetChildHandle(ChildIndex);
			if (ChildProperty->GetProperty() && ChildProperty->GetProperty()->GetFName() == WidgetProperty)
			{
				WidgetReferencePropertyHandle = ChildProperty;
			}
			else if (ChildProperty->GetProperty() && ChildProperty->GetProperty()->GetFName() == ConnectionTargetsProperty)
			{
				ConnectionTargetsPropertyHandle = ChildProperty;
			}
		}
		
		ChildBuilder.AddProperty(WidgetReferencePropertyHandle.ToSharedRef());
		CustomizeConnectionTargetsReferenceProperty(StructPropertyHandle, ConnectionTargetsPropertyHandle.ToSharedRef(), ChildBuilder, CustomizationUtils);
	}

	void FWidgetConnectionConfigTypeCustomization::CustomizeConnectionTargetsReferenceProperty(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		TSharedRef<IPropertyHandle> ConnectionTargetsPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils
		) const
	{
		ConnectionTargetsPropertyHandle->MarkHiddenByCustomization();
		const TSharedRef<FConnectionTargetNodeBuilder> CustomBuilder = MakeShared<FConnectionTargetNodeBuilder>(
			ConnectionTargetsPropertyHandle,
			CreateGetConnectionsFromChildWidgetAttribute(StructPropertyHandle, CustomizationUtils),
			CustomizationUtils
			);
		CustomBuilder->InitDelegates();
		ChildBuilder.AddCustomBuilder(CustomBuilder);
	}

	TAttribute<TArray<FName>> FWidgetConnectionConfigTypeCustomization::CreateGetConnectionsFromChildWidgetAttribute(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IPropertyTypeCustomizationUtils& CustomizationUtils
		) const
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = CustomizationUtils.GetPropertyUtilities()->GetSelectedObjects();
		if (SelectedObjects.Num() != 1 || !SelectedObjects[0].IsValid())
		{
			return TArray<FName>{};
		}
		
		UObject* EditedObject = SelectedObjects[0].Get();
		UVCamStateSwitcherWidget* StateSwitcherWidget = Cast<UVCamStateSwitcherWidget>(EditedObject);
		if (!StateSwitcherWidget)
		{
			UE_LOG(LogVCamEditor, Error, TEXT("FWidgetConnectionConfig was expected to be within an UVCamStateSwitcherWidget object!"));
			return TArray<FName>{};
		}

		return TAttribute<TArray<FName>>::CreateLambda([WeakStateSwitcher = TWeakObjectPtr<UVCamStateSwitcherWidget>(StateSwitcherWidget), StructPropertyHandle]() -> TArray<FName>
		{
			if (!WeakStateSwitcher.IsValid())
			{
				return {};
			}
			
			void* Data;
			const FPropertyAccess::Result AccessResult = StructPropertyHandle->GetValueData(Data);
			if (AccessResult != FPropertyAccess::Success)
			{
				return {};
			}

			FWidgetConnectionConfig* ConfigData = reinterpret_cast<FWidgetConnectionConfig*>(Data);
			const UVCamWidget* VCamWidget = ConfigData->ResolveWidget(WeakStateSwitcher.Get());
			if (!VCamWidget)
			{
				return {};
			}

			TArray<FName> Result;
			VCamWidget->Connections.GenerateKeyArray(Result);
			return Result;
		});
	}
}

#undef LOCTEXT_NAMESPACE