// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataprepParameterizableObject.h"

#include "DataprepBindingCommandChange.h"

#include "CoreGlobals.h"
#include "Misc/ITransaction.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/UnrealType.h"

void UDataprepParameterizableObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty( PropertyChangedEvent );

	if ( !bool( PropertyChangedEvent.ChangeType & ( EPropertyChangeType::Interactive ) ) )
	{
		OnPostEdit.Broadcast( *this, PropertyChangedEvent );
	}
}

