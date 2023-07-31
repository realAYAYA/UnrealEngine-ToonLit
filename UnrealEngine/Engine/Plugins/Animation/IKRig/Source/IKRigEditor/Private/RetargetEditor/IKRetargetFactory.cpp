// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargetFactory.h"
#include "Retargeter/IKRetargeter.h"
#include "IKRigEditor.h"
#include "AssetTypeCategories.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargetFactory)

#define LOCTEXT_NAMESPACE "IKRetargeterFactory"


UIKRetargetFactory::UIKRetargetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UIKRetargeter::StaticClass();
}

UObject* UIKRetargetFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	if (!SourceIKRig.IsValid())
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Unable to create IK Retargter. No source IK Rig asset supplied."));
		return nullptr;
	}
	
	if (!IsValid(SourceIKRig->GetPreviewMesh()))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Unable to create IK Retargter. Source IK Rig does not have an associated skeletal mesh."));
		return nullptr;
	}
	
	UIKRetargeter* Retargeter = NewObject<UIKRetargeter>(InParent, Class, Name, Flags);
	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	Controller->SetSourceIKRig(SourceIKRig.Get());
	return Retargeter;
}

bool UIKRetargetFactory::ShouldShowInNewMenu() const
{
	return true;
}

void UIKRetargetFactory::OnTargetIKRigSelected(const FAssetData& SelectedAsset)
{
	SourceIKRig = Cast<UIKRigDefinition>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

bool UIKRetargetFactory::ConfigureProperties()
{
	// Null the parent class so we can check for selection later
	SourceIKRig = nullptr;

	// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;

	/** The asset picker will only show skeletal meshes */
	AssetPickerConfig.Filter.ClassPaths.Add(UIKRigDefinition::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UIKRetargetFactory::OnTargetIKRigSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateRetargeterOptions", "Pick IK Rig To Copy Animation From"))
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false) .SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("Menu.Background") )
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];

	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	return SourceIKRig != nullptr;
}

FText UIKRetargetFactory::GetDisplayName() const
{
	return LOCTEXT("IKRetargeter_DisplayName", "IK Retargeter");
}

uint32 UIKRetargetFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Animation;
}

FText UIKRetargetFactory::GetToolTip() const
{
	return LOCTEXT("IKRetargeter_Tooltip", "Defines a pair of Source/Target Retarget Rigs and the mapping between them.");
}

FString UIKRetargetFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewIKRetargeter"));
}

#undef LOCTEXT_NAMESPACE

