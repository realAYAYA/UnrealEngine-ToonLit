// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TG_SystemTypes.h"  
#include "Misc/Change.h"

class UTG_Graph;
class UTG_Node;
class UTG_Pin;

/** This class helps to Undo the pin connection correctly
as the Blob is not serialized so if the expression property is not connected and is holding 
a reference of blob it is not rest when undoing the connection*/
class TEXTUREGRAPH_API FTG_PinConnectionChange : public FCommandChange 
{
public:
	static void StoreChange(UTG_Graph* InGraph, UTG_Node& NodeFrom, FTG_Name& PinFromName);
	FTG_PinConnectionChange(UTG_Graph* InGraph, UTG_Node& NodeFrom, FTG_Name& PinFromName);

	virtual FString ToString() const override
	{
		return FString(TEXT("Pin Connection"));
	}

	virtual void Apply(UObject* Object) override {};
	virtual void Revert(UObject* Object) override;

private:
	UTG_Graph* Graph;
	UTG_Pin* PinTo;
};

/** This class helps to Redo the pin connection Break correctly
as the Blob is not serialized so if the expression property is not connected and is holding
a reference of blob it is not rest when redoing the connection*/
class TEXTUREGRAPH_API FTG_PinConnectionBreakChange : public FCommandChange
{
public:
	static void StoreChange(UTG_Graph* InGraph, UTG_Node& NodeFrom, FTG_Name& PinFromName);
	FTG_PinConnectionBreakChange(UTG_Graph* InGraph, UTG_Node& NodeFrom, FTG_Name& PinFromName);

	virtual FString ToString() const override
	{
		return FString(TEXT("Pin Connection Break"));
	}

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override {};

private:
	UTG_Graph* Graph;
	UTG_Pin* PinTo;
};
