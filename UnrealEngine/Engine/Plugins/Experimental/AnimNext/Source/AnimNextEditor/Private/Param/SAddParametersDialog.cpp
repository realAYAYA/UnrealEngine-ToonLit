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
#include "Param/AnimNextParameterLibrary.h"
#include "Param/ParameterLibraryFactory.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "SAddParametersDialog"

namespace UE::AnimNext::Editor
{

namespace AddParametersDialog
{
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));
static FName Column_Library(TEXT("Library"));
static FName SelectLibraryMenuName(TEXT("AnimNext.AddParametersDialog.SelectedLibraryMenu"));
}

void SAddParametersDialog::Construct(const FArguments& InArgs)
{
	using namespace AddParametersDialog;

	Library = InArgs._Library;

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Add Parameters"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(500.f, 500.f))
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

						+SHeaderRow::Column(Column_Library)
						.DefaultLabel(LOCTEXT("LibraryColumnHeader", "Library"))
						.ToolTipText(LOCTEXT("LibraryColumnHeaderTooltip", "The library the new parameter will be created in"))
						.FillWidth(0.5f)
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
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("AddParametersButtonFormat", "Add {0} {0}|plural(one=Parameter,other=Parameters)"), FText::AsNumber(Entries.Num()));
						})
						.ToolTipText(LOCTEXT("AddParametersButtonTooltip", "Add the selected parameters to the current parameter block"))
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
	AddEntry();
}

void SAddParametersDialog::AddEntry()
{
	const UAnimNextParameterSettings* Settings = GetDefault<UAnimNextParameterSettings>();
	FAssetData LibraryAsset = Settings->GetLastLibrary();
	if(Library != nullptr)
	{
		LibraryAsset = FAssetData(Library);
	}
	
	TArray<FName> PendingNames;
	PendingNames.Reserve(Entries.Num());
	for(const TSharedRef<FParameterToAdd>& QueuedAdd : Entries)
	{
		if(QueuedAdd->Library == LibraryAsset)
		{
			PendingNames.Add(QueuedAdd->Name);
		}
	}
	FName ParameterName = FUtils::GetNewParameterNameInLibrary(LibraryAsset, TEXT("NewParameter"), PendingNames);
	Entries.Add(MakeShared<FParameterToAdd>(Settings->GetLastParameterType(), ParameterName, Settings->GetLastLibrary()));

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
						return FText::FromName(Entry->Name);
					})
					.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
					{
						if(InCommitType == ETextCommit::OnEnter)
						{
							Entry->Name = *InText.ToString();
						}
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();

						// Make sure the new name only contains valid characters
						if (!FName::IsValidXName(NewString, INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorText))
						{
							return false;
						}

						FName Name(*NewString);

						if(FUtils::DoesParameterExistInLibrary(Entry->Library, Name))
						{
							OutErrorText = LOCTEXT("Error_NameExists", "This name already exists in the specified library");
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

			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SPinTypeSelector, FGetPinTypeTree::CreateStatic(&Editor::FUtils::GetFilteredVariableTypeTree))
						.TargetPinType_Lambda(GetPinInfo)
						.OnPinTypeChanged_Lambda(PinInfoChanged)
						.Schema(GetDefault<UPropertyBagSchema>())
						.bAllowArrays(true)
						.TypeTreeFilter(ETypeTreeFilter::None)
						.Font(IDetailLayoutBuilder::GetDetailFont())
				];
		}
		else if(InColumnName == Column_Library)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.IsEnabled(WeakDialog.Pin()->Library == nullptr)
				[
					SNew(SSimpleComboButton)
					.UsesSmallText(true)
					.HasDownArrow(true)
					.Text_Lambda([this]()
					{
						return FText::FromName(Entry->Library.PackageName);
					})
					.ToolTipText_Lambda([this]()
					{
						return FText::Format(LOCTEXT("LibraryTooltip", "The library of the new parameter.\n{0}"), FText::FromName(Entry->Library.PackageName));
					})
					.OnGetMenuContent(WeakDialog.Pin().Get(), &SAddParametersDialog::HandleGetAddParameterMenuContent, Entry)
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

	if(!ToolMenus->IsMenuRegistered(SelectLibraryMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(SelectLibraryMenuName);

		{
			FToolMenuSection& Section = Menu->AddSection("AddNewLibrary", LOCTEXT("AddNewLibraryMenuSection", "Add New Library"));
			Section.AddSubMenu("AddNewLibrary",
				LOCTEXT("AddNewLibraryLabel", "Add New Library"),
				LOCTEXT("AddNewLibraryTooltip", "Add a new parameter library"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
				{
					if(UAddParameterDialogMenuContext* MenuContext = InMenu->FindContext<UAddParameterDialogMenuContext>())
					{
						FPathPickerConfig PathPickerConfig;
						PathPickerConfig.bShowFavorites = false;
						PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([MenuContext](const FString& InPath)
						{
							FSlateApplication::Get().DismissAllMenus();

							if(TSharedPtr<FParameterToAdd> Entry = MenuContext->Entry.Pin())
							{
								IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
								if(UObject* NewAsset = AssetTools.CreateAsset(TEXT("NewParameterLibrary"), InPath, UAnimNextParameterLibrary::StaticClass(), NewObject<UAnimNextParameterLibraryFactory>()))
								{
									Entry->Library = FAssetData(NewAsset);

									UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
									Settings->SetLastLibrary(Entry->Library);
								}
							}
						});

						FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						FToolMenuSection& Section = InMenu->AddSection("ChooseLibrary", LOCTEXT("ChooseLibraryMenuSection", "Choose Existing Library"));
						Section.AddEntry(FToolMenuEntry::InitWidget(
								"LibraryPicker",
								SNew(SBox)
								.WidthOverride(300.0f)
								.HeightOverride(400.0f)
								[
									ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
								],
								FText::GetEmpty(),
								true));
					}
				}));
		}

		{
			

			FToolMenuSection& Section = Menu->AddDynamicSection("ChooseLibrary",  FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				if(UAddParameterDialogMenuContext* MenuContext = InMenu->FindContext<UAddParameterDialogMenuContext>())
				{
					FAssetPickerConfig AssetPickerConfig;
					AssetPickerConfig.SelectionMode = ESelectionMode::Single;
					AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
					AssetPickerConfig.Filter.ClassPaths = { UAnimNextParameterLibrary::StaticClass()->GetClassPathName() };
					AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([MenuContext](const FAssetData& InAssetData)
					{
						FSlateApplication::Get().DismissAllMenus();

						if(TSharedPtr<FParameterToAdd> Entry = MenuContext->Entry.Pin())
						{
							Entry->Library = InAssetData;

							UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
							Settings->SetLastLibrary(InAssetData);
						}
					});

					FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					FToolMenuSection& Section = InMenu->AddSection("ChooseLibrary", LOCTEXT("ChooseLibraryMenuSection", "Choose Existing Library"));
					Section.AddEntry(FToolMenuEntry::InitWidget(
							"LibraryPicker",
							SNew(SBox)
							.WidthOverride(300.0f)
							.HeightOverride(400.0f)
							[
								ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
							],
							FText::GetEmpty(),
							true));
				}
			}));
		}
	}

	UAddParameterDialogMenuContext* MenuContext = NewObject<UAddParameterDialogMenuContext>();
	MenuContext->AddParametersDialog = SharedThis(this);
	MenuContext->Entry = InEntry;
	return ToolMenus->GenerateWidget(SelectLibraryMenuName, FToolMenuContext(MenuContext));
}

}

#undef LOCTEXT_NAMESPACE