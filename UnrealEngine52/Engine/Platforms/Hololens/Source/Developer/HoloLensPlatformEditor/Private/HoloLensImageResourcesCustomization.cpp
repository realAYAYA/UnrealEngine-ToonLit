// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensImageResourcesCustomization.h"

#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "SExternalImageReference.h"
#include "Widgets/Text/STextBlock.h"
#include "IExternalImagePickerModule.h"
#include "ISourceControlModule.h"
#include "PropertyHandle.h"
#include "EditorDirectories.h"
#include "Interfaces/IPluginManager.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Widgets/Input/SCheckBox.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "HoloLensImagesCustomization"

namespace
{
	FString GetPickerPath()
	{
		return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
	}


	bool HandlePostExternalIconCopy(const FString& InChosenImage)
	{
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
		return true;
	}

	void AddWidgetForResourceImage(IDetailChildrenBuilder& ChildrenBuilder, const FString& RootPath, const FString& Culture, const FString& ImageFileName, const FText& ImageCaption, const FVector2D& ImageDimensions)
	{
		const FString DefaultEngineImageSubPath = FString::Printf(TEXT("Platforms/HoloLens/Build/DefaultImages/%s.png"), *ImageFileName);
		const FString DefaultGameImageSubPath = FString::Printf(TEXT("Build/HoloLens/Resources/%s/%s.png"), *Culture, *ImageFileName);

		const FString EngineImagePath = FPaths::EngineDir() / DefaultEngineImageSubPath;
		const FString ProjectImagePath = RootPath / DefaultGameImageSubPath;

		TArray<FString> ImageExtensions;
		ImageExtensions.Add(TEXT("png"));

		ChildrenBuilder.AddCustomRow(ImageCaption)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(ImageCaption)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(ImageDimensions.X)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SExternalImageReference, EngineImagePath, ProjectImagePath)
				.FileDescription(ImageCaption)
				.MaxDisplaySize(ImageDimensions)
				.OnGetPickerPath(FOnGetPickerPath::CreateStatic(&GetPickerPath))
				.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateStatic(&HandlePostExternalIconCopy))
				.DeleteTargetWhenDefaultChosen(true)
				.FileExtensions(ImageExtensions)
				.DeletePreviousTargetWhenExtensionChanges(true)
			]
		];
	}

	void PickGltfFor(const FString& ModelPath)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			return;
		}

		FText Title = FText::FromString("glb");
		FString TitleExtensions = TEXT("*.glb");

		TArray<FString> OutFiles;
		const FString Filter = FString::Printf(TEXT("%s files (%s)|%s"), *Title.ToString(), *TitleExtensions, *TitleExtensions);
		const FString DefaultPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);

		if (DesktopPlatform->OpenFileDialog(nullptr, FText::Format(LOCTEXT("GltfPickerDialogTitle", "Choose a {0} file"), Title).ToString(), DefaultPath, TEXT(""), Filter, EFileDialogFlags::None, OutFiles))
		{
			check(OutFiles.Num() == 1);

			FString SourceImagePath = FPaths::ConvertRelativePathToFull(OutFiles[0]);
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(SourceImagePath));

			if (ModelPath != SourceImagePath)
			{
				IFileManager::Get().Copy(*ModelPath, *SourceImagePath);
			}
		}

	}

	void AddWidgetForResourceGltf(IDetailChildrenBuilder& ChildrenBuilder, const FString& RootPath, const FString& Culture, const FString& ModelFileName, const FText& ModelCaption)
	{
		const FString ModelPath = RootPath / TEXT("Build") / TEXT("HoloLens") / TEXT("Resources") / Culture / (ModelFileName + TEXT(".glb"));

		ChildrenBuilder.AddCustomRow(ModelCaption)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(ModelCaption)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([=]() -> ECheckBoxState 
			{
				return IFileManager::Get().FileExists(*ModelPath) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewAutoCloseState)
			{
				if (NewAutoCloseState == ECheckBoxState::Checked)
				{
					PickGltfFor(ModelPath);
				}
				else if (NewAutoCloseState == ECheckBoxState::Unchecked)
				{
					IFileManager::Get().Delete(*ModelPath, false, true, true);
				}
			})
		];
	}
}

TSharedRef<IPropertyTypeCustomization> FHoloLensCorePackageImagesCustomization::MakeInstance()
{
	return MakeShared<FHoloLensCorePackageImagesCustomization>();
}

void FHoloLensCorePackageImagesCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FHoloLensCorePackageImagesCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> CultureIdProperty = InStructPropertyHandle->GetParentHandle()->GetChildHandle(FName("CultureId")).ToSharedRef();
	FString CultureId;
	CultureIdProperty->GetValue(CultureId);

	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("Logo"), LOCTEXT("Square150x150Logo", "Square 150x150 Logo"), FVector2D(150.0f, 150.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("SmallLogo"), LOCTEXT("Square44x44Logo", "Square 44x44 Logo"), FVector2D(44.0f, 44.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("WideLogo"), LOCTEXT("Wide310x150Logo", "Wide 310x150 Logo"), FVector2D(310.0f, 150.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("SplashScreen"), LOCTEXT("SplashScreen", "Splash Screen"), FVector2D(620.0f, 300.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("StoreLogo"), LOCTEXT("StoreLogo", "Store Logo"), FVector2D(50.0f, 50.0f));
	AddWidgetForResourceGltf(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("3DLogo"), LOCTEXT("3DLogo", "3D Logo"));
}

#undef LOCTEXT_NAMESPACE