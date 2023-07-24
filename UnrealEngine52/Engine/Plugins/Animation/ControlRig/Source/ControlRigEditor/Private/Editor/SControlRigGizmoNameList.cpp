// Copyright Epic Games, Inc. All Rights Reserved.


#include "Editor/SControlRigGizmoNameList.h"
#include "Graph/SControlRigGraphPinNameListValueWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"
#include "DetailLayoutBuilder.h"
#include "ControlRigBlueprint.h"
#include "ControlRig.h"

void SControlRigShapeNameList::Construct(const FArguments& InArgs, FRigControlElement* ControlElement, UControlRigBlueprint* InBlueprint)
{
	TArray<FRigControlElement*> ControlElements;
	ControlElements.Add(ControlElement);
	return Construct(InArgs, ControlElements, InBlueprint);
}

void SControlRigShapeNameList::Construct(const FArguments& InArgs, TArray<FRigControlElement*> ControlElements, UControlRigBlueprint* InBlueprint)
{
	this->OnGetNameListContent = InArgs._OnGetNameListContent;
	this->ControlKeys.Reset();

	for(FRigControlElement* ControlElement : ControlElements)
	{
		this->ControlKeys.Add(ControlElement->GetKey());
	}
	this->Blueprint = InBlueprint;

	ConstructCommon();
}

void SControlRigShapeNameList::Construct(const FArguments& InArgs, TArray<FRigControlElement> ControlElements, UControlRigBlueprint* InBlueprint)
{
	this->OnGetNameListContent = InArgs._OnGetNameListContent;
	this->ControlKeys.Reset();

	for(const FRigControlElement& ControlElement : ControlElements)
	{
		this->ControlKeys.Add(ControlElement.GetKey());
	}
	this->Blueprint = InBlueprint;

	ConstructCommon();
}

void SControlRigShapeNameList::BeginDestroy()
{
	if(NameListComboBox.IsValid())
	{
		NameListComboBox->SetOptionsSource(&GetEmptyList());
	}
}

void SControlRigShapeNameList::ConstructCommon()
{
	SBox::Construct(SBox::FArguments());

	TSharedPtr<FString> InitialSelected;
	for (TSharedPtr<FString> Item : GetNameList())
	{
		if (Item->Equals(GetNameListText().ToString()))
		{
			InitialSelected = Item;
		}
	}

	SetContent(
		SNew(SBox)
		.MinDesiredWidth(150)
		.MaxDesiredWidth(400)
		[
			SAssignNew(NameListComboBox, SControlRigGraphPinNameListValueWidget)
			.OptionsSource(&GetNameList())
			.OnGenerateWidget(this, &SControlRigShapeNameList::MakeNameListItemWidget)
			.OnSelectionChanged(this, &SControlRigShapeNameList::OnNameListChanged)
			.OnComboBoxOpening(this, &SControlRigShapeNameList::OnNameListComboBox)
			.InitiallySelectedItem(InitialSelected)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SControlRigShapeNameList::GetNameListText)
			]
		]
	);
}

const TArray<TSharedPtr<FString>>& SControlRigShapeNameList::GetNameList() const
{
	if (OnGetNameListContent.IsBound())
	{
		return OnGetNameListContent.Execute();
	}
	return GetEmptyList();
}

FText SControlRigShapeNameList::GetNameListText() const
{
	FName FirstName = NAME_None;
	FText Text;
	for(int32 KeyIndex = 0; KeyIndex < ControlKeys.Num(); KeyIndex++)
	{
		const int32 ControlIndex = Blueprint->Hierarchy->GetIndex(ControlKeys[KeyIndex]);
		if (ControlIndex != INDEX_NONE)
		{
			const FName ShapeName = Blueprint->Hierarchy->GetChecked<FRigControlElement>(ControlIndex)->Settings.ShapeName; 
			if(KeyIndex == 0)
			{
				Text = FText::FromName(ShapeName);
				FirstName = ShapeName;
			}
			else if(FirstName != ShapeName)
			{
				static const FString MultipleValues = TEXT("Multiple Values");
				Text = FText::FromString(MultipleValues);
				break;
			}
		}
	}
	return Text;
}

void SControlRigShapeNameList::SetNameListText(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	const FScopedTransaction Transaction(NSLOCTEXT("ControlRigEditor", "ChangeShapeName", "Change Shape Name"));

	for(int32 KeyIndex = 0; KeyIndex < ControlKeys.Num(); KeyIndex++)
	{
		const int32 ControlIndex = Blueprint->Hierarchy->GetIndex(ControlKeys[KeyIndex]);
		if (ControlIndex != INDEX_NONE)
		{
			const FName NewName = *NewTypeInValue.ToString();
			URigHierarchy* Hierarchy = Blueprint->Hierarchy;

			FRigControlElement* ControlElement = Hierarchy->Get<FRigControlElement>(ControlIndex);
			if ((ControlElement != nullptr) && (ControlElement->Settings.ShapeName != NewName))
			{
				Hierarchy->Modify();

				FRigControlSettings Settings = ControlElement->Settings;
				Settings.ShapeName = NewName;
				Hierarchy->SetControlSettings(ControlElement, Settings, true, true, true);
			}
		}
	}
}

TSharedRef<SWidget> SControlRigShapeNameList::MakeNameListItemWidget(TSharedPtr<FString> InItem)
{
	return 	SNew(STextBlock).Text(FText::FromString(*InItem));// .Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}

void SControlRigShapeNameList::OnNameListChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		if (NewSelection.IsValid())
		{
			FString NewValue = *NewSelection.Get();
			SetNameListText(FText::FromString(NewValue), ETextCommit::OnEnter);
		}
		else
		{
			SetNameListText(FText(), ETextCommit::OnEnter);
		}
	}
}

void SControlRigShapeNameList::OnNameListComboBox()
{
	TSharedPtr<FString> CurrentlySelected;
	for (TSharedPtr<FString> Item : GetNameList())
	{
		if (Item->Equals(GetNameListText().ToString()))
		{
			CurrentlySelected = Item;
		}
	}
	NameListComboBox->SetSelectedItem(CurrentlySelected);
}
