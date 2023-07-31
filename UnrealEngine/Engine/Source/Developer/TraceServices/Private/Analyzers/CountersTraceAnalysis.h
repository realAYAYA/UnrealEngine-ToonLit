// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"
#include "Containers/Map.h"

namespace TraceServices
{

class IAnalysisSession;
class IEditableCounter;
class IEditableCounterProvider;

class FCountersAnalyzer
	: public UE::Trace::IAnalyzer
{
public:
	FCountersAnalyzer(IAnalysisSession& Session, IEditableCounterProvider& EditableCounterProvider);
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_Spec,
		RouteId_SetValueInt,
		RouteId_SetValueFloat,
	};

	IAnalysisSession& Session;
	IEditableCounterProvider& EditableCounterProvider;
	TMap<uint16, IEditableCounter*> EditableCountersMap;
};

} // namespace TraceServices
