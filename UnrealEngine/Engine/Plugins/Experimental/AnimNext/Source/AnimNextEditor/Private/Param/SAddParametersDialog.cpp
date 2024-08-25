// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAddParametersDialog.h"

#include "AddParameterDialogMenuContext.h"
#include "AnimNextParameterSettings.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DetailLayoutBuilder.h"
#include "IContentBrowserSingleton.h"
#include "EditorUtils.h"
#include "UncookedOnlyUtils.h"
#include "PropertyBagDetails.h"
#include "SParameterPickerCombo.h"
#include "SPinTypeSelector.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SSimpleButton.h"
#include "SSimpleComboButton.h"
#include "ToolMenus.h"
#include "String/ParseTokens.h"

#define LOCTEXT_NAMESPACE "SAddParametersDialog"

namespace UE::AnimNext::Editor
{

namespace AddParametersDialog
{
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));
static FName SelectLibraryMenuName(TEXT("AnimNext.AddParametersDialog.SelectedLibraryMenu"));
}

bool FParameterToAdd::IsValid(FText& OutReason) const
{
	if(Name == NAME_None)
	{
		OutReason = LOCTEXT("InvalidParameterName", "Invalid Parameter Name");
	}

	if(!Type.IsValid())
	{
		OutReason = LOCTEXT("InvalidParameterType", "Invalid Parameter Type");
	}
	
	return true; 
}

void SAddParametersDialog::Construct(const FArguments& InArgs)
{
	using namespace AddParametersDialog;

	TargetBlock = InArgs._Block;
	OnFilterParameterType = InArgs._OnFilterParameterType;

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Add Parameters"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(InArgs._AllowMultiple ? FVector2D(500.f, 500.f) : FVector2D(500.f, 100.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SBox)
			.Padding(5.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(0.0f, 5.0f)
				[
					SNew(SSimpleButton)
					.Visibility(InArgs._AllowMultiple ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(LOCTEXT("AddButton", "Add"))
					.ToolTipText(LOCTEXT("AddButtonTooltip", "Queue a new parameter for adding. New parameters will re-use the settings from the last queued parameter."))
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.OnClicked_Lambda([this]()
					{
						AddEntry();
						return FReply::Handled();
					})
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(EntriesList, SListView<TSharedRef<FParameterToAdd>>)
					.ListItemsSource(&Entries)
					.OnGenerateRow(this, &SAddParametersDialog::HandleGenerateRow)
					.ItemHeight(20.0f)
					.HeaderRow(
						SNew(SHeaderRow)
						+SHeaderRow::Column(Column_Name)
						.DefaultLabel(LOCTEXT("NameColumnHeader", "Name"))
						.ToolTipText(LOCTEXT("NameColumnHeaderTooltip", "The name of the new parameter"))
						.FillWidth(0.25f)

						+SHeaderRow::Column(Column_Type)
						.DefaultLabel(LOCTEXT("TypeColumnHeader", "Type"))
						.ToolTipText(LOCTEXT("TypeColumnHeaderTooltip", "The type of the new parameter"))
						.FillWidth(0.25f)
					)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.IsEnabled_Lambda([this]()
						{
							// Check each entry to see if the button can be pressed
							for(TSharedRef<FParameterToAdd> Entry : Entries)
							{
								if(!Entry->IsValid())
								{
									return false;
								}
							}

							return true;
						})
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("AddParametersButtonFormat", "Add {0} {0}|plural(one=Parameter,other=Parameters)"), FText::AsNumber(Entries.Num()));
						})
						.ToolTipText_Lambda([this]()
						{
							// Check each entry to see if the button can be pressed
							for(TSharedRef<FParameterToAdd> Entry : Entries)
							{
								FText Reason;
								if(!Entry->IsValid(Reason))
								{
									return FText::Format(LOCTEXT("AddParametersButtonTooltip_InvalidEntry", "A parameter to add is not valid: {0}"), Reason);
								}
							}
							return LOCTEXT("AddParametersButtonTooltip", "Add the selected parameters to the current parameter block");
						})
						.OnClicked_Lambda([this]()
						{
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.ToolTipText(LOCTEXT("CancelButtonTooltip", "Cancel adding new parameters"))
						.OnClicked_Lambda([this]()
						{
							bCancelPressed = true;
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]
		]);

	// Add an initial item
	AddEntry(InArgs._InitialParamType);
}

void SAddParametersDialog::AddEntry(const FAnimNextParamType& InParamType)
{
	const UAnimNextParameterSettings* Settings = GetDefault<UAnimNextParameterSettings>();
	
	TArray<FName> PendingNames;
	PendingNames.Reserve(Entries.Num());
	for(const TSharedRef<FParameterToAdd>& QueuedAdd : Entries)
	{
		PendingNames.Add(QueuedAdd->Name);
	}
	FName ParameterName = FUtils::GetNewParameterName(TEXT("NewParameter"), PendingNames);
	Entries.Add(MakeShared<FParameterToAdd>(InParamType.IsValid() ? InParamType : Settings->GetLastParameterType(), ParameterName));

	RefreshEntries();
}

void SAddParametersDialog::RefreshEntries()
{
	EntriesList->RequestListRefresh();
}

class SParameterToAdd : public SMultiColumnTableRow<TSharedRef<FParameterToAdd>>
{
	SLATE_BEGIN_ARGS(SParameterToAdd) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FParameterToAdd> InEntry, TSharedRef<SAddParametersDialog> InDialog)
	{
		Entry = InEntry;
		WeakDialog = InDialog;
		
		SMultiColumnTableRow<TSharedRef<FParameterToAdd>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterToAdd>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace AddParametersDialog;

		if(InColumnName == Column_Name)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsSelected(this, &SParameterToAdd::IsSelectedExclusively)
					.ToolTipText(LOCTEXT("NameTooltip", "The name of the new parameter"))
					.Text_Lambda([this]()
					{
						return UncookedOnly::FUtils::GetParameterDisplayNameText(Entry->Name);
					})
					.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
					{
						const FString UserInput = InText.ToString();
						// Parse out segments to collapse adjacent delimiters
						TStringBuilder<128> RebuiltInput;
						UE::String::ParseTokensMultiple(UserInput, { TEXT('.'), TEXT('_') }, [&RebuiltInput](const FStringView InToken)
						{
							if(RebuiltInput.Len() != 0)
							{
								RebuiltInput.Append(TEXT("_"));
							}
							RebuiltInput.Append(InToken);
						}, String::EParseTokensOptions::SkipEmpty);
						Entry->Name = RebuiltInput.ToString();
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();

						if(!FUtils::IsValidEntryNameString(NewString, OutErrorText))
						{
							return false;
						}

						const FName Name(*NewString);
						if(FUtils::DoesParameterNameExist(Name))
						{
							OutErrorText = LOCTEXT("Error_NameExists", "This name already exists in the project");
							return false;
						}

						return true;
					})
				];
		}
		else if(InColumnName == Column_Type)
		{
			auto GetPinInfo = [this]()
			{
				return UncookedOnly::FUtils::GetPinTypeFromParamType(Entry->Type);
			};

			auto PinInfoChanged = [this](const FEdGraphPinType& PinType)
			{
				Entry->Type = UncookedOnly::FUtils::GetParamTypeFromPinType(PinType);

				UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
				Settings->SetLastParameterType(Entry->Type);
			};
			
			auto GetFilteredVariableTypeTree = [this](TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
			{
				FUtils::GetFilteredVariableTypeTree(TypeTree, TypeTreeFilter);

				if(TSharedPtr<SAddParametersDialog> Dialog = WeakDialog.Pin())
				{
					if(Dialog->OnFilterParameterType.IsBound())
					{
						auto IsPinTypeAllowed = [&Dialog](const FEdGraphPinType& InType)
						{
							FAnimNextParamType Type = UncookedOnly::FUtils::GetParamTypeFromPinType(InType);
							if(Type.IsValid())
							{
								return Dialog->OnFilterParameterType.Execute(Type) == EFilterParameterResult::Include;
							}
							return false;
						};

						// Additionally filter by allowed types
						for (int32 Index = 0; Index < TypeTree.Num(); )
						{
							TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinType = TypeTree[Index];

							if (PinType->Children.Num() == 0 && !IsPinTypeAllowed(PinType->GetPinType(/*bForceLoadSubCategoryObject*/false)))
							{
								TypeTree.RemoveAt(Index);
								continue;
							}

							for (int32 ChildIndex = 0; ChildIndex < PinType->Children.Num(); )
							{
								TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> Child = PinType->Children[ChildIndex];
								if (Child.IsValid())
								{
									if (!IsPinTypeAllowed(Child->GetPinType(/*bForceLoadSubCategoryObject*/false)))
									{
										PinType->Children.RemoveAt(ChildIndex);
										continue;
									}
								}
								++ChildIndex;
							}

							++Index;
						}
					}
				}
			};

			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SPinTypeSelector, FGetPinTypeTree::CreateLambda(GetFilteredVariableTypeTree))
						.TargetPinType_Lambda(GetPinInfo)
						.OnPinTypeChanged_Lambda(PinInfoChanged)
						.Schema(GetDefault<UPropertyBagSchema>())
						.bAllowArrays(true)
						.TypeTreeFilter(ETypeTreeFilter::None)
						.Font(IDetailLayoutBuilder::GetDetailFont())
				];
		}

		return SNullWidget::NullWidget;
	}

	TSharedPtr<FParameterToAdd> Entry;
	TWeakPtr<SAddParametersDialog> WeakDialog;
};

TSharedRef<ITableRow> SAddParametersDialog::HandleGenerateRow(TSharedRef<FParameterToAdd> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SParameterToAdd, InOwnerTable, InEntry, SharedThis(this));
}

bool SAddParametersDialog::ShowModal(TArray<FParameterToAdd>& OutParameters)
{
	FSlateApplication::Get().AddModalWindow(SharedThis(this), FGlobalTabmanager::Get()->GetRootWindow());

	if(!bCancelPressed)
	{
		bool bHasValid = false;
		for(TSharedRef<FParameterToAdd>& Entry : Entries)
		{
			if(Entry->IsValid())
			{
				OutParameters.Add(*Entry);
				bHasValid = true;
			}
		}
		return bHasValid;
	}
	return false;
}

TSharedRef<SWidget> SAddParametersDialog::HandleGetAddParameterMenuContent(TSharedPtr<FParameterToAdd> InEntry)
{
	using namespace AddParametersDialog;

	UToolMenus* ToolMenus = UToolMenus::Get();

	UAddParameterDialogMenuContext* MenuContext = NewObject<UAddParameterDialogMenuContext>();
	MenuContext->AddParametersDialog = SharedThis(this);
	MenuContext->Entry = InEntry;
	return ToolMenus->GenerateWidget(SelectLibraryMenuName, FToolMenuContext(MenuContext));
}

}

#undef LOCTEXT_NAMESPACE