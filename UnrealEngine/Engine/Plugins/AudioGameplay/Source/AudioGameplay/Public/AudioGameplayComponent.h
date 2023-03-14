// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "AudioGameplayFlags.h"
#include "AudioGameplayComponent.generated.h"

UCLASS(Blueprintable, Config = Game, meta = (BlueprintSpawnableComponent))
class AUDIOGAMEPLAY_API UAudioGameplayComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	using PayloadFlags = AudioGameplay::EComponentPayload;

	virtual ~UAudioGameplayComponent() = default;

	//~ Begin UActorComponent interface
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	virtual bool HasPayloadType(PayloadFlags InType) const;

protected:

	virtual void Enable();
	virtual void Disable();

	PayloadFlags PayloadType = PayloadFlags::AGCP_None;
};