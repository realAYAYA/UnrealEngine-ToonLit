// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FRCSetAssetByPathBehaviourModel;

class REMOTECONTROLUI_API SRCBehaviourSetAssetByPath : public  SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRCBehaviourSetAssetByPath)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedRef<FRCSetAssetByPathBehaviourModel> InBehaviourItem);

private:
	/** The Behaviour (UI model) associated with us*/
	TWeakPtr<const FRCSetAssetByPathBehaviourModel> SetAssetByPathWeakPtr;
};
