// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtrTemplates.h"
#include "Param/IParameterSource.h"
#include "PropertyBag.h"
#include "UObject/StrongObjectPtr.h"

class UAnimNextParameterBlock;

namespace UE::AnimNext
{

// Proxy struct used to reference parameter block instance data
struct FParameterBlockProxy : public IParameterSource
{
	FParameterBlockProxy() = delete;

	FParameterBlockProxy(UAnimNextParameterBlock* InParameterBlock);

	// IParameterSource interface
	virtual void Update(float DeltaTime) override;
	virtual const FParamStackLayerHandle& GetLayerHandle() const override { return LayerHandle; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// The object that this proxy wraps
	TObjectPtr<UAnimNextParameterBlock> ParameterBlock;

	// Copy of the parameter block's data
	FInstancedPropertyBag PropertyBag;

	// Layer handle - must be updated if PropertyBag changes layout
	FParamStackLayerHandle LayerHandle;
};

}