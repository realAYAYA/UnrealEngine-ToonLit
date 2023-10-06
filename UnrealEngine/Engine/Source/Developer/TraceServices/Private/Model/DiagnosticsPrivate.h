// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/Model/Diagnostics.h"

namespace TraceServices
{

class FDiagnosticsProvider : public IDiagnosticsProvider
{
public:
	explicit FDiagnosticsProvider(IAnalysisSession& Session);
	virtual ~FDiagnosticsProvider() {}

	virtual bool IsSessionInfoAvailable() const override;
	virtual const FSessionInfo& GetSessionInfo() const override;
	void SetSessionInfo(const FSessionInfo& InSessionInfo);

private:
	IAnalysisSession& Session;
	FSessionInfo SessionInfo;
	bool bIsSessionInfoAvailable = false;
};

} // namespace TraceServices
