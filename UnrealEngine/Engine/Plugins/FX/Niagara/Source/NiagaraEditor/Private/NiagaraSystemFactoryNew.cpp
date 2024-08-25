// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemFactoryNew.h"
#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraEditorSettings.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "ImageUtils.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Misc/MessageDialog.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraRecentAndFavoritesManager.h"
#include "NiagaraSettings.h"
#include "ObjectTools.h"
#include "Widgets/AssetBrowser/SNiagaraAssetBrowser.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSystemFactoryNew)

#define LOCTEXT_NAMESPACE "NiagaraSystemFactory"

UNiagaraSystemFactoryNew::UNiagaraSystemFactoryNew(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UNiagaraSystem::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

bool UNiagaraSystemFactoryNew::ConfigureProperties()
{
	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	TSharedPtr<SWindow>	ParentWindow = MainFrame.GetParentWindow();

	SNiagaraAssetBrowser::FArguments AssetBrowserArgs;
	AssetBrowserArgs.AvailableClasses({UNiagaraEmitter::StaticClass(), UNiagaraSystem::StaticClass()})
	.RecentAndFavoritesList(FNiagaraEditorModule::Get().GetRecentsManager()->GetRecentEmitterAndSystemsList())
	.EmptySelectionMessage(LOCTEXT("EmptySystemFactorySelectionMessage", "Select an emitter or system as a starting point for your new system.\nA system consists of one or more emitters."));

	SNiagaraAssetBrowserWindow::FArguments AssetBrowserWindowArgs;
	AssetBrowserWindowArgs.AssetBrowserArgs(AssetBrowserArgs)
	.WindowTitle(LOCTEXT("SystemAssetBrowserWindowTitle", "Create Niagara System - Select an emitter or system as a base"));
	
	TSharedRef<SNiagaraCreateAssetWindow> CreateAssetBrowserWindow = SNew(SNiagaraCreateAssetWindow, *UNiagaraSystem::StaticClass()).AssetBrowserWindowArgs(AssetBrowserWindowArgs);

	FSlateApplication::Get().AddModalWindow(CreateAssetBrowserWindow, ParentWindow);

	if(CreateAssetBrowserWindow->ShouldProceedWithAction() == false)
	{
		return false;
	}

	TArray<FAssetData> SelectedAssetData = CreateAssetBrowserWindow->GetSelectedAssets();
	
	if(SelectedAssetData.Num() == 1)
	{
		FAssetData AssetData = SelectedAssetData[0];
		if(AssetData.GetClass() == UNiagaraSystem::StaticClass())
		{
			SystemToCopy = Cast<UNiagaraSystem>(AssetData.GetAsset());
		}
		else if(AssetData.GetClass() == UNiagaraEmitter::StaticClass())
		{
			UNiagaraEmitter* EmitterAsset = Cast<UNiagaraEmitter>(AssetData.GetAsset());
			FVersionedNiagaraEmitter VersionedNiagaraEmitter(EmitterAsset, EmitterAsset->GetExposedVersion().VersionGuid);
			
			EmittersToAddToNewSystem.Add(VersionedNiagaraEmitter);
		}
	}
	
	return true;
}

UObject* UNiagaraSystemFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraSystem::StaticClass()));

	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	check(Settings);

	UNiagaraSystem* NewSystem;
	
	if (SystemToCopy != nullptr)
	{
		if (SystemToCopy->IsReadyToRun() == false)
		{
			SystemToCopy->WaitForCompilationComplete();
		}
		
		FNiagaraEditorModule::Get().GetRecentsManager()->SystemUsed(*SystemToCopy);

		NewSystem = Cast<UNiagaraSystem>(StaticDuplicateObject(SystemToCopy, InParent, Name, Flags, Class));
		NewSystem->TemplateAssetDescription = FText();
		NewSystem->Category = FText();
		NewSystem->AssetTags.Empty();

		// if the new system doesn't have a thumbnail image, check the thumbnail map of the original asset's upackage
		if(NewSystem->ThumbnailImage == nullptr)
		{
			FString ObjectFullName = SystemToCopy->GetFullName();
			FName ObjectName = FName(ObjectFullName);
			FString PackageFullName;
			ThumbnailTools::QueryPackageFileNameForObject(ObjectFullName, PackageFullName);
			FThumbnailMap ThumbnailMap;
			ThumbnailTools::ConditionallyLoadThumbnailsFromPackage(PackageFullName, {FName(ObjectFullName)}, ThumbnailMap);

			// there should always be a dummy thumbnail in here
			if(ThumbnailMap.Contains(ObjectName))
			{
				FObjectThumbnail Thumbnail = ThumbnailMap[ObjectName];
				// we only want to copy the thumbnail over if it's not a dummy
				if(Thumbnail.GetImageWidth() != 0 && Thumbnail.GetImageHeight() != 0)
				{
					ThumbnailTools::CacheThumbnail(NewSystem->GetFullName(), &Thumbnail, NewSystem->GetOutermost());
				}
			}
		}
	}
	else if (EmittersToAddToNewSystem.Num() > 0)
	{
		NewSystem = NewObject<UNiagaraSystem>(InParent, Class, Name, Flags | RF_Transactional);
		InitializeSystem(NewSystem, true);

		for (FVersionedNiagaraEmitter& EmitterToAddToNewSystem : EmittersToAddToNewSystem)
		{
			FNiagaraEditorUtilities::AddEmitterToSystem(*NewSystem, *EmitterToAddToNewSystem.Emitter, EmitterToAddToNewSystem.Version);
		}
	}
	else
	{
		NewSystem = NewObject<UNiagaraSystem>(InParent, Class, Name, Flags | RF_Transactional);
		InitializeSystem(NewSystem, true);
	}

	NewSystem->RequestCompile(false);

	FNiagaraEditorModule::Get().GetRecentsManager()->SystemUsed(*NewSystem);
	return NewSystem;
}

void UNiagaraSystemFactoryNew::InitializeSystem(UNiagaraSystem* System, bool bCreateDefaultNodes)
{
	UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript();
	UNiagaraScript* SystemUpdateScript = System->GetSystemUpdateScript();

	UNiagaraScriptSource* SystemScriptSource = NewObject<UNiagaraScriptSource>(SystemSpawnScript, "SystemScriptSource", RF_Transactional);

	if (SystemScriptSource)
	{
		SystemScriptSource->NodeGraph = NewObject<UNiagaraGraph>(SystemScriptSource, "SystemScriptGraph", RF_Transactional);
	}
	
	SystemSpawnScript->SetLatestSource(SystemScriptSource);
	SystemUpdateScript->SetLatestSource(SystemScriptSource);

	if (bCreateDefaultNodes)
	{
		FSoftObjectPath SystemUpdateScriptRef = GetDefault<UNiagaraEditorSettings>()->RequiredSystemUpdateScript;
		UNiagaraScript* Script = Cast<UNiagaraScript>(SystemUpdateScriptRef.TryLoad());

		FAssetData ModuleScriptAsset(Script);
		if (SystemScriptSource && ModuleScriptAsset.IsValid())
		{
			UNiagaraNodeOutput* SpawnOutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*SystemScriptSource->NodeGraph, ENiagaraScriptUsage::SystemSpawnScript, SystemSpawnScript->GetUsageId());
			UNiagaraNodeOutput* UpdateOutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*SystemScriptSource->NodeGraph, ENiagaraScriptUsage::SystemUpdateScript, SystemUpdateScript->GetUsageId());

			if (UpdateOutputNode)
			{
				FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScriptAsset, *UpdateOutputNode);
			}
			FNiagaraStackGraphUtilities::RelayoutGraph(*SystemScriptSource->NodeGraph);
		}
	}

	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	check(Settings);

	UNiagaraEffectType* RequiredEffectType = Settings->GetRequiredEffectType();
	if (RequiredEffectType != nullptr)
	{
		System->SetEffectType(RequiredEffectType);
	}
	else
	{
		UNiagaraEffectType* DefaultEffectType = Settings->GetDefaultEffectType();
		if (DefaultEffectType != nullptr)
		{
			System->SetEffectType(DefaultEffectType);
		}
	}
}

#undef LOCTEXT_NAMESPACE

