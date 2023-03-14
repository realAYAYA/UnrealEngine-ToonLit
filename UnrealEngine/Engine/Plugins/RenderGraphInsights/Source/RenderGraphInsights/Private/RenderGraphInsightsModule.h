// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#include "RenderGraphTraceModule.h"
#include "RenderGraphTimingViewExtender.h"

class FTabManager;

namespace UE
{
namespace RenderGraphInsights
{

class FRenderGraphInsightsModule : public IModuleInterface
{
public:
	static FRenderGraphInsightsModule& Get();

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	FRenderGraphTraceModule TraceModule;
	FRenderGraphTimingViewExtender TimingViewExtender;
	TWeakPtr<FTabManager> InsightsTabManager;
};

} //namespace RenderGraphInsights
} //namespace UE