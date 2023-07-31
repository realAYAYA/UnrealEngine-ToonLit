// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_BlueprintGeneratedClass.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ToolMenus.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "Factories/BlueprintFactory.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_BlueprintGeneratedClass::GetSupportedClass() const
{
	return UBlueprintGeneratedClass::StaticClass();
}

void FAssetTypeActions_BlueprintGeneratedClass::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UBlueprintGeneratedClass>> BPGCs = GetTypedWeakObjectPtrs<UBlueprintGeneratedClass>(InObjects);

	if (BPGCs.Num() == 1)
	{
		TAttribute<FText>::FGetter DynamicTooltipGetter;
		DynamicTooltipGetter.BindSP(this, &FAssetTypeActions_BlueprintGeneratedClass::GetNewDerivedBlueprintTooltip, BPGCs[0]);
		TAttribute<FText> DynamicTooltipAttribute = TAttribute<FText>::Create(DynamicTooltipGetter);

		Section.AddMenuEntry(
			"BlueprintGeneratedClass_NewDerivedBlueprint",
			LOCTEXT("BlueprintGeneratedClass_NewDerivedBlueprint", "Create Child Blueprint Class"),
			DynamicTooltipAttribute,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Blueprint"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_BlueprintGeneratedClass::ExecuteNewDerivedBlueprint, BPGCs[0]),
				FCanExecuteAction::CreateSP(this, &FAssetTypeActions_BlueprintGeneratedClass::CanExecuteNewDerivedBlueprint, BPGCs[0])
			)
		);
	}
}

UFactory* FAssetTypeActions_BlueprintGeneratedClass::GetFactoryForNewDerivedBlueprint(UBlueprintGeneratedClass* InBPGC) const
{
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = InBPGC;
	return Factory;
}

UClass* FAssetTypeActions_BlueprintGeneratedClass::GetNewDerivedBlueprintClass() const
{
	return UBlueprint::StaticClass();
}

void FAssetTypeActions_BlueprintGeneratedClass::ExecuteNewDerivedBlueprint(TWeakObjectPtr<UBlueprintGeneratedClass> InObject)
{
	if (UBlueprintGeneratedClass* TargetParentClass = InObject.Get())
	{
		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(TargetParentClass))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("InvalidClassToMakeBlueprintFrom", "Invalid class with which to make a Blueprint."));
			return;
		}

		FString Name;
		FString PackageName;
		CreateUniqueAssetName(TargetParentClass->GetOutermost()->GetName(), TEXT("_Child"), PackageName, Name);
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().CreateNewAsset(Name, PackagePath, GetNewDerivedBlueprintClass(), GetFactoryForNewDerivedBlueprint(TargetParentClass));
	}
}

FText FAssetTypeActions_BlueprintGeneratedClass::GetNewDerivedBlueprintTooltip(TWeakObjectPtr<UBlueprintGeneratedClass> InObject)
{
	if (!CanExecuteNewDerivedBlueprint(InObject))
	{
		return LOCTEXT("BlueprintGeneratedClass_NewDerivedBlueprintIsDeprecatedTooltip", "Compiled Blueprint class is deprecated, cannot derive a child Blueprint!");
	}
	else
	{
		return LOCTEXT("BlueprintGeneratedClass_NewDerivedBlueprintTooltip", "Creates a Child Blueprint Class based on the current compiled Blueprint, allowing you to create variants easily.");
	}
}

bool FAssetTypeActions_BlueprintGeneratedClass::CanExecuteNewDerivedBlueprint(TWeakObjectPtr<UBlueprintGeneratedClass> InObject)
{
	UBlueprintGeneratedClass* BPGC = InObject.Get();
	return BPGC && !BPGC->HasAnyClassFlags(CLASS_Deprecated);
}

//@todo

//UThumbnailInfo* FAssetTypeActions_BlueprintGeneratedClass::GetThumbnailInfo(UObject* Asset) const
//{
//	// Blueprint thumbnail scenes are disabled for now
//	UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(Asset);
//	UThumbnailInfo* ThumbnailInfo = BPGC->ThumbnailInfo;
//	if (!ThumbnailInfo)
//	{
//		ThumbnailInfo = NewObject<USceneThumbnailInfo>(BPGC, NAME_None, RF_Transactional);
//		BPGC->ThumbnailInfo = ThumbnailInfo;
//	}
//	return ThumbnailInfo;
//}

TWeakPtr<IClassTypeActions> FAssetTypeActions_BlueprintGeneratedClass::GetClassTypeActions(const FAssetData& AssetData) const
{
	// Blueprints get the class type actions for their parent native class.
	// Using asset tags avoids us having to load the blueprint
	UClass* ParentClass = nullptr;
	FString ParentClassName;
	if(!AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassName))
	{
		AssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassName);
	}
	if (!ParentClassName.IsEmpty())
	{
		ParentClass = UClass::TryFindTypeSlow<UClass>(FPackageName::ExportTextPathToObjectPath(ParentClassName));
	}

	if (ParentClass)
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		return AssetToolsModule.Get().GetClassTypeActionsForClass(ParentClass);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
