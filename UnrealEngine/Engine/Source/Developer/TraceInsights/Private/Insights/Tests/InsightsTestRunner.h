// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Containers/Ticker.h"

#if !UE_BUILD_SHIPPING && !WITH_EDITOR

#include "Insights/IUnrealInsightsModule.h"

DECLARE_LOG_CATEGORY_EXTERN(InsightsTestRunner, Log, All);

class TRACEINSIGHTS_API FInsightsTestRunner : public TSharedFromThis<FInsightsTestRunner>, public IInsightsComponent
{
public:
	virtual ~FInsightsTestRunner();

	void ScheduleCommand(const FString& InCmd);

	virtual void Initialize(IUnrealInsightsModule& InsightsModule) override;
	virtual void Shutdown() override;
	virtual void RegisterMajorTabs(IUnrealInsightsModule& InsightsModule) override;
	virtual void UnregisterMajorTabs() override;

	static TSharedPtr<FInsightsTestRunner> CreateInstance();
	static TSharedPtr<FInsightsTestRunner> Get();

	bool Tick(float DeltaTime);

	void SetAutoQuit(bool InAutoQuit) { bAutoQuit = InAutoQuit; }
	bool GetAutoQuit() const { return bAutoQuit; }

	void SetInitAutomationModules(bool InInitAutomationModules) { bInitAutomationModules = InInitAutomationModules; }
	bool GetInitAutomationModules() const { return bInitAutomationModules; }
	void RunTests();

private:
	TSharedRef<SDockTab> SpawnAutomationWindowTab(const FSpawnTabArgs& Args);
	void OnSessionAnalysisCompleted();

private:
	/** The delegate to be invoked when this manager ticks. */
	FTickerDelegate OnTick;

	/** Handle to the registered OnTick. */
	FTSTicker::FDelegateHandle OnTickHandle;

	FDelegateHandle SessionAnalysisCompletedHandle;

	FString CommandToExecute;

	bool bAutoQuit = false;
	bool bInitAutomationModules = false;
	bool bIsRunningTests = false;
	bool bIsAnalysisComplete = false;

	static const TCHAR* AutoQuitMsgOnComplete;

	static TSharedPtr<FInsightsTestRunner> Instance;
};

#endif //UE_BUILD_SHIPPING && !WITH_EDITOR
