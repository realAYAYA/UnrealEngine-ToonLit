// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlendSpaceGraph.h"

#include "EdGraph/EdGraphSchema.h"
#include "Templates/SubclassOf.h"

void UBlendSpaceGraph::PostLoad()
{
	Super::PostLoad();

	// Fixup graph schema
	if(Schema && Schema != UEdGraphSchema::StaticClass())
	{
		Schema = UEdGraphSchema::StaticClass();
	}
}
