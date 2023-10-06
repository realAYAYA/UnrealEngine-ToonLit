// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/TransformProxy.h"

#include "SubTransformProxy.generated.h"

/*
 * A sub-class of UTransformProxy that adds support for sub-proxies that can be attached
 * to a Transform Proxy
*/
UCLASS()
class USubTransformProxy : public UTransformProxy
{
	GENERATED_BODY()

	public:

	USubTransformProxy();

	/**
	 * Attach a sub transform proxy
	 * @param bSubscribetoSchanges true if the SubProxy wants to listen to parent's updates
	 * @return The relative transform of the SubProxy that was passed in
	 */
	virtual FTransform AddSubTransformProxy(USubTransformProxy* InProxy, bool bSubscribeToChanges = true);

	/**
	 * This delegate is fired whenever the internal transform changes in a way that would change the relative transform, ie
	 * on AddSubTransformProxy and Add Component
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRelativeTransformChanged, UTransformProxy*, FTransform);
	FOnRelativeTransformChanged OnRelativeTransformChanged;

	protected:

	virtual void OnParentTransformChanged(UTransformProxy* Parent, FTransform ParentTransform);
	virtual void OnParentRelativeTransformChanged(UTransformProxy* Parent, FTransform ParentTransform);

	/** The relative transform of this SubTransformProxy if it is attached to a parent SubTransformProxy */
	UPROPERTY()
	FTransform RelativeTransform;
};