// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISessionTraceFilterService.h"
#include "BaseSessionFilterService.h"

namespace TraceServices
{
	class IAnalysisSession;
	typedef uint64 FSessionHandle;
}

/** Implementation of ISessionTraceFilterService specifically to use with Trace, using TraceServices::IChannelProvider to provide information about Channels available on the running application 
 and TraceServices::ISessionService to change the Channel state(s). */
class FSessionTraceFilterService : public FBaseSessionFilterService
{
public:
	FSessionTraceFilterService(TraceServices::FSessionHandle InHandle, TSharedPtr<const TraceServices::IAnalysisSession> InSession);
	virtual ~FSessionTraceFilterService() {}

	/** Begin FBaseSessionFilterService overrides */
	virtual void OnApplyChannelChanges() override;
	/** End OnApplyChannelChanges overrides */
};
