// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_Renderer.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "DMXPixelMappingTypes.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "SWarningOrErrorBox.h"
#include "Engine/Texture.h"
#include "Layout/Visibility.h"
#include "Materials/Material.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_Renderer"


void FDMXPixelMappingDetailCustomization_Renderer::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& RenderSettingsSettingsCategory = DetailLayout.EditCategory("Render Settings", FText::GetEmpty(), ECategoryPriority::Important);

	// Register all handles
	RendererTypePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingRendererComponent, RendererType));
	InputTexturePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingRendererComponent, InputTexture));
	InputMaterialPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingRendererComponent, InputMaterial));
	InputWidgetPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingRendererComponent, InputWidget));

	// Bind property changes
	FSimpleDelegate OnRendererTypePropertyPreChangeDelegate = FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_Renderer::OnRendererTypePropertyPreChange);
	RendererTypePropertyHandle->SetOnPropertyValuePreChange(OnRendererTypePropertyPreChangeDelegate);

	FSimpleDelegate OnRendererTypePropertyChangedDelegate = FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_Renderer::OnRendererTypePropertyChanged);
	RendererTypePropertyHandle->SetOnPropertyValueChanged(OnRendererTypePropertyChangedDelegate);

	FSimpleDelegate OnInputTexturePropertyChangedDelegate = FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_Renderer::OnInputTexturePropertyChanged);
	RendererTypePropertyHandle->SetOnPropertyValueChanged(OnInputTexturePropertyChangedDelegate);

	// Hide Output Component properties
	TSharedRef<IPropertyHandle> PositionXPropertyHandle = DetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionXPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	TSharedRef<IPropertyHandle> PositionYPropertyHandle = DetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionYPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	TSharedRef<IPropertyHandle> IsLockInDesignerPropertyHandle = DetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetLockInDesignerPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	TSharedRef<IPropertyHandle> IsVisibleInDesignerPropertyHandle = DetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetVisibleInDesignerPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	TSharedRef<IPropertyHandle> PixelBlendingPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingOutputComponent, CellBlendingQuality), UDMXPixelMappingOutputComponent::StaticClass());
	DetailLayout.HideProperty(PositionXPropertyHandle);
	DetailLayout.HideProperty(PositionYPropertyHandle);
	DetailLayout.HideProperty(IsLockInDesignerPropertyHandle);
	DetailLayout.HideProperty(IsVisibleInDesignerPropertyHandle);
	DetailLayout.HideProperty(PixelBlendingPropertyHandle);

	// Add properties
	RenderSettingsSettingsCategory.AddProperty(RendererTypePropertyHandle);

	// Get editing UObject
	TArray<TWeakObjectPtr<UObject>> OuterObjects;
	DetailLayout.GetObjectsBeingCustomized(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RendererComponent = Cast<UDMXPixelMappingRendererComponent>(OuterObjects[0]);

		if (RendererComponent.IsValid())
		{
			// Remember the selected renderer type
			PreviousRendererType = RendererComponent->RendererType;

		// Add Warning InputTexture Row
		AddInputTextureWarning(RenderSettingsSettingsCategory);

		// Add non UI Material Warning
		AddMaterialWarning(RenderSettingsSettingsCategory);

		// Retister properties
		RenderSettingsSettingsCategory.AddProperty(InputTexturePropertyHandle)
			.Visibility(
				TAttribute<EVisibility>::Create(
						TAttribute<EVisibility>::FGetter::CreateLambda([this]
							{
								return IsSelectedRendererType(EDMXPixelMappingRendererType::Texture) ? EVisibility::Visible : EVisibility::Hidden;
							}
					)));

		RenderSettingsSettingsCategory.AddProperty(InputMaterialPropertyHandle)
			.Visibility(
				TAttribute<EVisibility>::Create(
						TAttribute<EVisibility>::FGetter::CreateLambda([this]
							{
								return IsSelectedRendererType(EDMXPixelMappingRendererType::Material) ? EVisibility::Visible : EVisibility::Hidden;
							}
					)));

		RenderSettingsSettingsCategory.AddProperty(InputWidgetPropertyHandle)
			.Visibility(
				TAttribute<EVisibility>::Create(
						TAttribute<EVisibility>::FGetter::CreateLambda([this]
							{
								return IsSelectedRendererType(EDMXPixelMappingRendererType::UMG) ? EVisibility::Visible : EVisibility::Hidden;
							}
					)));

		}
	}
}

void FDMXPixelMappingDetailCustomization_Renderer::OnRendererTypePropertyPreChange()
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		PreviousRendererType = Component->RendererType;
	}
}

void FDMXPixelMappingDetailCustomization_Renderer::OnRendererTypePropertyChanged()
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->RendererType != PreviousRendererType)
		{
			PreviousRendererType = Component->RendererType;
			ResetSize();
		}
	}
}

void FDMXPixelMappingDetailCustomization_Renderer::OnInputTexturePropertyChanged()
{
	ResetSize();
}

bool FDMXPixelMappingDetailCustomization_Renderer::IsSelectedRendererType(EDMXPixelMappingRendererType PropertyRendererType) const
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->RendererType == PropertyRendererType)
		{
			return true;
		}
	}

	return false;
}

bool FDMXPixelMappingDetailCustomization_Renderer::IsNotSelectedRendererType(EDMXPixelMappingRendererType PropertyRendererType) const
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->RendererType != PropertyRendererType)
		{
			return true;
		}
	}

	return false;
}

void FDMXPixelMappingDetailCustomization_Renderer::AddInputTextureWarning(IDetailCategoryBuilder& InCategory)
{
	const FSlateBrush* WarningIcon = FAppStyle::GetBrush("SettingsEditor.WarningIcon");

	InCategory.AddCustomRow(FText::GetEmpty())
			.Visibility(TAttribute<EVisibility>(this, &FDMXPixelMappingDetailCustomization_Renderer::GetInputTextureWarning))
			.WholeRowContent()
			[
				SNew(SWarningOrErrorBox)
				.MessageStyle(EMessageStyle::Warning)
				.Message(this, &FDMXPixelMappingDetailCustomization_Renderer::GetInputTextureWarningText)
			];
}

void FDMXPixelMappingDetailCustomization_Renderer::AddMaterialWarning(IDetailCategoryBuilder& InCategory)
{
	InCategory.AddCustomRow(FText::GetEmpty())
			.Visibility(TAttribute<EVisibility>(this, &FDMXPixelMappingDetailCustomization_Renderer::GetMaterialWarningVisibility))
			.WholeRowContent()
			[
				SNew(SWarningOrErrorBox)
				.MessageStyle(EMessageStyle::Warning)
				.Message(LOCTEXT("WarningNonUIMaterial", "Selected Material is not UI Material.\nChange Material Domain to User Interface.\nOr select another Material."))
			];
}

EVisibility FDMXPixelMappingDetailCustomization_Renderer::GetMaterialWarningVisibility() const
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->InputMaterial != nullptr)
		{
			if (UMaterial* Material = Component->InputMaterial->GetMaterial())
			{
				if (Component->RendererType == EDMXPixelMappingRendererType::Material && !Material->IsUIMaterial())
				{
					return EVisibility::Visible;
				}
			}
		}
	}

	return EVisibility::Collapsed;
}

EVisibility FDMXPixelMappingDetailCustomization_Renderer::GetInputTextureWarning() const
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->GetRendererInputTexture() == nullptr)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FText FDMXPixelMappingDetailCustomization_Renderer::GetInputTextureWarningText() const
{
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->GetRendererInputTexture() == nullptr)
		{
			if (Component->RendererType == EDMXPixelMappingRendererType::Texture)
			{
				return LOCTEXT("WarningCategoryDisplayName.TextureNotSet", "Texture is not set.");
			}
			else if (Component->RendererType == EDMXPixelMappingRendererType::Material)
			{
				return LOCTEXT("WarningCategoryDisplayName.MaterialNotSet", "Material is not set.");
			}
			else if (Component->RendererType == EDMXPixelMappingRendererType::UMG)
			{
				return LOCTEXT("WarningCategoryDisplayName.UMGNotSet", "UMG is not set.");
			}
		}
	}

	return FText();
}

void FDMXPixelMappingDetailCustomization_Renderer::ResetSize()
{
	// Set a new size depending on the selected type
	if (UDMXPixelMappingRendererComponent* Component = RendererComponent.Get())
	{
		if (Component->RendererType == EDMXPixelMappingRendererType::Texture)
		{
			if (const UTexture* InputTexture = Component->InputTexture)
			{
				if (const FTextureResource* TextureResource = InputTexture->GetResource())
				{
					// Set to the texture size
					const FVector2D NewSize = FVector2D(TextureResource->GetSizeX(), TextureResource->GetSizeY());
					Component->SetSize(NewSize);
					return;
				}
			}

			// Reset to default size
			const FVector2D DefaultSize = FVector2D(100.f, 100.f);
			Component->SetSize(DefaultSize);
		}
		else if (Component->RendererType == EDMXPixelMappingRendererType::Material)
		{
			// Reset to default size
			const FVector2D DefaultSize = FVector2D(100.f, 100.f);
			Component->SetSize(DefaultSize);
		}
		else if (Component->RendererType == EDMXPixelMappingRendererType::UMG)
		{
			// Reset to default size
			const FVector2D DefaultSize = FVector2D(1024.f, 768.f);
			Component->SetSize(DefaultSize);
		}
	}
}

#undef LOCTEXT_NAMESPACE
