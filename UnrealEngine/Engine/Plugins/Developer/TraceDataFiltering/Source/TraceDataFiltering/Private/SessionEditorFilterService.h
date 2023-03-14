// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISessionTraceFilterService.h"
#include "BaseSessionFilterService.h"

namespace TraceServices
{
	class IAnalysisSession;
	typedef uint64 FSessionHandle;
}

/** Implementation of ISessionTraceFilterService specifically to use in the Editor */
class FSessionEditorFilterService : public FBaseSessionFilterService
{
public:
	FSessionEditorFilterService(TraceServices::FSessionHandle InHandle, TSharedPtr<const TraceServices::IAnalysisSession> InSession);
	virtual ~FSessionEditorFilterService() {}

	/** Begin FBaseSessionFilterService overrides */
	virtual void OnApplyChannelChanges() override;
	/** End OnApplyChannelChanges overrides */
};
