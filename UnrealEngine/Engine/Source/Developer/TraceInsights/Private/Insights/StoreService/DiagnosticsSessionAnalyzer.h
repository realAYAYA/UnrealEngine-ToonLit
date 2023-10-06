// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trace/Analyzer.h"

namespace Insights
{

struct FDiagnosticsSessionAnalyzer : public UE::Trace::IAnalyzer
{
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle, const FOnEventContext& Context) override;

	enum : uint16
	{
		RouteId_Session,
		RouteId_Session2,
	};

	FString Platform;
	FString AppName;
	FString ProjectName;
	FString CommandLine;
	FString Branch;
	FString BuildVersion;
	uint32 Changelist = 0;
	EBuildConfiguration ConfigurationType = EBuildConfiguration::Unknown;
	EBuildTargetType TargetType = EBuildTargetType::Unknown;
};

} // namespace Insights
