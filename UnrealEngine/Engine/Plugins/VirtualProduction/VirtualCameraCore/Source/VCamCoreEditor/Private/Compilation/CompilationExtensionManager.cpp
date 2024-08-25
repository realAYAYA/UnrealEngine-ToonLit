// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompilationExtensionManager.h"

#include "ModifierCompilationBlueprintExtension.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modifier/VCamModifier.h"

namespace UE::VCamCoreEditor::Private
{
	FCompilationExtensionManager::~FCompilationExtensionManager()
	{
		FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);
		
	}

	void FCompilationExtensionManager::Init()
	{
		// This function exists because AddSP cannot be used in the constructor.
		FCoreUObjectDelegates::OnAssetLoaded.AddSP(this, &FCompilationExtensionManager::OnAssetLoaded);
	}

	void FCompilationExtensionManager::OnAssetLoaded(UObject* Object) const
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Object);
		if (!Blueprint)
		{
			return;
		}

		const bool bIsModifierBlueprint = Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UVCamModifier::StaticClass());
		if (!bIsModifierBlueprint)
		{
			return;
		}

		TArrayView<const TObjectPtr<UBlueprintExtension>> Extensions = Blueprint->GetExtensions();
		const int32 Index = Extensions.IndexOfByPredicate([](UBlueprintExtension* Extension){ return Extension->IsA<UModifierCompilationBlueprintExtension>(); });
		UModifierCompilationBlueprintExtension* Extension;
		if (Index == INDEX_NONE)
		{
			Extension = NewObject<UModifierCompilationBlueprintExtension>(Blueprint);
			Blueprint->AddExtension(Extension);
		}
		else
		{
			Extension = Cast<UModifierCompilationBlueprintExtension>(Extensions[Index].Get());
		}
		
		// Blueprints are compiled during the loading (editor) screen. The FLinker loads the InputMappingContext reference but will only serialize it AFTER the initial compilation hence we request a recompilation here if necessary.
		if (Extension->RequiresRecompileToDetectIssues(*Blueprint))
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
	}
}
