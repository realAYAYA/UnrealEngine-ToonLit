// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimTransitionNodeDetails.h"
#include "Templates/SharedPointer.h"

class IDetailCustomization;
class IDetailLayoutBuilder;

class FAnimStateNodeDetails : public FAnimTransitionNodeDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	
	void GenerateAnimationStateEventRow(IDetailCategoryBuilder& SegmentCategory, const FText & StateEventLabel, const FString & TransitionName);
};

