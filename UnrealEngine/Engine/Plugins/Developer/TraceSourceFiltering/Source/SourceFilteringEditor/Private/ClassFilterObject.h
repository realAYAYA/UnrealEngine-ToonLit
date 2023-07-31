// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class FClassFilterObject : public TSharedFromThis<FClassFilterObject>
{
public:
	FClassFilterObject(UClass* InClass, bool bDerivedClasses) : Class(InClass), bIncludeDerived(bDerivedClasses){}

	const FText GetDisplayText() const 
	{
		return FText::FromString(
			FString::Printf(TEXT("%s%s"),
				bIncludeDerived ? TEXT("Is Child Of ") : TEXT("Is a "),
				*Class->GetName()
			)
		);
	}

	UClass* GetClass() const { return Class; }

	bool IncludesDerivedClasses() const { return bIncludeDerived; }
protected:
	UClass* Class;
	bool bIncludeDerived;
};