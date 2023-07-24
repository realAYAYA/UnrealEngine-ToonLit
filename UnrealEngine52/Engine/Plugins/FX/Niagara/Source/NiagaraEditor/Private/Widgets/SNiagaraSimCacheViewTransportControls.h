// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class NIAGARAEDITOR_API SNiagaraSimCacheViewTransportControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheViewTransportControls) {}
		SLATE_ARGUMENT(TWeakPtr<class FNiagaraSimCacheViewModel>, WeakViewModel)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	
	// Transport Controls
	TSharedRef<SWidget> MakeTransportControlsWidget(class FEditorWidgetsModule& EditorWidgetsModule);
	TOptional<int32> GetCurrentFrame() const;
	void OnFrameIndexChanged(const int32 InFrameIndex, ETextCommit::Type Arg);
	TOptional<int32> GetMaxFrameIndex() const;
	// Custom transport widget for frame selection
	TSharedRef<SWidget> CreateFrameWidget();
	
	FReply OnTransportBackwardEnd();
	FReply OnTransportBackwardStep();
	FReply OnTransportForwardStep();
	FReply OnTransportForwardEnd();

private:
	TWeakPtr<FNiagaraSimCacheViewModel> WeakViewModel;
	TSharedPtr<SWidget>	TransportControls;
};
