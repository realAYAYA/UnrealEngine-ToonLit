// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FDisplayClusterOperatorStyle : public FSlateStyleSet
{
public:
	FDisplayClusterOperatorStyle();
	~FDisplayClusterOperatorStyle();

	static FDisplayClusterOperatorStyle& Get()
	{
		static FDisplayClusterOperatorStyle Inst;
		return Inst;
	}

private:
	void Initialize();

};
