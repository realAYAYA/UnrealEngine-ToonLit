// Copyright Epic Games, Inc. All Rights Reserved.


#include "SGraphNodeDefault.h"

#include "GenericPlatform/ICursor.h"
#include "Misc/Optional.h"

void SGraphNodeDefault::Construct( const FArguments& InArgs )
{
	this->GraphNode = InArgs._GraphNodeObj;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}
