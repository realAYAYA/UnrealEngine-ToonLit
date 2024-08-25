// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "IPropertyUtilities.h"
#include "Graph/MovieEdGraphNode.h"
#include "Graph/MovieGraphConfig.h"
#include "PropertyBagDetails.h"
#include "PropertyHandle.h"
#include "SPinTypeSelector.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditor"

/** Customize how members for a graph appear in the details panel. */
class FMovieGraphMemberCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FMovieGraphMemberCustomization>();
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

	bool IsPinTypeAllowed(const FEdGraphPinType& InPinType) const
	{
		// Property bags do not support interface types
		if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
		{
			return false;
		}

		return true;
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TSharedRef<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();
		
		auto GetFilteredVariableTypeTree = [this](TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
		{
			// Most types from the schema are allowed
			check(GetDefault<UEdGraphSchema_K2>());
			GetDefault<UPropertyBagSchema>()->GetVariableTypeTree(TypeTree, TypeTreeFilter);

			// Filter out disallowed types
			for (int32 Index = 0; Index < TypeTree.Num(); )
			{
				TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinType = TypeTree[Index];
				if (!PinType.IsValid())
				{
					return;
				}

				constexpr bool bForceLoadedSubCategoryObject = false;
				if (!IsPinTypeAllowed(PinType->GetPinType(bForceLoadedSubCategoryObject)))
				{
					TypeTree.RemoveAt(Index);
					continue;
				}

				for (int32 ChildIndex = 0; ChildIndex < PinType->Children.Num(); )
				{
					TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> Child = PinType->Children[ChildIndex];
					if (Child.IsValid())
					{
						if (!IsPinTypeAllowed(Child->GetPinType(bForceLoadedSubCategoryObject)))
						{
							PinType->Children.RemoveAt(ChildIndex);
							continue;
						}
					}
					++ChildIndex;
				}

				++Index;
			}
		};

		auto PinInfoChanged = [PropUtils](const FEdGraphPinType& PinType, const TWeakObjectPtr<UMovieGraphMember>& GraphMember)
		{
			// The SPinTypeSelector popup might outlive this details view, so the member could be invalid
			if (GraphMember.IsValid())
			{
				GraphMember->SetValueType(UMoviePipelineEdGraphNodeBase::GetValueTypeFromPinType(PinType), PinType.PinSubCategoryObject.Get());

				// Need the ForceRefresh to make sure the details panel refreshes immediately after the data type change.
				// Can result in a crash without it.
				PropUtils->ForceRefresh();
			}
		};
		
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		for (const TWeakObjectPtr<UObject>& CustomizedObject : ObjectsBeingCustomized)
		{
			// Note: The graph members inherit the Value property from a base class, so the enable/disable state cannot
			// be driven by UPROPERTY metadata. Hence why this needs to be done w/ a details customization.
			
			// Enable/disable the value property for inputs/outputs based on whether it is specified as a branch or not
			const UMovieGraphInterfaceBase* InterfaceBase = Cast<UMovieGraphInterfaceBase>(CustomizedObject);
			if (InterfaceBase)
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

			const bool bIsNameRowEnabled = MemberObject->IsEditable();
			
			// Add a custom row for the Name property (to allow for proper validation)
			IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("General");
			GeneralCategory.AddCustomRow(FText::GetEmpty())
			.IsEnabled(bIsNameRowEnabled)
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
				.OnTextChanged(this, &FMovieGraphMemberCustomization::OnNameChanged, MemberObject)
				.OnTextCommitted(this, &FMovieGraphMemberCustomization::OnNameCommitted, MemberObject)
				.SelectAllTextWhenFocused(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];

			// If this is an interface (eg, input/output) only enable the type selector if it's not a branch and editable.
			// Otherwise, editability is the only factor in enable state.
			const bool bIsTypeRowEnabled = InterfaceBase ? (!InterfaceBase->bIsBranch && MemberObject->IsEditable()) : MemberObject->IsEditable();

			TWeakObjectPtr<UMovieGraphMember> WeakMemberObject = MakeWeakObjectPtr(MemberObject);

			// Add a PinTypeSelector widget to pick the data type the member uses
			IDetailCategoryBuilder& ValueCategory = DetailBuilder.EditCategory("Value");
			ValueCategory.AddCustomRow(FText::GetEmpty())
			.IsEnabled(bIsTypeRowEnabled)
			.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MemberPropertyLabel_Type", "Type"))
				.Font(DetailBuilder.GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SPinTypeSelector, FGetPinTypeTree::CreateLambda(GetFilteredVariableTypeTree))
				.TargetPinType_Lambda([WeakMemberObject]()
				{
					// The SPinTypeSelector popup might outlive this details view, so the member could be invalid
					if (!WeakMemberObject.IsValid())
					{
						return FEdGraphPinType();
					}

					constexpr bool bIsBranch = false;
					return UMoviePipelineEdGraphNodeBase::GetPinType(WeakMemberObject->GetValueType(), bIsBranch, WeakMemberObject->GetValueTypeObject());
				})
				.OnPinTypeChanged_Lambda(PinInfoChanged, WeakMemberObject)
				.Schema(GetDefault<UPropertyBagSchema>())
				.bAllowArrays(false)
				.TypeTreeFilter(ETypeTreeFilter::None)
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