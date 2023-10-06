// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/ModuleService.h"

namespace TraceServices
{

class FDiagnosticsModule
	: public IModule
{
public:
	virtual void GetModuleInfo(FModuleInfo& OutModuleInfo) override;
	virtual void GetLoggers(TArray<const TCHAR*>& OutLoggers) override;
	virtual void OnAnalysisBegin(IAnalysisSession& Session) override;
};

} // namespace TraceServices
