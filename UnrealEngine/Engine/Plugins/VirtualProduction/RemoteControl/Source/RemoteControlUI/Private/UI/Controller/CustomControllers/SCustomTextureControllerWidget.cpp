// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCustomTextureControllerWidget.h"
#include "Action/Bind/RCCustomBindActionUtilities.h"
#include "AssetThumbnail.h"
#include "Controller/RCController.h"
#include "EditorDirectories.h"
#include "Engine/Texture2D.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "RCVirtualProperty.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "ExternalTextureControllerWidget"

void SCustomTextureControllerWidget::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InOriginalPropertyHandle)
{
	if (!InOriginalPropertyHandle.IsValid())
	{
		return;
	}

	OriginalPropertyHandle = InOriginalPropertyHandle;

	FString ControllerString;
	OriginalPropertyHandle->GetValueAsFormattedString(ControllerString);
	bInternal = FPackageName::IsValidTextForLongPackageName(ControllerString);
	if (FPackageName::IsValidTextForLongPackageName(ControllerString))
	{
		bInternal = true;
		CurrentAssetPath = ControllerString;
	}
	else
	{
		bInternal = false;
		CurrentExternalPath = ControllerString;
	}

	ControllerTypes.Add(MakeShared<FString>(TEXT("External")));
	ControllerTypes.Add(MakeShared<FString>(TEXT("Asset")));
	
	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(ThumbnailWidgetBox, SBox)
			.WidthOverride(64)
			.HeightOverride(64)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.05)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.6)
		[
			SAssignNew(ValueWidgetBox, SBox)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextComboBox)
			.OptionsSource(&ControllerTypes)
			.OnSelectionChanged(this, &SCustomTextureControllerWidget::OnControllerTypeChanged)
			.InitiallySelectedItem(ControllerTypes[bInternal])
		]
	];

	UpdateValueWidget();
	UpdateThumbnailWidget();
}

FString SCustomTextureControllerWidget::GetFilePath() const
{
	return CurrentExternalPath;
}

FString SCustomTextureControllerWidget::GetAssetPath() const
{
	return CurrentAssetPath;
}

FString SCustomTextureControllerWidget::GetAssetPathName() const
{
	if (Texture)
	{
		return Texture->GetPathName();
	}

	return TEXT("");
}

void SCustomTextureControllerWidget::OnControllerTypeChanged(TSharedPtr<FString, ESPMode::ThreadSafe> InString, ESelectInfo::Type InArg)
{
	const FString& Selection = *InString.Get();
	
	if (Selection == TEXT("Asset"))
	{
		bInternal = true;
	}
	if (Selection == TEXT("External"))
	{
		bInternal = false;
	}

	Texture = nullptr;
	
	UpdateValueWidget();
	UpdateThumbnailWidget();
	UpdateControllerValue();
}

void SCustomTextureControllerWidget::OnAssetSelected(const FAssetData& AssetData)
{
	UObject* TextureAsset = AssetData.GetAsset();
	if (TextureAsset != nullptr)
	{
		if (TextureAsset->IsA<UTexture2D>())
		{
			Texture = Cast<UTexture2D>(TextureAsset);
			CurrentAssetPath = AssetData.PackageName.ToString();
			UpdateControllerValue();
		}
	}
}

FString SCustomTextureControllerWidget::GetCurrentPath() const
{
	if (bInternal)
	{
		return GetAssetPath();
	}
	else
	{
		return GetFilePath();
	}
}

void SCustomTextureControllerWidget::UpdateControllerValue()
{
	const FString& Path = GetCurrentPath();

	if (OriginalPropertyHandle.IsValid())
	{
		OriginalPropertyHandle->SetValueFromFormattedString(Path);
	}
	RefreshThumbnailImage();
}

void SCustomTextureControllerWidget::HandleFilePathPickerPathPicked(const FString& InPickedPath)
{
	// The following code is adapted from FFilePath customization
	FString FinalPath = InPickedPath;

	// Keeping these here in case need additional customization
	bool bLongPackageName = false;
	bool bRelativeToGameDir = false;
	
	if (bLongPackageName)
	{
		FString LongPackageName;
		FString StringFailureReason;
		if (FPackageName::TryConvertFilenameToLongPackageName(InPickedPath, LongPackageName, &StringFailureReason) == false)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(StringFailureReason));
		}
		FinalPath = LongPackageName;
	}
	else if (bRelativeToGameDir && !InPickedPath.IsEmpty())
	{
		// A filepath under the project directory will be made relative to the project directory
		// Otherwise, the absolute path will be returned unless it doesn't exist, the current path will
		// be kept. This can happen if it's already relative to project dir (tabbing when selected)

		const FString ProjectDir = FPaths::ProjectDir();
		const FString AbsoluteProjectDir = FPaths::ConvertRelativePathToFull(ProjectDir);
		const FString AbsolutePickedPath = FPaths::ConvertRelativePathToFull(InPickedPath);

		// Verify if absolute path to file exists. If it was already relative to content directory
		// the absolute will be to binaries and will possibly be garbage
		if (FPaths::FileExists(AbsolutePickedPath))
		{
			// If file is part of the project dir, chop the project dir part
			// Otherwise, use the absolute path
			if (AbsolutePickedPath.StartsWith(AbsoluteProjectDir))
			{
				FinalPath = AbsolutePickedPath.RightChop(AbsoluteProjectDir.Len());
			}
			else
			{
				FinalPath = AbsolutePickedPath;
			}
		}
		else
		{
			// If absolute file doesn't exist, it might already be relative to project dir
			// If not, then it might be a manual entry, so keep it untouched either way
			FinalPath = InPickedPath;
		}
	}

	CurrentExternalPath = FinalPath;
	
	UpdateControllerValue();
	
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InPickedPath));
}

TSharedRef<SWidget> SCustomTextureControllerWidget::GetAssetThumbnailWidget()
{
	if (!Texture)
	{
		RefreshThumbnailImage();
	}
	
	const TSharedRef<FAssetThumbnail> AssetThumbnail = MakeShared<FAssetThumbnail>(Texture, 64, 64, UThumbnailManager::Get().GetSharedThumbnailPool());
	return AssetThumbnail->MakeThumbnailWidget();
}

TSharedRef<SWidget> SCustomTextureControllerWidget::GetExternalTextureValueWidget()
{
	static const FString FileTypeFilter = TEXT("Image files (*.jpg; *.png; *.bmp; *.ico; *.exr; *.icns; *.jpeg; *.tga; *.hdr)|*.jpg; *.png; *.bmp; *.ico; *.exr; *.icns; *.jpeg; *.tga; *.hdr");
	
	return SNew(SFilePathPicker)
			.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a file from this computer"))
			.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
			.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
			.FilePath(this, &SCustomTextureControllerWidget::GetFilePath)
			.FileTypeFilter(FileTypeFilter)
			.OnPathPicked(this, &SCustomTextureControllerWidget::HandleFilePathPickerPathPicked);
}

TSharedRef<SWidget> SCustomTextureControllerWidget::GetAssetTextureValueWidget()
{
	return SNew(SObjectPropertyEntryBox)
			.AllowedClass(UTexture2D::StaticClass())
			.OnObjectChanged(this, &SCustomTextureControllerWidget::OnAssetSelected)
			.ObjectPath(this, &SCustomTextureControllerWidget::GetAssetPathName)
			.DisplayUseSelected(true)
			.DisplayBrowse(true);
}

void SCustomTextureControllerWidget::UpdateValueWidget()
{
	if (!ValueWidgetBox)
	{
		return;
	}
	
	if (bInternal)
	{
		ValueWidgetBox->SetContent(GetAssetTextureValueWidget());
	}
	else
	{
		ValueWidgetBox->SetContent(GetExternalTextureValueWidget());
	}
}

void SCustomTextureControllerWidget::UpdateThumbnailWidget()
{
	if (!ThumbnailWidgetBox)
	{
		return;
	}
	
	ThumbnailWidgetBox->SetContent(GetAssetThumbnailWidget());
}

void SCustomTextureControllerWidget::RefreshThumbnailImage()
{
	if (UTexture2D* LoadedTexture = UE::RCCustomBindActionUtilities::LoadTextureFromPath(GetCurrentPath()))
	{
		Texture = LoadedTexture;
		UpdateThumbnailWidget();
	}
}

#undef LOCTEXT_NAMESPACE
