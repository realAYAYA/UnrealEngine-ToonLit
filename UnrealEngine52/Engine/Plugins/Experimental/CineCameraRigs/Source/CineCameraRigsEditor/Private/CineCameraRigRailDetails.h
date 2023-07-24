// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "CineCameraRigRail.h"

namespace ETextCommit { enum Type : int; }

/**
 * Implements a details view customization for the ACineCameraRigRail
 */
class FCineCameraRigRailDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FCineCameraRigRailDetails>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TOptional<float> CurrentPositionValue = 1.0f;

private:
	TWeakObjectPtr<ACineCameraRigRail> RigRailActorPtr;

	TOptional<float> GetAbsolutePosition() const;

	TOptional<float> GetAbsolutePositionSliderMinValue() const;
	TOptional<float> GetAbsolutePositionSliderMaxValue() const;
	void OnBeginAbsolutePositionSliderMovement();
	void OnEndAbsolutePositionSliderMovement(float NewValue);
	void OnAbsolutePositionCommitted(float NewValue, ETextCommit::Type CommitType);
	void OnAbsolutePositionChanged(float NewValue);

	bool bAbsolutePositionSliderStartedTransaction = false;

};
