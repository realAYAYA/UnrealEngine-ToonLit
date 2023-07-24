// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureAssetTypeActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "EditorSupportDelegates.h"
#include "FileHelpers.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "IContentBrowserSingleton.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/IToolkitHost.h"
#include "VT/RuntimeVirtualTexture.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogRuntimeVirtualTextureFixMaterial, Log, Log);

namespace
{
	template <class T> 
	void GetReferencersOfType(UObject *Object, TArray<T*> &OutObjects)
	{
		TArray<FAssetData> AssetDatas;
		UAssetRegistryHelpers::FindReferencersOfAssetOfClass(Object, { T::StaticClass() }, AssetDatas);

		for (auto Data : AssetDatas)
		{
			T* TypedAsset = Cast<T>(Data.GetAsset());
			if (TypedAsset != nullptr)
			{
				OutObjects.Add(TypedAsset);
			}
		}
	}

	void FindAllMaterials(URuntimeVirtualTexture* RuntimeVirtualTexture, TArray<UMaterial*>& OutMaterials, TArray<UMaterialFunctionInterface*>& OutFunctions)
	{
		TArray<UMaterialInterface*> MaterialInterfaces;
		GetReferencersOfType(RuntimeVirtualTexture, MaterialInterfaces);

		for (UMaterialInterface *MaterialInterface : MaterialInterfaces)
		{
			UMaterial* Material = Cast<UMaterial>(MaterialInterface);
			
			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
			if (MaterialInstance != nullptr)
			{
				Material = MaterialInstance->GetMaterial();
			}

			OutMaterials.AddUnique(Material);
		}

		GetReferencersOfType(RuntimeVirtualTexture, OutFunctions);
	}

	void FixMaterialUsage(URuntimeVirtualTexture* RuntimeVirtualTexture)
	{
		TArray<UPackage*> PackagesToSave;

		{
			UE_LOG(LogRuntimeVirtualTextureFixMaterial, Log, TEXT("Begin fix material usage for '%s' ..."), *RuntimeVirtualTexture->GetName());

			TArray<UMaterial*> Materials;
			TArray<UMaterialFunctionInterface*> Functions;
			FindAllMaterials(RuntimeVirtualTexture, Materials, Functions);

			int32 TaskCount = Materials.Num() + Functions.Num();
			FScopedSlowTask Task(TaskCount, LOCTEXT("RuntimeVirtualTexture_FixMaterialUsageProgress", "Fixing materials for Runtime Virtual Texture usage..."));
			Task.MakeDialog();

			for (UMaterial* Material : Materials)
			{
				Task.EnterProgressFrame();

				bool bMaterialModified = false;
				for (UMaterialExpression* Expression : Material->GetExpressions())
				{
					UMaterialExpressionRuntimeVirtualTextureSample* RVTSampleExpression = Cast<UMaterialExpressionRuntimeVirtualTextureSample>(Expression);
					if (RVTSampleExpression)
					{
						if (RuntimeVirtualTexture == RVTSampleExpression->VirtualTexture)
						{
							if (RVTSampleExpression->InitVirtualTextureDependentSettings())
							{
								Expression->Modify();

								FPropertyChangedEvent Event(UMaterialExpressionTextureBase::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMaterialExpressionRuntimeVirtualTextureSample, MaterialType)));
								Expression->PostEditChangeProperty(Event);

								bMaterialModified = true;
							}
						}
					}
				}

				if (bMaterialModified)
				{
					UE_LOG(LogRuntimeVirtualTextureFixMaterial, Log, TEXT("  Recompile material '%s' ..."), *Material->GetName());

					FScopedSlowTask CompileTask(1, FText::AsCultureInvariant(Material->GetName()));
					CompileTask.MakeDialog();
					CompileTask.EnterProgressFrame();

					UMaterialEditingLibrary::RecompileMaterial(Material);

					PackagesToSave.Add(Material->GetOutermost());
				}
			}

			for (UMaterialFunctionInterface *Function : Functions)
			{
				Task.EnterProgressFrame();

				bool bFunctionModified = false;
				for (const TObjectPtr<UMaterialExpression>& Expression : Function->GetExpressions())
				{
					UMaterialExpressionRuntimeVirtualTextureSample* RVTSampleExpression = Cast<UMaterialExpressionRuntimeVirtualTextureSample>(Expression);
					if (RVTSampleExpression)
					{
						if (RuntimeVirtualTexture == RVTSampleExpression->VirtualTexture)
						{
							if (RVTSampleExpression->InitVirtualTextureDependentSettings())
							{
								Expression->Modify();

								FPropertyChangedEvent Event(UMaterialExpressionTextureBase::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMaterialExpressionRuntimeVirtualTextureSample, MaterialType)));
								Expression->PostEditChangeProperty(Event);

								bFunctionModified = true;
							}
						}
					}
				}

				if (bFunctionModified)
				{
					UE_LOG(LogRuntimeVirtualTextureFixMaterial, Log, TEXT("  Update function '%s' ..."), *Function->GetName());

					FScopedSlowTask CompileTask(1, FText::AsCultureInvariant(Function->GetName()));
					CompileTask.MakeDialog();
					CompileTask.EnterProgressFrame();

					UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);

					PackagesToSave.Add(Function->GetOutermost());
				}
			}

			UE_LOG(LogRuntimeVirtualTextureFixMaterial, Log, TEXT("End fix material usage for '%s' ..."), *RuntimeVirtualTexture->GetName());
		}
	
		if (PackagesToSave.Num())
		{
			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, true);
		}
	}
}


UClass* FAssetTypeActions_RuntimeVirtualTexture::GetSupportedClass() const
{
	return URuntimeVirtualTexture::StaticClass();
}

FText FAssetTypeActions_RuntimeVirtualTexture::GetName() const
{
	return LOCTEXT("AssetTypeActions_RuntimeVirtualTexture", "Runtime Virtual Texture"); 
}

FColor FAssetTypeActions_RuntimeVirtualTexture::GetTypeColor() const 
{
	return FColor(128, 128, 128); 
}

uint32 FAssetTypeActions_RuntimeVirtualTexture::GetCategories() 
{
	return EAssetTypeCategories::Textures; 
}

void FAssetTypeActions_RuntimeVirtualTexture::GetActions(TArray<UObject*> const& InObjects, FMenuBuilder& MenuBuilder)
{
	if (InObjects.Num() == 1)
	{
		TArray<TWeakObjectPtr<URuntimeVirtualTexture>> RuntimeVirtualTextures = GetTypedWeakObjectPtrs<URuntimeVirtualTexture>(InObjects);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RuntimeVirtualTexture_FindMaterials", "Find Materials Using This"),
			LOCTEXT("RuntimeVirtualTexture_FindMaterialsTooltip", "Finds all materials that use this texture in the content browser."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_RuntimeVirtualTexture::ExecuteFindMaterials, RuntimeVirtualTextures[0]),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RuntimeVirtualTexture_FixMaterialUsage", "Fix Material Usage"),
			LOCTEXT("RuntimeVirtualTexture_FixMaterialUsageTooltip", "Find materials using this Runtime Virtual Texture and fix any mismatching content types."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_RuntimeVirtualTexture::ExecuteFixMaterialUsage, RuntimeVirtualTextures[0]),
				FCanExecuteAction()
			)
		);
	}
}

void FAssetTypeActions_RuntimeVirtualTexture::ExecuteFindMaterials(TWeakObjectPtr<URuntimeVirtualTexture> Object)
{
	TArray<FAssetData> Materials;

	URuntimeVirtualTexture* RuntimeVirtualTexture = Object.Get();
	if (RuntimeVirtualTexture != nullptr)
	{
		UAssetRegistryHelpers::FindReferencersOfAssetOfClass(RuntimeVirtualTexture, { UMaterialInterface::StaticClass(), UMaterialFunction::StaticClass() }, Materials);
	}

	if (Materials.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(Materials);
	}
}

void FAssetTypeActions_RuntimeVirtualTexture::ExecuteFixMaterialUsage(TWeakObjectPtr<URuntimeVirtualTexture> Object)
{
	URuntimeVirtualTexture* RuntimeVirtualTexture = Object.Get();
	if (RuntimeVirtualTexture != nullptr)
	{
		FixMaterialUsage(RuntimeVirtualTexture);

		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
