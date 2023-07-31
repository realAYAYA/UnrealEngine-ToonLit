// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/ModuleService.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{
	class FTasksModule : public IModule
	{
	public:
		virtual void GetModuleInfo(FModuleInfo& OutModuleInfo) override;
		virtual void OnAnalysisBegin(IAnalysisSession& Session) override;
		virtual void GetLoggers(TArray<const TCHAR*>& OutLoggers) override
		{
		}
		virtual const TCHAR* GetCommandLineArgument() override
		{
			return nullptr;
		}
		virtual void GenerateReports(const IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory) override
		{
		}
#if WITH_EDITOR
		virtual bool ShouldBeEnabledByDefault() const override
		{
			return false;
		}
#endif
	};
} // namespace TraceServices
