// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametricSurfaceExtensionModule.h"

#include "ParametricRetessellateAction.h"
#include "ParametricRetessellateAction_Impl.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IStaticMeshEditor.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshEditorModule.h"
#include "UObject/StrongObjectPtr.h"


#define LOCTEXT_NAMESPACE "ParametricSurfaceExtensionModule"


/** UI extension that displays a Retessellate action in the StaticMeshEditor */
namespace StaticMeshEditorExtenser
{
	bool CanExecute(UStaticMesh* Target)
	{
		TArray<FAssetData> AssetData;
		AssetData.Emplace(Target);

		FParametricRetessellateAction_Impl RetessellateAction;
		return RetessellateAction.CanApplyOnAssets(AssetData);
	}

	void Execute(UStaticMesh* Target)
	{
		TArray<FAssetData> AssetData;
		AssetData.Emplace(Target);

		FParametricRetessellateAction_Impl RetessellateAction;
		RetessellateAction.ApplyOnAssets(AssetData);
	}

	void ExtendAssetMenu(FMenuBuilder& MenuBuilder, UStaticMesh* Target)
	{
		MenuBuilder.AddMenuEntry(
			FParametricRetessellateAction_Impl::Label,
			FParametricRetessellateAction_Impl::Tooltip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&StaticMeshEditorExtenser::Execute, Target),
				FCanExecuteAction::CreateStatic(&StaticMeshEditorExtenser::CanExecute, Target)
			)
		);
	}

	TSharedRef<FExtender> CreateExtenderForObjects(const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> Objects)
	{
		TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

		if (UStaticMesh* Target = Objects.Num() ? Cast<UStaticMesh>(Objects[0]) : nullptr)
		{
			Extender->AddMenuExtension(
				"AssetEditorActions",
				EExtensionHook::Position::After,
				CommandList,
				FMenuExtensionDelegate::CreateStatic(&StaticMeshEditorExtenser::ExtendAssetMenu, Target)
			);
		}

		return Extender;
	}

	void Register()
	{
		if (!IsRunningCommandlet())
		{
			IStaticMeshEditorModule& StaticMeshEditorModule = FModuleManager::Get().LoadModuleChecked<IStaticMeshEditorModule>("StaticMeshEditor");
			TArray<FAssetEditorExtender>& ExtenderDelegates = StaticMeshEditorModule.GetMenuExtensibilityManager()->GetExtenderDelegates();
			ExtenderDelegates.Add(FAssetEditorExtender::CreateStatic(&StaticMeshEditorExtenser::CreateExtenderForObjects));
		}
	}
};

FParametricSurfaceExtensionModule& FParametricSurfaceExtensionModule::Get()
{
	return FModuleManager::LoadModuleChecked< FParametricSurfaceExtensionModule >(PARAMETRICSURFACEEXTENSION_MODULE_NAME);
}

bool FParametricSurfaceExtensionModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(PARAMETRICSURFACEEXTENSION_MODULE_NAME);
}

void FParametricSurfaceExtensionModule::StartupModule()
{
	if (!IsRunningCommandlet())
	{
		StaticMeshEditorExtenser::Register();
	}
}

IMPLEMENT_MODULE(FParametricSurfaceExtensionModule, ParametricSurfaceExtension);

#undef LOCTEXT_NAMESPACE // "ParametricSurfaceExtensionModule"

