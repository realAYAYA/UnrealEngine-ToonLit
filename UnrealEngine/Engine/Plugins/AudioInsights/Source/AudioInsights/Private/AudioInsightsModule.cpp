// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioInsightsModule.h"

#include "AudioInsightsDashboardAssetCommands.h"
#include "AudioInsightsDashboardFactory.h"
#include "AudioInsightsLog.h"
#include "AudioInsightsStyle.h"
#include "AudioInsightsTraceModule.h"
#include "Features/IModularFeatures.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "TraceServices/ModuleService.h"
#include "UObject/NameTypes.h"
#include "Views/AudioBusesDashboardViewFactory.h"
#include "Views/AudioMetersDashboardViewFactory.h"
#include "Views/LogDashboardViewFactory.h"
#include "Views/MixerSourceDashboardViewFactory.h"
#include "Views/OutputMeterDashboardViewFactory.h"
#include "Views/OutputOscilloscopeDashboardViewFactory.h"
#include "Views/SubmixesDashboardViewFactory.h"
#include "Views/ViewportDashboardViewFactory.h"
#include "Views/VirtualLoopDashboardViewFactory.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "AudioInsights"
DEFINE_LOG_CATEGORY(LogAudioInsights);


namespace UE::Audio::Insights
{
	void FAudioInsightsModule::StartupModule()
	{
		// Don't run providers in cook commandlet to avoid additional, unnecessary overhead as audio insights is dormant.
		if (!IsRunningCookCommandlet())
		{
			IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &TraceModule);

			RegisterMenus();

			DashboardFactory = MakeShared<FDashboardFactory>();
			DashboardFactory->RegisterViewFactory(MakeShared<FViewportDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FLogDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FMixerSourceDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FVirtualLoopDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FSubmixesDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FAudioBusesDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FAudioMetersDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FOutputMeterDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FOutputOscilloscopeDashboardViewFactory>());

			FDashboardAssetCommands::Register();
		}
	}

	void FAudioInsightsModule::ShutdownModule()
	{
		if (!IsRunningCookCommandlet())
		{
			DashboardFactory.Reset();
			IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &TraceModule);

			FDashboardAssetCommands::Unregister();
		}
	}

	void FAudioInsightsModule::RegisterDashboardViewFactory(TSharedRef<IDashboardViewFactory> InDashboardFactory)
	{
		DashboardFactory->RegisterViewFactory(InDashboardFactory);
	}

	void FAudioInsightsModule::UnregisterDashboardViewFactory(FName InName)
	{
		DashboardFactory->UnregisterViewFactory(InName);
	}

	::Audio::FDeviceId FAudioInsightsModule::GetDeviceId() const
	{
		return DashboardFactory->GetDeviceId();
	}

	FAudioInsightsModule& FAudioInsightsModule::GetChecked()
	{
		return static_cast<FAudioInsightsModule&>(FModuleManager::GetModuleChecked<IAudioInsightsModule>(GetName()));
	}

	TSharedRef<FDashboardFactory> FAudioInsightsModule::GetDashboardFactory()
	{
		return DashboardFactory->AsShared();
	}

	const TSharedRef<FDashboardFactory> FAudioInsightsModule::GetDashboardFactory() const
	{
		return DashboardFactory->AsShared();
	}

	FTraceModule& FAudioInsightsModule::GetTraceModule()
	{
		return TraceModule;
	}

	TSharedRef<SDockTab> FAudioInsightsModule::CreateDashboardTabWidget(const FSpawnTabArgs& Args)
	{
		return DashboardFactory->MakeDockTabWidget(Args);
	}

	void FAudioInsightsModule::RegisterMenus()
	{
		const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("AudioInsights", FOnSpawnTab::CreateRaw(this, &FAudioInsightsModule::CreateDashboardTabWidget))
			.SetDisplayName(LOCTEXT("OpenDashboard_TabDisplayName", "Audio Insights"))
			.SetTooltipText(LOCTEXT("OpenDashboard_TabTooltip", "Opens Audio Insights, an extensible suite of tools and visualizers which enable monitoring and debugging audio in the Unreal Engine."))
			.SetGroup(MenuStructure.GetToolsCategory())
			.SetIcon(FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Dashboard"));
	};
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE // AudioInsights

IMPLEMENT_MODULE(UE::Audio::Insights::FAudioInsightsModule, AudioInsights)
