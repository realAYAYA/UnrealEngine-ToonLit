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
#include "SNewSystemDialog.h"
#include "Misc/MessageDialog.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSettings.h"
#include "ObjectTools.h"

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

	TSharedRef<SNewSystemDialog> NewSystemDialog = SNew(SNewSystemDialog);
	FSlateApplication::Get().AddModalWindow(NewSystemDialog, ParentWindow);

	if (NewSystemDialog->GetUserConfirmedSelection() == false)
	{
		// User cancelled or closed the dialog so abort asset creation.
		return false;
	}

	TOptional<FAssetData> SelectedSystemAsset = NewSystemDialog->GetSelectedSystemAsset();
	TArray<FAssetData> EmitterAssetsToAddToNewSystem = NewSystemDialog->GetSelectedEmitterAssets();
	if (SelectedSystemAsset.IsSet())
	{
		SystemToCopy = Cast<UNiagaraSystem>(SelectedSystemAsset.GetValue().GetAsset());
		if (SystemToCopy == nullptr)
		{
			FText Title = LOCTEXT("FailedToLoadSystemTitle", "Create Default?");
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::OkCancel, EAppReturnType::Cancel,
				LOCTEXT("FailedToLoadEmitterMessage", "The selected system failed to load.\nWould you like to create an empty system?"),
				&Title);
			if (DialogResult == EAppReturnType::Cancel)
			{
				return false;
			}
			else
			{
				SystemToCopy = nullptr;
				EmitterAssetsToAddToNewSystem.Empty();
			}
		}
	}
	else if (EmitterAssetsToAddToNewSystem.Num() > 0)
	{
		bool bAllEmittersLoaded = true;
		for (const FAssetData& EmitterAssetToAdd : EmitterAssetsToAddToNewSystem)
		{
			UNiagaraEmitter* EmitterToAdd = Cast<UNiagaraEmitter>(EmitterAssetToAdd.GetAsset());
			if (EmitterToAdd != nullptr)
			{
				EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(EmitterToAdd, EmitterToAdd->GetExposedVersion().VersionGuid));
			}
			else
			{
				bAllEmittersLoaded = false;
				break;
			}
		}

		if(bAllEmittersLoaded == false)
		{
			FText Title = LOCTEXT("FailedToLoadEmitterTitle", "Create Default?");
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::OkCancel, EAppReturnType::Cancel,
				LOCTEXT("FailedToLoadMessage", "A selected emitter failed to load.\nWould you like to create an empty system system?"),
				&Title);
			if (DialogResult == EAppReturnType::Cancel)
			{
				return false;
			}
			else
			{
				EmittersToAddToNewSystem.Empty();
			}
		}
	}
	else
	{
		SystemToCopy = nullptr;
		EmittersToAddToNewSystem.Empty();
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
		NewSystem = Cast<UNiagaraSystem>(StaticDuplicateObject(SystemToCopy, InParent, Name, Flags, Class));
		NewSystem->TemplateSpecification = ENiagaraScriptTemplateSpecification::None;
		NewSystem->TemplateAssetDescription = FText();
		NewSystem->Category = FText();

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

	System->TemplateSpecification = ENiagaraScriptTemplateSpecification::None;
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
	System->SetEffectType(Settings->GetDefaultEffectType());
}

#undef LOCTEXT_NAMESPACE

