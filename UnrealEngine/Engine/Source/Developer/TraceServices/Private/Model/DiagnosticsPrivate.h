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

public:
	void SetSessionInfo(const FSessionInfo& InSessionInfo);
	virtual const FSessionInfo& GetSessionInfo() const override;
	virtual bool IsSessionInfoAvailable() const override;

private:
	IAnalysisSession& Session;
	FSessionInfo SessionInfo;
	bool bIsSessionInfoAvailable = false;
};

} // namespace TraceServices
