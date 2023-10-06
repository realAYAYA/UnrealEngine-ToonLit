// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "Graph/MovieGraphConfig.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditor"

/** Customize how members for a graph appear in the details panel. */
class FGraphMemberCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FGraphMemberCustomization>();
	}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override
	{
		CustomizeDetails(*DetailBuilder);
	}

	void OnNameChanged(const FText& InText, UMovieGraphMember* MovieGraphMember) const
	{
		NameEditableTextBox->SetError(FText::GetEmpty());

		FText Error;
		if (!MovieGraphMember->CanRename(InText, Error))
		{
			NameEditableTextBox->SetError(Error);
		}
	}
	
	void OnNameCommitted(const FText& InText, ETextCommit::Type Arg, UMovieGraphMember* MovieGraphMember) const
	{
		if (MovieGraphMember->SetMemberName(InText.ToString()))
		{
			NameEditableTextBox->SetError(FText::GetEmpty());
		}
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		for (const TWeakObjectPtr<UObject>& CustomizedObject : ObjectsBeingCustomized)
		{
			// Note: The graph members inherit the Value property from a base class, so the enable/disable state cannot
			// be driven by UPROPERTY metadata. Hence why this needs to be done w/ a details customization.
			
			// Enable/disable the value property for inputs/outputs based on whether it is specified as a branch or not
			if (const UMovieGraphInterfaceBase* InterfaceBase = Cast<UMovieGraphInterfaceBase>(CustomizedObject))
			{
				const TSharedRef<IPropertyHandle> ValueProperty = DetailBuilder.GetProperty("Value", UMovieGraphValueContainer::StaticClass());
				if (ValueProperty->IsValidHandle())
				{
					DetailBuilder.EditDefaultProperty(ValueProperty)->IsEnabled(!InterfaceBase->bIsBranch);
				}
			}

			// Enable/disable the value property for variables based on the editable state
			if (const UMovieGraphVariable* Variable = Cast<UMovieGraphVariable>(CustomizedObject))
			{
				const TSharedRef<IPropertyHandle> ValueProperty = DetailBuilder.GetProperty("Value", UMovieGraphValueContainer::StaticClass());
				if (ValueProperty->IsValidHandle())
				{
					DetailBuilder.EditDefaultProperty(ValueProperty)->IsEnabled(Variable->IsEditable());
				}
			}

			UMovieGraphMember* MemberObject = Cast<UMovieGraphMember>(CustomizedObject);
			if (!MemberObject)
			{
				continue;
			}

			// Add a custom row for the Name property (to allow for proper validation)
			IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("General");
			GeneralCategory.AddCustomRow(FText::GetEmpty())
			.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MemberPropertyLabel_Name", "Name"))
				.Font(DetailBuilder.GetDetailFont())
			]
			.ValueContent()
			[
				SAssignNew(NameEditableTextBox, SEditableTextBox)
				.Text_Lambda([MemberObject]() { return FText::FromString(MemberObject->GetMemberName()); })
				.OnTextChanged(this, &FGraphMemberCustomization::OnNameChanged, MemberObject)
				.OnTextCommitted(this, &FGraphMemberCustomization::OnNameCommitted, MemberObject)
				.IsReadOnly_Lambda([MemberObject]() { return !MemberObject->IsEditable(); })
				.SelectAllTextWhenFocused(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
		}
	}
	//~ End IDetailCustomization interface

private:
	/** Text box for the "Name" property. */
	TSharedPtr<SEditableTextBox> NameEditableTextBox;
};

#undef LOCTEXT_NAMESPACE