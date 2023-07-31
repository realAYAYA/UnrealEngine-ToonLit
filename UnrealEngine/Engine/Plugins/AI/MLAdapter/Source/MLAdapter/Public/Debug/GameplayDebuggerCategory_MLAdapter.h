// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"
#include "MLAdapterTypes.h"


class UMLAdapterAgent;

class FGameplayDebuggerCategory_MLAdapter : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_MLAdapter();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	void ResetProps();

	void OnShowNextAgent();
	void OnRequestAvatarUpdate();
	void OnSetAvatarAsDebugAgent();

	void OnCurrentSessionChanged();
	void OnAgentAvatarChanged(UMLAdapterAgent& Agent, AActor* OldAvatar);
	void OnBeginAgentRemove(UMLAdapterAgent& Agent);

	void Init();

protected:
	AActor* CachedDebugActor;
	FMLAdapter::FAgentID CachedAgentID;
};

#endif // WITH_GAMEPLAY_DEBUGGER
