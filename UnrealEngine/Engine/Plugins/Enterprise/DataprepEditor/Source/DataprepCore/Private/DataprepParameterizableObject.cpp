// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataprepParameterizableObject.h"



void UDataprepParameterizableObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty( PropertyChangedEvent );

	if ( !bool( PropertyChangedEvent.ChangeType & ( EPropertyChangeType::Interactive ) ) )
	{
		OnPostEdit.Broadcast( *this, PropertyChangedEvent );
	}
}

