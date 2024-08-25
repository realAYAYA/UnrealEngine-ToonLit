// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleDataDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DMXControlConsoleData.h"
#include "Editor.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "PropertyHandle.h"
#include "Widgets/SDMXControlConsoleEditorLayoutPicker.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleDataDetails"

namespace UE::DMX::Private
{
	FDMXControlConsoleDataDetails::FDMXControlConsoleDataDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakEditorModel(InWeakEditorModel)
	{}

	TSharedRef<IDetailCustomization> FDMXControlConsoleDataDetails::MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		return MakeShared<FDMXControlConsoleDataDetails>(InWeakEditorModel);
	}

	void FDMXControlConsoleDataDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
	{
		UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
		if (!EditorModel)
		{
			return;
		}

		const TSharedRef<IPropertyHandle> DMXLibraryHandle = InDetailLayout.GetProperty(UDMXControlConsoleData::GetDMXLibraryPropertyName());
		InDetailLayout.AddPropertyToCategory(DMXLibraryHandle);

		// Edit Mode selection section
		IDetailCategoryBuilder& ControlConsoleCategory = InDetailLayout.EditCategory("DMX Control Console", FText::GetEmpty());
		ControlConsoleCategory.AddCustomRow(FText::GetEmpty())
			.NameContent()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("EditModeLabel", "Edit Mode"))
			]
			.ValueContent()
			[
				SNew(SDMXControlConsoleEditorLayoutPicker, EditorModel)
			];
	}
}

#undef LOCTEXT_NAMESPACE
