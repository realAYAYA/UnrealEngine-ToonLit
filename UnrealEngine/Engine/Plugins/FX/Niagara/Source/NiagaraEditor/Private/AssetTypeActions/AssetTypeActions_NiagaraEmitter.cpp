// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_NiagaraEmitter.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemToolkit.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraSystemFactoryNew.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_NiagaraEmitter::FAssetTypeActions_NiagaraEmitter()
{
}

FAssetTypeActions_NiagaraEmitter::~FAssetTypeActions_NiagaraEmitter()
{
}

FColor FAssetTypeActions_NiagaraEmitter::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.Emitter").ToFColor(true);
}

void FAssetTypeActions_NiagaraEmitter::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(*ObjIt))
		{
			if (!Emitter->VersionToOpenInEditor.IsValid())
			{
				Emitter->VersionToOpenInEditor = Emitter->GetExposedVersion().VersionGuid;
			}
			TSharedRef<FNiagaraSystemToolkit> SystemToolkit(new FNiagaraSystemToolkit());
			SystemToolkit->InitializeWithEmitter(Mode, EditWithinLevelEditor, *Emitter);
		}
	}
}

UClass* FAssetTypeActions_NiagaraEmitter::GetSupportedClass() const
{
	return UNiagaraEmitter::StaticClass();
}

void FAssetTypeActions_NiagaraEmitter::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UNiagaraEmitter>> NiagaraEmitters = GetTypedWeakObjectPtrs<UNiagaraEmitter>(InObjects);

	Section.AddMenuEntry(
		"Emitter_NewNiagaraSystem",
		LOCTEXT("Emitter_NewNiagaraSystem", "Create Niagara System"),
		LOCTEXT("Emitter_NewNiagaraSystemTooltip", "Creates a niagara system using this emitter as a base."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ParticleSystem"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_NiagaraEmitter::ExecuteNewNiagaraSystem, NiagaraEmitters)
		)
	);

	Section.AddMenuEntry(
		"Emitter_CreateDuplicateParent",
		LOCTEXT("Emitter_CreateDuplicateParent", "Create Duplicate Parent"),
		LOCTEXT("Emitter_CreateDuplicateParentTooltip", "Duplicate this emitter and set this emitter's parent to the new emitter."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_NiagaraEmitter::ExecuteCreateDuplicateParent, NiagaraEmitters),
			FCanExecuteAction::CreateLambda([NiagaraEmitters]()
			{
				for (TWeakObjectPtr<UNiagaraEmitter> EmitterPtr : NiagaraEmitters)
				{
					if (EmitterPtr.IsValid() && EmitterPtr->IsVersioningEnabled())
					{
						return false;
					}
				}
				return true;
			})
		)
	);

	Section.AddMenuEntry(
		"MarkDependentCompilableAssetsDirty",
		LOCTEXT("MarkDependentCompilableAssetsDirtyLabel", "Mark Dependent Compilable Assets Dirty"),
		LOCTEXT("MarkDependentCompilableAssetsDirtyToolTip", "Finds all niagara assets which depend on this asset either directly or indirectly,\n and marks them dirty so they can be saved with the latest version."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FNiagaraEditorUtilities::MarkDependentCompilableAssetsDirty, InObjects))
	);
}

void FAssetTypeActions_NiagaraEmitter::ExecuteNewNiagaraSystem(TArray<TWeakObjectPtr<UNiagaraEmitter>> Objects)
{
	const FString DefaultSuffix = TEXT("_System");

	FNiagaraSystemViewModelOptions SystemOptions;
	SystemOptions.bCanModifyEmittersFromTimeline = true;
	SystemOptions.EditMode = ENiagaraSystemViewModelEditMode::SystemAsset;

	TArray<UObject*> ObjectsToSync;
	for (TWeakObjectPtr<UNiagaraEmitter> Emitter : Objects)
	{
		if (Emitter.IsValid())
		{
			// Determine an appropriate names
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Emitter->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			// Create the factory used to generate the asset
			UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();
			Factory->EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(Emitter.Get(), Emitter->GetExposedVersion().VersionGuid));
			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
			UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UNiagaraSystem::StaticClass(), Factory);
			
			UNiagaraSystem* System = Cast<UNiagaraSystem>(NewAsset);
			if (System != nullptr)
			{
				ObjectsToSync.Add(NewAsset);
			}
		}
	}

	if (ObjectsToSync.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

void FAssetTypeActions_NiagaraEmitter::ExecuteCreateDuplicateParent(TArray<TWeakObjectPtr<UNiagaraEmitter>> Emitters)
{
	const FString DefaultSuffix = TEXT("_Parent");

	TArray<UObject*> ObjectsToSync;
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	for (TWeakObjectPtr<UNiagaraEmitter> EmitterPtr : Emitters)
	{
		UNiagaraEmitter* Emitter = EmitterPtr.Get();
		if (Emitter != nullptr)
		{
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Emitter->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			// Create the factory used to generate the asset
			UNiagaraEmitter* NewEmitter = Cast<UNiagaraEmitter>(AssetToolsModule.Get().DuplicateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), Emitter));

			if (NewEmitter != nullptr)
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Emitter);

				NewEmitter->Modify();
				NewEmitter->GetLatestEmitterData()->GraphSource->MarkNotSynchronized(TEXT("Emitter created"));

				Emitter->Modify();
				Emitter->SetParent(FVersionedNiagaraEmitter(NewEmitter, NewEmitter->GetExposedVersion().VersionGuid));
				Emitter->GetLatestEmitterData()->GraphSource->MarkNotSynchronized(TEXT("Emitter parent changed"));

				ObjectsToSync.Add(NewEmitter);
			}
		}
	}

	if (ObjectsToSync.Num() > 0)
	{
		AssetToolsModule.Get().OpenEditorForAssets(ObjectsToSync);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

#undef LOCTEXT_NAMESPACE
