// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRewindDebuggerView.h"
#include "IRewindDebuggerViewCreator.h"
#include "SVariantValueView.h"

namespace TraceServices { class IAnalysisSession; }

class SPropertiesDebugViewBase : public IRewindDebuggerView
{
	SLATE_BEGIN_ARGS(SPropertiesDebugViewBase) {}
	SLATE_ATTRIBUTE(double, CurrentTime)
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, uint64 InObjectId, double InTimeMarker, const TraceServices::IAnalysisSession& InAnalysisSession);

	virtual void SetTimeMarker(double InTimeMarker) override;
	virtual uint64 GetObjectId() const override { return ObjectId; }

	virtual void GetVariantsAtFrame(const TraceServices::FFrame& InFrame, TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const = 0;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
protected:
	TSharedPtr<SVariantValueView> View;

	uint64 ObjectId;
	double TimeMarker;

	TAttribute<double> CurrentTime;
	const TraceServices::IAnalysisSession* AnalysisSession;
};