// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/ModuleService.h"

namespace TraceServices
{

class FLoadTimeProfilerModule
	: public IModule
{
public:
	#if WITH_EDITOR
	virtual bool ShouldBeEnabledByDefault() const override { return false; }
	#endif
	virtual void GetModuleInfo(FModuleInfo& OutModuleInfo) override;
	virtual void OnAnalysisBegin(IAnalysisSession& Session) override;
	virtual void GenerateReports(const IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory) override;
	virtual void GetLoggers(TArray<const TCHAR*>& OutLoggers) override;
	virtual const TCHAR* GetCommandLineArgument() override
	{
		return TEXT("loadtimetrace");
	}
};

} // namespace TraceServices
