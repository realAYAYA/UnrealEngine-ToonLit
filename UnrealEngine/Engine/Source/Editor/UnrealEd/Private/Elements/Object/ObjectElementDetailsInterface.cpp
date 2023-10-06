// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Object/ObjectElementDetailsInterface.h"

#include "Elements/Object/ObjectElementData.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FObjectTypedElementDetailsObject : public ITypedElementDetailsObject
{
public:
	explicit FObjectTypedElementDetailsObject(UObject* InObject)
		: ObjectPtr(InObject)
	{
	}

	virtual UObject* GetObject() override
	{
		return ObjectPtr.Get();
	}

private:
	TWeakObjectPtr<UObject> ObjectPtr;
};

TUniquePtr<ITypedElementDetailsObject> UObjectElementDetailsInterface::GetDetailsObject(const FTypedElementHandle& InElementHandle)
{
	if (UObject* Object = ObjectElementDataUtil::GetObjectFromHandle(InElementHandle))
	{
		return MakeUnique<FObjectTypedElementDetailsObject>(Object);
	}
	return nullptr;
}
