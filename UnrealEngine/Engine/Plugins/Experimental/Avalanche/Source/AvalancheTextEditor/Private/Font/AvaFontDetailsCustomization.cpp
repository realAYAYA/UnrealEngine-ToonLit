// Copyright Epic Games, Inc. All Rights Reserved.

#include "Font/AvaFontDetailsCustomization.h"
#include "AvaFontManagerSubsystem.h"
#include "AvaFontView.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Font/AvaFont.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Slate/SAvaFontField.h"
#include "Slate/SAvaFontSelector.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaFontDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FAvaFontDetailsCustomization::MakeInstance()
{
	// Create the instance and return a SharedRef
	return MakeShared<FAvaFontDetailsCustomization>();
}

FAvaFontDetailsCustomization::~FAvaFontDetailsCustomization()
{
	UnregisterFontManagerCallbacks();
}

void FAvaFontDetailsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> FontStructPropertyHandle
	, FDetailWidgetRow& HeaderRow
	, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	AvaFontPropertyHandle = FontStructPropertyHandle;

	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!ensureMsgf(FontManagerSubsystem, TEXT("Font Manager is unavailable!")))
	{
		return;
	}

	FontManagerSubsystemWeak = TWeakObjectPtr<UAvaFontManagerSubsystem>(FontManagerSubsystem);

	// Refresh also works as a main initialization method for available options and current font
	// Calling it before FontSelector since it sets up data needed for its initialization
	Refresh();

	SAssignNew(MissingFontText, STextBlock)
		.Justification(ETextJustify::Right)
		.Visibility(EVisibility::Collapsed);

	SAssignNew(ComboBoxContent, SBox)
		.HAlign(HAlign_Fill);

	bool bIsShowMonospacedActive = false;
	bool bIsShowBoldActive = false;
	bool bIsShowItalicActive = false;

	if (const UAvaFontConfig* FontManagerConfig = FontManagerSubsystem->GetFontManagerConfig())
	{
		bIsShowMonospacedActive = FontManagerConfig->bShowOnlyMonospaced;
		bIsShowBoldActive = FontManagerConfig->bShowOnlyBold;
		bIsShowItalicActive = FontManagerConfig->bShowOnlyItalic;
	}

	SAssignNew(FontSelector, SAvaFontSelector)
		.ShowMonospacedFontsOnly(bIsShowMonospacedActive)
		.ShowBoldFontsOnly(bIsShowBoldActive)
		.ShowItalicFontsOnly(bIsShowItalicActive)
		.OptionsSource(&Options)
		.InitiallySelectedItem(SelectedOption)
		.FontPropertyHandle(AvaFontPropertyHandle)
		.OnGenerateWidget(this, &FAvaFontDetailsCustomization::HandleGenerateWidget)
		.OnSelectionChanged(this, &FAvaFontDetailsCustomization::HandleSelectionChanged)
		[
			ComboBoxContent.ToSharedRef()
		];

	HeaderRow.NameContent()[FontStructPropertyHandle->CreatePropertyNameWidget()]
	.ValueContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.FillWidth(1.f)
			[
				FontSelector.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				MakeImportButton()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				MakeBrowseButton()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				MakeRefreshFontButton()
			]
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			[
				MissingFontText.ToSharedRef()
			]
		]
	];

	RegisterFontManagerCallbacks();
	FontStructPropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateSP(this, &FAvaFontDetailsCustomization::SetDefaultValue));

	RefreshComboboxContent();
	RefreshMissingFontWidget();
}

void FAvaFontDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FAvaFontDetailsCustomization::RefreshSelectedFont()
{
	const UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	FPropertyAccess::Result AccessResult;
	const TSharedPtr<FAvaFontView> CurrAvaFont = GetCurrentAvaFontView(AccessResult);

	bool bLoadDefault = false;

	if (AccessResult == FPropertyAccess::Fail)
	{
		bLoadDefault = true;
	}
	else
	{
		FAvaMultiFontSelectionData MultiFontSelectionData;
		FontManagerSubsystem->GetMultipleSelectionInformation(AvaFontPropertyHandle, MultiFontSelectionData);

		if (MultiFontSelectionData.bMultipleValues && ComboBoxContent)
		{
			ComboBoxContent->SetContent(GenerateMultipleSelectionField());
			bImportEnabled = false;
			bRefreshFontEnabled = false;
			return;
		}

		// If there's a font already selected, update the option ptr accordingly
		if (CurrAvaFont)
		{
			const UAvaFontObject* CurrFontObj = CurrAvaFont->GetFontObject();

			if (CurrFontObj && CurrFontObj->GetFont())
			{
				TSharedPtr<FAvaFontView> Option;

				const bool bCurrFontObjectMatchesOption = Options.ContainsByPredicate([&Option, CurrFontObj](const TSharedPtr<FAvaFontView>& AvaFontValue)
				{
					if (AvaFontValue.IsValid() && AvaFontValue->HasValidFont())
					{
						const bool bContains = AvaFontValue->GetFontObject()->GetFontName() == CurrFontObj->GetFontName();

						if (bContains)
						{
							Option = AvaFontValue;
						}

						return bContains;
					}

					return false;
				});

				if (bCurrFontObjectMatchesOption) // This bool should also guard Option validity
				{
					HandleSelectionChanged(Option, ESelectInfo::Direct);
				}
			}
			else
			{
				// Font is not available, try to load a default option
				bLoadDefault = true;
			}
		}
	}

	if (bLoadDefault)
	{
		SetDefaultValue();
	}
}

void FAvaFontDetailsCustomization::RefreshLocalFontOptions()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	Options = FontManagerSubsystem->GetFontOptions();

	if (!Options.IsEmpty())
	{
		const int32 DefaultOptionIndex = FontManagerSubsystem->GetDefaultFontIndex();

		if (DefaultOptionIndex >= 0)
		{
			DefaultOption = Options[DefaultOptionIndex];
		}
		else
		{
			DefaultOption = Options[0];
		}
	}
}

void FAvaFontDetailsCustomization::Refresh()
{
	RefreshLocalFontOptions();
	RefreshSelectedFont();

	if (FontSelector)
	{
		FontSelector->RefreshOptions();
	}
}

void FAvaFontDetailsCustomization::SetDefaultValue()
{
	if (DefaultOption.IsValid())
	{
		HandleSelectionChanged(DefaultOption, ESelectInfo::Direct);
	}
}

FReply FAvaFontDetailsCustomization::ImportButtonClicked()
{
	FPropertyAccess::Result AccessResult;

	if (const TSharedPtr<FAvaFontView> CurrAvaFont = GetCurrentAvaFontView(AccessResult))
	{
		const UAvaFontObject* CurrFontObj = CurrAvaFont->GetFontObject();

		UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();

		if (IsValid(CurrFontObj) && FontManagerSubsystem)
		{
			if (FontManagerSubsystem->ImportFont(AvaFontPropertyHandle))
			{
				AvaFontPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				Refresh();
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> FAvaFontDetailsCustomization::HandleGenerateWidget(const TSharedPtr<FAvaFontView>& InItem)
{
	return SNew(SAvaFontField)
			.OnFontFieldModified(this, &FAvaFontDetailsCustomization::OnFontFieldUpdated)
			.FontView(InItem);
}

TSharedRef<SWidget> FAvaFontDetailsCustomization::GenerateMultipleSelectionField() const
{
	return SNew(STextBlock)
			.Text(FText::FromString("Multiple Values"));
}

FReply FAvaFontDetailsCustomization::OnBrowseToAssetClicked()
{
	if (AvaFontPropertyHandle.IsValid())
	{
		FPropertyAccess::Result AccessResult;
		if (const TSharedPtr<FAvaFontView> CurrentAvaFont = GetCurrentAvaFontView(AccessResult))
		{
			UFont* CurrentFont = CurrentAvaFont->GetFont();

			const TArray<UObject*> ObjectsToFocus {CurrentFont};
			GEditor->SyncBrowserToObjects(ObjectsToFocus);
		}
	}

	return FReply::Handled();
}

FReply FAvaFontDetailsCustomization::OnRefreshFontClicked()
{
	if (AvaFontPropertyHandle.IsValid())
	{
		FPropertyAccess::Result AccessResult;
		if (const TSharedPtr<FAvaFontView> CurrentAvaFont = GetCurrentAvaFontView(AccessResult))
		{
			UFont* CurrentFont = CurrentAvaFont->GetFont();

			if (UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get())
			{
				FontManagerSubsystem->RefreshSystemFont(CurrentFont);
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> FAvaFontDetailsCustomization::MakeBrowseButton()
{
	const FSlateBrush* BrowseIcon = FAppStyle::Get().GetBrush("Icons.BrowseContent");

	return
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT( "BrowseButtonToolTipText", "Browse to Font asset in Content Browser"))
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked(this, &FAvaFontDetailsCustomization::OnBrowseToAssetClicked)
			.ContentPadding(0)
			.Visibility_Lambda([this](){ return !bImportEnabled ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SImage)
				.Image(BrowseIcon)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

TSharedRef<SWidget> FAvaFontDetailsCustomization::MakeRefreshFontButton()
{
	const FSlateBrush* BrowseIcon = FAppStyle::Get().GetBrush("EditorViewport.RotateMode");

	return
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT( "RefreshFontButtonToolTipText", "Refresh font"))
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked(this, &FAvaFontDetailsCustomization::OnRefreshFontClicked)
			.ContentPadding(0)
			.Visibility_Lambda([this](){ return bRefreshFontEnabled ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SImage)
				.Image(BrowseIcon)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

TSharedRef<SWidget> FAvaFontDetailsCustomization::MakeImportButton()
{
	return
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.WidthOverride(22)
		.HeightOverride(22)
		.ToolTipText(LOCTEXT("ImportSystemFont", "Import system font to project"))
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton" )
			.OnClicked(this, &FAvaFontDetailsCustomization::ImportButtonClicked)
			.ContentPadding(0)
			.Visibility_Lambda([this](){ return bImportEnabled ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Import"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

void FAvaFontDetailsCustomization::UpdateOrGenerateWidget(const TSharedPtr<FAvaFontView>& InItem)
{
	const UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	if (ComboBoxContent)
	{
		FAvaMultiFontSelectionData MultiFontSelectionData;
		FontManagerSubsystem->GetMultipleSelectionInformation(AvaFontPropertyHandle, MultiFontSelectionData);

		if (MultiFontSelectionData.bMultipleValues)
		{
			ComboBoxContent->SetContent(GenerateMultipleSelectionField());
			bImportEnabled = false;
			bRefreshFontEnabled = false;
		}
		else
		{
			const TSharedRef<SAvaFontField> FontFieldWidget = StaticCastSharedRef<SAvaFontField>(HandleGenerateWidget(InItem));
			FontFieldWidget->Select();
			ComboBoxContent->SetContent(FontFieldWidget);
		}
	}
}

void FAvaFontDetailsCustomization::RefreshComboboxContent()
{
	if (ComboBoxContent.IsValid())
	{
		UpdateOrGenerateWidget(SelectedOption);
	}
}

void FAvaFontDetailsCustomization::HandleSelectionChanged(const TSharedPtr<FAvaFontView> InItem, ESelectInfo::Type InSelectionType)
{
	// Sometimes when removing an item from favorites, this gets triggered, but the item is not there anymore,
	// so we need to check if the callback Item argument is actually valid
	if (InItem && InItem != SelectedOption)
	{
		SelectedOption = InItem;

		if (InItem->GetFontObject())
		{
			UAvaFontObject* const FontObjectSlateAttribute = InItem->GetFontObject();

			if (IsValid(FontObjectSlateAttribute))
			{
				UpdateFontObjectProperty(FontObjectSlateAttribute);
				UpdateButtonsStatus(SelectedOption);

				UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
                if (!FontManagerSubsystem)
                {
                	return;
                }

				if (SelectedOption.IsValid())
				{
					// If the newly selected font obj is not yet imported, let's mark it for auto save
					if (SelectedOption->GetFontSource() == EAvaFontSource::System)
					{
						FontManagerSubsystem->MarkFontForAutoImport(AvaFontPropertyHandle);
					}
				}
			}
		}

		// When the selection changes we always generate another widget to represent the content area of the combobox.
		RefreshComboboxContent();

		RefreshMissingFontWidget();
	}
}

void FAvaFontDetailsCustomization::OnFontFieldUpdated(const TSharedPtr<FAvaFontView>& InItem) const
{
	if (FontSelector)
	{
		FontSelector->RefreshOptions();
	}
}

void FAvaFontDetailsCustomization::UpdateFontObjectProperty(UAvaFontObject* InFontObject)
{
	check(InFontObject);

	if (!AvaFontPropertyHandle)
	{
		return;
	}

	TArray<UObject*> OuterObjects;
	AvaFontPropertyHandle->GetOuterObjects(OuterObjects);

	FPropertyAccess::Result AccessResult;
	const TSharedPtr<FAvaFontView> CurrAvaFontView = GetCurrentAvaFontView(AccessResult);

	if (AccessResult == FPropertyAccess::Success || !CurrAvaFontView)
	{
		bool bFontObjNeedsUpdate;

		const UAvaFontObject* CurrFontObjectPtr = nullptr;
		if (CurrAvaFontView)
		{
			CurrFontObjectPtr = CurrAvaFontView->GetFontObject();
		}

		if (!CurrFontObjectPtr)
		{
			bFontObjNeedsUpdate = true;
		}
		else
		{
			bFontObjNeedsUpdate = CurrFontObjectPtr->GetFontName() != InFontObject->GetFontName();
		}

		if (bFontObjNeedsUpdate)
		{
			if (!OuterObjects.IsEmpty())
			{
				for (int32 Index = 0; Index < OuterObjects.Num(); Index++)
				{
					if (UObject* Object = OuterObjects[Index])
					{
						UAvaFontObject* NewFontObj = NewObject<UAvaFontObject>(Object, InFontObject->GetFName(), RF_Public);
						NewFontObj->InitFromFontObject(InFontObject);

						FString ValueString;
						FAvaFont::GenerateFontFormattedString(NewFontObj, ValueString);

						AvaFontPropertyHandle->SetPerObjectValue(Index, ValueString);
					}
				}
			}
			else
			{
				// No direct outer(s), so we try to find the closest one (e.g. this happens for ava fonts in RC Panel)

				UAvaFontObject* SourceFontObject = nullptr;
				if (FProperty* Property = AvaFontPropertyHandle->GetProperty())
				{
					if (const FProperty* OwnerProperty = Property->GetOwnerProperty())
					{
						if (OwnerProperty->Owner.IsUObject())
						{
							if (UObject* Outer = OwnerProperty->Owner.ToUObject())
							{
								SourceFontObject = NewObject<UAvaFontObject>(Outer, InFontObject->GetFName(), RF_Public);
								SourceFontObject->InitFromFontObject(InFontObject);
							}
						}
					}
				}

				// If we couldn't reach an outer, let's use the input font object, which might be transient
				if (!SourceFontObject)
				{
					SourceFontObject = InFontObject;
				}

				FString ValueString;
				bool bStringSuccess = FAvaFont::GenerateFontFormattedString(SourceFontObject, ValueString);

				if (bStringSuccess)
				{
					AvaFontPropertyHandle->SetPerObjectValues({ValueString});
				}
			}

			AvaFontPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}
	// MultipleValues needs to be handled differently, since we want to act on all selected objects
	else if (AccessResult == FPropertyAccess::MultipleValues)
	{
		AccessResult = FPropertyAccess::MultipleValues;

		const UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
		if (!FontManagerSubsystem)
		{
			return;
		}

		FAvaMultiFontSelectionData MultiFontSelectionData;
		FontManagerSubsystem->GetMultipleSelectionInformation(AvaFontPropertyHandle, MultiFontSelectionData);

		const bool bFontObjNeedsUpdate = InFontObject->GetFontName() != MultiFontSelectionData.FirstSelectionFontName;

		if (bFontObjNeedsUpdate)
		{
			TArray<FString> PerObjectValues;

			for (UObject* const OuterObject : OuterObjects)
			{
				UAvaFontObject* NewFontObj = NewObject<UAvaFontObject>(OuterObject, InFontObject->GetFName(), RF_Public);
				NewFontObj->InitFromFontObject(InFontObject);

				FString ValueString;
				FAvaFont::GenerateFontFormattedString(NewFontObj, ValueString);

				PerObjectValues.Add(ValueString);
			}

			AvaFontPropertyHandle->SetPerObjectValues(PerObjectValues);
		}
	}
}

TSharedPtr<FAvaFontView> FAvaFontDetailsCustomization::GetCurrentAvaFontView(FPropertyAccess::Result& OutAccessResult)
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return nullptr;
	}

	CurrentAvaFontValue = FontManagerSubsystem->GetFontViewFromPropertyHandle(AvaFontPropertyHandle, OutAccessResult);
	return CurrentAvaFontValue;
}

FAvaFont* FAvaFontDetailsCustomization::GetCurrentAvaFont(FPropertyAccess::Result& OutAccessResult) const
{
	const UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return nullptr;
	}

	return FontManagerSubsystem->GetFontFromPropertyHandle(AvaFontPropertyHandle, OutAccessResult);
}

void FAvaFontDetailsCustomization::UpdateButtonsStatus(const TSharedPtr<FAvaFontView>& InAvaFont)
{
	if (InAvaFont.IsValid() && InAvaFont->GetFontSource() == EAvaFontSource::System)
	{
		bImportEnabled = true;
		bRefreshFontEnabled = false;
	}
	else
	{
		bImportEnabled = false;
		if (const UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get())
		{
			bRefreshFontEnabled = FontManagerSubsystem->IsFontAvailableOnLocalOS(InAvaFont->GetFont());
		}
	}
}

void FAvaFontDetailsCustomization::OnProjectFontCreated(const UFont* InFont)
{
	Refresh();
}

void FAvaFontDetailsCustomization::OnProjectFontDeleted(const UFont* InFont)
{
	FPropertyAccess::Result AccessResult;
	if (const TSharedPtr<FAvaFontView> CurrAvaFont = GetCurrentAvaFontView(AccessResult))
	{
		const UAvaFontObject* CurrFontObj = CurrAvaFont->GetFontObject();

		if (CurrFontObj && CurrFontObj->GetFont() == InFont)
		{
			SetDefaultValue();

			if (AvaFontPropertyHandle)
			{
				AvaFontPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			}
		}
	}

	Refresh();
}

void FAvaFontDetailsCustomization::OnSystemFontsUpdated()
{
	Refresh();

	if (FontSelector.IsValid())
	{
		FontSelector->RefreshOptions();
	}
}

void FAvaFontDetailsCustomization::RegisterFontManagerCallbacks()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	FontManagerSubsystem->OnProjectFontCreated().AddRaw(this, &FAvaFontDetailsCustomization::OnProjectFontCreated);
	FontManagerSubsystem->OnProjectFontDeleted().AddRaw(this, &FAvaFontDetailsCustomization::OnProjectFontDeleted);
	FontManagerSubsystem->OnSystemFontsUpdated().AddRaw(this, &FAvaFontDetailsCustomization::OnSystemFontsUpdated);
}

void FAvaFontDetailsCustomization::UnregisterFontManagerCallbacks() const
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = FontManagerSubsystemWeak.Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	FontManagerSubsystem->OnProjectFontCreated().RemoveAll(this);
	FontManagerSubsystem->OnProjectFontDeleted().RemoveAll(this);
	FontManagerSubsystem->OnSystemFontsUpdated().RemoveAll(this);
}

void FAvaFontDetailsCustomization::RefreshMissingFontWidget() const
{
	if (SelectedOption && MissingFontText)
	{
		// Check if the font is in fallback state - this happens when a font asset could not be loaded (e.g. missing from disk)
		if (SelectedOption->IsFallbackFont())
		{
			MissingFontText->SetVisibility(EVisibility::Visible);

			const FText FontName = FText::FromString(*SelectedOption->GetMissingFontName());
			const FText Message = FText::Format(LOCTEXT("MissingFontMessage", "Failed to load font {0}"), FontName);

			MissingFontText->SetText(Message);
			MissingFontText->SetColorAndOpacity(FLinearColor::Red);
		}
		else
		{
			MissingFontText->SetVisibility(EVisibility::Collapsed);
		}
	}
}

#undef LOCTEXT_NAMESPACE
