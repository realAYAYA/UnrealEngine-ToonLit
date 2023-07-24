// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"

class FMessageLog;

namespace UE {
namespace Trace {

class IAnalyzer;
class FStreamReader;

////////////////////////////////////////////////////////////////////////////////
class FAnalysisEngine
{
public:
						FAnalysisEngine(TArray<IAnalyzer*>&& Analyzers, FMessageLog* InLog);
						~FAnalysisEngine();
	void				Begin();
	void				End();
	bool				OnData(FStreamReader& Reader);

private:
	class FImpl;
						FAnalysisEngine(const FAnalysisEngine&) = delete;
						FAnalysisEngine(const FAnalysisEngine&&) = delete;
	FAnalysisEngine		operator = (const FAnalysisEngine&) = delete;
	FAnalysisEngine		operator = (const FAnalysisEngine&&) = delete;
	FImpl*				Impl;
};

} // namespace Trace
} // namespace UE
