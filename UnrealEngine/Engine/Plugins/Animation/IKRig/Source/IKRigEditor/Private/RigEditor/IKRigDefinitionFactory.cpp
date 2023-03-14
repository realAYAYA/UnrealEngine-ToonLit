// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigDefinitionFactory.h"
#include "IKRigEditor.h"
#include "IKRigDefinition.h"
#include "AssetTypeCategories.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "RigEditor/IKRigController.h"
#include "Widgets/SWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigDefinitionFactory)

#define LOCTEXT_NAMESPACE "IKRigDefinitionFactory"


UIKRigDefinitionFactory::UIKRigDefinitionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UIKRigDefinition::StaticClass();
}

UObject* UIKRigDefinitionFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context, 
	FFeedbackContext* Warn)
{
	if (!SkeletalMesh.IsValid())
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Unable to create IK Rig. No Skeletal Mesh asset supplied."));
		return nullptr;
	}
	
	UIKRigDefinition* IKRig = NewObject<UIKRigDefinition>(InParent, Name, Flags | RF_Transactional);
	
	// imports the skeleton data into the IK Rig
	UIKRigController* Controller = UIKRigController::GetIKRigController(IKRig);
	Controller->SetSkeletalMesh(SkeletalMesh.Get());
	
	return IKRig;
}

bool UIKRigDefinitionFactory::ShouldShowInNewMenu() const
{
	return true;
}

void UIKRigDefinitionFactory::OnSkeletalMeshSelected(const FAssetData& SelectedAsset)
{
	SkeletalMesh = Cast<USkeletalMesh>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

bool UIKRigDefinitionFactory::ConfigureProperties()
{
	// Null so we can check for selection later
	SkeletalMesh = nullptr;

	// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;

	/** The asset picker will only show skeletal meshes */
	AssetPickerConfig.Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UIKRigDefinitionFactory::OnSkeletalMeshSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateIKRigOptions", "Pick Skeletal Mesh"))
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

	return SkeletalMesh != nullptr;
}

FText UIKRigDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("IKRigDefinition_DisplayName", "IK Rig");
}

uint32 UIKRigDefinitionFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Animation;
}

FText UIKRigDefinitionFactory::GetToolTip() const
{
	return LOCTEXT("IKRigDefinition_Tooltip", "Defines a set of IK Solvers and Effectors to pose a skeleton with Goals.");
}

FString UIKRigDefinitionFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewIKRig"));
}
#undef LOCTEXT_NAMESPACE

