// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMoviePipelineEditorSettingsCustomization.h"

#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"
#include "Widgets/SDisplayClusterMoviePipelineEditorSearchableComboBox.h"

#include "DisplayClusterMoviePipelineSettings.h"

#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DisplayClusterMoviePipelineEditorSettingsCustomization"

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterMoviePipelineEditorSettingsCustomization
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterMoviePipelineEditorSettingsCustomization::Initialize(const TSharedRef<IPropertyHandle>& InPropertyHandle, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FDisplayClusterMoviePipelineEditorBaseTypeCustomization::Initialize(InPropertyHandle, CustomizationUtils);

	TSharedPtr<IPropertyHandle> DCRootActorHandle = GET_CHILD_HANDLE(FDisplayClusterMoviePipelineConfiguration, DCRootActor);
	check(DCRootActorHandle.IsValid());

	DCRootActorHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDisplayClusterMoviePipelineEditorSettingsCustomization::OnRootActorReferencedPropertyValueChanged));

	TSharedPtr<IPropertyHandle> AllowedViewportNamesListHandle = GET_CHILD_HANDLE(FDisplayClusterMoviePipelineConfiguration, AllowedViewportNamesList);
	check(AllowedViewportNamesListHandle);
	AllowedViewportNamesListHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDisplayClusterMoviePipelineEditorSettingsCustomization::OnAllowedViewportNamesListHandleChanged));

	NodeSelection = MakeShared<FDisplayClusterMoviePipelineEditorNodeSelection>(FDisplayClusterMoviePipelineEditorNodeSelection::EOperationMode::Viewports, DCRootActorHandle, AllowedViewportNamesListHandle);
}

void FDisplayClusterMoviePipelineEditorSettingsCustomization::OnAllowedViewportNamesListHandleChanged()
{
	if (NodeSelection.IsValid())
	{
		NodeSelection->ResetOptionsList();
	}
}
void FDisplayClusterMoviePipelineEditorSettingsCustomization::OnRootActorReferencedPropertyValueChanged()
{
	// When a referenced property is changed, we have to trigger layout refresh
	if (NodeSelection.IsValid())
	{
		NodeSelection->ResetOptionsList();
	}
}

void FDisplayClusterMoviePipelineEditorSettingsCustomization::SetHeader(const TSharedRef<IPropertyHandle>& InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FText ElementTooltip = FText::GetEmpty();

	TSharedPtr<IPropertyHandle> ParentArrayHandle = InPropertyHandle->GetParentHandle();
	if (ParentArrayHandle.IsValid() && ParentArrayHandle->IsValidHandle())
	{
		ElementTooltip = ParentArrayHandle->GetPropertyDisplayName();
	}

	InHeaderRow.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget(FText::GetEmpty(), ElementTooltip)
		];
}

void FDisplayClusterMoviePipelineEditorSettingsCustomization::SetChildren(const TSharedRef<IPropertyHandle>& InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> DCRootActorHandle = GET_CHILD_HANDLE(FDisplayClusterMoviePipelineConfiguration, DCRootActor);
	TSharedPtr<IPropertyHandle> UseViewportResolutionsHandle = GET_CHILD_HANDLE(FDisplayClusterMoviePipelineConfiguration, bUseViewportResolutions);
	TSharedPtr<IPropertyHandle> IsRenderAllViewportsHandle = GET_CHILD_HANDLE(FDisplayClusterMoviePipelineConfiguration, bRenderAllViewports);
	TSharedPtr<IPropertyHandle> ArrayHandle = GET_CHILD_HANDLE(FDisplayClusterMoviePipelineConfiguration, AllowedViewportNamesList);

	InChildBuilder.AddProperty(DCRootActorHandle.ToSharedRef());
	InChildBuilder.AddProperty(UseViewportResolutionsHandle.ToSharedRef());
	InChildBuilder.AddProperty(IsRenderAllViewportsHandle.ToSharedRef());

	if (NodeSelection.IsValid())
	{
		const TAttribute<bool> IsEnabledEditCondition = TAttribute<bool>::Create([this, IsRenderAllViewportsHandle]()
			{
				bool bCond1 = false;
				IsRenderAllViewportsHandle->GetValue(bCond1);
				return bCond1 == false;
			});


		ArrayHandle->SetInstanceMetaData(FDisplayClusterMoviePipelineEditorNodeSelection::NAME_ElementToolTip, ArrayHandle->GetPropertyDisplayName().ToString());

		const TAttribute<bool> IsArrayHandleEnabledEditCondition = TAttribute<bool>::Create([this]()
			{
				return true;
			});
		NodeSelection->IsEnabled(IsArrayHandleEnabledEditCondition);
		NodeSelection->CreateArrayBuilder(ArrayHandle.ToSharedRef(), InChildBuilder);
	}
}
#undef LOCTEXT_NAMESPACE
