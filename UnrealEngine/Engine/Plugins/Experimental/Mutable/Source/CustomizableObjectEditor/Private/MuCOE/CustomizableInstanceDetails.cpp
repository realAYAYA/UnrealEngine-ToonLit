// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableInstanceDetails.h"

#include "CustomizableObjectInstanceEditor.h"

#include "ContentBrowserModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "ScopedTransaction.h"
#include "Serialization/BufferArchive.h"
#include "Toolkits/ToolkitManager.h"

#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCOE/SCustomizableInstanceProperties.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"

#include "SSearchableComboBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SUniformGridPanel.h"

class UObject;

#define LOCTEXT_NAMESPACE "CustomizableInstanceDetails"

TAutoConsoleVariable<bool> CVarUseOldInstanceUI(
	TEXT("mutable.UseOldInstanceUI"),
	false,
	TEXT("Enables the old Parameters UI for the Instance Properties tab."));


TSharedRef<IDetailCustomization> FCustomizableInstanceDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableInstanceDetails);
}


void FCustomizableInstanceDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	LayoutBuilder = DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder->GetDetailsView();
	check(DetailsView->GetSelectedObjects().Num());

	CustomInstance = Cast<UCustomizableObjectInstance>(DetailsView->GetSelectedObjects()[0].Get());
	check(CustomInstance.IsValid());

	TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(CustomInstance.Get()); // Tab spawned in a COEInstanceEditor
	if (!FoundAssetEditor)
	{
		FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(CustomInstance->GetCustomizableObject()); // Tab spawned in a COEditor
	}
	check(FoundAssetEditor);

	WeakEditor = StaticCastSharedPtr<ICustomizableObjectInstanceEditor>(FoundAssetEditor).ToWeakPtr();

	if (CustomInstance->GetPrivate()->IsSelectedParameterProfileDirty())
	{
		CustomInstance->GetPrivate()->SaveParametersToProfile(CustomInstance->GetPrivate()->SelectedProfileIndex);
	}

	// Delegate to refresh the detils when the instance has finished the Update
	CustomInstance->UpdatedNativeDelegate.AddSP(this, &FCustomizableInstanceDetails::InstanceUpdated);

	// New Category that will store all properties widgets
	IDetailCategoryBuilder& ResourcesCategory = DetailBuilder->EditCategory("Generated Resources");
	IDetailCategoryBuilder& VisibilitySettingsCategory = DetailBuilder->EditCategory("ParametersVisibility");
	IDetailCategoryBuilder& ParametersCategory = DetailBuilder->EditCategory("Instance Parameters");
	IDetailCategoryBuilder& OldParametersCategory = DetailBuilder->EditCategory("Old Instance Parameters");
	IDetailCategoryBuilder& SkeletalMeshCategory = DetailBuilder->EditCategory("CustomizableSkeletalMesh");
	IDetailCategoryBuilder& TextureParametersCategory = DetailBuilder->EditCategory("TextureParameter");

	// Showing warning message in case that the instance has not been compiled
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	if (!CustomizableObject)
	{
		VisibilitySettingsCategory.AddCustomRow(LOCTEXT("CustomizableInstanceDetails_NoCOMessage", "Instance Parameters"))
		[
			SNew(STextBlock).Text(LOCTEXT("Model not compiled", "Model not compiled"))
		];

		return;
	}

	TArray<UObject*> Private;
	Private.Add(CustomInstance->GetPrivate());

	FAddPropertyParams PrivatePropertyParams;
	PrivatePropertyParams.HideRootObjectNode(true);

	IDetailPropertyRow* PrivateDataRow = ResourcesCategory.AddExternalObjects(Private, EPropertyLocation::Default, PrivatePropertyParams);

	// Store which Texture Parameter values can be selected.
	GenerateTextureParameterOptions();

	// In case that something of the new UI doesn't work as expected
	if(CVarUseOldInstanceUI.GetValueOnGameThread())
	{
		OldParametersCategory.AddCustomRow(LOCTEXT("CustomizableInstanceDetails_OldUI", "Old Instance Parameters"))
		[
			SNew(SCustomizableInstanceProperties)
			.CustomInstance(CustomInstance.Get())
			.InstanceDetails(SharedThis(this))
		];

		return;
	}

	// State Selector Widget
	VisibilitySettingsCategory.AddCustomRow(LOCTEXT("CustomizableInstanceDetails_StateSelector", "State"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("StateSelector_Text","State"))
		.ToolTipText(LOCTEXT("StateSelector_Tooltip","Select a state."))
	]
	.ValueContent()
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	[
		GenerateStateSelector()
	];

	// Profile Selector Widget
	VisibilitySettingsCategory.AddCustomRow(LOCTEXT("CustomizableInstanceDetails_InstanceProfileSelector", "Preview Instance Parameter Profiles"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ProfileSelector_Text","Parameter Profile"))
		.ToolTipText(LOCTEXT("ProfileSelector_Tooltip", "Select a profile to save the parameter options selected."))
	]
	.ValueContent()
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	[
		GenerateInstanceProfileSelector()
	];

	// Only Runtime Parameters Option
	VisibilitySettingsCategory.AddCustomRow( LOCTEXT("CustomizableInstanceDetails_RuntimeParm", "Only Runtime") )
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Only Runtime"))
	]
	.ValueContent()
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	[
		SNew(SCheckBox)
		.IsChecked(CustomInstance->GetPrivate()->bShowOnlyRuntimeParameters ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged(this, &FCustomizableInstanceDetails::OnShowOnlyRuntimeSelectionChanged)
	];
	
	// Only Relevant Parameters Option
	VisibilitySettingsCategory.AddCustomRow( LOCTEXT("CustomizableInstanceDetails_RelevantParam", "Only Relevant") )
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Only Relevant"))
	]
	.ValueContent()
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	[
		SNew(SCheckBox)
		.IsChecked(CustomInstance->GetPrivate()->bShowOnlyRelevantParameters ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged(this, &FCustomizableInstanceDetails::OnShowOnlyRelevantSelectionChanged)
	];
	
	// Show UI sections Option
	VisibilitySettingsCategory.AddCustomRow( LOCTEXT("CustomizableInstanceDetails_UISections", "UI Sections") )
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("UI Sections"))
	]
	.ValueContent()
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	[
		SNew(SCheckBox)
		.IsChecked(CustomInstance->GetPrivate()->bShowUISections ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged(this, &FCustomizableInstanceDetails::OnUseUISectionsSelectionChanged)
	];

	// Copy, Paste and Reset Parameters
	ParametersCategory.AddCustomRow(LOCTEXT("CustomizableInstanceDetails_CopyPasteResetButtons", "Copy Paste Reset"))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(2.0f)
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("Copy_Parameters", "Copy Parameters"))
			.OnClicked(this, &FCustomizableInstanceDetails::OnCopyAllParameters)
			.IsEnabled(CustomInstance->HasAnyParameters())
			.ToolTipText(FText::FromString(FString("Copy the preview Instance parameters")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(2.0f)
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("Paste_Parameters", "Paste Parameters"))
			.OnClicked(this, &FCustomizableInstanceDetails::OnPasteAllParameters)
			.IsEnabled(CustomInstance->HasAnyParameters())
			.ToolTipText(FText::FromString(FString("Paste the preview Instance parameters")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(2.0f)
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("Reset_Integer_Paramaters", "Reset parameters"))
			.OnClicked(this, &FCustomizableInstanceDetails::OnResetAllParameters)
			.IsEnabled(CustomInstance->HasAnyParameters())
			.ToolTipText(FText::FromString(FString("Clear the preview Instance parameters")))
		]
	];

	// Parameters Widgets
	bool bHiddenParamsRuntime = GenerateParametersView(ParametersCategory);

	if (bHiddenParamsRuntime)
	{
		FText HiddenParamsRuntimeMessage = LOCTEXT("CustomizableInstanceDetails_HiddemParamsRuntime", "Parameters are hidden due to their Runtime type. \nUncheck the Only Runtime checkbox to see them.");

		ParametersCategory.AddCustomRow(LOCTEXT("CustomizableInstanceDetails_HiddemParamsRuntimeRow", "Parameters are hidden"))
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(2.0f)
				.Padding(0.0f, 15.0f, 0.0f, 5.0f)
				[
					SNew(STextBlock)
						.Text(HiddenParamsRuntimeMessage)
						.ToolTipText(HiddenParamsRuntimeMessage)
						.AutoWrapText(true)
				]
		];
	}
}


void FCustomizableInstanceDetails::Refresh() const
{
	if (IDetailLayoutBuilder* Layout = LayoutBuilder.Pin().Get()) // Raw because we don't want to keep alive the details builder when calling the force refresh details
	{
		Layout->ForceRefreshDetails();
	}
}


void FCustomizableInstanceDetails::InstanceUpdated(UCustomizableObjectInstance* Instance) const
{
	if (!bUpdatingSlider)
	{
		Refresh();
	}
}


// STATE SELECTOR -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateStateSelector()
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	// States selector options
	const int NumStates = CustomizableObject->GetStateCount();
	const int CurrentState = CustomInstance->GetPrivate()->GetState();
	TSharedPtr<FString> CurrentStateName = nullptr;
	
	//I think that this is not necessary. There is always a "Default" state
	if (NumStates == 0)
	{
		StateNames.Add(MakeShareable(new FString("Default")));
		CurrentStateName = StateNames.Last();
	}

	for (int StateIndex = 0; StateIndex < NumStates; ++StateIndex)
	{
		if (StateIndex == CurrentState)
		{
			CurrentStateName = MakeShareable(new FString(CustomizableObject->GetStateName(StateIndex)));
			StateNames.Add(CurrentStateName);
		}
		else
		{
			StateNames.Add(MakeShareable(new FString(CustomizableObject->GetStateName(StateIndex))));
		}
	}

	StateNames.Sort([](const TSharedPtr<FString>& LHS, const TSharedPtr<FString>& RHS)
	{
		return *LHS < *RHS;
	});

	return SNew(STextComboBox)
		.OptionsSource(&StateNames)
		.InitiallySelectedItem((CustomInstance->GetPrivate()->GetState() != -1) ? CurrentStateName : StateNames[0])
		.OnSelectionChanged(this, &FCustomizableInstanceDetails::OnStateComboBoxSelectionChanged);
}


void FCustomizableInstanceDetails::OnStateComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->SetCurrentState(*Selection);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


// INSTANCE PROFILE SELECTOR -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateInstanceProfileSelector()
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	const int32 ProfileIdx = CustomInstance->GetPrivate()->SelectedProfileIndex;

	ParameterProfileNames.Emplace(MakeShared<FString>("None"));
	TSharedPtr<FString> CurrentProfileName = ParameterProfileNames.Last();

	for (FProfileParameterDat& Profile : CustomInstance->GetCustomizableObject()->GetPrivate()->GetInstancePropertiesProfiles())
	{
		ParameterProfileNames.Emplace(MakeShared<FString>(Profile.ProfileName));

		if (ProfileIdx != INDEX_NONE)
		{
			const FProfileParameterDat CurrentInstanceProfile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIdx];

			if (Profile.ProfileName == CurrentInstanceProfile.ProfileName)
			{
				CurrentProfileName = ParameterProfileNames.Last();
			}
		}
	}

	ParameterProfileNames.Sort([](const TSharedPtr<FString>& LHS, const TSharedPtr<FString>& RHS)
	{
		return *LHS < *RHS;
	});

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(STextComboBox)
			.OptionsSource(&ParameterProfileNames)
			.OnSelectionChanged(this, &FCustomizableInstanceDetails::OnProfileSelectedChanged)
			.ToolTipText(FText::FromString(ParameterProfileNames.Num() > 1 ? FString("Select an existing profile") : FString("No profiles are available")))
			.InitiallySelectedItem(CurrentProfileName)
			.IsEnabled(ParameterProfileNames.Num() > 1)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("AddButtonLabel", " + "))
			.ToolTipText(FText::FromString(CustomInstance->HasAnyParameters() ? FString("Add new profile") : FString("Create a profile functionality is not available, no parameters were found.")))
			.IsEnabled(CustomInstance->HasAnyParameters())
			.IsFocusable(false)
			.OnClicked(this, &FCustomizableInstanceDetails::CreateParameterProfileWindow)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveButtonLabel", " - "))
			.ToolTipText(FText::FromString(CustomInstance->HasAnyParameters() ? FString("Delete selected profile") : FString("Delete selected profile functionality is not available, no profile is selected.")))
			.IsEnabled(CustomInstance->GetPrivate()->SelectedProfileIndex != INDEX_NONE)
			.IsFocusable(false)
			.OnClicked(this, &FCustomizableInstanceDetails::RemoveParameterProfile)
		];
}


class SProfileParametersWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SProfileParametersWindow) {}
		SLATE_ARGUMENT(FText, DefaultAssetPath)
		SLATE_ARGUMENT(FText, DefaultFileName)
	SLATE_END_ARGS()

	SProfileParametersWindow() : UserResponse(EAppReturnType::Cancel) {}
	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** FileName getter */
	FString GetFileName() const { return FileName.ToString(); }

protected:

	//FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	void OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo);
	
	EAppReturnType::Type UserResponse;
	FText AssetPath;
	FText FileName;

public:
	TWeakObjectPtr<UCustomizableObjectInstance> CustomInstance;

	FCustomizableInstanceDetails* InstanceDetails = nullptr;
};


FReply FCustomizableInstanceDetails::CreateParameterProfileWindow()
{
	const TSharedRef<SProfileParametersWindow> FolderDlg =
		SNew(SProfileParametersWindow)
		.DefaultAssetPath(LOCTEXT("DefaultAssethPath", "/Game"))
		.DefaultFileName(FText::FromString("ProfileParameterData"));

	FolderDlg->CustomInstance = CustomInstance;
	FolderDlg->InstanceDetails = this;
	FolderDlg->Construct(SProfileParametersWindow::FArguments());
	FolderDlg->ShowModal();

	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::RemoveParameterProfile()
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	const int32 ProfileIdx = CustomInstance->GetPrivate()->SelectedProfileIndex;

	if (ProfileIdx == INDEX_NONE)
	{
		return FReply::Handled();
	}

	TArray<FProfileParameterDat>& Profiles = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles();

	Profiles.RemoveAt(ProfileIdx);
	CustomInstance->GetPrivate()->SelectedProfileIndex = INDEX_NONE;
	CustomizableObject->Modify();

	CustomInstance->PreEditChange(nullptr);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();

	return FReply::Handled();
}


void FCustomizableInstanceDetails::OnProfileSelectedChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	const int32 ProfileIdx = CustomInstance->GetPrivate()->SelectedProfileIndex;
	if (CustomInstance->GetPrivate()->IsSelectedParameterProfileDirty())
	{
		CustomInstance->GetPrivate()->SaveParametersToProfile(ProfileIdx);
	}

	if (*Selection == "None")
	{
		CustomInstance->GetPrivate()->SelectedProfileIndex = INDEX_NONE;
	}
	else
	{
		//Set selected profile
		TArray<FProfileParameterDat>& Profiles = CustomInstance->GetCustomizableObject()->GetPrivate()->GetInstancePropertiesProfiles();
		for (int32 Idx = 0; Idx < Profiles.Num(); ++Idx)
		{
			if (Profiles[Idx].ProfileName == *Selection)
			{
				CustomInstance->GetPrivate()->SelectedProfileIndex = Idx;
				break;
			}
		}
	}	

	CustomInstance->GetPrivate()->LoadParametersFromProfile(CustomInstance->GetPrivate()->SelectedProfileIndex);
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


void FCustomizableInstanceDetails::OnShowOnlyRuntimeSelectionChanged(ECheckBoxState InCheckboxState)
{
	CustomInstance->GetPrivate()->bShowOnlyRuntimeParameters = InCheckboxState == ECheckBoxState::Checked;
	Refresh();
}


void FCustomizableInstanceDetails::OnShowOnlyRelevantSelectionChanged(ECheckBoxState InCheckboxState)
{
	CustomInstance->GetPrivate()->bShowOnlyRelevantParameters = InCheckboxState == ECheckBoxState::Checked;
	Refresh();
}


void FCustomizableInstanceDetails::OnUseUISectionsSelectionChanged(ECheckBoxState InCheckboxState)
{
	CustomInstance->GetPrivate()->bShowUISections = InCheckboxState == ECheckBoxState::Checked;
	Refresh();
}


// PARAMETER INFO STRUCT -----------------------------------------------------------------------------------------------------------------

struct FParameterInfo
{
	int32 ParamIndexInObject = -1;
	int32 ParamUIOrder = 0;
	FString ParamName = "";

	bool operator==(const FParameterInfo& Other)
	{
		return ParamName == Other.ParamName;
	}

	friend bool operator<(const FParameterInfo& A, const FParameterInfo& B)
	{
		if (A.ParamUIOrder != B.ParamUIOrder)
		{
			return A.ParamUIOrder < B.ParamUIOrder;
		}
		else
		{
			return A.ParamName < B.ParamName;
		}
	}
};


// PARAMETERS WIDGET GENERATION -----------------------------------------------------------------------------------------------------------------

bool FCustomizableInstanceDetails::GenerateParametersView(IDetailCategoryBuilder& DetailsCategory)
{
	// Make this arrays locals
	ParamChildren.Empty();
	ParamHasParent.Empty();
	GeneratedSections.Empty();

	TArray<FParameterInfo> ParametersTree;
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	if (!CustomizableObject)
	{
		return false;
	}

	bool bParametersHiddenRuntime = false;

	if (CustomInstance->GetPrivate()->bShowOnlyRuntimeParameters)
	{
		const int32 NumStateParameters = CustomizableObject->GetStateParameterCount(CustomInstance->GetPrivate()->GetState());

		if (NumStateParameters < CustomizableObject->GetParameterCount())
		{
			bParametersHiddenRuntime = true;
		}

		for (int32 ParamIndexInState = 0; ParamIndexInState < NumStateParameters; ++ParamIndexInState)
		{
			FParameterInfo ParameterSortInfo;
			ParameterSortInfo.ParamIndexInObject = CustomizableObject->GetStateParameterIndex(CustomInstance->GetPrivate()->GetState(), ParamIndexInState);

			if (CustomInstance->IsParameterRelevant(ParameterSortInfo.ParamIndexInObject) && IsVisible(ParameterSortInfo.ParamIndexInObject))
			{
				ParameterSortInfo.ParamUIOrder = CustomizableObject->GetParameterUIMetadataFromIndex(ParameterSortInfo.ParamIndexInObject).ParamUIMetadata.UIOrder;
				ParameterSortInfo.ParamName = CustomizableObject->GetParameterName(ParameterSortInfo.ParamIndexInObject);
				ParametersTree.Add(ParameterSortInfo);
			}
		}

		ParametersTree.Sort();

		for (int32 ParamIndex = 0; ParamIndex < ParametersTree.Num(); ++ParamIndex)
		{
			const FParameterInfo& ParamInfo = ParametersTree[ParamIndex];

			if (CustomInstance->GetPrivate()->bShowUISections)
			{
				IDetailGroup* CurrentSection = GenerateParameterSection(ParamInfo.ParamIndexInObject, DetailsCategory);
				check(CurrentSection);

				if (!IsMultidimensionalProjector(ParamInfo.ParamIndexInObject))
				{
					GenerateWidgetRow(CurrentSection->AddWidgetRow(), ParamInfo.ParamName, ParamInfo.ParamIndexInObject);
				}
				else
				{
					IDetailGroup* ProjectorGroup = &CurrentSection->AddGroup(FName(*ParamInfo.ParamName), FText::GetEmpty());

					// Call Order between the following lines maters.
					ParentsGroups.Add(ParamInfo.ParamName, ProjectorGroup);
					GenerateWidgetRow(ProjectorGroup->HeaderRow(), ParamInfo.ParamName, ParamInfo.ParamIndexInObject);
				}
			}
			else
			{
				if (!IsMultidimensionalProjector(ParamInfo.ParamIndexInObject))
				{
					GenerateWidgetRow(DetailsCategory.AddCustomRow(FText::FromString(ParamInfo.ParamName)), ParamInfo.ParamName, ParamInfo.ParamIndexInObject);
				}
				else
				{
					IDetailGroup* ProjectorGroup = &DetailsCategory.AddGroup(FName(*ParamInfo.ParamName), FText::GetEmpty());

					// Call Order between the following lines maters.
					ParentsGroups.Add(ParamInfo.ParamName, ProjectorGroup);
					GenerateWidgetRow(ProjectorGroup->HeaderRow(), ParamInfo.ParamName, ParamInfo.ParamIndexInObject);
				}
			}
		}
	}
	else
	{
		const int32 NumObjectParameter = CustomizableObject->GetParameterCount();

		//TODO: get all parameters and sort, then make the next "for" use that sorted list as source of indexes
		for (int32 ParamIndexInObject = 0; ParamIndexInObject < NumObjectParameter; ++ParamIndexInObject)
		{
			FParameterInfo ParameterSortInfo;
			ParameterSortInfo.ParamIndexInObject = ParamIndexInObject;
			if ((!CustomInstance->GetPrivate()->bShowOnlyRelevantParameters || CustomInstance->IsParameterRelevant(ParameterSortInfo.ParamIndexInObject)) && IsVisible(ParameterSortInfo.ParamIndexInObject))
			{
				ParameterSortInfo.ParamUIOrder = CustomizableObject->GetParameterUIMetadataFromIndex(ParameterSortInfo.ParamIndexInObject).ParamUIMetadata.UIOrder;
				ParameterSortInfo.ParamName = CustomizableObject->GetParameterName(ParameterSortInfo.ParamIndexInObject);
				ParametersTree.Add(ParameterSortInfo);
			}
		}

		ParametersTree.Sort();

		for (int32 ParamIndexInObject = 0; ParamIndexInObject < ParametersTree.Num(); ++ParamIndexInObject)
		{
			FillChildrenMap(ParametersTree[ParamIndexInObject].ParamIndexInObject);
		}

		for (int32 ParamIndexInObject = 0; ParamIndexInObject < ParametersTree.Num(); ++ParamIndexInObject)
		{
			if (!ParamHasParent.Find(ParametersTree[ParamIndexInObject].ParamIndexInObject))
			{
				RecursivelyAddParamAndChildren(ParametersTree[ParamIndexInObject].ParamIndexInObject, "", DetailsCategory);
			}
		}
	}

	return bParametersHiddenRuntime;
}


void FCustomizableInstanceDetails::RecursivelyAddParamAndChildren(const int32 ParamIndexInObject, const FString ParentName, IDetailCategoryBuilder& DetailsCategory)
{
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	const FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);	
	TArray<int32> Children;
	
	ParamChildren.MultiFind(ParamName, Children, true);

	if (ParentName.IsEmpty())
	{
		if (CustomInstance->GetPrivate()->bShowUISections)
		{
			IDetailGroup* CurrentSection = GenerateParameterSection(ParamIndexInObject, DetailsCategory);
			check(CurrentSection);

			if (Children.Num() == 0 && !IsMultidimensionalProjector(ParamIndexInObject))
			{
				GenerateWidgetRow(CurrentSection->AddWidgetRow(), ParamName, ParamIndexInObject);
			}
			else
			{
				IDetailGroup* ParentGroup = &CurrentSection->AddGroup(FName(*ParamName), FText::GetEmpty());
				
				// Call Order between the following lines maters.
				ParentsGroups.Add(ParamName, ParentGroup);
				GenerateWidgetRow(ParentGroup->HeaderRow(), ParamName, ParamIndexInObject);
			}
		}
		else
		{
			if (Children.Num() == 0 && !IsMultidimensionalProjector(ParamIndexInObject))
			{
				GenerateWidgetRow(DetailsCategory.AddCustomRow(FText::FromString(ParamName)), ParamName, ParamIndexInObject);
			}
			else
			{
				IDetailGroup* ParentGroup = &DetailsCategory.AddGroup(FName(*ParamName), FText::GetEmpty());
				
				// Call Order between the following lines maters.
				ParentsGroups.Add(ParamName, ParentGroup);
				GenerateWidgetRow(ParentGroup->HeaderRow(), ParamName, ParamIndexInObject);
			}
		}
	}
	else
	{
		IDetailGroup* ParentGroup = *ParentsGroups.Find(ParentName);

		if (Children.Num() == 0 && !IsMultidimensionalProjector(ParamIndexInObject))
		{
			GenerateWidgetRow(ParentGroup->AddWidgetRow(), ParamName, ParamIndexInObject);
		}
		else
		{
			IDetailGroup* ChildGroup = &ParentGroup->AddGroup(FName(*ParamName), FText::GetEmpty());

			// Call Order between the following lines maters
			ParentsGroups.Add(ParamName, ChildGroup);
			GenerateWidgetRow(ParentGroup->HeaderRow(), ParamName, ParamIndexInObject);
		}
	}

	for (const int32 ChildIndexInObject : Children)
	{
		RecursivelyAddParamAndChildren(ChildIndexInObject, ParamName, DetailsCategory);
	}
}


void FCustomizableInstanceDetails::FillChildrenMap(int32 ParamIndexInObject)
{
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);
	FParameterUIData UIData = CustomizableObject->GetParameterUIMetadataFromIndex(ParamIndexInObject);

	const FString* ParentName = UIData.ParamUIMetadata.ExtraInformation.Find(FString("__ParentParamName"));

	if (ParentName)
	{
		ParamChildren.Add(*ParentName, ParamIndexInObject);
		ParamHasParent.Add(ParamIndexInObject, true);
	}
}


bool FCustomizableInstanceDetails::IsVisible(int32 ParamIndexInObject)
{
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);
	FParameterUIData UIData = CustomizableObject->GetParameterUIMetadataFromIndex(ParamIndexInObject);
	FString* ParentName = UIData.ParamUIMetadata.ExtraInformation.Find(FString("__ParentParamName"));

	bool IsAProjectorParam = ParamName.EndsWith(FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX)
		|| (ParamName.EndsWith(FMultilayerProjector::IMAGE_PARAMETER_POSTFIX) && CustomizableObject->IsParameterMultidimensional(ParamIndexInObject))
		|| (ParamName.EndsWith(FMultilayerProjector::OPACITY_PARAMETER_POSTFIX) && CustomizableObject->IsParameterMultidimensional(ParamIndexInObject))
		|| (ParamName.EndsWith(FMultilayerProjector::POSE_PARAMETER_POSTFIX));

	if (!IsAProjectorParam && ParentName && CustomInstance->GetPrivate()->bShowOnlyRelevantParameters)
	{
		FString* Value = UIData.ParamUIMetadata.ExtraInformation.Find(FString("__DisplayWhenParentValueEquals"));

		if (Value && CustomizableObject->FindParameter(*ParentName) != INDEX_NONE && CustomInstance->GetIntParameterSelectedOption(*ParentName) != *Value)
		{
			return false;
		}
	}

	return !IsAProjectorParam;
}


bool FCustomizableInstanceDetails::IsMultidimensionalProjector(int32 ParamIndexInObject)
{
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	return CustomizableObject->GetParameterType(ParamIndexInObject) == EMutableParameterType::Projector && CustomInstance->GetCustomizableObject()->IsParameterMultidimensional(ParamIndexInObject);
}


IDetailGroup* FCustomizableInstanceDetails::GenerateParameterSection(const int32 ParamIndexInObject, IDetailCategoryBuilder& DetailsCategory)
{
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	FMutableParamUIMetadata UIData = CustomizableObject->GetParameterUIMetadataFromIndex(ParamIndexInObject).ParamUIMetadata;
	FString SectionName = UIData.UISectionName.IsEmpty() ? "Miscellaneous" : UIData.UISectionName;
	IDetailGroup* CurrentSection = nullptr;

	if (GeneratedSections.Contains(SectionName))
	{
		CurrentSection = *GeneratedSections.Find(SectionName);
	}

	if (!CurrentSection)
	{
		CurrentSection = &DetailsCategory.AddGroup(FName(SectionName), FText::FromString(SectionName));

		GeneratedSections.Add(SectionName, CurrentSection);
	}

	return CurrentSection;
}


void FCustomizableInstanceDetails::GenerateWidgetRow(FDetailWidgetRow& WidgetRow, const FString& ParamName, const int32 ParamIndexInObject)
{
	WidgetRow.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(ParamName))
	]
	.ValueContent()
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	.VAlign(EVerticalAlignment::VAlign_Fill)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		[
			GenerateParameterWidget(ParamIndexInObject)
		]
	]
	.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableInstanceDetails::OnResetParameterButtonClicked, ParamIndexInObject)))
	.FilterString(FText::FromString(ParamName));
}


TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateParameterWidget(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	switch (CustomizableObject->GetParameterType(ParamIndexInObject))
	{
		case EMutableParameterType::Bool:
		{
			return GenerateBoolWidget(ParamIndexInObject);
		}
		case EMutableParameterType::Float:
		{
			return GenerateFloatWidget(ParamIndexInObject);
		}
		case EMutableParameterType::Color:
		{
			return GenerateColorWidget(ParamIndexInObject);
		}
		case EMutableParameterType::Texture:
		{
			return GenerateTextureWidget(ParamIndexInObject);
		}
		case EMutableParameterType::Projector:
		{
			bool bIsParamMultidimensional = CustomInstance->GetCustomizableObject()->IsParameterMultidimensional(ParamIndexInObject);

			if (!bIsParamMultidimensional)
			{
				return GenerateSimpleProjector(ParamIndexInObject);
			}
			else 
			{
				return GenerateMultidimensionalProjector(ParamIndexInObject);
			}
		}
		case EMutableParameterType::Int:
		{
			return GenerateIntWidget(ParamIndexInObject);
		}
		case EMutableParameterType::None:
		{
			return SNew(STextBlock).Text(LOCTEXT("ParameterTypeNotSupported_Text", "Parameter Type not supported"));
		}
	}

	return SNew(STextBlock);
}


// INT PARAMETERS -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateIntWidget(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	int32 numValues = CustomizableObject->GetIntParameterNumOptions(ParamIndexInObject);
	bool bIsParamMultidimensional = CustomInstance->GetCustomizableObject()->IsParameterMultidimensional((ParamIndexInObject));

	if (!bIsParamMultidimensional && numValues)
	{
		FString ToolTipText = FString("None");
		FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);
		FString Value = CustomInstance->GetIntParameterSelectedOption(ParamName, -1);

		TSharedPtr<TArray<TSharedPtr<FString>>>* FoundOptions = IntParameterOptions.Find(ParamIndexInObject);
		TArray<TSharedPtr<FString>>& OptionNamesAttribute = FoundOptions && FoundOptions->IsValid() ? 
															*FoundOptions->Get() :
															*(IntParameterOptions.Add(ParamIndexInObject, MakeShared<TArray<TSharedPtr<FString>>>()).Get());

		OptionNamesAttribute.Empty();

		int32 ValueIndex = 0;

		for (int32 i = 0; i < numValues; ++i)
		{
			FString PossibleValue = CustomizableObject->GetIntParameterAvailableOption(ParamIndexInObject, i);

			if (PossibleValue == Value)
			{
				ValueIndex = i;

				const FString* Identifier = CustomizableObject->GetPrivate()->GroupNodeMap.FindKey(FCustomizableObjectIdPair(ParamName, PossibleValue));
				if (Identifier)
				{
					if (FString* CustomizableObjectPath = CustomizableObject->GetPrivate()->CustomizableObjectPathMap.Find(*Identifier))
					{
						ToolTipText = *CustomizableObjectPath;
					}
				}
			}

			OptionNamesAttribute.Add(TSharedPtr<FString>(new FString(CustomizableObject->GetIntParameterAvailableOption(ParamIndexInObject, i))));
		}

		return SNew(SSearchableComboBox)
			.ToolTipText(FText::FromString(ToolTipText))
			.OptionsSource(&OptionNamesAttribute)
			.InitiallySelectedItem(OptionNamesAttribute[ValueIndex])
			.Method(EPopupMethod::UseCurrentWindow)
			.OnSelectionChanged(this, &FCustomizableInstanceDetails::OnIntParameterComboBoxChanged, ParamName)
			.OnGenerateWidget(this, &FCustomizableInstanceDetails::OnGenerateWidgetIntParameter)
			.Content()
			[
				SNew(STextBlock)
					.Text(FText::FromString(*OptionNamesAttribute[ValueIndex]))
			];
	}

	return SNew(STextBlock).Text(LOCTEXT("MultidimensionalINTParameter_Text", "Multidimensional INT Parameter not supported"));
}


TSharedRef<SWidget> FCustomizableInstanceDetails::OnGenerateWidgetIntParameter(TSharedPtr<FString> InItem) const
{
	return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
}


void FCustomizableInstanceDetails::OnIntParameterComboBoxChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, FString ParamName)
{
	TArray<FCustomizableObjectIntParameterValue>& IntParameters = CustomInstance->GetIntParameters();
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	for (int32 i = 0; i < IntParameters.Num(); ++i)
	{
		if (IntParameters[i].ParameterName == ParamName)
		{
			const int32 ParamIndexInObject = CustomizableObject->FindParameter(ParamName);
			if (ParamIndexInObject != INDEX_NONE)
			{
				for (int32 v = 0; v < CustomizableObject->GetIntParameterNumOptions(ParamIndexInObject); ++v)
				{
					FString ValueName = CustomizableObject->GetIntParameterAvailableOption(ParamIndexInObject, v);

					if (ValueName == *Selection)
					{
						IntParameters[i].ParameterValueName = ValueName;
						break;
					}
				}
			}
			else
			{
				UE_LOG(LogMutable, Error, TEXT("Failed to find parameter."));
			}
		}
	}

	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


// FLOAT PARAMETERS -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateFloatWidget(const int32 ParamIndexInObject)
{
	const UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);

	return SNew(SSpinBox<float>)
		.Value(this, &FCustomizableInstanceDetails::GetFloatParameterValue, ParamName, -1)
		.MinValue(CustomizableObject->GetParameterUIMetadataFromIndex(ParamIndexInObject).ParamUIMetadata.MinimumValue)
		.MaxValue(CustomizableObject->GetParameterUIMetadataFromIndex(ParamIndexInObject).ParamUIMetadata.MaximumValue)
		.OnValueChanged(this, &FCustomizableInstanceDetails::OnFloatParameterChanged, ParamName, -1)
		.OnBeginSliderMovement(this, &FCustomizableInstanceDetails::OnFloatParameterSliderBegin)
		.OnEndSliderMovement(this, &FCustomizableInstanceDetails::OnFloatParameterSliderEnd, ParamName, -1);
}


float FCustomizableInstanceDetails::GetFloatParameterValue(FString ParamName, int32 RangeIndex) const
{
	if (RangeIndex == INDEX_NONE)
	{
		return CustomInstance->GetFloatParameterSelectedOption(ParamName, RangeIndex);
	}
	else //multidimensional
	{
		// We may have deleted a range but the Instance has not been updated yet
		if (CustomInstance->GetFloatValueRange(ParamName) > RangeIndex)
		{
			return CustomInstance->GetFloatParameterSelectedOption(ParamName, RangeIndex);
		}
	}

	return 0;
}


void FCustomizableInstanceDetails::OnFloatParameterChanged(float Value, FString ParamName, int32 RangeIndex)
{
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();

	CustomInstance->SetFloatParameterSelectedOption(ParamName, Value, RangeIndex);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	
	CustomInstance->PostEditChange();
}


void FCustomizableInstanceDetails::OnFloatParameterSliderBegin()
{
	bUpdatingSlider = true;
}


void FCustomizableInstanceDetails::OnFloatParameterSliderEnd(float Value, FString ParamName, int32 RangeIndex)
{
	bUpdatingSlider = false;

	CustomInstance->PreEditChange(nullptr);
	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->SetFloatParameterSelectedOption(ParamName, Value, RangeIndex);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


// TEXTURE PARAMETERS -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateTextureWidget(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);
	const FName ParameterValue = CustomInstance->GetTextureParameterSelectedOption(ParamName);
	TSharedPtr<FString> InitiallySelected;

	// Look for the value index
	for (int32 ValueIndex = 0; ValueIndex < TextureParameterValueNames.Num(); ++ValueIndex)
	{
		if (ParameterValue == TextureParameterValues[ValueIndex])
		{
			InitiallySelected = TextureParameterValueNames[ValueIndex];
			break;
		}
	}

	return SNew(STextComboBox)
		.OptionsSource(&TextureParameterValueNames)
		.InitiallySelectedItem(InitiallySelected)
		.OnSelectionChanged(this, &FCustomizableInstanceDetails::OnTextureParameterComboBoxSelectionChanged, ParamName);
}


void FCustomizableInstanceDetails::GenerateTextureParameterOptions()
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();

	for (const FCustomizableObjectTextureParameterValue& TextureParameter : CustomInstance->GetTextureParameters())
	{
		// Get default values.
		const FName& DefaultValue = CustomizableObject->GetTextureParameterDefaultValue(TextureParameter.ParameterName);

		if (!TextureParameterValues.Contains(DefaultValue))
		{
			TextureParameterValueNames.Add(MakeShareable(new FString(DefaultValue.ToString())));
			TextureParameterValues.Add(DefaultValue);
		}

		// Selected parameter value
		const FName& SelectedValue = TextureParameter.ParameterValue;

		if (!TextureParameterValues.Contains(SelectedValue))
		{
			TextureParameterValueNames.Add(MakeShareable(new FString(SelectedValue.ToString())));
			TextureParameterValues.Add(SelectedValue);
		}
	}

	// Get values from registered providers providers.
	TArray<FCustomizableObjectExternalTexture> Textures = UCustomizableObjectSystem::GetInstance()->GetTextureParameterValues();
	for (int i = 0; i < Textures.Num(); ++i)
	{
		const FName& Value = FName(Textures[i].Value);

		if (!TextureParameterValues.Contains(Value))
		{
			TextureParameterValueNames.Add(MakeShareable(new FString(Textures[i].Name)));
			TextureParameterValues.Add(Value);
		}
	}

	// Get values from TextureParameterDeclarations.
	for (TObjectPtr<UTexture2D> Declaration : CustomInstance->TextureParameterDeclarations)
	{
		if (!Declaration)
		{
			continue;
		}

		const FName Value = FName(Declaration.GetPathName());

		if (!TextureParameterValues.Contains(Value))
		{
			TextureParameterValueNames.Add(MakeShareable(new FString(Declaration.GetName())));
			TextureParameterValues.Add(Value);
		}
	}
}


void FCustomizableInstanceDetails::OnTextureParameterComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, FString ParamName)
{
	CustomInstance->PreEditChange(nullptr);

	auto& TextureParameters = CustomInstance->GetTextureParameters();

	for (int i = 0; i < TextureParameters.Num(); ++i)
	{
		if (TextureParameters[i].ParameterName == ParamName)
		{
			for (int o = 0; o < TextureParameterValueNames.Num(); ++o)
			{
				if (Selection.IsValid() && *TextureParameterValueNames[o] == *Selection)
				{
					TextureParameters[i].ParameterValue = TextureParameterValues[o];
				}
			}
		}
	}

	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


// COLOR PARAMETERS -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateColorWidget(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);

	return SNew(SColorBlock)
		.Color(this, &FCustomizableInstanceDetails::GetColorParameterValue, ParamName)
		.ShowBackgroundForAlpha(false)
		.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
		.UseSRGB(false)
		.OnMouseButtonDown(this, &FCustomizableInstanceDetails::OnColorBlockMouseButtonDown, ParamName)
		.CornerRadius(FVector4(4.0f, 4.0f, 4.0f, 4.0f));
}


FLinearColor FCustomizableInstanceDetails::GetColorParameterValue(FString ParamName) const
{
	return CustomInstance->GetColorParameterSelectedOption(ParamName);
}


FReply FCustomizableInstanceDetails::OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FString ParamName)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	FLinearColor Col = GetColorParameterValue(ParamName);

	FColorPickerArgs args;
	args.bIsModal = true;
	args.bUseAlpha = true;
	args.bOnlyRefreshOnMouseUp = false;
	args.InitialColor = Col;
	args.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FCustomizableInstanceDetails::OnSetColorFromColorPicker, ParamName);
	OpenColorPicker(args);

	return FReply::Handled();
}


void FCustomizableInstanceDetails::OnSetColorFromColorPicker(FLinearColor NewColor, FString PickerParamName) const
{
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->SetColorParameterSelectedOption(PickerParamName, NewColor);
	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();

	PickerParamName = FString();

	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();
}


// PROJECTOR PARAMETERS -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateSimpleProjector(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);

	const TSharedPtr<ICustomizableObjectInstanceEditor> Editor = GetEditorChecked();
	const UProjectorParameter* ProjectorParameter = Editor->GetProjectorParameter();
	const bool bSelectedProjector = ProjectorParameter->IsProjectorSelected(ParamName);

	TSharedPtr<SButton> Button;
	TSharedPtr<SHorizontalBox> SimpleProjectorBox;

	SAssignNew(SimpleProjectorBox,SHorizontalBox)
	+ SHorizontalBox::Slot()
	.FillWidth(0.25f)
	[
		SAssignNew(Button, SButton)
		.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorSelectChanged, ParamName, -1)
		.HAlign(HAlign_Center)
		.Content()
		[
			SNew(STextBlock)
			.Text(bSelectedProjector ? LOCTEXT("Unselect Projector", "Unselect Projector") : LOCTEXT("Select Projector", "Select Projector"))
			.ToolTipText(bSelectedProjector ? LOCTEXT("Unselect Projector", "Unselect Projector") : LOCTEXT("Select Projector", "Select Projector"))
			.Font(LayoutBuilder.Pin()->GetDetailFont())
		]
	]

	+ SHorizontalBox::Slot()
	.FillWidth(0.25f)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorCopyTransform, ParamName, -1)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CopyTransform_Text", "Copy Transform"))
			.ToolTipText(LOCTEXT("CopyTransform_Tooltip", "Copy Transform"))
			.Font(LayoutBuilder.Pin()->GetDetailFont())
		]
	]

	+ SHorizontalBox::Slot()
	.FillWidth(0.25f)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorPasteTransform, ParamName, -1)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PasteTransform_Text", "Paste Transform"))
			.ToolTipText(LOCTEXT("PasteTransform_Tooltip", "Paste Transform"))
			.Font(LayoutBuilder.Pin()->GetDetailFont())
		]
	]

	+ SHorizontalBox::Slot()
	.FillWidth(0.25f)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorResetTransform, ParamName, -1)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ResetTransform_Text", "Reset Transform"))
			.ToolTipText(LOCTEXT("ResetTransform_Tooltip", "Reset Transform"))
			.Font(LayoutBuilder.Pin()->GetDetailFont())
		]
	];

	Button->SetBorderBackgroundColor(bSelectedProjector ? FLinearColor::Green : FLinearColor::White);

	return SimpleProjectorBox.ToSharedRef();
}


FReply FCustomizableInstanceDetails::OnProjectorSelectChanged(const FString ParamName, const int32 RangeIndex) const
{
	const TSharedPtr<ICustomizableObjectInstanceEditor> Editor = GetEditorChecked();

	const UProjectorParameter* ProjectorParameter = Editor->GetProjectorParameter();
	if (ProjectorParameter->IsProjectorSelected(ParamName, RangeIndex))
	{
		Editor->HideGizmo();
	}
	else
	{
		Editor->ShowGizmoProjectorParameter(ParamName, RangeIndex);
	}

	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::OnProjectorCopyTransform(const FString ParamName, const int32 RangeIndex) const
{
	const int32 ParameterIndexInObject = CustomInstance->GetCustomizableObject()->FindParameter(ParamName);
	const int32 ProjectorParamIndex = CustomInstance->FindProjectorParameterNameIndex(ParamName);

	if ((ParameterIndexInObject >= 0) && (ProjectorParamIndex >= 0))
	{
		TArray<FCustomizableObjectProjectorParameterValue>& ProjectorParameters = CustomInstance->GetProjectorParameters();
		FCustomizableObjectProjector Value;

		if (RangeIndex == -1)
		{
			Value = ProjectorParameters[ProjectorParamIndex].Value;
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			Value = ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex];
		}
		else
		{
			check(false);
		}

		const UScriptStruct* Struct = Value.StaticStruct();
		FString Output = TEXT("");
		Struct->ExportText(Output, &Value, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);

		FPlatformApplicationMisc::ClipboardCopy(*Output);
	}

	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::OnProjectorPasteTransform(const FString ParamName, const int32 RangeIndex)
{
	FScopedTransaction Transaction(LOCTEXT("PasteTransform", "Paste Transform"));
	CustomInstance->Modify();

	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	FCustomizableObjectProjector DefaultValue;
	UScriptStruct* Struct = DefaultValue.StaticStruct();
	Struct->ImportText(*ClipboardText, &DefaultValue, nullptr, 0, GLog, GetPathNameSafe(Struct));

	CustomInstance->SetProjectorValue(ParamName,
		static_cast<FVector>(DefaultValue.Position),
		static_cast<FVector>(DefaultValue.Direction),
		static_cast<FVector>(DefaultValue.Up),
		static_cast<FVector>(DefaultValue.Scale),
		DefaultValue.Angle,
		RangeIndex);

	const TSharedPtr<ICustomizableObjectInstanceEditor> Editor = GetEditorChecked();

	Editor->ShowGizmoProjectorParameter(ParamName, RangeIndex);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);

	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::OnProjectorResetTransform(const FString ParamName, const int32 RangeIndex)
{
	FScopedTransaction Transaction(LOCTEXT("ResetTransform", "Reset Transform"));
	CustomInstance->Modify();

	const FCustomizableObjectProjector DefaultValue = CustomInstance->GetCustomizableObject()->GetProjectorParameterDefaultValue(ParamName);

	CustomInstance->SetProjectorValue(ParamName,
		static_cast<FVector>(DefaultValue.Position),
		static_cast<FVector>(DefaultValue.Direction),
		static_cast<FVector>(DefaultValue.Up),
		static_cast<FVector>(DefaultValue.Scale),
		DefaultValue.Angle,
		RangeIndex);

	const TSharedPtr<ICustomizableObjectInstanceEditor> Editor = GetEditorChecked();

	Editor->ShowGizmoProjectorParameter(ParamName, RangeIndex);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);

	return FReply::Handled();
}


TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateMultidimensionalProjector(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);

	const TSharedPtr<ICustomizableObjectInstanceEditor> Editor = GetEditorChecked();
	TArray<FCustomizableObjectProjectorParameterValue>& ProjectorParameters = CustomInstance->GetProjectorParameters();
	const int32 ProjectorParamIndex = CustomInstance->FindProjectorParameterNameIndex(ParamName);

	check(ProjectorParamIndex < ProjectorParameters.Num());

	// Selected Pose UI
	const FString PoseSwitchEnumParamName = ParamName + FMultilayerProjector::POSE_PARAMETER_POSTFIX;
	const int32 PoseSwitchEnumParamIndexInObject = CustomizableObject->FindParameter(PoseSwitchEnumParamName);
	TSharedPtr<SVerticalBox> ProjectorBox = SNew(SVerticalBox);

	if (PoseSwitchEnumParamIndexInObject != INDEX_NONE)
	{
		const int32 NumPoseValues = CustomizableObject->GetIntParameterNumOptions(PoseSwitchEnumParamIndexInObject);

		TSharedPtr<TArray<TSharedPtr<FString>>>* FoundOptions = ProjectorParameterPoseOptions.Find(PoseSwitchEnumParamIndexInObject);
		TArray<TSharedPtr<FString>>& PoseOptionNamesAttribute = FoundOptions && FoundOptions->IsValid() ?
																*FoundOptions->Get()
																: *(ProjectorParameterPoseOptions.Add(PoseSwitchEnumParamIndexInObject, MakeShared<TArray<TSharedPtr<FString>>>()).Get());

		PoseOptionNamesAttribute.Empty();

		FString PoseValue = CustomInstance->GetIntParameterSelectedOption(PoseSwitchEnumParamName, -1);
		int32 PoseValueIndex = 0;

		for (int32 j = 0; j < NumPoseValues; ++j)
		{
			const FString PossibleValue = CustomizableObject->GetIntParameterAvailableOption(PoseSwitchEnumParamIndexInObject, j);
			if (PossibleValue == PoseValue)
			{
				PoseValueIndex = j;
			}

			PoseOptionNamesAttribute.Add(MakeShared<FString>(PossibleValue));
		}

		ProjectorBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.FillHeight(10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(0.45f)
			[
				SNew(SSearchableComboBox)
				.ToolTipText(LOCTEXT("Pose selector tooltip", "Select the skeletal mesh pose used for projection. This does not control the actual visual mesh pose in the viewport (or during gameplay for that matter). It has to be manually set. You can drag&drop a pose onto the preview viewport."))
				.OptionsSource(&PoseOptionNamesAttribute)
				.InitiallySelectedItem(PoseOptionNamesAttribute[PoseValueIndex])
				.Method(EPopupMethod::UseCurrentWindow)
				.OnSelectionChanged(this, &FCustomizableInstanceDetails::OnProjectorTextureParameterComboBoxChanged, PoseSwitchEnumParamName, -1)
				.OnGenerateWidget(this, &FCustomizableInstanceDetails::OnGenerateWidgetProjectorParameter)
				.Content()
				[
					SNew(STextBlock)
						.Text(FText::FromString(*PoseOptionNamesAttribute[PoseValueIndex]))
				]
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(0.3f)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("Add Layer", "Add Layer"))
				.Text(LOCTEXT("Add Layer", "Add Layer"))
				.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorLayerAdded, ParamName)
				.HAlign(HAlign_Fill)
			]
		];
	}
	else
	{
		ProjectorBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		//.FillHeight(10.f)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("Add Layer", "Add Layer"))
			.Text(LOCTEXT("Add Layer", "Add Layer"))
			.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorLayerAdded, ParamName)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
		];
	}

	const FString TextureSwitchEnumParamName = ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX;
	const FString OpacitySliderParamName = ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX;

	IDetailGroup* ProjectorGroup = *ParentsGroups.Find(ParamName);

	for (int32 RangeIndex = 0; RangeIndex < ProjectorParameters[ProjectorParamIndex].RangeValues.Num(); ++RangeIndex)
	{
		const int32 TextureSwitchEnumParamIndexInObject = CustomizableObject->FindParameter(TextureSwitchEnumParamName);
		check(TextureSwitchEnumParamIndexInObject >= 0); TSharedPtr<FString> CurrentStateName = nullptr;

		const UProjectorParameter* ProjectorParameter = Editor->GetProjectorParameter();
		const bool bSelectedProjector = ProjectorParameter->IsProjectorSelected(ParamName, RangeIndex);

		//Vertical box that owns all the layer properties
		TSharedPtr<SVerticalBox> LayerProperties = SNew(SVerticalBox);
		//Horizontal box that owns all the projector properties
		TSharedPtr<SHorizontalBox> ProjectorProperties;
		// Widget to set the opacity and remove a layer
		TSharedPtr<SHorizontalBox> OpacityRemoveWidget = SNew(SHorizontalBox);
		// Button Ptr needed to edit its style
		TSharedPtr<SButton> Button;

		SAssignNew(ProjectorProperties, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1, 0)
		[
			SNew(SBox)
			.MinDesiredWidth(115.f)
			.MaxDesiredWidth(115.f)
			[
				SAssignNew(Button, SButton)
				.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorSelectChanged, ParamName, RangeIndex)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(STextBlock)
					.Text(bSelectedProjector ? LOCTEXT("Unselect Projector", "Unselect Projector") : LOCTEXT("Select Projector", "Select Projector"))
					.ToolTipText(bSelectedProjector ? LOCTEXT("Unselect Projector", "Unselect Projector") : LOCTEXT("Select Projector", "Select Projector"))
					.Justification(ETextJustify::Center)
					.Font(LayoutBuilder.Pin()->GetDetailFont())
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(1, 0)
		[
			SNew(SButton)
				.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorCopyTransform, ParamName, RangeIndex)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(STextBlock)
						.ToolTipText(LOCTEXT("Copy Transform", "Copy Transform"))
						.Text(LOCTEXT("Copy Transform", "Copy Transform"))
						.AutoWrapText(true)
						.Justification(ETextJustify::Center)
						.Font(LayoutBuilder.Pin()->GetDetailFont())
				]
		]

		+ SHorizontalBox::Slot()
		.Padding(1, 0)
		[
			SNew(SButton)
				.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorPasteTransform, ParamName, RangeIndex)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(STextBlock)
						.ToolTipText(LOCTEXT("Paste Transform", "Paste Transform"))
						.Text(LOCTEXT("Paste Transform", "Paste Transform"))
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
						.Font(LayoutBuilder.Pin()->GetDetailFont())
				]
		]

		+ SHorizontalBox::Slot()
		.Padding(1, 0)
		[
			SNew(SButton)
				.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorResetTransform, ParamName, RangeIndex)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(STextBlock)
						.ToolTipText(LOCTEXT("Reset Transform", "Reset Transform"))
						.Text(LOCTEXT("Reset Transform", "Reset Transform"))
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
						.Font(LayoutBuilder.Pin()->GetDetailFont())
				]
		];

		Button->SetBorderBackgroundColor(bSelectedProjector ? FLinearColor::Green : FLinearColor::White);

		// If number of options is equal to 1, Mutable does not consider it multidimensional parameters
		int32 NumValues = CustomizableObject->GetIntParameterNumOptions(TextureSwitchEnumParamIndexInObject);
		FString Value = CustomizableObject->IsParameterMultidimensional(TextureSwitchEnumParamName) ? 
			CustomInstance->GetIntParameterSelectedOption(TextureSwitchEnumParamName, RangeIndex) : CustomInstance->GetIntParameterSelectedOption(TextureSwitchEnumParamName);

		TArray<TSharedPtr<FString>> OptionNamesAttribute;
		int32 ValueIndex = 0;

		for (int32 CandidateIndex = 0; CandidateIndex < NumValues; ++CandidateIndex)
		{
			FString PossibleValue = CustomizableObject->GetIntParameterAvailableOption(TextureSwitchEnumParamIndexInObject, CandidateIndex);
			if (PossibleValue == Value)
			{
				ValueIndex = CandidateIndex;
			}

			OptionNamesAttribute.Add(MakeShared<FString>(CustomizableObject->GetIntParameterAvailableOption(TextureSwitchEnumParamIndexInObject, CandidateIndex)));
		}
		
		// Avoid filling this arraw with repeated array  options
		if (RangeIndex == 0)
		{
			ProjectorTextureOptions.Add(MakeShared<TArray<TSharedPtr<FString>>>(OptionNamesAttribute));
		}

		if (NumValues > 1)
		{
			OpacityRemoveWidget->AddSlot()
			.Padding(1, 0)
			.FillWidth(0.3f)
			[
				SNew(SBox)
				[
					SNew(SSearchableComboBox)
					.OptionsSource(ProjectorTextureOptions.Last().Get())
					.InitiallySelectedItem((*ProjectorTextureOptions.Last())[ValueIndex])
					.OnGenerateWidget(this, &FCustomizableInstanceDetails::MakeTextureComboEntryWidget)
					.OnSelectionChanged(this, &FCustomizableInstanceDetails::OnProjectorTextureParameterComboBoxChanged, TextureSwitchEnumParamName, RangeIndex)
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromString(*(*ProjectorTextureOptions.Last())[ValueIndex]))
					]
				]
			];
		}
		
		OpacityRemoveWidget->AddSlot()
		.HAlign(HAlign_Fill)
		.Padding(1, 0)
		.FillWidth(0.7f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(1, 0)
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Value(this, &FCustomizableInstanceDetails::GetFloatParameterValue, OpacitySliderParamName, RangeIndex)
				.OnValueChanged(this, &FCustomizableInstanceDetails::OnFloatParameterChanged, OpacitySliderParamName, RangeIndex)
				.OnBeginSliderMovement(this, &FCustomizableInstanceDetails::OnFloatParameterSliderBegin)
				.OnEndSliderMovement(this, &FCustomizableInstanceDetails::OnFloatParameterSliderEnd, OpacitySliderParamName, RangeIndex)
				.Font(LayoutBuilder.Pin()->GetDetailFont())
			]

			+ SHorizontalBox::Slot()
			.Padding(1, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FCustomizableInstanceDetails::OnProjectorLayerRemoved, ParamName, RangeIndex)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(STextBlock)
						.ToolTipText(LOCTEXT("LayerProjectorRemoveLayer_ToolTip", "Remove Layer"))
						.Text(LOCTEXT("LayerProjectorRemoveLayer_Text", "X"))
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
						.Font(LayoutBuilder.Pin()->GetDetailFont())
				]
			]
		];

		LayerProperties->AddSlot()
		.AutoHeight()
		[
			OpacityRemoveWidget.ToSharedRef()
		];
		LayerProperties->AddSlot()
		.AutoHeight()
		.Padding(0.0f,5.0f,0.0f,0.0f)
		[
			ProjectorProperties.ToSharedRef()
		];

		// Final composed widget
		ProjectorGroup->AddWidgetRow()
		.NameContent()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FText::FromString("Layer " + FString::FromInt(RangeIndex)))
		]
		.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			[
				LayerProperties.ToSharedRef()
			]
		];
	}
	
	return ProjectorBox.ToSharedRef();
}


TSharedRef<SWidget> FCustomizableInstanceDetails::OnGenerateWidgetProjectorParameter(TSharedPtr<FString> InItem) const
{
	return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
}


FReply FCustomizableInstanceDetails::OnProjectorLayerAdded(FString ParamName) const
{
	const int32 NumLayers = CustomInstance->AddValueToProjectorRange(ParamName) + 1;
	if (NumLayers == 0)
	{
		return FReply::Handled();
	}

	const FString TextureSwitchEnumParamName = ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX;
	CustomInstance->AddValueToIntRange(TextureSwitchEnumParamName);
	check(CustomInstance->FindIntParameterNameIndex(TextureSwitchEnumParamName) != INDEX_NONE);
	check(NumLayers == CustomInstance->GetIntParameters()[CustomInstance->FindIntParameterNameIndex(TextureSwitchEnumParamName)].ParameterRangeValueNames.Num());

	TArray<FCustomizableObjectFloatParameterValue>& FloatParameters = CustomInstance->GetFloatParameters();

	const FString NumLayersParamName = ParamName + FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX;
	check(CustomInstance->FindFloatParameterNameIndex(NumLayersParamName) != INDEX_NONE);
	FloatParameters[CustomInstance->FindFloatParameterNameIndex(NumLayersParamName)].ParameterValue = NumLayers;

	const FString OpacitySliderParamName = ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX;
	CustomInstance->AddValueToFloatRange(OpacitySliderParamName);
	check(CustomInstance->FindFloatParameterNameIndex(OpacitySliderParamName) != INDEX_NONE);
	check(NumLayers == FloatParameters[CustomInstance->FindFloatParameterNameIndex(OpacitySliderParamName)].ParameterRangeValues.Num());

	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	FCoreUObjectDelegates::BroadcastOnObjectModified(CustomInstance.Get());

	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::OnProjectorLayerRemoved(const FString ParamName, const int32 RangeIndex) const
{
	const int32 projectorParameterIndex = CustomInstance->FindProjectorParameterNameIndex(ParamName);
	if (projectorParameterIndex == INDEX_NONE
		|| CustomInstance->GetProjectorParameters()[projectorParameterIndex].RangeValues.Num() <= 0)
	{
		return FReply::Handled();
	}

	const int32 NumLayers = CustomInstance->RemoveValueFromProjectorRange(ParamName, RangeIndex) + 1;

	const FString TextureSwitchEnumParamName = ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX;
	CustomInstance->RemoveValueFromIntRange(TextureSwitchEnumParamName, RangeIndex);
	check(CustomInstance->FindIntParameterNameIndex(TextureSwitchEnumParamName) != INDEX_NONE);
	check(NumLayers == CustomInstance->GetIntParameters()[CustomInstance->FindIntParameterNameIndex(TextureSwitchEnumParamName)].ParameterRangeValueNames.Num());

	TArray<FCustomizableObjectFloatParameterValue>& FloatParameters = CustomInstance->GetFloatParameters();

	const FString NumLayersParamName = ParamName + FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX;
	check(CustomInstance->FindFloatParameterNameIndex(NumLayersParamName) != INDEX_NONE);
	FloatParameters[CustomInstance->FindFloatParameterNameIndex(NumLayersParamName)].ParameterValue = NumLayers;

	const FString OpacitySliderParamName = ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX;
	CustomInstance->RemoveValueFromFloatRange(OpacitySliderParamName, RangeIndex);
	check(CustomInstance->FindFloatParameterNameIndex(OpacitySliderParamName) != INDEX_NONE);
	check(NumLayers == FloatParameters[CustomInstance->FindFloatParameterNameIndex(OpacitySliderParamName)].ParameterRangeValues.Num());

	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	FCoreUObjectDelegates::BroadcastOnObjectModified(CustomInstance.Get());

	return FReply::Handled();
}


void FCustomizableInstanceDetails::OnProjectorTextureParameterComboBoxChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, FString ParamName, int32 RangeIndex) const
{
	TArray<FCustomizableObjectIntParameterValue>& IntParameters = CustomInstance->GetIntParameters();

	const UCustomizableObject* CustomObject = CustomInstance->GetCustomizableObject();
	for (int32 i = 0; i < IntParameters.Num(); ++i)
	{
		if (IntParameters[i].ParameterName == ParamName)
		{
			const int32 ParamIndexInObject = CustomObject->FindParameter(ParamName);
			if (ParamIndexInObject != INDEX_NONE)
			{
				for (int32 v = 0; v < CustomObject->GetIntParameterNumOptions(ParamIndexInObject); ++v)
				{
					FString ValueName = CustomObject->GetIntParameterAvailableOption(ParamIndexInObject, v);
					if (ValueName == *Selection)
					{
						if (IntParameters[i].ParameterRangeValueNames.IsValidIndex(RangeIndex))
						{
							IntParameters[i].ParameterRangeValueNames[RangeIndex] = ValueName;
						}
						else
						{
							IntParameters[i].ParameterValueName = ValueName;
						}

						break;
					}
				}
			}
			else
			{
				UE_LOG(LogMutable, Error, TEXT("Failed to find parameter."));
			}
		}
	}

	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


TSharedPtr<ICustomizableObjectInstanceEditor> FCustomizableInstanceDetails::GetEditorChecked() const
{
	TSharedPtr<ICustomizableObjectInstanceEditor> Editor = WeakEditor.Pin();
	check(Editor);

	return Editor;
}


TSharedRef<SWidget> FCustomizableInstanceDetails::MakeTextureComboEntryWidget(TSharedPtr<FString> InItem) const
{
	return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
}


// BOOL PARAMETERS -----------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> FCustomizableInstanceDetails::GenerateBoolWidget(const int32 ParamIndexInObject)
{
	UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
	FString ParamName = CustomizableObject->GetParameterName(ParamIndexInObject);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		[
			SNew(SCheckBox)
			.HAlign(HAlign_Left)
			.IsChecked(this, &FCustomizableInstanceDetails::GetBoolParameterValue, ParamName)
			.OnCheckStateChanged(this, &FCustomizableInstanceDetails::OnBoolParameterChanged, ParamName)
		];
}


ECheckBoxState FCustomizableInstanceDetails::GetBoolParameterValue(FString ParamName) const
{
	const bool bResult = CustomInstance->GetBoolParameterSelectedOption(ParamName);
	return bResult ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void FCustomizableInstanceDetails::OnBoolParameterChanged(ECheckBoxState InCheckboxState, FString ParamName)
{
	CustomInstance->PreEditChange(nullptr);
	CustomInstance->SetBoolParameterSelectedOption(ParamName, InCheckboxState == ECheckBoxState::Checked);
	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();
}


// PARAMETERS -----------------------------------------------------------------------------------------------------------------

FReply FCustomizableInstanceDetails::OnCopyAllParameters()
{
	const FString ExportedText = CustomInstance->GetPrivate()->GetDescriptor().ToString();
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::OnPasteAllParameters()
{
	CustomInstance->Modify();

	FString ClipText;
	FPlatformApplicationMisc::ClipboardPaste(ClipText);
	
	FCustomizableObjectInstanceDescriptor& Descriptor = CustomInstance->GetPrivate()->GetDescriptor();
	const UScriptStruct* Struct = Descriptor.StaticStruct();

	const int32 MinLOD = Descriptor.GetMinLod();
	const TArray<uint16> RequestedLODLevels = Descriptor.GetRequestedLODLevels();
	
	if (Struct->ImportText(*ClipText, &Descriptor, nullptr, 0, GLog, GetPathNameSafe(Struct)))
	{
		// Keep current LOD
		Descriptor.SetMinLod(MinLOD);
		Descriptor.SetRequestedLODLevels(RequestedLODLevels);
		
		CustomInstance->UpdateSkeletalMeshAsync(true, true);		
	}

	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();

	
	return FReply::Handled();
}


FReply FCustomizableInstanceDetails::OnResetAllParameters()
{
	CustomInstance->PreEditChange(nullptr);

	TArray<FCustomizableObjectIntParameterValue>& IntParameters = CustomInstance->GetIntParameters();

	const UCustomizableObject* CustomObject = CustomInstance->GetCustomizableObject();
	const int32 NumObjectParameter = CustomObject->GetParameterCount();

	for (int32 ParameterIndex = 0; ParameterIndex < NumObjectParameter; ++ParameterIndex)
	{
		SetParameterValueToDefault(ParameterIndex);
	}

	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();
	CustomInstance->GetPrivate()->SelectedProfileIndex = INDEX_NONE;

	// Non-continuous change: collect garbage.
	GEngine->ForceGarbageCollection();

	return FReply::Handled();
}


void FCustomizableInstanceDetails::OnResetParameterButtonClicked(int32 ParameterIndex)
{	
	CustomInstance->PreEditChange(nullptr);
	SetParameterValueToDefault(ParameterIndex);
	CustomInstance->GetPrivate()->SetSelectedParameterProfileDirty();
	CustomInstance->UpdateSkeletalMeshAsync(true, true);
	CustomInstance->PostEditChange();
}


void FCustomizableInstanceDetails::SetParameterValueToDefault(int32 ParameterIndex)
{
	UCustomizableObject* CustomObject = CustomInstance->GetCustomizableObject();
	FString ParameterName = CustomObject->GetParameterName(ParameterIndex);
	EMutableParameterType ParameterType = CustomObject->GetParameterType(ParameterIndex);

	// Checking if there are parameteres with the same name
	if (ParameterType != CustomObject->GetParameterTypeByName(ParameterName))
	{
		return;
	}

	switch (ParameterType)
	{
	case EMutableParameterType::None:
		break;
	case EMutableParameterType::Bool:
	{
		CustomInstance->SetBoolParameterSelectedOption(ParameterName, CustomObject->GetBoolParameterDefaultValue(ParameterName));
		break;
	}
	case EMutableParameterType::Int:
	{
		int32 DefaultValue = CustomObject->GetIntParameterDefaultValue(ParameterName);
		FString ValueName = CustomObject->FindIntParameterValueName(ParameterIndex, DefaultValue);

		if (!ValueName.IsEmpty())
		{
			if (CustomObject->IsParameterMultidimensional(ParameterName))
			{
				int32 NumRanges = CustomInstance->GetIntValueRange(ParameterName);

				for (int32 RangeIndex = 0; RangeIndex < NumRanges; ++RangeIndex)
				{
					CustomInstance->SetIntParameterSelectedOption(ParameterName, ValueName, RangeIndex);
				}
			}
			else
			{
				CustomInstance->SetIntParameterSelectedOption(ParameterName, ValueName);
			}
		}
		break;
	}
	case EMutableParameterType::Float:
	{
		float DefaultValue = CustomObject->GetFloatParameterDefaultValue(ParameterName);

		if (CustomObject->IsParameterMultidimensional(ParameterName))
		{
			int32 NumRanges = CustomInstance->GetFloatValueRange(ParameterName);

			for (int32 RangeIndex = 0; RangeIndex < NumRanges; ++RangeIndex)
			{
				CustomInstance->SetFloatParameterSelectedOption(ParameterName, DefaultValue, RangeIndex);
			}
		}
		else
		{
			CustomInstance->SetFloatParameterSelectedOption(ParameterName, DefaultValue);
		}
		break;
	}
	case EMutableParameterType::Color:
	{
		CustomInstance->SetColorParameterSelectedOption(ParameterName, CustomObject->GetColorParameterDefaultValue(ParameterName));
		break;
	}
	case EMutableParameterType::Projector:
	{
		FCustomizableObjectProjector DefaultValue = CustomObject->GetProjectorParameterDefaultValue(ParameterName);

		if (CustomObject->IsParameterMultidimensional(ParameterName))
		{
			int32 NumRanges = CustomInstance->GetProjectorValueRange(ParameterName);

			for (int32 RangeIndex = 0; RangeIndex < NumRanges; ++RangeIndex)
			{
				CustomInstance->SetProjectorValue(ParameterName, FVector(DefaultValue.Position), FVector(DefaultValue.Direction), FVector(DefaultValue.Up), FVector(DefaultValue.Scale), DefaultValue.Angle, RangeIndex);
			}
		}
		else
		{
			CustomInstance->SetProjectorValue(ParameterName, FVector(DefaultValue.Position), FVector(DefaultValue.Direction), FVector(DefaultValue.Up), FVector(DefaultValue.Scale), DefaultValue.Angle);
		}

		break;
	}
	case EMutableParameterType::Texture:
	{
		FName DefaultValue = CustomObject->GetTextureParameterDefaultValue(ParameterName);

		if (CustomObject->IsParameterMultidimensional(ParameterName))
		{
			int32 NumRanges = CustomInstance->GetTextureValueRange(ParameterName);

			for (int32 RangeIndex = 0; RangeIndex < NumRanges; ++RangeIndex)
			{
				CustomInstance->SetTextureParameterSelectedOption(ParameterName, DefaultValue.ToString(), RangeIndex);
			}
		}
		else
		{
			CustomInstance->SetTextureParameterSelectedOption(ParameterName, DefaultValue.ToString());
		}
		break;
	}
	}

	return;
}


// PROFILES WINDOW -----------------------------------------------------------------------------------------------------------------

void SProfileParametersWindow::Construct(const FArguments& InArgs)
{
	if (AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game/"));
	}

	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SWindow::Construct(SWindow::FArguments()
	.Title(LOCTEXT("SSelectFolderDlg_Title", "Add a name to the new profile"))
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	.ClientSize(FVector2D(450, 85))
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot() // Add user input block
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CustomizableProfileName", "Customizable Profile Name"))
					.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SEditableTextBox)
					.Text(InArgs._DefaultFileName)
					.OnTextCommitted(this, &SProfileParametersWindow::OnNameChange)
					.MinDesiredWidth(250)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(5)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(UE_MUTABLE_GET_MARGIN("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(UE_MUTABLE_GET_FLOAT("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(UE_MUTABLE_GET_FLOAT("StandardDialog.MinDesiredSlotHeight"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(UE_MUTABLE_GET_MARGIN("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("OK", "OK"))
				.OnClicked(this, &SProfileParametersWindow::OnButtonClick, EAppReturnType::Ok)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(UE_MUTABLE_GET_MARGIN("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SProfileParametersWindow::OnButtonClick, EAppReturnType::Cancel)
			]
		]
	]);
}


EAppReturnType::Type SProfileParametersWindow::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));

	return UserResponse;
}


void SProfileParametersWindow::OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		FileName = NewName;
		
		RequestDestroyWindow();

		UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
		CustomizableObject->GetPrivate()->AddNewParameterProfile(GetFileName(), *CustomInstance.Get());

		if (CustomInstance->GetPrivate()->bSelectedProfileDirty && CustomInstance->GetPrivate()->SelectedProfileIndex != INDEX_NONE)
		{
			CustomInstance->GetPrivate()->SaveParametersToProfile(CustomInstance->GetPrivate()->SelectedProfileIndex);
		}
		CustomInstance->GetPrivate()->SelectedProfileIndex = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num() - 1;

		if (InstanceDetails)
		{
			InstanceDetails->Refresh();
		}
	}
}


FReply SProfileParametersWindow::OnButtonClick(EAppReturnType::Type ButtonID)
{
	if (ButtonID == EAppReturnType::Ok)
	{
		UserResponse = ButtonID;

		RequestDestroyWindow();

		UCustomizableObject* CustomizableObject = CustomInstance->GetCustomizableObject();
		CustomizableObject->GetPrivate()->AddNewParameterProfile(GetFileName(), *CustomInstance.Get());

		if (CustomInstance->GetPrivate()->bSelectedProfileDirty && CustomInstance->GetPrivate()->SelectedProfileIndex != INDEX_NONE)
		{
			CustomInstance->GetPrivate()->SaveParametersToProfile(CustomInstance->GetPrivate()->SelectedProfileIndex);
		}
		CustomInstance->GetPrivate()->SelectedProfileIndex = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num() - 1;

		if (InstanceDetails)
		{
			InstanceDetails->Refresh();
		}
	}
	else if (ButtonID == EAppReturnType::Cancel)
	{
		UserResponse = ButtonID;

		RequestDestroyWindow();
	}
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
