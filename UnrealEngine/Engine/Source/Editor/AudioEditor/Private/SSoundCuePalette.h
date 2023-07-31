// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "SGraphPalette.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FGraphActionListBuilderBase;

//////////////////////////////////////////////////////////////////////////

class SSoundCuePalette : public SGraphPalette
{
public:
	SLATE_BEGIN_ARGS( SSoundCuePalette ) {};
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	/** Callback used to populate all actions list in SGraphActionMenu */
	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override;
};
