// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Factories/MaterialFactoryNew.h"
#include "EditorFramework/AssetImportData.h"
#include "AssetTools.h"
#include "Interfaces/ITextureEditorModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Factories/SubUVAnimationFactory.h"
#include "Particles/SubUVAnimation.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Editor.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "ToolMenus.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Misc/ScopedSlowTask.h"
#include "EditorSupportDelegates.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialEditingLibrary.h"
#include "AssetVtConversion.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Texture::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Textures = GetTypedWeakObjectPtrs<UTexture>(InObjects);
	bool bHasVirtualTextures = false;
	bool bHasNonVirtualTextures = false;
	for (const TWeakObjectPtr<UTexture>& Texture : Textures)
	{
		if (Texture->VirtualTextureStreaming)
		{
			bHasVirtualTextures = true;
		}
		else if(Cast<UTexture2D>(Texture.Get()))
		{
			// Currently only texture2D may be converted to VT
			bHasNonVirtualTextures = true;
		}
	}

	Section.AddMenuEntry(
		"Texture_CreateMaterial",
		LOCTEXT("Texture_CreateMaterial", "Create Material"),
		LOCTEXT("Texture_CreateMaterialTooltip", "Creates a new material using this texture."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Material"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture::ExecuteCreateMaterial, Textures ),
			FCanExecuteAction()
			)
		);

	if (bHasNonVirtualTextures)
	{
		Section.AddMenuEntry(
			"Texture_ConvertToVT",
			LOCTEXT("Texture_ConvertToVT", "Convert to Virtual Texture"),
			LOCTEXT("Texture_ConvertToVTTooltip", "Converts this texture to a virtual texture if it fits the size limit imposed in the texture importer settings."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_Texture::ExecuteConvertToVirtualTexture, Textures),
				FCanExecuteAction()
			)
		);
	}

	if (bHasVirtualTextures)
	{
		Section.AddMenuEntry(
			"Texture_ConvertToRegular",
			LOCTEXT("Texture_ConvertToRegular", "Convert to Regular Texture"),
			LOCTEXT("Texture_ConvertToRegularTooltip", "Converts this texture to a regular 2D texture if it is a virtual texture."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_Texture::ExecuteConvertToRegularTexture, Textures),
				FCanExecuteAction()
			)
		);
	}

	if ( InObjects.Num() == 1 )
	{
		Section.AddMenuEntry(
			"Texture_FindMaterials",
			LOCTEXT("Texture_FindMaterials", "Find Materials Using This"),
			LOCTEXT("Texture_FindMaterialsTooltip", "Finds all materials that use this material in the content browser."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Find"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture::ExecuteFindMaterials, Textures[0]),
				FCanExecuteAction()
				)
			);
	}
}

void FAssetTypeActions_Texture::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		UTexture* Texture = CastChecked<UTexture>(Asset);
		Texture->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

void FAssetTypeActions_Texture::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Texture = Cast<UTexture>(*ObjIt);
		if (Texture != NULL)
		{
			ITextureEditorModule* TextureEditorModule = &FModuleManager::LoadModuleChecked<ITextureEditorModule>("TextureEditor");
			TextureEditorModule->CreateTextureEditor(Mode, EditWithinLevelEditor, Texture);
		}
	}
}

void FAssetTypeActions_Texture::ExecuteCreateMaterial(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	const FString DefaultSuffix = TEXT("_Mat");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
			Factory->InitialTexture = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UMaterial::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
				Factory->InitialTexture = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterial::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

struct FConversionStatus
{
	bool UserSelected = true; // Originally selected by the user
	bool UnderSized = false; // Too small to convert according to user settings
	bool NonPowerOf2 = false; // Not a power of 2, can't convert
	bool InvalidMaterialUsage = false;
};

/**
* FConvertToVTDlg
*
* Wrapper class for SConvertToVTDlg. This class creates and launches a dialog then awaits the
* result to return to the user.
*/
class FConvertToVTDlg
{
public:
	enum EResult
	{
		Cancel = 0,			// No/Cancel, normal usage would stop the current action
		Confirm = 1,		// Yes/Ok/Etc, normal usage would continue with action
	};

	FConvertToVTDlg(const TArray<UTexture2D *> &Textures, bool bBackwards);

	/**  Shows the dialog box and waits for the user to respond. */
	EResult ShowModal();

private:
	TSharedPtr<SWindow> DialogWindow;
	TSharedPtr<class SConvertToVTDlg> DialogWidget;
};

struct FMaterialItem
{
	FName Name;
};

class SConvertToVTDlg : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SConvertToVTDlg)
	{}
	/** Window in which this widget resides */
	SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		UserResponse = FConvertToVTDlg::Cancel;
		ParentWindow = InArgs._ParentWindow.Get();
		static FName ErrorIcon = "MessageLog.Error";


		for (int i = 0; i < 16; i++)
		{
			TextureSizes.Add(MakeShareable(new int32(1 << i)));
		}
		ThresholdValue = *TextureSizes[10];

		this->ChildSlot[
			SNew(SVerticalBox)
			// Textbox at the top giving an introductory message
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Visibility(this, &SConvertToVTDlg::GetIntroMessageVisibility)
					.Text(this, &SConvertToVTDlg::GetIntroMessage)
				]
			// Error message at the top giving a common error message
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SConvertToVTDlg::GetErrorMessageVisibility)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(ErrorIcon))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(this, &SConvertToVTDlg::GetErrorMessage)
					]
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SSeparator)
				]
			// The actual list of assets
			+ SVerticalBox::Slot()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SBorder)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(AssetListContainer, SVerticalBox)
						]
					]
				]
			// The bottom row of widgets: texture size selector
			+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ConvertToVT_Size", "Texture size threshold: "))
						.AutoWrapText(true)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SComboBox<TSharedPtr<int32>>)
						.OptionsSource(&TextureSizes)
						.OnSelectionChanged(this, &SConvertToVTDlg::OnThresholdChanged)
						.OnGenerateWidget(this, &SConvertToVTDlg::OnGenerateThresholdWidget)
						.InitiallySelectedItem(TextureSizes[10])
						[
							SNew(STextBlock)
							.Text(this, &SConvertToVTDlg::GetThresholdText)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SConvertToVTDlg::OnFilterButtonClicked)
						.IsEnabled(this, &SConvertToVTDlg::GetFilterButtonEnabled)
						.Text(LOCTEXT("ConvertToVT_Filter", "Apply Filter"))
					]
				]
			// Separator
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SSeparator)
				]
			// Dialog ok/cancel buttons
			+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SConvertToVTDlg::OnButtonClick, FConvertToVTDlg::Confirm)
						.IsEnabled(this, &SConvertToVTDlg::GetOkButtonEnabled)
						.Text(LOCTEXT("ConvertToVT_OK", "OK"))
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SConvertToVTDlg::OnButtonClick, FConvertToVTDlg::Cancel)
						.Text(LOCTEXT("ConvertToVT_Cancel", "Cancel"))
					]
				]
		];

		UpdateList();
		bFilterButtonEnabled = false;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	void SetBackwards(bool bSetBackwards)
	{
		Worker.SetConversionDirection(bSetBackwards);
		bBackwards = bSetBackwards;
	}

	void SetUserTextures(const TArray<UTexture2D *> &Textures)
	{
		Worker.UserTextures = Textures;
		UpdateList();
	}

	FConvertToVTDlg::EResult GetUserResponse() const
	{
		return UserResponse;
	}

private:

	/**
	* Creates a single line showing an asset and it's status related to VT conversion
	*/
	TSharedRef<SWidget> CreateAssetLine(int index, const FAssetData &Asset, const FConversionStatus &Status)
	{
		const bool bEngineAsset = Asset.PackagePath.ToString().StartsWith(TEXT("/Engine/"));
		FName SeverityIcon = NAME_None;
		FText DetailedInfoText;
	
		if (Status.UserSelected)
		{
			if (Status.InvalidMaterialUsage)
			{
				SeverityIcon = "MessageLog.Error";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_InvalidUsage", "The texture could not be converted to VT due to its usage in materials (may be connected to a property that doesn't support VT).");
			}
			else if (Status.NonPowerOf2)
			{
				SeverityIcon = "MessageLog.Error";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_NonPowerOf2", "The texture could not be converted to VT because its size is not a power of 2.");
			}
			else if (bEngineAsset)
			{
				SeverityIcon = "MessageLog.Note";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_Engine", "The texture is an engine asset, a converted copy will be created in the current project.");
			}
			else if (Status.UnderSized)
			{
				SeverityIcon = "MessageLog.Note";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_Undersize", "The texture was under the threshold size but should still be converted to vt because it's using a shared parameter with other VT textures.");
			}
		}
		else
		{
			if (Status.InvalidMaterialUsage)
			{
				SeverityIcon = "MessageLog.Warning";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_InvalidUsageNotSelected", "The texture could not be converted to VT due to its usage in materials (may be connected to a property that doesn't support VT).");
			}
			else if (Status.NonPowerOf2)
			{
				SeverityIcon = "MessageLog.Warning";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_NonPowerOf2NotSelected", "The texture could not be converted to VT because its size is not a power of 2.");
			}
			else if (bEngineAsset)
			{
				SeverityIcon = "MessageLog.Note";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_EngineNotSelected", "The engine asset texture was not selected but a converted copy will be created in the current project, because it's using a shared parameter with other VT textures.");
			}
			else if (Status.UnderSized)
			{
				SeverityIcon = "MessageLog.Note";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_UndersizeNotSelected", "The texture was not selected and furthermore is under the threshold size but should still be converted to vt because it's using a shared parameter with other VT textures.");
			}
			else
			{
				SeverityIcon = "MessageLog.Note";
				DetailedInfoText = LOCTEXT("ConvertToVT_ToolTip_NotSelected", "The texture was not selected but should still be converted to vt because it's using a shared parameter with other VT textures.");
			}
		}
	
		auto Result =
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				// Class icon
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.Padding(2.0f)
					[
						SNew(SImage)
						.Image(FSlateIcon(FAppStyle::GetAppStyleSetName(),
					(Asset.GetClass() == UTexture2D::StaticClass()) ?
							"ClassIcon.Texture2D" :
							((Asset.GetClass() == UMaterialFunction::StaticClass()) ? "ClassIcon.MaterialFunction" : "ClassIcon.Material")).GetIcon())
					]
				]
				// Error/warning icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Padding(2.0f)
					[
						(SeverityIcon == NAME_None) ? SNullWidget::NullWidget :
						static_cast<TSharedRef<SWidget>>(SNew(SImage).Image(FAppStyle::GetBrush(SeverityIcon)))
					]
				]
				// Fold out button with asset name
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
						(DetailedInfoText.IsEmpty())
						? static_cast<TSharedRef<SWidget>>(SNew(STextBlock)
							.Text(FText::FromString(Asset.GetObjectPathString())))
					: SNew(SButton)
					.ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.ClickMethod(EButtonClickMethod::MouseDown)
					.OnClicked(this, &SConvertToVTDlg::OnExpanderClicked, index)
					.ContentPadding(0.f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					//.Text(FText::FromName(Asset.ObjectPath))
					[
						// Fold out icon
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							SNew(SImage)
							.Image(this, &SConvertToVTDlg::GetExpanderImage, index)
						.ColorAndOpacity(FSlateColor::UseForeground())
						]
						// Fold out text
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Asset.GetObjectPathString()))
						]
					]
			    ]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(DetailedInfoText)
				.Visibility(this, &SConvertToVTDlg::GetDetailVisibility, index)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				//SNew(STextBlock)
				//.Text(GetAuditTrailText(Asset))
				//.Visibility(this, &SConvertToVTDlg::GetDetailVisibility, index)
				GetAuditTrailText(Asset, index)
			];
		return Result;
	}

	TSharedRef<SWidget> GetAuditTrailText(const FAssetData &Asset, int32 index)
	{
		//FString Result;
		UObject *MaybeOk = Asset.GetAsset();
		auto Trail = Worker.AuditTrail.Find(MaybeOk);

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		.Visibility(this, &SConvertToVTDlg::GetDetailVisibility, index);

		Box->AddSlot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew(SSeparator)
		];

		Box->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConvertToVT_AuditTrail","This texture was included because of the following dependencies:"))
		];



		while (Trail)
		{
			Box->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Trail->PathDescription))
			];

			if (Trail->Destination)
			{
				Box->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Trail->Destination->GetPathName()))
				];

				// May return null at which point we'll break out of the loop
				auto NewTrail = Worker.AuditTrail.Find(Trail->Destination);
				if (NewTrail == nullptr || NewTrail == Trail)
				{
					break;
				}
				Trail = NewTrail;
			}
			else
			{
				break;
			}

			//TODO: We just display the first step only as there isn't much more usefull info
			//in the trail anyway for now and it could actually confuse people seeing half a trail.
			break;
		}

		return Box;
	}

	void UpdateList()
	{
		Worker.FilterList(ThresholdValue);

		AssetList.Empty();
		AssetStatus.Empty();

		for (UTexture2D *Texture : Worker.Textures)
		{
			AssetList.Add(Texture);
			FConversionStatus* Status = new(AssetStatus) FConversionStatus();
			Status->UserSelected = Worker.UserTextures.Contains(Texture);
			Status->UnderSized = (Texture->GetSizeX()*Texture->GetSizeY() < ThresholdValue*ThresholdValue);
			Status->NonPowerOf2 = !Texture->Source.IsPowerOfTwo() && Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::None;
		}

		for (UTexture2D *Texture : Worker.MaterialRejectedTextures)
		{
			AssetList.Add(Texture);
			FConversionStatus* Status = new(AssetStatus) FConversionStatus();
			Status->UserSelected = Worker.UserTextures.Contains(Texture);
			Status->UnderSized = (Texture->GetSizeX()*Texture->GetSizeY() < ThresholdValue*ThresholdValue);
			Status->NonPowerOf2 = !Texture->Source.IsPowerOfTwo() && Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::None;
			Status->InvalidMaterialUsage = true;
		}

		for (UMaterial *RootMat : Worker.Materials)
		{
			// Don't want to show anything from the transient package, this may include preview materials from any currently active material editor
			// We patch these up so material editor remains valid, but not useful to display this to user
			if (RootMat->GetOutermost() != GetTransientPackage())
			{
				AssetList.Add(RootMat);
				AssetStatus.Add(FConversionStatus());
			}
		}

		for (UMaterialFunctionInterface *Func : Worker.Functions)
		{
			if (Func->GetOutermost() != GetTransientPackage())
			{
				AssetList.Add(Func);
				AssetStatus.Add(FConversionStatus());
			}
		}

		check(AssetList.Num() == AssetStatus.Num());
		AssetListContainer->ClearChildren();

		for (int Id = 0; Id < AssetList.Num(); Id++)
		{
			AssetListContainer->AddSlot()
				.AutoHeight()
				[
					CreateAssetLine(Id, AssetList[Id], AssetStatus[Id])
				];
		}

		ErrorMessage = FText();
	}


	FReply OnButtonClick(FConvertToVTDlg::EResult ButtonID)
	{
		ParentWindow->RequestDestroyWindow();
		UserResponse = ButtonID;

		if (ButtonID == FConvertToVTDlg::EResult::Confirm)
		{
			Worker.DoConvert();
		}

		return FReply::Handled();
	}

	void OnThresholdChanged(TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo)
	{
		ThresholdValue = *InSelectedItem;
		bFilterButtonEnabled = true;
	}

	FText GetThresholdText() const
	{
		return FText::FromString(FString::Format(TEXT("{0}"), TArray<FStringFormatArg>({ ThresholdValue })));
	}

	TSharedRef<SWidget> OnGenerateThresholdWidget(TSharedPtr<int32> InItem)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(FString::Format(TEXT("{0}"), TArray<FStringFormatArg>({ *InItem }))));
	}

	bool GetFilterButtonEnabled() const
	{
		return bFilterButtonEnabled;
	}

	FReply OnFilterButtonClicked()
	{
		UpdateList();
		bFilterButtonEnabled = false;

		return FReply::Handled();
	}

	TArray<int> ExpandedIndexes;

	FReply OnExpanderClicked(int index)
	{
		if (ExpandedIndexes.Contains(index))
		{
			ExpandedIndexes.Remove(index);
		}
		else
		{
			ExpandedIndexes.Add(index);
		}
		return FReply::Handled();
	}

	bool GetOkButtonEnabled() const
	{
		return ErrorMessage.IsEmpty();
	}

	EVisibility GetDetailVisibility(int index) const
	{
		return (ExpandedIndexes.Contains(index)) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetExpanderImage(int index) const
	{
		FName ResourceName;
		if (GetDetailVisibility(index) == EVisibility::Visible)
		{
			static FName ExpandedName = "TreeArrow_Expanded";
			ResourceName = ExpandedName;
		}
		else
		{
			static FName CollapsedName = "TreeArrow_Collapsed";
			ResourceName = CollapsedName;
		}
		return FCoreStyle::Get().GetBrush(ResourceName);
	}

	EVisibility GetIntroMessageVisibility() const
	{
		return (IntroMessage.IsEmpty()) ? EVisibility::Collapsed : EVisibility::Visible;
	}

	EVisibility GetErrorMessageVisibility() const
	{
		return (ErrorMessage.IsEmpty()) ? EVisibility::Collapsed : EVisibility::Visible;
	}

	EVisibility GetThresholdVisibility() const
	{
		return (bThresholdVisible) ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FText GetIntroMessage() const
	{
		return IntroMessage;
	}

	FText GetErrorMessage() const
	{
		return ErrorMessage;
	}

	bool bBackwards;
	FText IntroMessage;
	FText ErrorMessage;
	FConvertToVTDlg::EResult	 UserResponse;

	TSharedPtr<SVerticalBox>	 AssetListContainer;
	//TSharedPtr<STextBlock>		 MessageTextBlock;
	//TSharedPtr<SHorizontalBox>	 ThresholdContainer;
	//TSharedPtr<SHorizontalBox>	 ErrorContainer;
	//TSharedPtr<SButton>			 OkButton;

	/** Pointer to the window which holds this Widget, required for modal control */
	TSharedPtr<SWindow>			 ParentWindow;

	FVTConversionWorker Worker;
	TArray<FAssetData> AssetList;
	TArray<FConversionStatus> AssetStatus;

	int ThresholdValue;
	bool bThresholdVisible;

	TArray<TSharedPtr<int32>> TextureSizes;

	bool bFilterButtonEnabled;
};

FConvertToVTDlg::FConvertToVTDlg(const TArray<UTexture2D *> &Textures, bool bBackwards)
{
	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title((bBackwards) ? LOCTEXT("ConvertToVTDlgTitle_Backwards", "Convert From VT") : LOCTEXT("ConvertToVTDlgTitle", "Convert To VT"))
			.SupportsMinimize(false).SupportsMaximize(false)
			.ClientSize(FVector2D(500, 500));

		TSharedPtr<SBorder> DialogWrapper =
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SConvertToVTDlg)
				.ParentWindow(DialogWindow)
			];

		DialogWidget->SetBackwards(bBackwards);
		DialogWidget->SetUserTextures(Textures);
		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

FConvertToVTDlg::EResult FConvertToVTDlg::ShowModal()
{
	//Show Dialog
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
	EResult UserResponse = (EResult)DialogWidget->GetUserResponse();
	DialogWindow->GetParentWindow()->RemoveDescendantWindow(DialogWindow.ToSharedRef());
	return UserResponse;
}


void FAssetTypeActions_Texture::ConvertVTTexture(TArray<TWeakObjectPtr<UTexture>> Objects, bool backwards)
{
	TArray<FAssetData> AllRelevantMaterials;
	TArray<UTexture2D*> UserTextures; // The original selection of the user

	//FScopedSlowTask SlowTask(4.0f, LOCTEXT("ConvertToVT_Progress_ConvertToVt", "Converting textures to VT..."));
	//SlowTask.MakeDialog();

	// First simply assemble a list of materials to show the user.
	//SlowTask.EnterProgressFrame();
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{

		auto Object = (*ObjIt).Get();
		UTexture2D *Texture = Cast<UTexture2D>(Object);
		if (Texture != nullptr && Texture->VirtualTextureStreaming == backwards)
		{
			GetReferencersData(Object, UMaterialInterface::StaticClass(), AllRelevantMaterials);
			UserTextures.Add(Texture);
		}
	}

	FConvertToVTDlg ConvertDlg(UserTextures, backwards);
	/*ConvertDlg.AssetList.Append(UserTextures);
	ConvertDlg.AssetList.Append(AllRelevantMaterials);
	ConvertDlg.AssetStatus.AddDefaulted(UserTextures.Num());
	ConvertDlg.AssetStatus.AddDefaulted(AllRelevantMaterials.Num());
	ConvertDlg.Message = LOCTEXT("ConvertToVTMaterialList", "The following textures & materials (and their dependencies) will be loaded by this operation. This could take a while.");
	ConvertDlg.Threshold = virtualTextureAutoEnableThreshold;*/

	if (ConvertDlg.ShowModal() == FConvertToVTDlg::Confirm)
	{
#if 0
		//FVTConversionWorker Conversion;
		//Conversion.UserTextures = UserTextures;
		//Conversion.FilterList(ConvertDlg.Threshold);

		/*FScopedSlowTask SlowTask(3.0f, LOCTEXT("ConvertToVT_Progress_ConvertToVt", "Converting textures to VT..."));
		SlowTask.MakeDialog();

		TArray<UTexture2D*> Textures; // All textures that should be converted
		TArray<UTexture2D*> SizeRejectedTextures; // The original selection of the user filtered by textures that match the threshold size

		for (UTexture2D *Texture: UserTextures)
		{
			if (Texture->GetSizeX()*Texture->GetSizeY() >= ConvertDlg.Threshold*ConvertDlg.Threshold)
			{
				Textures.Add(Texture);
			}
			else
			{
				SizeRejectedTextures.Add(Texture);
			}
		}

		SlowTask.EnterProgressFrame();
		TArray<UMaterial *>Materials;
		TArray<UMaterialFunctionInterface *> Functions;
//		FindAllTexturesAndMaterials(Materials, Functions, Textures);
*/
		// Show the dialog yet again warning the user...

		/*FConvertToVTDlg ConvertDlg2;
		ConvertDlg2.AssetList.Empty();
		for (UTexture2D *Texture : Conversion.Textures)
		{
			ConvertDlg2.AssetList.Add(Texture);
			ConvertDlg2.AssetStatus.Add(FConversionStatus(UserTextures.Contains(Texture), (Texture->GetSizeX()*Texture->GetSizeY() < ConvertDlg.Threshold*ConvertDlg.Threshold)));
		}

		for (UMaterial *RootMat : Conversion.Materials)
		{
			ConvertDlg2.AssetList.Add(RootMat);
			ConvertDlg2.AssetStatus.Add(FConversionStatus());
		}

		for (UMaterialFunctionInterface *Func : Conversion.Functions)
		{
			ConvertDlg2.AssetList.Add(Func);
			ConvertDlg2.AssetStatus.Add(FConversionStatus());
		}

		ConvertDlg2.Message = LOCTEXT("ConvertToVTTextureList", "The following textures and materials will converted. Please carefully inspect this list as textures not originally selected may also be included for technical reasons.");
		ConvertDlg2.Threshold = ConvertDlg.Threshold;
		ConvertDlg2.EditThreshold = false;
		if (ConvertDlg2.ShowModal() == FConvertToVTDlg::Confirm)
		{
			Conversion.DoConvert(backwards);
			/*UE_LOG(LogVirtualTextureConversion, Display, TEXT("Beginning conversion..."));
			SlowTask.EnterProgressFrame();
			{
				FScopedSlowTask TextureTask(Textures.Num(), LOCTEXT("ConvertToVT_Progress_TextureTask", "Updating textures..."));

				for (UTexture2D *Tex : Textures)
				{
					UE_LOG(LogVirtualTextureConversion, Display, TEXT("Texture %s"), *Tex->GetName());
					TextureTask.EnterProgressFrame();

					bool OldVt = Tex->VirtualTextureStreaming;
					Tex->VirtualTextureStreaming = !backwards;
					if (OldVt != Tex->VirtualTextureStreaming)
					{
						Tex->Modify();
						Tex->PostEditChange();
					}
				}
			}

			SlowTask.EnterProgressFrame();
			{
				FScopedSlowTask MaterialTask(Materials.Num() + Functions.Num(), LOCTEXT("ConvertToVT_Progress_MaterialTask", "Updating materials..."));
				FMaterialUpdateContext UpdateContext;

				TMap<UMaterialFunctionInterface*, TArray<UMaterial*>> FunctionToMaterialMap;

				for (UMaterial *Mat : Materials)
				{
					UE_LOG(LogVirtualTextureConversion, Display, TEXT("Material %s"), *Mat->GetName());

					MaterialTask.EnterProgressFrame();

					bool MatModified = false;
					for (UMaterialExpression *Expr : Mat->Expressions)
					{
						UMaterialExpressionTextureBase *TexExpr = Cast<UMaterialExpressionTextureBase>(Expr);
						if (TexExpr)
						{
							if (Textures.Contains(TexExpr->Texture))
							{
								UE_LOG(LogVirtualTextureConversion, Display, TEXT("Adjusting sampler %s."), *TexExpr->GetName());
								auto OldType = TexExpr->SamplerType;
								TexExpr->AutoSetSampleType();
								if (TexExpr->SamplerType != OldType)
								{
									TexExpr->Modify();
									TexExpr->PostEditChange();
									MatModified = true;
								}
							}
						}
					}

					TArray<UMaterialFunctionInterface *>MaterialFunctions;
					Mat->GetDependentFunctions(MaterialFunctions);
					for (auto Function : MaterialFunctions)
					{
						FunctionToMaterialMap.FindOrAdd(Function).AddUnique(Mat);
					}

					if (MatModified)
					{
						UE_LOG(LogVirtualTextureConversion, Display, TEXT("Material %s added to update list."), *Mat->GetName());
						UMaterialEditingLibrary::RecompileMaterial(Mat);
					}
					else
					{
						UE_LOG(LogVirtualTextureConversion, Display, TEXT("Material %s was not modified, skipping."), *Mat->GetName());
					}
				}

				for (UMaterialFunctionInterface *Func : Functions)
				{
					UE_LOG(LogVirtualTextureConversion, Display, TEXT("Function %s"), *Func->GetName());

					MaterialTask.EnterProgressFrame();

					bool FuncModified = false;
					const TArray<UMaterialExpression*> *Expressions = Func->GetExpressions();
					for (UMaterialExpression *Expr : *Expressions)
					{
						UMaterialExpressionTextureBase *TexExpr = Cast<UMaterialExpressionTextureBase>(Expr);
						if (TexExpr)
						{
							if (Textures.Contains(TexExpr->Texture))
							{
								UE_LOG(LogVirtualTextureConversion, Display, TEXT("Adjusting sampler %s."), *TexExpr->GetName());
								auto OldType = TexExpr->SamplerType;
								TexExpr->AutoSetSampleType();
								if (TexExpr->SamplerType != OldType)
								{
									TexExpr->Modify();
									TexExpr->PostEditChange();
									FuncModified = true;
								}
							}
						}
					}

					if (FuncModified)
					{
						UMaterialEditingLibrary::UpdateMaterialFunction(Func, nullptr);
						UE_LOG(LogVirtualTextureConversion, Display, TEXT("Function %s added to update list."), *Func->GetName());
					}
					else
					{
						UE_LOG(LogVirtualTextureConversion, Display, TEXT("Function %s was not modified, skipping."), *Func->GetName());
					}
				}

				// update the world's viewports
				UE_LOG(LogVirtualTextureConversion, Display, TEXT("Broadcasting to editor."));
				FEditorDelegates::RefreshEditor.Broadcast();
				FEditorSupportDelegates::RedrawAllViewports.Broadcast();
			}
		}*/
#endif
	}
}

void FAssetTypeActions_Texture::ExecuteConvertToVirtualTexture(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	ConvertVTTexture(Objects, false);
}

void FAssetTypeActions_Texture::ExecuteConvertToRegularTexture(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	ConvertVTTexture(Objects, true);
}

void FAssetTypeActions_Texture::ExecuteCreateSubUVAnimation(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	const FString DefaultSuffix = TEXT("_SubUV");

	if ( Objects.Num() == 1 )
	{
		UTexture2D* Object = Cast<UTexture2D>(Objects[0].Get());

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USubUVAnimationFactory* Factory = NewObject<USubUVAnimationFactory>();
			Factory->InitialTexture = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USubUVAnimation::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			UTexture2D* Object = Cast<UTexture2D>((*ObjIt).Get());

			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USubUVAnimationFactory* Factory = NewObject<USubUVAnimationFactory>();
				Factory->InitialTexture = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USubUVAnimation::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_Texture::ExecuteFindMaterials(TWeakObjectPtr<UTexture> Object)
{

	TArray<FAssetData> MaterialsUsingTexture;

	// This finds "Material like" objects. It's called material in the UI string but this generally
	// seems more useful
	auto Texture = Object.Get();
	if ( Texture )
	{
		GetReferencersData(Texture, UMaterialInterface::StaticClass(), MaterialsUsingTexture);
		GetReferencersData(Texture, UMaterialFunction::StaticClass(), MaterialsUsingTexture);
	}

	if (MaterialsUsingTexture.Num() > 0)
	{
		FAssetTools::Get().SyncBrowserToAssets(MaterialsUsingTexture);
	}
}

#undef LOCTEXT_NAMESPACE
