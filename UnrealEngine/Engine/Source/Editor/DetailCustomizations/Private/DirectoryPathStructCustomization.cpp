// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirectoryPathStructCustomization.h"

#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Delegates/Delegate.h"
#include "DesktopPlatformModule.h"
#include "DetailWidgetRow.h"
#include "EditorDirectories.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/SlateDelegates.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "HAL/PlatformMisc.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Layout/WidgetPath.h"
#include "Misc/Attribute.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

class SWidget;

#define LOCTEXT_NAMESPACE "DirectoryPathStructCustomization"

TSharedRef<IPropertyTypeCustomization> FDirectoryPathStructCustomization::MakeInstance()
{
	return MakeShareable(new FDirectoryPathStructCustomization());
}

void FDirectoryPathStructCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> PathProperty = StructPropertyHandle->GetChildHandle("Path");

	const bool bRelativeToGameContentDir = StructPropertyHandle->HasMetaData( TEXT("RelativeToGameContentDir") );
	const bool bUseRelativePath = StructPropertyHandle->HasMetaData( TEXT("RelativePath") );
	const bool bContentDir = StructPropertyHandle->HasMetaData( TEXT("ContentDir") ) || StructPropertyHandle->HasMetaData(TEXT("LongPackageName"));

	AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

	if(PathProperty.IsValid())
	{
		TSharedPtr<SWidget> PickerWidget = nullptr;

		if(bContentDir)
		{
			PickerWidget = SAssignNew(PickerButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "HoverHintOnly" )
			.ToolTipText( LOCTEXT( "FolderComboToolTipText", "Choose a content directory") )
			.OnClicked( this, &FDirectoryPathStructCustomization::OnPickContent, PathProperty.ToSharedRef() )
			.ContentPadding(2.0f)
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable(false)
			.IsEnabled(this, &FDirectoryPathStructCustomization::IsBrowseEnabled, StructPropertyHandle)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];

		}
		else
		{
			PickerWidget = SAssignNew(BrowseButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "HoverHintOnly" )
			.ToolTipText( LOCTEXT( "FolderButtonToolTipText", "Choose a directory from this computer") )
			.OnClicked( this, &FDirectoryPathStructCustomization::OnPickDirectory, PathProperty.ToSharedRef(), bRelativeToGameContentDir, bUseRelativePath )
			.ContentPadding( 2.0f )
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable( false )
			.IsEnabled( this, &FDirectoryPathStructCustomization::IsBrowseEnabled, StructPropertyHandle )
			[
				SNew( SImage )
				.Image( FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			];
		}

		HeaderRow.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(600.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				PathProperty->CreatePropertyValueWidget()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				PickerWidget.ToSharedRef()
			]
		]
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
}

void FDirectoryPathStructCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
}

FReply FDirectoryPathStructCustomization::OnPickContent(TSharedRef<IPropertyHandle> PropertyHandle) 
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FPathPickerConfig PathPickerConfig;
	PropertyHandle->GetValue(PathPickerConfig.DefaultPath);
	PathPickerConfig.bAllowContextMenu = false;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &FDirectoryPathStructCustomization::OnPathPicked, PropertyHandle);

	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.AddWidget(SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.0f)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		], FText());


	PickerMenu = FSlateApplication::Get().PushMenu(PickerButton.ToSharedRef(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

	return FReply::Handled();
}

FReply FDirectoryPathStructCustomization::OnPickDirectory(TSharedRef<IPropertyHandle> PropertyHandle, const bool bRelativeToGameContentDir, const bool bUseRelativePath) const
{
	FString Directory;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(BrowseButton.ToSharedRef());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString StartDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT);
		if (bRelativeToGameContentDir && !IsValidPath(StartDirectory, bRelativeToGameContentDir))
		{
			StartDirectory = AbsoluteGameContentDir;
		}

		// Loop until; a) the user cancels (OpenDirectoryDialog returns false), or, b) the chosen path is valid (IsValidPath returns true)
		for (;;)
		{
			if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, LOCTEXT("FolderDialogTitle", "Choose a directory").ToString(), StartDirectory, Directory))
			{
				FText FailureReason;
				if (IsValidPath(Directory, bRelativeToGameContentDir, &FailureReason))
				{
					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, Directory);

					if (bRelativeToGameContentDir)
					{
						Directory.RightChopInline(AbsoluteGameContentDir.Len(), EAllowShrinking::No);
					}
					else if (bUseRelativePath)
					{
						Directory = IFileManager::Get().ConvertToRelativePath(*Directory);
					}

					PropertyHandle->SetValue(Directory);
				}
				else
				{
					StartDirectory = Directory;
					FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
					continue;
				}
			}
			break;
		}
	}

	return FReply::Handled();
}

bool FDirectoryPathStructCustomization::IsValidPath(const FString& AbsolutePath, const bool bRelativeToGameContentDir, FText* const OutReason) const
{
	if(bRelativeToGameContentDir)
	{
		if(!AbsolutePath.StartsWith(AbsoluteGameContentDir))
		{
			if(OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("Error_InvalidRootPath", "The chosen directory must be within {0}"), FText::FromString(AbsoluteGameContentDir));
			}
			return false;
		}
	}

	return true;
}

void FDirectoryPathStructCustomization::OnPathPicked(const FString& Path, TSharedRef<IPropertyHandle> PropertyHandle)
{
	if (PickerMenu.IsValid())
	{
		PickerMenu->Dismiss();
		PickerMenu.Reset();
	}

	PropertyHandle->SetValue(Path);
}

bool FDirectoryPathStructCustomization::IsBrowseEnabled(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	return PropertyHandle->IsEditable();
}

#undef LOCTEXT_NAMESPACE
