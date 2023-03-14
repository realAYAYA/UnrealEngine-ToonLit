// Copyright Epic Games, Inc. All Rights Reserved.

#include "CascadeToNiagaraConverterModule.h"
#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "ContentBrowserModule.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleSystem.h"
#include "IPythonScriptPlugin.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "NiagaraMessageManager.h"
#include "NiagaraStackGraphUtilitiesAdapterLibrary.h"

IMPLEMENT_MODULE(ICascadeToNiagaraConverterModule, CascadeToNiagaraConverter);
DEFINE_LOG_CATEGORY(LogFXConverter);

#define LOCTEXT_NAMESPACE "CascadeToNiagaraConverterModule"


const FName FNiagaraConverterMessageTopics::VerboseConversionEventTopicName = "VerboseConversionEvent";
const FName FNiagaraConverterMessageTopics::ConversionEventTopicName = "ConversionEvent";

void ICascadeToNiagaraConverterModule::StartupModule()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&ICascadeToNiagaraConverterModule::OnExtendContentBrowserAssetSelectionMenu));
	FNiagaraMessageManager* MessageManager = FNiagaraMessageManager::Get();
	MessageManager->RegisterMessageTopic(FNiagaraConverterMessageTopics::VerboseConversionEventTopicName);
	MessageManager->RegisterMessageTopic(FNiagaraConverterMessageTopics::ConversionEventTopicName);
	MessageManager->RegisterAdditionalMessageLogTopic(FNiagaraConverterMessageTopics::ConversionEventTopicName);
}

void ICascadeToNiagaraConverterModule::ShutdownModule()
{
}

TSharedRef<FExtender> ICascadeToNiagaraConverterModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateStatic(&AddMenuExtenderConvertEntry, SelectedAssets)
		);

	return Extender;
}

void ICascadeToNiagaraConverterModule::AddMenuExtenderConvertEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	if (SelectedAssets.Num() > 0)
	{
		// check that all selected assets are cascade systems.
		TArray<UParticleSystem*> CascadeSystems;
		for (const FAssetData& SelectedAsset : SelectedAssets)
		{
			if (SelectedAsset.GetClass()->IsChildOf<UParticleSystem>() == false)
			{
				return;
			}
			CascadeSystems.Add(static_cast<UParticleSystem*>(SelectedAsset.GetAsset()));
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ConvertToNiagaraSystem", "Convert To Niagara System"),
			LOCTEXT("ConvertToNiagaraSystem_Tooltip", "Duplicate and convert the selected Cascade Systems to equivalent Niagara Systems."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&ExecuteConvertCascadeSystemToNiagaraSystem, CascadeSystems))
		);
	}
}

void ICascadeToNiagaraConverterModule::ExecuteConvertCascadeSystemToNiagaraSystem(TArray<UParticleSystem*> CascadeSystems)
{
	UConvertCascadeToNiagaraResults* Results = NewObject<UConvertCascadeToNiagaraResults>();

	for (UParticleSystem* CascadeSystem : CascadeSystems)
	{
		Results->Init();
		FPythonCommandEx PythonCommand = FPythonCommandEx();
		PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;

		PythonCommand.Command = "../../Plugins/FX/CascadeToNiagaraConverter/Content/Python/ConvertCascadeToNiagara.py ";
		PythonCommand.Command.Append(*CascadeSystem->GetPathName());
		PythonCommand.Command.Append(" ");
		PythonCommand.Command.Append(*Results->GetPathName());
		IPythonScriptPlugin::Get()->ExecPythonCommandEx(PythonCommand);

		if (Results->bCancelledByUser)
		{
			// raise modal dialog of status
			return;
		}
		else if (Results->bCancelledByPythonError)
		{
			// raise modal dialog of status
			//PythonCommand.CommandResult
			return;
		}
	}
}

#undef LOCTEXT_NAMESPACE //"CascadeToNiagaraConverterModule"
