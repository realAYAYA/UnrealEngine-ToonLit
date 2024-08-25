// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Details/Media/DisplayClusterConfiguratorICVFXMediaCustomization.h"
#include "Views/Details/Media/DisplayClusterConfiguratorMediaUtils.h"

#include "DisplayClusterConfigurationTypes_Media.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FDisplayClusterConfiguratorICVFXMediaCustomization"

void FDisplayClusterConfiguratorICVFXMediaCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// SplitType property
	TSharedPtr<IPropertyHandle> SplitTypeHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, SplitType);
	check(SplitTypeHandle->IsValidHandle());

	// Layout property
	TilesLayoutHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledSplitLayout);
	check(TilesLayoutHandle->IsValidHandle());

	// Separate groups specific for every split type available
	TArray<TSharedPtr<IPropertyHandle>> FullFramePropertyHandles;
	TArray<TSharedPtr<IPropertyHandle>> UniformTilePropertyHandles;

	// FullFrame properties
	FullFramePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, MediaInputGroups));
	FullFramePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, MediaOutputGroups));

	// UniformTile properties
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledSplitLayout));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TileOverscan));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, ClusterNodesToRenderUnboundTiles));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledMediaInputGroups));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledMediaOutputGroups));

	// Get current split type
	EDisplayClusterConfigurationMediaSplitType SplitTypeValue = EDisplayClusterConfigurationMediaSplitType::UniformTiles;
	SplitTypeHandle->GetValue((uint8&)SplitTypeValue);

	TSharedPtr<IPropertyUtilities> PropertyUtils = InCustomizationUtils.GetPropertyUtilities();
	check(PropertyUtils);

	// Setup details update on frustum type change
	SplitTypeHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateLambda([PropertyUtils]()
		{
			if (PropertyUtils)
			{
				PropertyUtils->RequestForceRefresh();
			}
		}));

	// Filter properties to hide
	TArray<TArray<TSharedPtr<IPropertyHandle>>*> HiddenPropertyHandles;
	switch (SplitTypeValue)
	{
	case EDisplayClusterConfigurationMediaSplitType::FullFrame:
		HiddenPropertyHandles.Add(&UniformTilePropertyHandles);
		break;

	case EDisplayClusterConfigurationMediaSplitType::UniformTiles:
		HiddenPropertyHandles.Add(&FullFramePropertyHandles);
		break;

	default:
		checkNoEntry();
	}

	// Hide unnecessary properties depending on the frustum (split) type currently selected
	for (const TArray<TSharedPtr<IPropertyHandle>>* HiddenGroup : HiddenPropertyHandles)
	{
		for (const TSharedPtr<IPropertyHandle>& PropertyHandle : *HiddenGroup)
		{
			PropertyHandle->MarkHiddenByCustomization();
		}
	}

	// Create all the property widgets
	FDisplayClusterConfiguratorBaseTypeCustomization::CustomizeChildren(InPropertyHandle, InChildBuilder, InCustomizationUtils);

	// Create auto-configure button in the bottom
	AddAutoConfigurationButton(InChildBuilder);
}

void FDisplayClusterConfiguratorICVFXMediaCustomization::AddAutoConfigurationButton(IDetailChildrenBuilder& InChildBuilder)
{
	InChildBuilder.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SBox)
				.Padding(5.f)
				[
					SNew(SButton)
						.HAlign(HAlign_Center)
						.OnClicked(this, &FDisplayClusterConfiguratorICVFXMediaCustomization::OnAutoConfigureButtonClicked)
						[
							SNew(STextBlock)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.Text(LOCTEXT("AutoConfigureInputButtonTitle", "Auto-Configure"))
						]
				]
		];
}

FReply FDisplayClusterConfiguratorICVFXMediaCustomization::OnAutoConfigureButtonClicked()
{
	// Notify tile customizators to re-initialize their media subjects
	FDisplayClusterConfiguratorMediaUtils::Get().OnMediaAutoConfiguration().Broadcast(EditingObject.Get());

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
