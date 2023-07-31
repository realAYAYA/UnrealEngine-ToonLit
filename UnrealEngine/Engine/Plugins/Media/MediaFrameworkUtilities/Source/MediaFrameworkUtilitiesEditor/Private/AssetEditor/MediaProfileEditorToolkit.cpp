// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetEditor/MediaProfileEditorToolkit.h"
#include "Profile/MediaProfile.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "MediaProfileCommands.h"
#include "Misc/MessageDialog.h"
#include "Profile/MediaProfileSettings.h"
#include "Profile/IMediaProfileManager.h"

#define LOCTEXT_NAMESPACE "MediaProfileEditor"

TSharedRef<FMediaProfileEditorToolkit> FMediaProfileEditorToolkit::CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UMediaProfile* InMediaProfile)
{
	TSharedRef<FMediaProfileEditorToolkit> NewEditor = MakeShared<FMediaProfileEditorToolkit>();

	NewEditor->InitMediaProfileEditor(Mode, InitToolkitHost, InMediaProfile);

	return NewEditor;
}

void FMediaProfileEditorToolkit::InitMediaProfileEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMediaProfile* InMediaProfile)
{
	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(InMediaProfile);

	Super::InitEditor(Mode, InitToolkitHost, ObjectsToEdit, FGetDetailsViewObjects());

	bSubPropertyWasModified = false;

	BindCommands();

	ExtendToolBar();

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FMediaProfileEditorToolkit::HandleCoreObjectPropertyChanged);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddSP(this, &FMediaProfileEditorToolkit::HandleCorePreObjectPropertyChanged);
	GetMutableDefault<UMediaProfileSettings>()->OnMediaProxiesChanged.AddSP(this, &FMediaProfileEditorToolkit::HandleMediaProxiesChanged);
}

FMediaProfileEditorToolkit::~FMediaProfileEditorToolkit()
{
	GetMutableDefault<UMediaProfileSettings>()->OnMediaProxiesChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

void FMediaProfileEditorToolkit::ExtendToolBar()
{
	TSharedRef<FExtender> ToolbarExtender = MakeShared<FExtender>();

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic([](FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Apply");
			{
				ToolbarBuilder.AddToolBarButton(FMediaProfileCommands::Get().Apply);
			}
		})
	);

	AddToolbarExtender(ToolbarExtender);

	RegenerateMenusAndToolbars();
}

void FMediaProfileEditorToolkit::BindCommands()
{
	ToolkitCommands->MapAction(
		FMediaProfileCommands::Get().Apply,
		FExecuteAction::CreateSP(this, &FMediaProfileEditorToolkit::ApplyMediaProfile),
		FCanExecuteAction::CreateLambda([this]()
			{
				if (UMediaProfile* MediaProfile = Cast<UMediaProfile>(GetEditingObject()))
				{
					return MediaProfile->bNeedToBeReapplied;
				}
				return false;
			})
	);
}

void FMediaProfileEditorToolkit::ApplyMediaProfile()
{
	UMediaProfile* MediaProfile = Cast<UMediaProfile>(GetEditingObject());
	IMediaProfileManager::Get().SetCurrentMediaProfile(nullptr);

	if (MediaProfile != nullptr)
	{
		IMediaProfileManager::Get().SetCurrentMediaProfile(MediaProfile);
	}
}

void FMediaProfileEditorToolkit::HandleCorePreObjectPropertyChanged(UObject* Object, const FEditPropertyChain& EditPropertyChain)
{
	if (Object == GetEditingObject())
	{
		bSubPropertyWasModified = false;
	}
}


void FMediaProfileEditorToolkit::HandleCoreObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& ChangedEvent)
{
	if (Object == nullptr)
	{
		return;
	}

	if (Object == GetEditingObject())
	{
		// We don't want to display apply if a sub property was modified (ie. FilePath in a FileMediaSource),
		// So only show the apply button if the ObjectPropertyChanged was fired for a direct property change on MediaProfile.
		if (!bSubPropertyWasModified)
		{
			if (UMediaProfile* MediaProfile = Cast<UMediaProfile>(GetEditingObject()))
			{
				MediaProfile->bNeedToBeReapplied = true;
			}
		}

		bSubPropertyWasModified = false;
	}
	else if (Object->GetOuter() == GetEditingObject())
	{
		bSubPropertyWasModified = true;
	}
}


void FMediaProfileEditorToolkit::HandleMediaProxiesChanged()
{
	CloseWindow();
}


void FMediaProfileEditorToolkit::SaveAsset_Execute()
{
	ApplyMediaProfile();
	Super::SaveAsset_Execute();
}

void FMediaProfileEditorToolkit::SaveAssetAs_Execute()
{
	ApplyMediaProfile();
	Super::SaveAssetAs_Execute();
}

bool FMediaProfileEditorToolkit::OnRequestClose()
{
	bool bClose = true;
	if (UMediaProfile* MediaProfile = Cast<UMediaProfile>(GetEditingObject()))
	{
		if (MediaProfile->bNeedToBeReapplied)
		{
			// find out the user wants to do with this dirty profile
			EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNoCancel, LOCTEXT("Prompt_EditorClose", "Would you like to apply the modifications?"));

			switch (YesNoCancelReply)
			{
			case EAppReturnType::Yes:
				ApplyMediaProfile();
				break;
			case EAppReturnType::Cancel:
				bClose = false;
				break;
			}
		}
	}
	return Super::OnRequestClose();
}

#undef LOCTEXT_NAMESPACE