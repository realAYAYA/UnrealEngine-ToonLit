// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/SMInstance/SMInstanceElementDetailsInterface.h"
#include "Elements/SMInstance/SMInstanceElementDetailsProxyObject.h"
#include "Elements/SMInstance/SMInstanceElementData.h"
#include "Components/InstancedStaticMeshComponent.h"

class FSMInstanceTypedElementDetailsObject : public ITypedElementDetailsObject
{
public:
	explicit FSMInstanceTypedElementDetailsObject(const FSMInstanceElementId& InSMInstanceElementId)
	{
		USMInstanceElementDetailsProxyObject* InstanceProxyObjectPtr = NewObject<USMInstanceElementDetailsProxyObject>();
		InstanceProxyObjectPtr->Initialize(InSMInstanceElementId);

		InstanceProxyObject = InstanceProxyObjectPtr;
	}

	~FSMInstanceTypedElementDetailsObject()
	{
		if (USMInstanceElementDetailsProxyObject* InstanceProxyObjectPtr = InstanceProxyObject.Get())
		{
			InstanceProxyObjectPtr->Shutdown();
		}
	}

	virtual UObject* GetObject() override
	{
		return InstanceProxyObject.Get();
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		if (USMInstanceElementDetailsProxyObject* InstanceProxyObjectPtr = InstanceProxyObject.Get())
		{
			Collector.AddReferencedObject(InstanceProxyObject);
			InstanceProxyObject = InstanceProxyObjectPtr;
		}
	}

private:
	TWeakObjectPtr<USMInstanceElementDetailsProxyObject> InstanceProxyObject;
};

TUniquePtr<ITypedElementDetailsObject> USMInstanceElementDetailsInterface::GetDetailsObject(const FTypedElementHandle& InElementHandle)
{
	if (const FSMInstanceElementData* SMInstanceElement = InElementHandle.GetData<FSMInstanceElementData>())
	{
		return MakeUnique<FSMInstanceTypedElementDetailsObject>(SMInstanceElement->InstanceElementId);
	}
	return nullptr;
}
