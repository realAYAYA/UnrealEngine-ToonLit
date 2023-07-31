// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Skeleton.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FeedbackContext.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "Components/SkeletalMeshComponent.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMeshSocket.h"
#include "FileHelpers.h"
#include "Animation/Rig.h"
#include "SDiscoveringAssetsDialog.h"
#include "AssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PersonaModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SSkeletonWidget.h"
#include "AnimationEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ISkeletonEditorModule.h"
#include "Preferences/PersonaOptions.h"
#include "Algo/Transform.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if WITH_EDITOR
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/** 
* FBoneCheckbox
* 
* Context data for the SCreateRigDlg panel check boxes
*/
struct FBoneCheckbox
{
	FName BoneName;
	int32 BoneID;
	bool  bUsed;
};

/** 
* FCreateRigDlg
* 
* Wrapper class for SCreateRigDlg. This class creates and launches a dialog then awaits the
* result to return to the user. 
*/
class FCreateRigDlg
{
public:
	enum EResult
	{
		Cancel = 0,			// No/Cancel, normal usage would stop the current action
		Confirm = 1,		// Yes/Ok/Etc, normal usage would continue with action
	};

	FCreateRigDlg( USkeleton* InSkeleton );

	/**  Shows the dialog box and waits for the user to respond. */
	EResult ShowModal();

	// Map of RequiredBones of (BoneIndex, ParentIndex)
	TMap<int32, int32> RequiredBones;
private:

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> DialogWindow;

	/** Cached pointer to the merge skeleton widget */
	TSharedPtr<class SCreateRigDlg> DialogWidget;

	/** The Skeleton to merge bones to */
	USkeleton* Skeleton;
};

/** 
 * Slate panel for choosing which bones to merge into the skeleton
 */
class SCreateRigDlg : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCreateRigDlg)
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
		UserResponse = FCreateRigDlg::Cancel;
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
					.OnClicked(this, &SCreateRigDlg::ChangeAllOptions, true)
					.Text(LOCTEXT("SkeletonMergeSelectAll", "Select All"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SCreateRigDlg::ChangeAllOptions, false)
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
					.OnClicked( this, &SCreateRigDlg::OnButtonClick, FCreateRigDlg::Confirm )
					.Text(LOCTEXT("SkeletonMergeOk", "OK"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked( this, &SCreateRigDlg::OnButtonClick, FCreateRigDlg::Cancel )
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
			.IsChecked( this, &SCreateRigDlg::IsCheckboxChecked, ButtonId )
			.OnCheckStateChanged( this, &SCreateRigDlg::OnCheckboxChanged, ButtonId )
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
		FBoneCheckbox& Info = CheckBoxInfoMap.FindChecked(CheckboxThatChanged);
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
			FBoneCheckbox& Info = Iter.Value();
			Info.bUsed = bNewCheckedState;
		}
		return FReply::Handled();
	}

	/**
	 * Populated the dialog with multiple check boxes, each corresponding to a bone
	 *
	 * @param	BoneInfos	The list of Bones to populate the dialog with
	 */
	void PopulateOptions(TArray<FBoneCheckbox>& BoneInfos)
	{
		for(auto Iter = BoneInfos.CreateIterator(); Iter; ++Iter)
		{
			FBoneCheckbox& Info = (*Iter);
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
	FCreateRigDlg::EResult GetUserResponse() const 
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
	FReply OnButtonClick(FCreateRigDlg::EResult ButtonID)
	{
		ParentWindow->RequestDestroyWindow();
		UserResponse = ButtonID;

		return FReply::Handled();
	}

	/** Stores the users response to this dialog */
	FCreateRigDlg::EResult	 UserResponse;

	/** The slate container that the bone check boxes get added to */
	TSharedPtr<SVerticalBox>	 CheckBoxContainer;
	/** Store the check box state for each bone */
	TMap<int32,FBoneCheckbox> CheckBoxInfoMap;

	/** Pointer to the window which holds this Widget, required for modal control */
	TSharedPtr<SWindow>			 ParentWindow;
};

FCreateRigDlg::FCreateRigDlg( USkeleton* InSkeleton )
{
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
				SAssignNew(DialogWidget, SCreateRigDlg)
				.ParentWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

FCreateRigDlg::EResult FCreateRigDlg::ShowModal()
{
	RequiredBones.Empty();

	TArray<FBoneCheckbox> BoneInfos;

	// Make a list of all skeleton bone list
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	for ( int32 BoneTreeId=0; BoneTreeId<RefSkeleton.GetRawBoneNum(); ++BoneTreeId )
	{
		const FName& BoneName = RefSkeleton.GetBoneName(BoneTreeId);

		FBoneCheckbox Info;
		Info.BoneID = BoneTreeId;
		Info.BoneName = BoneName;
		BoneInfos.Add(Info);
	}

	if (BoneInfos.Num() == 0)
	{
		// something wrong
		return EResult::Cancel;
	}

	DialogWidget->PopulateOptions(BoneInfos);

	//Show Dialog
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
	EResult UserResponse = (EResult)DialogWidget->GetUserResponse();

	if(UserResponse == EResult::Confirm)
	{
		for ( int32 RefBoneId= 0 ; RefBoneId< RefSkeleton.GetRawBoneNum() ; ++RefBoneId )
		{
			if ( DialogWidget->IsBoneIncluded(RefBoneId) )
			{
				// I need to find parent that exists in the RefSkeleton
				int32 ParentIndex = RefSkeleton.GetParentIndex(RefBoneId);
				bool bFoundParent = false;

				// make sure RequiredBones already have ParentIndex
				while (ParentIndex >= 0 )
				{
					// if I don't have it yet
					if ( RequiredBones.Contains(ParentIndex) )
					{
						bFoundParent = true;
						// find the Parent that is related
						break;
					}
					else
					{
						ParentIndex = RefSkeleton.GetParentIndex(ParentIndex);					
					}
				}

				if (bFoundParent)
				{
					RequiredBones.Add(RefBoneId, ParentIndex);
				}
				else
				{
					RequiredBones.Add(RefBoneId, INDEX_NONE);
				}
			}
		}
	}

	return (RequiredBones.Num() > 0)? EResult::Confirm : EResult::Cancel;
}

///////////////////////////////
void FAssetTypeActions_Skeleton::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Skeletons = GetTypedWeakObjectPtrs<USkeleton>(InObjects);

	// create menu
	Section.AddSubMenu(
			"CreateSkeletonSubmenu",
			LOCTEXT("CreateSkeletonSubmenu", "Create"),
			LOCTEXT("CreateSkeletonSubmenu_ToolTip", "Create assets for this skeleton"),
			FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_Skeleton::FillCreateMenu, Skeletons),
			false, 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
			);
}

void FAssetTypeActions_Skeleton::FillCreateMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeleton>> Skeletons) const
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	Algo::Transform(Skeletons, Objects, [](const TWeakObjectPtr<USkeleton>& Skeleton) { return Skeleton; });
	AnimationEditorUtils::FillCreateAssetMenu(MenuBuilder, Objects, FAnimAssetCreated::CreateSP(const_cast<FAssetTypeActions_Skeleton*>(this), &FAssetTypeActions_Skeleton::OnAssetCreated));
}

void FAssetTypeActions_Skeleton::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Skeleton = Cast<USkeleton>(*ObjIt);
		if (Skeleton != NULL)
		{
			const bool bBringToFrontIfOpen = true;
#if WITH_EDITOR
			if (IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Skeleton, bBringToFrontIfOpen))
			{
				EditorInstance->FocusWindow(Skeleton);
			}
			else
#endif
			{
				ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
				SkeletonEditorModule.CreateSkeletonEditor(Mode, EditWithinLevelEditor, Skeleton);
			}
		}
	}
}

bool FAssetTypeActions_Skeleton::OnAssetCreated(TArray<UObject*> NewAssets) const
{
	if(NewAssets.Num() > 1)
	{
		FAssetTools::Get().SyncBrowserToAssets(NewAssets);
	}
	return true;
}
#undef LOCTEXT_NAMESPACE
