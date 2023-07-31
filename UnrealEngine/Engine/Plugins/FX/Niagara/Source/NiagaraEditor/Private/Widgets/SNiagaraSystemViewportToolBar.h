// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SViewportToolBar.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SNiagaraSystemViewport.h"

// In-viewport toolbar widget used in the Niagara editor
class SNiagaraSystemViewportToolBar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSystemViewportToolBar) {}
		SLATE_ARGUMENT(TWeakPtr<ISequencer>, Sequencer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SNiagaraSystemViewport> InViewport);

	// SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;
	virtual void ExtendOptionsMenu(FMenuBuilder& OptionsMenuBuilder) const override;
	/** Used for a custom realtime button as the default realtime button is for viewport realtime, not simulation realtime and overrides only allow for direct setting */
	virtual void ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const override;
	virtual bool GetShowScalabilityMenu() const override
	{
		return true;
	}
	// End of SCommonEditorViewportToolbarBase

	virtual bool IsViewModeSupported(EViewModeIndex ViewModeIndex) const override;

private:
	TWeakPtr<ISequencer> Sequencer = nullptr;
	bool bIsSimulationRealtime = true;
	
	EVisibility GetSimulationRealtimeWarningVisibility() const;
	FReply OnSimulationRealtimeWarningClicked() const;

	FText GetSimulationSpeedText() const;
};