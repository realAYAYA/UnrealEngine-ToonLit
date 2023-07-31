// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MediaProfileMenuEntry.h"

#include "AssetEditor/MediaProfileCommands.h"
#include "AssetToolsModule.h"
#include "ClassViewerFilter.h"
#include "Factories/MediaProfileFactoryNew.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAssetTools.h"
#include "Kismet2/SClassPickerDialog.h"
#include "LevelEditor.h"
#include "Misc/FeedbackContext.h"
#include "Profile/IMediaProfileManager.h"
#include "Profile/MediaProfile.h"
#include "Profile/MediaProfileSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "ToolMenuEntry.h"
#include "ToolMenus.h"

#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "MediaProfileEditor"

struct FMediaProfileMenuEntryImpl
{
	FMediaProfileMenuEntryImpl()
	{
		TSharedPtr<FUICommandList> Actions = MakeShared<FUICommandList>();

		// Action to edit the current selected media profile
		Actions->MapAction(FMediaProfileCommands::Get().Edit,
			FExecuteAction::CreateLambda([this]()
			{
				if (UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile())
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(MediaProfile);
				}
			}),
			FCanExecuteAction::CreateLambda([] { return IMediaProfileManager::Get().GetCurrentMediaProfile() != nullptr; })
		);

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		FToolMenuSection& Section = Menu->FindOrAddSection("MediaProfile");

		auto ButtonTooltipLambda = [this]()
		{
			UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile();
			if (MediaProfile == nullptr)
			{
				return LOCTEXT("EmptyMediaProfile_ToolTip", "Select a Media Profile to edit it.");
			}
			return FText::Format(LOCTEXT("MediaProfile_ToolTip", "Edit '{0}'")
				, FText::FromName(MediaProfile->GetFName()));
		};

		// Add a button to edit the current media profile
		FToolMenuEntry MediaProfileButtonEntry = FToolMenuEntry::InitToolBarButton(
			FMediaProfileCommands::Get().Edit,
			LOCTEXT("MediaProfile_Label", "Media Profile"),
			MakeAttributeLambda(ButtonTooltipLambda),
			FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ToolbarIcon.MediaProfile"))
		);
		MediaProfileButtonEntry.SetCommandList(Actions);

		FToolMenuEntry MediaProfileMenuEntry = FToolMenuEntry::InitComboButton(
		"MediaProfileMenu",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FMediaProfileMenuEntryImpl::GenerateMenuContent),
		LOCTEXT("LevelEditorToolbarMediaProfileButtonLabel", "MediaProfile"),
		LOCTEXT("LevelEditorToolbarMediaProfileButtonTooltip", "Configure current MediaProfile"),
		FSlateIcon(),
		true //bInSimpleComboBox
		);

		MediaProfileMenuEntry.StyleNameOverride = "CalloutToolbar";
		Section.AddEntry(MediaProfileButtonEntry);
		Section.AddEntry(MediaProfileMenuEntry);
	}

	~FMediaProfileMenuEntryImpl()
	{
		if (!IsEngineExitRequested() && UObjectInitialized())
		{
			UToolMenus::Get()->RemoveSection("LevelEditor.LevelEditorToolBar.User", "MediaProfile");
		}
	}

	UMediaProfile* GetCurrentProfile()
	{
		return IMediaProfileManager::Get().GetCurrentMediaProfile();
	}

	void CreateNewProfile()
	{
		class FModifierClassFilter : public IClassViewerFilter
		{
		public:
			bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
			{
				return InClass->IsChildOf(UMediaProfile::StaticClass()) && !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown);
			}

			virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
			{
				return InClass->IsChildOf(UMediaProfile::StaticClass()) && !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown);
			}
		};

		const FText TitleText = LOCTEXT("CreateMediaProfileOptions", "Pick Media Profile Class");
		FClassViewerInitializationOptions Options;
		Options.ClassFilters.Add(MakeShared<FModifierClassFilter>());
		UClass* ChosenClass = nullptr;
		const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UMediaProfile::StaticClass());

		if (bPressedOk && ChosenClass != nullptr)
		{
			UMediaProfileFactoryNew* FactoryInstance = DuplicateObject<UMediaProfileFactoryNew>(GetDefault<UMediaProfileFactoryNew>(), GetTransientPackage());
			FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
			UMediaProfile* NewAsset = Cast<UMediaProfile>(FAssetToolsModule::GetModule().Get().CreateAssetWithDialog(ChosenClass, FactoryInstance));
			if (NewAsset != nullptr)
			{
				GetMutableDefault<UMediaProfileEditorSettings>()->SetUserMediaProfile(NewAsset);
				IMediaProfileManager::Get().SetCurrentMediaProfile(NewAsset);
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewAsset);
			}
		}
	}

	void NewProfileSelected(const FAssetData& AssetData)
	{
		FSlateApplication::Get().DismissAllMenus();

		GWarn->BeginSlowTask(LOCTEXT("MediaProfileLoadPackage", "Loading Media Profile"), true, false);
		UMediaProfile* Asset = Cast<UMediaProfile>(AssetData.GetAsset());
		GWarn->EndSlowTask();

		GetMutableDefault<UMediaProfileEditorSettings>()->SetUserMediaProfile(Asset);
		IMediaProfileManager::Get().SetCurrentMediaProfile(Asset);
	}

	TSharedRef<SWidget> GenerateMenuContent()
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

		MenuBuilder.BeginSection("Profile", LOCTEXT("NewMediaProfileSection", "New"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateMenuLabel", "New Empty Media Profile"),
				LOCTEXT("CreateMenuTooltip", "Create a new Media Profile asset."),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ClassIcon.MediaProfile")),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMediaProfileMenuEntryImpl::CreateNewProfile)
				)
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Profile", LOCTEXT("MediaProfileSection", "Media Profile"));
		{
			UMediaProfile* Profile = GetCurrentProfile();
			const bool bIsProfileValid = Profile != nullptr;

			MenuBuilder.AddSubMenu(
				bIsProfileValid ? FText::FromName(Profile->GetFName()) : LOCTEXT("SelectMenuLabel", "Select a Media Profile"),
				LOCTEXT("SelectMenuTooltip", "Select the current profile for this editor."),
				FNewMenuDelegate::CreateRaw(this, &FMediaProfileMenuEntryImpl::AddObjectSubMenu),
				FUIAction(),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	void AddObjectSubMenu(FMenuBuilder& MenuBuilder)
	{
		UMediaProfile* CurrentMediaProfile = GetCurrentProfile();
		FAssetData CurrentAssetData = CurrentMediaProfile ? FAssetData(CurrentMediaProfile) : FAssetData();

		TArray<const UClass*> ClassFilters;
		ClassFilters.Add(UMediaProfile::StaticClass());

		MenuBuilder.AddWidget(
			PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
				CurrentAssetData,
				CurrentMediaProfile != nullptr,
				false,
				ClassFilters,
				TArray<UFactory*>(),
				FOnShouldFilterAsset::CreateLambda([CurrentAssetData](const FAssetData& InAssetData){ return InAssetData == CurrentAssetData; }),
				FOnAssetSelected::CreateRaw(this, &FMediaProfileMenuEntryImpl::NewProfileSelected),
				FSimpleDelegate()
			),
			FText::GetEmpty(),
			true,
			false
		);
	}

public:

	static TUniquePtr<FMediaProfileMenuEntryImpl> Implementation;
};

TUniquePtr<FMediaProfileMenuEntryImpl> FMediaProfileMenuEntryImpl::Implementation;

void FMediaProfileMenuEntry::Register()
{
	if (!IsRunningCommandlet() && GetDefault<UMediaProfileEditorSettings>()->bDisplayInToolbar)
	{
		FMediaProfileMenuEntryImpl::Implementation = MakeUnique<FMediaProfileMenuEntryImpl>();
	}
}

void FMediaProfileMenuEntry::Unregister()
{
	FMediaProfileMenuEntryImpl::Implementation.Reset();
}

#undef LOCTEXT_NAMESPACE
