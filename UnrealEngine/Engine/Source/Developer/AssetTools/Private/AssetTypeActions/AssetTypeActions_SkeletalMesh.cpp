// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Misc/PackageName.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/MessageDialog.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "Factories/SkeletonFactory.h"
#include "EditorFramework/AssetImportData.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "PhysicsAssetUtils.h"
#include "AssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SSkeletonWidget.h"
#include "PhysicsAssetEditorModule.h"
#include "PersonaModule.h"
#include "ContentBrowserModule.h"
#include "AnimationEditorUtils.h"
#include "Preferences/PersonaOptions.h"

#include "FbxMeshUtils.h"
#include "AssetNotifications.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ISkeletalMeshEditorModule.h"
#include "ApexClothingUtils.h"
#include "Algo/Transform.h"
#include "Factories/PhysicsAssetFactory.h"

#include "EditorReimportHandler.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Factories/FbxSkeletalMeshImportData.h"

#if WITH_EDITOR
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/** 
* FBoneCheckboxInfo
* 
* Context data for the SDlgMergeSkeleton panel check boxes
*/
struct FBoneCheckboxInfo
{
	FName BoneName;
	int32 BoneID;
	bool  bUsed;
};

/** 
* FDlgMergeSkeleton
* 
* Wrapper class for SDlgMergeSkeleton. This class creates and launches a dialog then awaits the
* result to return to the user. 
*/
class FDlgMergeSkeleton
{
public:
	enum EResult
	{
		Cancel = 0,			// No/Cancel, normal usage would stop the current action
		Confirm = 1,		// Yes/Ok/Etc, normal usage would continue with action
	};

	FDlgMergeSkeleton( USkeletalMesh* InMesh, USkeleton* InSkeleton );

	/**  Shows the dialog box and waits for the user to respond. */
	EResult ShowModal();

	// List of required bones for skeleton
	TArray<int32> RequiredBones;
private:

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> DialogWindow;

	/** Cached pointer to the merge skeleton widget */
	TSharedPtr<class SDlgMergeSkeleton> DialogWidget;

	/** The SkeletalMesh to merge bones from*/
	USkeletalMesh* Mesh;
	/** The Skeleton to merge bones to */
	USkeleton* Skeleton;
};

/** 
 * Slate panel for choosing which bones to merge into the skeleton
 */
class SDlgMergeSkeleton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDlgMergeSkeleton)
		{}
		/** Window in which this widget resides */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()	

	/**
	 * Constructs this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		UserResponse = FDlgMergeSkeleton::Cancel;
		ParentWindow = InArgs._ParentWindow.Get();

		this->ChildSlot[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("MergeSkeletonDlgDescription", "Would you like to add following bones to the skeleton?"))
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SSeparator)
			]
			+SVerticalBox::Slot()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SBorder)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					[
						//Save this widget so we can populate it later with check boxes
						SAssignNew(CheckBoxContainer, SVerticalBox)
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SDlgMergeSkeleton::ChangeAllOptions, true)
					.Text(LOCTEXT("SkeletonMergeSelectAll", "Select All"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SDlgMergeSkeleton::ChangeAllOptions, false)
					.Text(LOCTEXT("SkeletonMergeDeselectAll", "Deselect All"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SSeparator)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked( this, &SDlgMergeSkeleton::OnButtonClick, FDlgMergeSkeleton::Confirm )
					.Text(LOCTEXT("SkeletonMergeOk", "OK"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked( this, &SDlgMergeSkeleton::OnButtonClick, FDlgMergeSkeleton::Cancel )
					.Text(LOCTEXT("SkeletonMergeCancel", "Cancel"))
				]
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Creates a Slate check box 
	 *
	 * @param	Label		Text label for the check box
	 * @param	ButtonId	The ID for the check box
	 * @return				The created check box widget
	 */
	TSharedRef<SWidget> CreateCheckBox( const FString& Label, int32 ButtonId )
	{
		return
			SNew(SCheckBox)
			.IsChecked( this, &SDlgMergeSkeleton::IsCheckboxChecked, ButtonId )
			.OnCheckStateChanged( this, &SDlgMergeSkeleton::OnCheckboxChanged, ButtonId )
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			];
	}

	/**
	 * Returns the state of the check box
	 *
	 * @param	ButtonId	The ID for the check box
	 * @return				The status of the check box
	 */
	ECheckBoxState IsCheckboxChecked( int32 ButtonId ) const
	{
		return CheckBoxInfoMap.FindChecked(ButtonId).bUsed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/**
	 * Handler for all check box clicks
	 *
	 * @param	NewCheckboxState	The new state of the check box
	 * @param	CheckboxThatChanged	The ID of the radio button that has changed. 
	 */
	void OnCheckboxChanged( ECheckBoxState NewCheckboxState, int32 CheckboxThatChanged )
	{
		FBoneCheckboxInfo& Info = CheckBoxInfoMap.FindChecked(CheckboxThatChanged);
		Info.bUsed = !Info.bUsed;
	}

	/**
	 * Handler for the Select All and Deselect All buttons
	 *
	 * @param	bNewCheckedState	The new state of the check boxes
	 */
	FReply ChangeAllOptions(bool bNewCheckedState)
	{
		for(auto Iter = CheckBoxInfoMap.CreateIterator(); Iter; ++Iter)
		{
			FBoneCheckboxInfo& Info = Iter.Value();
			Info.bUsed = bNewCheckedState;
		}
		return FReply::Handled();
	}

	/**
	 * Populated the dialog with multiple check boxes, each corresponding to a bone
	 *
	 * @param	BoneInfos	The list of Bones to populate the dialog with
	 */
	void PopulateOptions(TArray<FBoneCheckboxInfo>& BoneInfos)
	{
		for(auto Iter = BoneInfos.CreateIterator(); Iter; ++Iter)
		{
			FBoneCheckboxInfo& Info = (*Iter);
			Info.bUsed = true;

			CheckBoxInfoMap.Add(Info.BoneID, Info);

			CheckBoxContainer->AddSlot()
			.AutoHeight()
			[
				CreateCheckBox(Info.BoneName.GetPlainNameString(), Info.BoneID)
			];
		}
	}

	/** 
	 * Returns the EResult of the button which the user pressed. Closing of the dialog
	 * in any other way than clicking "Ok" results in this returning a "Cancel" value
	 */
	FDlgMergeSkeleton::EResult GetUserResponse() const 
	{
		return UserResponse; 
	}

	/** 
	 * Returns whether the user selected that bone to be used (checked its respective check box)
	 */
	bool IsBoneIncluded(int32 BoneID)
	{
		auto* Item = CheckBoxInfoMap.Find(BoneID);
		return Item ? Item->bUsed : false;
	}

private:
	
	/** 
	 * Handles when a button is pressed, should be bound with appropriate EResult Key
	 * 
	 * @param ButtonID - The return type of the button which has been pressed.
	 */
	FReply OnButtonClick(FDlgMergeSkeleton::EResult ButtonID)
	{
		ParentWindow->RequestDestroyWindow();
		UserResponse = ButtonID;

		return FReply::Handled();
	}

	/** Stores the users response to this dialog */
	FDlgMergeSkeleton::EResult	 UserResponse;

	/** The slate container that the bone check boxes get added to */
	TSharedPtr<SVerticalBox>	 CheckBoxContainer;
	/** Store the check box state for each bone */
	TMap<int32,FBoneCheckboxInfo> CheckBoxInfoMap;

	/** Pointer to the window which holds this Widget, required for modal control */
	TSharedPtr<SWindow>			 ParentWindow;
};

FDlgMergeSkeleton::FDlgMergeSkeleton( USkeletalMesh* InMesh, USkeleton* InSkeleton )
{
	Mesh = InMesh;
	Skeleton = InSkeleton;

	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title( LOCTEXT("MergeSkeletonDlgTitle", "Merge Bones") )
			.SupportsMinimize(false) .SupportsMaximize(false)
			.ClientSize(FVector2D(350, 500));

		TSharedPtr<SBorder> DialogWrapper = 
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SDlgMergeSkeleton)
				.ParentWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

FDlgMergeSkeleton::EResult FDlgMergeSkeleton::ShowModal()
{
	RequiredBones.Empty();

	TMap<FName, int32> BoneIndicesMap;
	TArray<FBoneCheckboxInfo> BoneInfos;

	// Make a list of all skeleton bone list
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	for ( int32 BoneTreeId=0; BoneTreeId<RefSkeleton.GetRawBoneNum(); ++BoneTreeId )
	{
		const FName& BoneName = RefSkeleton.GetBoneName(BoneTreeId);
		BoneIndicesMap.Add(BoneName, BoneTreeId);
	}

	for ( int32 RefBoneId=0 ; RefBoneId< Mesh->GetRefSkeleton().GetRawBoneNum() ; ++RefBoneId )
	{
		const FName& BoneName = Mesh->GetRefSkeleton().GetBoneName(RefBoneId);
		// if I can't find this from Skeleton
		if (BoneIndicesMap.Find(BoneName)==NULL)
		{
			FBoneCheckboxInfo Info;
			Info.BoneID = RefBoneId;
			Info.BoneName = BoneName;
			BoneInfos.Add(Info);
		}
	}

	if (BoneInfos.Num() == 0)
	{
		// it's all identical, but still need to return RequiredBones
		// for the case, where they'd like to replace the one exactly same hierarchy but different skeleton 
		for ( int32 RefBoneId= 0 ; RefBoneId< Mesh->GetRefSkeleton().GetRawBoneNum() ; ++RefBoneId )
		{
			const FName& BoneName = Mesh->GetRefSkeleton().GetBoneName(RefBoneId);
			RequiredBones.Add(RefBoneId);
		}

		return EResult::Confirm;
	}

	DialogWidget->PopulateOptions(BoneInfos);

	//Show Dialog
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
	EResult UserResponse = (EResult)DialogWidget->GetUserResponse();

	if(UserResponse == EResult::Confirm)
	{
		for ( int32 RefBoneId= 0 ; RefBoneId< Mesh->GetRefSkeleton().GetRawBoneNum() ; ++RefBoneId )
		{
			if ( DialogWidget->IsBoneIncluded(RefBoneId) )
			{
				TArray<int32> ParentList;
				
				// I need to make sure parent exists first
				int32 ParentIndex = Mesh->GetRefSkeleton().GetParentIndex(RefBoneId);
				
				// make sure RequiredBones already have ParentIndex
				while (ParentIndex >= 0 )
				{
					// if I don't have it yet
					if ( RequiredBones.Contains(ParentIndex)==false )
					{
						ParentList.Add(ParentIndex);
					}

					ParentIndex = Mesh->GetRefSkeleton().GetParentIndex(ParentIndex);
				}

				if ( ParentList.Num() > 0 )
				{
					// if we need to add parent list
					// add from back to front (since it's added from child to up
					for (int32 I=ParentList.Num()-1; I>=0; --I)
					{
						RequiredBones.Add(ParentList[I]);
					}
				}

				RequiredBones.Add(RefBoneId);
			}
		}
	}
	return UserResponse;
}

FAssetTypeActions_SkeletalMesh::FAssetTypeActions_SkeletalMesh()
: FAssetTypeActions_Base()
{
	//No need to remove since the asset registry module clear this multicast delegate when terminating
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FAssetTypeActions_SkeletalMesh::OnAssetRemoved);
}

void FAssetTypeActions_SkeletalMesh::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Meshes = GetTypedWeakObjectPtrs<USkeletalMesh>(InObjects);

	Section.AddSubMenu(
			"CreateSkeletalMeshSubmenu",
			LOCTEXT("CreateSkeletalMeshSubmenu", "Create"),
			LOCTEXT("CreateSkeletalMeshSubmenu_ToolTip", "Create related assets"),
			FNewToolMenuDelegate::CreateSP(this, &FAssetTypeActions_SkeletalMesh::FillCreateMenu, Meshes),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
			);

	Section.AddSubMenu(
		"SkeletalMesh_LODImport",	
		LOCTEXT("SkeletalMesh_LODImport", "Import LOD"),
		LOCTEXT("SkeletalMesh_LODImportTooltip", "Select which LODs to import."),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SkeletalMesh::GetLODMenu, Meshes),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.LOD")
		);

	// skeleton menu
	Section.AddSubMenu(
		"SkeletonSubmenu",
		LOCTEXT("SkeletonSubmenu", "Skeleton"),
		LOCTEXT("SkeletonSubmenu_ToolTip", "Skeleton related actions"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SkeletalMesh::FillSkeletonMenu, Meshes),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.SkeletalMesh")
	);
}

void FAssetTypeActions_SkeletalMesh::FillCreateMenu(UToolMenu* Menu, TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	if (AssetTools.IsAssetClassSupported(UPhysicsAsset::StaticClass()))
	{
		FToolMenuSection& Section = Menu->AddSection("CreatePhysicsAsset", LOCTEXT("CreatePhysicsAssetMenuHeading", "Physics Asset"));
		Section.AddSubMenu(
			"NewPhysicsAssetMenu",
			LOCTEXT("SkeletalMesh_NewPhysicsAssetMenu", "Physics Asset"),
			LOCTEXT("SkeletalMesh_NewPhysicsAssetMenu_ToolTip", "Options for creating new physics assets from the selected meshes."),
			FNewMenuDelegate::CreateSP(const_cast<FAssetTypeActions_SkeletalMesh*>(this), &FAssetTypeActions_SkeletalMesh::GetPhysicsAssetMenu, Meshes));
	}

	TArray<TWeakObjectPtr<UObject>> Objects;
	Algo::Transform(Meshes, Objects, [](const TWeakObjectPtr<USkeletalMesh>& SkelMesh) { return SkelMesh; });
	Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegateLegacy::CreateLambda([this, Objects](FMenuBuilder& MenuBuilder, UToolMenu* Menu)
	{
		AnimationEditorUtils::FillCreateAssetMenu(MenuBuilder, Objects, FAnimAssetCreated::CreateSP(this, &FAssetTypeActions_SkeletalMesh::OnAssetCreated));
	}));
}

void FAssetTypeActions_SkeletalMesh::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<USkeletalMesh>(*ObjIt);
		if (Mesh != nullptr)
		{
			if (Mesh->GetSkeleton() == nullptr)
			{
				if ( FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("MissingSkeleton", "This mesh currently has no valid Skeleton. Would you like to create a new Skeleton?")) == EAppReturnType::Yes )
				{
					const FString DefaultSuffix = TEXT("_Skeleton");

					// Determine an appropriate name
					FString Name;
					FString PackageName;
					CreateUniqueAssetName(Mesh->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

					USkeletonFactory* Factory = NewObject<USkeletonFactory>();
					Factory->TargetSkeletalMesh = Mesh;

					FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
					AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USkeleton::StaticClass(), Factory);
				}
				else
				{
					AssignSkeletonToMesh(Mesh);
				}

				if( Mesh->GetSkeleton() == NULL )
				{
					// error message
					FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("CreateSkeletonOrAssign", "You need to create a Skeleton or assign one in order to open this in Persona."));
				}
			}

			if ( Mesh->GetSkeleton() != NULL )
			{
				const bool bBringToFrontIfOpen = true;
#if WITH_EDITOR
				if (IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Mesh, bBringToFrontIfOpen))
				{
					EditorInstance->FocusWindow(Mesh);
				}
				else
#endif
				{
					ISkeletalMeshEditorModule& SkeletalMeshEditorModule = FModuleManager::LoadModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
					SkeletalMeshEditorModule.CreateSkeletalMeshEditor(Mode, EditWithinLevelEditor, Mesh);
				}
			}
		}
	}
}

UThumbnailInfo* FAssetTypeActions_SkeletalMesh::GetThumbnailInfo(UObject* Asset) const
{
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>(Asset);
	UThumbnailInfo* ThumbnailInfo = SkeletalMesh->GetThumbnailInfo();
	if ( ThumbnailInfo == NULL )
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(SkeletalMesh, NAME_None, RF_Transactional);
		SkeletalMesh->SetThumbnailInfo(ThumbnailInfo);
	}

	return ThumbnailInfo;
}

EVisibility FAssetTypeActions_SkeletalMesh::GetThumbnailSkinningOverlayVisibility(const FAssetData AssetData) const
{
	//If the asset was delete it will be remove from the list, in that case do not use the
	//asset since it can be invalid if the GC has collect the object point by the AssetData.
	if (!ThumbnailSkinningOverlayAssetNames.Contains(AssetData.GetFullName()))
	{
		return EVisibility::Collapsed;
	}

	//Prevent loading assets when we display the thumbnail use the asset registry tags

	//Legacy fbx code
	FAssetDataTagMapSharedView::FFindTagResult Result = AssetData.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(UFbxSkeletalMeshImportData, LastImportContentType));
	if (Result.IsSet() && Result.GetValue() == TEXT("FBXICT_Geometry"))
	{
		//Show the icon
		return EVisibility::HitTestInvisible;
	}
#if WITH_EDITORONLY_DATA
	//Generic Interchange code
	Result = AssetData.TagsAndValues.FindTag(NSSkeletalMeshSourceFileLabels::GetSkeletalMeshLastImportContentTypeMetadataKey());
	if (Result.IsSet() && Result.GetValue().Equals(NSSkeletalMeshSourceFileLabels::GeometryMetaDataValue()))
	{
		//Show the icon
		return EVisibility::HitTestInvisible;
	}
#endif
	return EVisibility::Collapsed;
}

void FAssetTypeActions_SkeletalMesh::OnAssetRemoved(const FAssetData& AssetData)
{
	//Remove the object from the list before it get garbage collect
	if (ThumbnailSkinningOverlayAssetNames.Contains(AssetData.GetFullName()))
	{
		ThumbnailSkinningOverlayAssetNames.Remove(AssetData.GetFullName());
	}
}

TSharedPtr<SWidget> FAssetTypeActions_SkeletalMesh::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	const FSlateBrush* Icon = FAppStyle::GetBrush("ClassThumbnailOverlays.SkeletalMesh_NeedSkinning");

	ThumbnailSkinningOverlayAssetNames.AddUnique(AssetData.GetFullName());

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetNoBrush())
		.Visibility(this, &FAssetTypeActions_SkeletalMesh::GetThumbnailSkinningOverlayVisibility, AssetData)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SImage)
			.ToolTipText(LOCTEXT("FAssetTypeActions_SkeletalMesh_NeedSkinning_ToolTip", "Asset geometry was imported, the skinning need to be validate"))
			.Image(Icon)
		];
}

void FAssetTypeActions_SkeletalMesh::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto SkeletalMesh = CastChecked<USkeletalMesh>(Asset);
		SkeletalMesh->GetAssetImportData()->ExtractFilenames(OutSourceFilePaths);
	}
}

void FAssetTypeActions_SkeletalMesh::GetSourceFileLabels(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFileLabels) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto SkeletalMesh = CastChecked<USkeletalMesh>(Asset);
		TArray<FString> SourceFilePaths;
		SkeletalMesh->GetAssetImportData()->ExtractFilenames(SourceFilePaths);
		for (int32 SourceIndex = 0; SourceIndex < SourceFilePaths.Num(); ++SourceIndex)
		{
			FText SourceIndexLabel = USkeletalMesh::GetSourceFileLabelFromIndex(SourceIndex);
			OutSourceFileLabels.Add(SourceIndexLabel.ToString());
		}
	}
}

void FAssetTypeActions_SkeletalMesh::GetLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	check(Objects.Num() > 0);
	//Use the first object
	USkeletalMesh* SkeletalMesh = Objects[0].Get();

	int32 LODMax = SkeletalMesh->GetLODNum();
	for(int32 LOD = 1; LOD <= LODMax; ++LOD)
	{
		const FText Description = (LOD == LODMax) ? FText::Format(LOCTEXT("AddLODLevel", "Add LOD {0}"), FText::AsNumber(LOD)) : FText::Format( LOCTEXT("LODLevel", "Reimport LOD {0}"), FText::AsNumber( LOD ) );
		const FText ToolTip = ( LOD == LODMax ) ? LOCTEXT("NewImportTip", "Import new LOD") : LOCTEXT("ReimportTip", "Reimport over existing LOD");
		MenuBuilder.AddMenuEntry(	Description, 
									ToolTip, FSlateIcon(),
									FUIAction(FExecuteAction::CreateStatic( &FAssetTypeActions_SkeletalMesh::ExecuteImportMeshLOD, static_cast<UObject*>(SkeletalMesh), LOD) )) ;
	}
}

void FAssetTypeActions_SkeletalMesh::GetPhysicsAssetMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("PhysAsset_Create", "Create"),
		LOCTEXT("PhysAsset_Create_ToolTip", "Create new physics assets without assigning it to the selected meshes"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_SkeletalMesh::ExecuteNewPhysicsAsset, Objects, false)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("PhysAsset_CreateAssign", "Create and Assign"),
		LOCTEXT("PhysAsset_CreateAssign_ToolTip", "Create new physics assets and assign it to each of the selected meshes"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_SkeletalMesh::ExecuteNewPhysicsAsset, Objects, true)));
}

void FAssetTypeActions_SkeletalMesh::ExecuteNewPhysicsAsset(TArray<TWeakObjectPtr<USkeletalMesh>> Objects, bool bSetAssetToMesh)
{
	TArray<UObject*> CreatedObjects;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			if (Object->GetOutermost()->bIsCookedForEditor)
			{
				FAssetNotifications::CannotEditCookedAsset(Object);
				continue;
			}
			if(UObject* PhysicsAsset = UPhysicsAssetFactory::CreatePhysicsAssetFromMesh(NAME_None, nullptr, Object, bSetAssetToMesh))
			{
				CreatedObjects.Add(PhysicsAsset);
			}
		}
	}

	if(CreatedObjects.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(CreatedObjects);
#if WITH_EDITOR
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(CreatedObjects);
#endif
	}
}

void FAssetTypeActions_SkeletalMesh::ExecuteNewSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	const FString DefaultSuffix = TEXT("_Skeleton");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			if (Object->GetOutermost()->bIsCookedForEditor)
			{
				FAssetNotifications::CannotEditCookedAsset(Object);
				return;
			}

			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			USkeletonFactory* Factory = NewObject<USkeletonFactory>();
			Factory->TargetSkeletalMesh = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USkeleton::StaticClass(), Factory);
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
				if (Object->GetOutermost()->bIsCookedForEditor)
				{
					FAssetNotifications::CannotEditCookedAsset(Object);
					continue;
				}
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USkeletonFactory* Factory = NewObject<USkeletonFactory>();
				Factory->TargetSkeletalMesh = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USkeleton::StaticClass(), Factory);

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

void FAssetTypeActions_SkeletalMesh::ExecuteAssignSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			AssignSkeletonToMesh(Object);
		}
	}
}

void FAssetTypeActions_SkeletalMesh::ExecuteFindSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->GetSkeleton();
			if (Skeleton)
			{
				ObjectsToSync.AddUnique(Skeleton);
			}
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}


void FAssetTypeActions_SkeletalMesh::ExecuteImportMeshLOD(UObject* Mesh, int32 LOD)
{
	if (Mesh->GetOutermost()->bIsCookedForEditor)
	{
		FAssetNotifications::CannotEditCookedAsset(Mesh);
		return;
	}

	if (LOD == 0)
	{
		//re-import of the asset
		TArray<UObject*> AssetArray;
		AssetArray.Add(Mesh);
		FReimportManager::Instance()->ValidateAllSourceFileAndReimport(AssetArray);
	}
	else
	{
		FbxMeshUtils::ImportMeshLODDialog(Mesh, LOD);
	}
}

void FAssetTypeActions_SkeletalMesh::FillSkeletonMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const
{
	MenuBuilder.BeginSection("SkeletonMenu", LOCTEXT("SkeletonMenuHeading", "Skeleton"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SkeletalMesh_NewSkeleton", "Create Skeleton"),
		LOCTEXT("SkeletalMesh_NewSkeletonTooltip", "Creates a new skeleton for each of the selected meshes."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetIcons.Skeleton"),
		FUIAction(
			FExecuteAction::CreateSP(const_cast<FAssetTypeActions_SkeletalMesh*>(this), &FAssetTypeActions_SkeletalMesh::ExecuteNewSkeleton, Meshes),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SkeletalMesh_AssignSkeleton", "Assign Skeleton"),
		LOCTEXT("SkeletalMesh_AssignSkeletonTooltip", "Assigns a skeleton to the selected meshes."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.AssignSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP(const_cast<FAssetTypeActions_SkeletalMesh*>(this), &FAssetTypeActions_SkeletalMesh::ExecuteAssignSkeleton, Meshes),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SkeletalMesh_FindSkeleton", "Find Skeleton"),
		LOCTEXT("SkeletalMesh_FindSkeletonTooltip", "Finds the skeleton used by the selected meshes in the content browser."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.FindSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP(const_cast<FAssetTypeActions_SkeletalMesh*>(this), &FAssetTypeActions_SkeletalMesh::ExecuteFindSkeleton, Meshes),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_SkeletalMesh::AssignSkeletonToMesh(USkeletalMesh* SkelMesh) const
{
	if (!SkelMesh || SkelMesh->GetOutermost()->bIsCookedForEditor)
	{
		FAssetNotifications::CannotEditCookedAsset(SkelMesh);
		return;
	}

	// Create a skeleton asset from the selected skeletal mesh. Defaults to being in the same package/group as the skeletal mesh.
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title(LOCTEXT("ChooseSkeletonWindowTitle", "Choose Skeleton"))
		.ClientSize(FVector2D(400,600));
	TSharedPtr<SSkeletonSelectorWindow> SkeletonSelectorWindow;
	WidgetWindow->SetContent
		(
			SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(SkeletonSelectorWindow, SSkeletonSelectorWindow)
					.Object(SkelMesh)
					.WidgetWindow(WidgetWindow)
			]
		);

	GEditor->EditorAddModalWindow(WidgetWindow);
	USkeleton* SelectedSkeleton = SkeletonSelectorWindow->GetSelectedSkeleton();

	// only do this if not same
	if ( SelectedSkeleton )
	{
		FDlgMergeSkeleton AssetDlg( SkelMesh, SelectedSkeleton );
		if(AssetDlg.ShowModal() == FDlgMergeSkeleton::Confirm)
		{			
			TArray<int32> RequiredBones;
			RequiredBones.Append(AssetDlg.RequiredBones);

			if ( RequiredBones.Num() > 0 )
			{
				// Do automatic asset generation.
				bool bSuccess = SelectedSkeleton->MergeBonesToBoneTree( SkelMesh, RequiredBones );
				if ( bSuccess )
				{
					if (SkelMesh->GetSkeleton() != SelectedSkeleton)
					{
						SkelMesh->SetSkeleton(SelectedSkeleton);
						SkelMesh->MarkPackageDirty();
					}
					FAssetNotifications::SkeletonNeedsToBeSaved(SelectedSkeleton);
				}
				else
				{
					// if failed, ask if user would like to regenerate skeleton hierarchy
					if ( EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, 
						LOCTEXT("SkeletonMergeBones_Override", "FAILED TO MERGE BONES:  \n\nThis could happen if significant hierarchical changes have been made,\ne.g. inserting a bone between nodes.\nWould you like to regenerate the skeleton from this mesh? \n\n***WARNING: THIS WILL INVALIDATE ALL ANIMATION DATA THAT IS LINKED TO THIS SKELETON***\n")))
					{
						if ( SelectedSkeleton->RecreateBoneTree( SkelMesh ) )
						{
							FAssetNotifications::SkeletonNeedsToBeSaved( SelectedSkeleton );
						}
					}
					else
					{
						FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("SkeletonMergeBonesFailure", "Failed to merge bones to Skeleton") );
					}
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("SkeletonMergeBonesFailure", "Failed to merge bones to Skeleton") );
			}
		}
	}
}

bool FAssetTypeActions_SkeletalMesh::OnAssetCreated(TArray<UObject*> NewAssets) const
{
	if (NewAssets.Num() > 1)
	{
		FAssetTools::Get().SyncBrowserToAssets(NewAssets);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
