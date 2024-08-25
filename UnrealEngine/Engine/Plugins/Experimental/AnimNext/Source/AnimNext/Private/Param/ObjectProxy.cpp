// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectProxy.h"

#include "ClassProxy.h"
#include "ObjectAccessor.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FObjectProxy::FObjectProxy(UObject* InObject, const TSharedRef<FObjectAccessor>& InObjectAccessor)
	: Object(InObject)
	, ObjectAccessor(InObjectAccessor)
	, RootParameterName(ObjectAccessor->AccessorName)
{
	// Always supply the root parameter in index 0
	ParameterCache.AddProperty(RootParameterName, EPropertyBagPropertyType::Object, InObject->GetClass());
	ParameterCache.SetValueObject(RootParameterName, InObject);
}

void FObjectProxy::Update(float DeltaTime)
{
	const UPropertyBag* PropertyBag = ParameterCache.GetPropertyBagStruct();
	TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs = PropertyBag->GetPropertyDescs();
	uint8* StructData = ParameterCache.GetMutableValue().GetMemory();
	UObject* ResolvedObject = Object.Get();
	PropertyDescs[0].CachedProperty->SetValue_InContainer(StructData, &ResolvedObject);
	if(ResolvedObject)
	{
		for(int32 ParameterIndex = 0; ParameterIndex < ParametersToUpdate.Num(); ++ParameterIndex)
		{
			const FAnimNextObjectProxyParameter& ParameterToUpdate = ParametersToUpdate[ParameterIndex];
			const FProperty* ResultProperty = PropertyDescs[ParameterToUpdate.ValueParamIndex].CachedProperty;
			void* ResultBuffer = ResultProperty->ContainerPtrToValuePtr<void>(StructData);

			switch(ParameterToUpdate.AccessType)
			{
			case EClassProxyParameterAccessType::Property:
				{
					const FProperty* SourceProperty = ParameterToUpdate.GetProperty();
					checkSlow(SourceProperty);
					checkSlow(SourceProperty->GetClass() == ResultProperty->GetClass());

					const void* SourceBuffer = SourceProperty->ContainerPtrToValuePtr<const void>(ResolvedObject);
					SourceProperty->CopyCompleteValue(ResultBuffer, SourceBuffer);
					break;
				}
			case EClassProxyParameterAccessType::AccessorFunction:
				{
					UFunction* Function = ParameterToUpdate.GetFunction();
					checkSlow(Function);
					checkSlow(ResolvedObject->GetClass()->IsChildOf(Function->GetOuterUClass()));

					FFrame Stack(ResolvedObject, Function, nullptr, nullptr, Function->ChildProperties);
					Function->Invoke(ResolvedObject, Stack, ResultBuffer);
					break;
				}
			case EClassProxyParameterAccessType::HoistedFunction:
				{
					UFunction* Function = ParameterToUpdate.GetFunction();
					checkSlow(Function);

					const FObjectProperty* ObjectProperty = CastFieldChecked<FObjectProperty>(PropertyDescs[0].CachedProperty);
					check(ResolvedObject->GetClass()->IsChildOf(ObjectProperty->PropertyClass));
					UObject** ObjectBuffer = ObjectProperty->ContainerPtrToValuePtr<UObject*>(StructData);
					*ObjectBuffer = ResolvedObject;

					FFrame Stack(ResolvedObject, Function, ObjectBuffer, nullptr, Function->ChildProperties);
					Function->Invoke(ResolvedObject, Stack, ResultBuffer);
					break;
				}
			default:
				checkNoEntry();
				break;
			}
		}
	}
}

void FObjectProxy::RequestParameterCache(TConstArrayView<FName> InParameterNames)
{
	using namespace UE::AnimNext;

	const int32 NumExistingProperties = ParameterCache.GetNumPropertiesInBag();

	TArray<FPropertyBagPropertyDesc> PropertyDescsToAdd;
	PropertyDescsToAdd.Reserve(InParameterNames.Num());

	for(FName ParameterName : InParameterNames)
	{
		if(!ParameterNameMap.Contains(ParameterName))
		{
			if(const int32* ParameterIndexPtr = ObjectAccessor->RemappedParametersMap.Find(ParameterName))
			{
				const FClassProxyParameter& ClassProxyParameter = ObjectAccessor->ClassProxy->Parameters[*ParameterIndexPtr];

				FAnimNextObjectProxyParameter& NewParameterToUpdate = ParametersToUpdate.AddDefaulted_GetRef();
				NewParameterToUpdate.AccessType = ClassProxyParameter.AccessType;
				NewParameterToUpdate.Function = ClassProxyParameter.Function.Get();
				NewParameterToUpdate.Property = ClassProxyParameter.Property.Get();

				int32 ValueParamIndex = PropertyDescsToAdd.Emplace(ParameterName, ClassProxyParameter.Type.GetContainerType(), ClassProxyParameter.Type.GetValueType(), ClassProxyParameter.Type.GetValueTypeObject());
				NewParameterToUpdate.ValueParamIndex = NumExistingProperties + ValueParamIndex;
				ParameterNameMap.Add(ParameterName, NewParameterToUpdate.ValueParamIndex);
			}
		}
	}

	// Update parameter bag struct
	ParameterCache.AddProperties(PropertyDescsToAdd);

	// Recreate layer handle as layout has changed
	LayerHandle = FParamStack::MakeReferenceLayer(ParameterCache);
}

void FObjectProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	ParameterCache.AddStructReferencedObjects(Collector);
}

}