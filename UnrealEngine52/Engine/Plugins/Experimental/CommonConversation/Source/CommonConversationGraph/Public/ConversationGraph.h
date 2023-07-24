// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIGraph.h"
#include "ConversationGraph.generated.h"

class UConversationDatabase;

UCLASS()
class COMMONCONVERSATIONGRAPH_API UConversationGraph : public UAIGraph
{
	GENERATED_UCLASS_BODY()

public:
	// UAIGraph interface
	virtual void UpdateAsset(int32 UpdateFlags) override;
	// End of UAIGraph interface
};
