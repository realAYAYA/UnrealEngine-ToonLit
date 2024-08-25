// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/PropertyBagProxy.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FPropertyBagProxy::FPropertyBagProxy()
{
	LayerHandle = FParamStack::MakeReferenceLayer(PropertyBag);
}

void FPropertyBagProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	PropertyBag.AddStructReferencedObjects(Collector);
}

void FPropertyBagProxy::AddPropertyAndValue(FName InName, const FProperty* InProperty, const void* InContainerPtr)
{
	PropertyBag.AddProperty(InName, InProperty);
	PropertyBag.SetValue(InName, InProperty, InContainerPtr);

	// Recreate the layer handle as the bag layout has changed
	LayerHandle = FParamStack::MakeReferenceLayer(PropertyBag);
}

void FPropertyBagProxy::AddPropertiesAndValues(TConstArrayView<FPropertyAndValue> InPropertiesAndValues)
{
	TArray<FPropertyBagPropertyDesc, TInlineAllocator<16>> NewDescs;
	NewDescs.Reserve(InPropertiesAndValues.Num());

	// Add all new properties
	for(const FPropertyAndValue& PropertyAndValue : InPropertiesAndValues)
	{
		NewDescs.Emplace(PropertyAndValue.Name, PropertyAndValue.Property);
	}

	PropertyBag.AddProperties(NewDescs);

	// Set each value - note that SetValue uses a linear search internally so this is quite slow
	for(const FPropertyAndValue& PropertyAndValue : InPropertiesAndValues)
	{
		PropertyBag.SetValue(PropertyAndValue.Name, PropertyAndValue.Property, PropertyAndValue.ContainerPtr);
	}

	// Recreate the layer handle as the bag layout has changed
	LayerHandle = FParamStack::MakeReferenceLayer(PropertyBag);
}

}
