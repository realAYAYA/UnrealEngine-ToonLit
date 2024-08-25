// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"
#include "PropertyBag.h"

namespace UE::AnimNext
{
	struct FObjectAccessor;
	enum class EClassProxyParameterAccessType : int32;
}

namespace UE::AnimNext
{

// Cached info about a parameter to update
struct FAnimNextObjectProxyParameter
{
	// How to access this parameter
	EClassProxyParameterAccessType AccessType;

	// The function to call
#if WITH_EDITOR
	// TWeakObjectPtr to accomodate potential reinstancing in editor
	TWeakObjectPtr<UFunction> Function;
#else
	UFunction* Function = nullptr;
#endif

	// The property to copy
#if WITH_EDITOR
	// TFieldPath to accomodate potential reinstancing in editor
	TFieldPath<FProperty> Property;
#else
	FProperty* Property = nullptr;
#endif

	// Index into the ParameterCache property bag's PropertyDescs array
	int32 ValueParamIndex = 0;

	FProperty* GetProperty() const
	{
#if WITH_EDITOR
		return Property.Get();
#else
		return Property;
#endif
	}

	UFunction* GetFunction() const
	{
#if WITH_EDITOR
		return Function.Get();
#else
		return Function;
#endif
	}
};

// Proxy struct used to fetch and cache external UObject data
struct FObjectProxy : public IParameterSource
{
	FObjectProxy() = delete;

	FObjectProxy(UObject* InObject, const TSharedRef<FObjectAccessor>& InObjectAccessor);

	// IParameterSource interface
	virtual void Update(float DeltaTime) override;
	virtual const FParamStackLayerHandle& GetLayerHandle() const override { return LayerHandle; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// Adds a set of parameters to cache each time the layer is updated
	void RequestParameterCache(TConstArrayView<FName> InParameterNames);

	// The object that this proxy wraps
	TWeakObjectPtr<UObject> Object;

	// Cache of properties, fetched from Object
	FInstancedPropertyBag ParameterCache;

	// Layer handle - must be updated if ParameterCache changes layout
	FParamStackLayerHandle LayerHandle;

	// Properties to update each call to UpdateLayer
	TArray<FAnimNextObjectProxyParameter> ParametersToUpdate;

	// Map of parameter name to index in ParametersToUpdate array
	TMap<FName, int32> ParameterNameMap;

	// Object accessor used to lookup parameters
	TSharedRef<FObjectAccessor> ObjectAccessor;

	// The name of the root parameter
	FName RootParameterName;
};

}