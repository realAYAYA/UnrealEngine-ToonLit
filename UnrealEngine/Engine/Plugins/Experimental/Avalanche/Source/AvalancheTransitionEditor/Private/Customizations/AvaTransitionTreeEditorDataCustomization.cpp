// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTreeEditorDataCustomization.h"
#include "AvaTransitionTreeEditorData.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

TSharedRef<IDetailCustomization> FAvaTransitionTreeEditorDataCustomization::MakeInstance()
{
	return MakeShared<FAvaTransitionTreeEditorDataCustomization>();
}

void FAvaTransitionTreeEditorDataCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	if (TSharedPtr<IDetailCustomization> Customization = GetDefaultCustomization())
	{
		Customization->CustomizeDetails(InDetailBuilder);
	}

	// Hide property as it's going to show in the Toolbar
	TSharedRef<IPropertyHandle> LayerHandle = InDetailBuilder.GetProperty(UAvaTransitionTreeEditorData::GetTransitionLayerPropertyName()
		, UAvaTransitionTreeEditorData::StaticClass());

	InDetailBuilder.HideProperty(LayerHandle);
}

TSharedPtr<IDetailCustomization> FAvaTransitionTreeEditorDataCustomization::GetDefaultCustomization() const
{
	const FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FCustomDetailLayoutNameMap& CustomizationMap = PropertyModule.GetClassNameToDetailLayoutNameMap();
	const FDetailLayoutCallback* DefaultLayoutCallback = CustomizationMap.Find(UStateTreeEditorData::StaticClass()->GetFName());

	if (DefaultLayoutCallback && DefaultLayoutCallback->DetailLayoutDelegate.IsBound())
	{
		return DefaultLayoutCallback->DetailLayoutDelegate.Execute();
	}

	return nullptr;
}
