// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "ISessionSourceFilterService.h"

namespace TraceServices
{
	class IAnalysisSession;
}

class FSourceFilterService
{
public:
	/** Retrieves the ISessionSourceFilterService for the provided Trace Session Handle */
	static TSharedRef<ISessionSourceFilterService> GetFilterServiceForSession(uint32 InHandle, TSharedRef<const TraceServices::IAnalysisSession> AnalysisSession);
};
