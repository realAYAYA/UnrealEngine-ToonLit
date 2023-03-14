// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SPropertiesDebugViewBase.h"

namespace TraceServices { class IAnalysisSession; }

class SObjectPropertiesView : public SPropertiesDebugViewBase
{
public:
	virtual void GetVariantsAtFrame(const TraceServices::FFrame& InFrame, TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const override;
	virtual FName GetName() const override;
};

class FObjectPropertiesViewCreator : public IRewindDebuggerViewCreator
{
	public:
		virtual FName GetTargetTypeName() const;
		virtual FName GetName() const override;
		virtual FText GetTitle() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual TSharedPtr<IRewindDebuggerView> CreateDebugView(uint64 ObjectId, double CurrentTime, const TraceServices::IAnalysisSession& InAnalysisSession) const override;
		virtual bool HasDebugInfo(uint64 ObjectId) const override;
};