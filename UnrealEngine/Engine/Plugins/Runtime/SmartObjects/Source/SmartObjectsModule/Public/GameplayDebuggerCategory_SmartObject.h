// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"

#if WITH_GAMEPLAY_DEBUGGER && WITH_SMARTOBJECT_DEBUG

#include "GameplayDebuggerCategory.h"

class APlayerController;
class AActor;

class FGameplayDebuggerCategory_SmartObject : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_SmartObject();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void ToggleInstanceTags();
	void ToggleSlotDetails();
	void ToggleAnnotations();

	struct FReplicationData
	{
		void Serialize(FArchive& Ar);

		bool bDisplayInstanceTags = false;
		bool bDisplaySlotDetails = false;
		bool bDisplayAnnotations = false;
	};

	FReplicationData DataPack;
};

#endif // WITH_GAMEPLAY_DEBUGGER && WITH_SMARTOBJECT_DEBUG
