// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "AudioGameplayFlags.h"
#include "Templates/SharedPointer.h"
#include "AudioGameplayVolumeProxy.generated.h"

// Forward Declarations 
struct FBodyInstance;
struct FAudioProxyMutatorPriorities;
struct FAudioProxyMutatorSearchResult;
class FProxyVolumeMutator;
class UPrimitiveComponent;
class UAudioGameplayVolumeComponent;

/**
 *  UAudioGameplayVolumeProxy - Abstract proxy used on audio thread to represent audio gameplay volumes.
 */
UCLASS(Abstract, EditInlineNew, HideDropdown)
class AUDIOGAMEPLAYVOLUME_API UAudioGameplayVolumeProxy : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	using PayloadFlags = AudioGameplay::EComponentPayload;
	using ProxyMutatorList = TArray<TSharedPtr<FProxyVolumeMutator>>;

	virtual ~UAudioGameplayVolumeProxy() = default;

	virtual bool ContainsPosition(const FVector& Position) const;
	virtual void InitFromComponent(const UAudioGameplayVolumeComponent* Component);

	void FindMutatorPriority(FAudioProxyMutatorPriorities& Priorities) const;
	void GatherMutators(const FAudioProxyMutatorPriorities& Priorities, FAudioProxyMutatorSearchResult& OutResult) const;

	void AddPayloadType(PayloadFlags InType);
	bool HasPayloadType(PayloadFlags InType) const;

	uint32 GetVolumeID() const;
	uint32 GetWorldID() const;

protected:

	ProxyMutatorList ProxyVolumeMutators;

	uint32 VolumeID = INDEX_NONE;
	uint32 WorldID = INDEX_NONE;
	PayloadFlags PayloadType = PayloadFlags::AGCP_None;
};

/**
 *  UAGVPrimitiveComponentProxy - Proxy based on a volume's primitive component
 */
UCLASS(meta = (DisplayName = "AGV Primitive Proxy"))
class UAGVPrimitiveComponentProxy : public UAudioGameplayVolumeProxy
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UAGVPrimitiveComponentProxy() = default;

	virtual bool ContainsPosition(const FVector& Position) const override;
	virtual void InitFromComponent(const UAudioGameplayVolumeComponent* Component) override;

protected:
	
	/** Returns false if we can skip the physics body query, which can be expensive */
	bool NeedsPhysicsQuery(UPrimitiveComponent* PrimitiveComponent, const FVector& Position) const;

	TWeakObjectPtr<UPrimitiveComponent> WeakPrimative;
};

/**
 *  UAGVConditionProxy - Proxy for use with the UAudioGameplayCondition interface
 */
UCLASS(meta = (DisplayName = "AGV Condition Proxy"))
class UAGVConditionProxy : public UAudioGameplayVolumeProxy
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UAGVConditionProxy() = default;

	virtual bool ContainsPosition(const FVector& Position) const override;
	virtual void InitFromComponent(const UAudioGameplayVolumeComponent* Component) override;

protected:

	TWeakObjectPtr<const UObject> WeakObject;
};
