// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/ModuleService.h"

namespace TraceServices
{

class FMemoryModule
	: public IModule
{
public:
	#if WITH_EDITOR
	virtual bool ShouldBeEnabledByDefault() const override { return false; }
	#endif
	virtual void GetModuleInfo(FModuleInfo& OutModuleInfo) override;
	virtual void OnAnalysisBegin(IAnalysisSession& Session) override;
	virtual void GetLoggers(TArray<const TCHAR*>& OutLoggers) override;
};

} // namespace TraceServices
