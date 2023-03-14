// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsSettingsCustomization.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsModule.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "FGameplayTagsSettingsCustomization"

TSharedRef<IDetailCustomization> FGameplayTagsSettingsCustomization::MakeInstance()
{
	return MakeShareable( new FGameplayTagsSettingsCustomization() );
}

FGameplayTagsSettingsCustomization::FGameplayTagsSettingsCustomization()
{
	IGameplayTagsModule::OnTagSettingsChanged.AddRaw(this, &FGameplayTagsSettingsCustomization::OnTagTreeChanged);
}

FGameplayTagsSettingsCustomization::~FGameplayTagsSettingsCustomization()
{
	IGameplayTagsModule::OnTagSettingsChanged.RemoveAll(this);
}

void FGameplayTagsSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const float MaxPropertyWidth = 480.0f;
	const float MaxPropertyHeight = 240.0f;

	IDetailCategoryBuilder& GameplayTagsCategory = DetailLayout.EditCategory("GameplayTags");
	{
		TArray<TSharedRef<IPropertyHandle>> GameplayTagsProperties;
		GameplayTagsCategory.GetDefaultProperties(GameplayTagsProperties, true, true);

		TSharedPtr<IPropertyHandle> TagListProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayTagsList, GameplayTagList), UGameplayTagsList::StaticClass());
		TagListProperty->MarkHiddenByCustomization();

		for (TSharedPtr<IPropertyHandle> Property : GameplayTagsProperties)
		{
			if (Property->GetProperty() != TagListProperty->GetProperty())
			{
				GameplayTagsCategory.AddProperty(Property);
			}
			else
			{
				// Create a custom widget for the tag list

				GameplayTagsCategory.AddCustomRow(TagListProperty->GetPropertyDisplayName(), false)
				.NameContent()
				[
					TagListProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SAssignNew(TagWidget, SGameplayTagWidget, TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum>())
					.Filter(TEXT(""))
					.MultiSelect(false)
					.GameplayTagUIMode(EGameplayTagUIMode::ManagementMode)
					.MaxHeight(MaxPropertyHeight)
					.OnTagChanged(this, &FGameplayTagsSettingsCustomization::OnTagChanged)
					.RestrictedTags(false)
				];
			}
		}
	}

	IDetailCategoryBuilder& AdvancedGameplayTagsCategory = DetailLayout.EditCategory("Advanced Gameplay Tags");
	{
		TArray<TSharedRef<IPropertyHandle>> GameplayTagsProperties;
		AdvancedGameplayTagsCategory.GetDefaultProperties(GameplayTagsProperties, true, true);

		TSharedPtr<IPropertyHandle> RestrictedTagListProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayTagsSettings, RestrictedTagList));
		RestrictedTagListProperty->MarkHiddenByCustomization();

		for (TSharedPtr<IPropertyHandle> Property : GameplayTagsProperties)
		{
			if (Property->GetProperty() == RestrictedTagListProperty->GetProperty())
			{
				// Create a custom widget for the restricted tag list

				AdvancedGameplayTagsCategory.AddCustomRow(RestrictedTagListProperty->GetPropertyDisplayName(), true)
				.NameContent()
				[
					RestrictedTagListProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SAssignNew(RestrictedTagWidget, SGameplayTagWidget, TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum>())
					.Filter(TEXT(""))
					.MultiSelect(false)
					.GameplayTagUIMode(EGameplayTagUIMode::ManagementMode)
					.MaxHeight(MaxPropertyHeight)
					.OnTagChanged(this, &FGameplayTagsSettingsCustomization::OnTagChanged)
					.RestrictedTags(true)
				];
			}
			else
			{
				AdvancedGameplayTagsCategory.AddProperty(Property);
			}
		}
	}
}

void FGameplayTagsSettingsCustomization::OnTagChanged()
{
	if (TagWidget.IsValid())
	{
		TagWidget->RefreshTags();
	}

	if (RestrictedTagWidget.IsValid())
	{
		RestrictedTagWidget->RefreshTags();
	}
}

void FGameplayTagsSettingsCustomization::OnTagTreeChanged()
{
	if (TagWidget.IsValid())
	{
		TagWidget->RefreshOnNextTick();
	}

	if (RestrictedTagWidget.IsValid())
	{
		RestrictedTagWidget->RefreshOnNextTick();
	}
}

#undef LOCTEXT_NAMESPACE
