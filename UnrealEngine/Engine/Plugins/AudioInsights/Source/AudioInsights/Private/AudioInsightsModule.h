// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "AudioInsightsDashboardFactory.h"
#include "AudioInsightsTraceModule.h"
#include "Framework/Docking/TabManager.h"
#include "IAudioInsightsModule.h"
#include "Templates/SharedPointer.h"
#include "Views/DashboardViewFactory.h"
#include "Widgets/Docking/SDockTab.h"


namespace UE::Audio::Insights
{
	// Forward Declarations
	class FMixerSourceTraceProvider;

	class FAudioInsightsModule final : public IAudioInsightsModule
	{
		TSharedPtr<FDashboardFactory> DashboardFactory;
		FTraceModule TraceModule;

	public:
		FAudioInsightsModule() = default;

		virtual ~FAudioInsightsModule() = default;

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		virtual void RegisterDashboardViewFactory(TSharedRef<IDashboardViewFactory> InDashboardFactory) override;
		virtual void UnregisterDashboardViewFactory(FName InName) override;
		virtual ::Audio::FDeviceId GetDeviceId() const override;

		FTraceModule& GetTraceModule();

		TSharedRef<FDashboardFactory> GetDashboardFactory();
		const TSharedRef<FDashboardFactory> GetDashboardFactory() const;

		static FAudioInsightsModule& GetChecked();

	private:
		TSharedRef<SDockTab> CreateDashboardTabWidget(const FSpawnTabArgs& Args);
		void RegisterMenus();
	};
} // namespace UE::Audio::Insights
