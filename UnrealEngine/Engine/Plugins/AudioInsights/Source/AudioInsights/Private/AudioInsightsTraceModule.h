// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceChannelHandle.h"
#include "AudioInsightsTraceProviderBase.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ModuleService.h"
#include "UObject/NameTypes.h"

namespace UE::Audio::Insights
{
	class FTraceModule : public TraceServices::IModule
	{
	public:
		FTraceModule();
		virtual ~FTraceModule() = default;

		//~ Begin TraceServices::IModule interface
		virtual void GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo) override;
		virtual void OnAnalysisBegin(TraceServices::IAnalysisSession& Session) override;
		virtual void GetLoggers(TArray<const TCHAR *>& OutLoggers) override;
		virtual void GenerateReports(const TraceServices::IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory) override;
		virtual const TCHAR* GetCommandLineArgument() override { return TEXT("audiotrace"); }
		//~ End TraceServices::IModule interface

		template <typename TraceProviderType>
		TSharedPtr<TraceProviderType> FindAudioTraceProvider() const
		{
			return StaticCastSharedPtr<TraceProviderType>(TraceProviders.FindRef(TraceProviderType::GetName_Static()));
		}

		TSharedRef<FTraceChannelManager> GetChannelManager();

		void StartTraceAnalysis() const;
		bool IsTraceAnalysisActive() const;
		void StopTraceAnalysis() const;

	private:
		static const FName GetName();

		static void DisableAllTraceChannels();
		static void EnableAudioInsightsTraceChannels();

		TSharedRef<FTraceChannelManager> ChannelManager;
		TMap<FName, TSharedPtr<FTraceProviderBase>> TraceProviders;
	};
} // namespace UE::Audio::Insights
