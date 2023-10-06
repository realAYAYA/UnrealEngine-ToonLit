// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserPropertyAccess.h"

#include "IObjectChooser.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "IPropertyAccessEditor.h"
#endif

namespace UE::Chooser
{
	bool ResolvePropertyChain(FChooserEvaluationContext& Context, const FChooserPropertyBinding& PropertyBinding, const void*& OutContainer, const UStruct*& OutStructType)
	{
		if (Context.Params.IsValidIndex(PropertyBinding.ContextIndex))
		{
			if (FChooserEvaluationInputObject* ObjectParam = Context.Params[PropertyBinding.ContextIndex].GetMutablePtr<FChooserEvaluationInputObject>())
			{
				OutContainer = ObjectParam->Object;
				if (OutContainer)
				{
					OutStructType = ObjectParam->Object->GetClass();
				}
				else
				{
					OutStructType = nullptr;
				}
			}
			else
			{
				OutContainer = Context.Params[PropertyBinding.ContextIndex].GetMutableMemory();
				OutStructType = Context.Params[PropertyBinding.ContextIndex].GetScriptStruct();
			}

			if (OutContainer == nullptr || OutStructType == nullptr)
			{
				return false;
			}

			return ResolvePropertyChain(OutContainer, OutStructType, PropertyBinding.PropertyBindingChain);
		}

		return false;
	}
	
	bool ResolvePropertyChain(const void*& Container, const UStruct*& StructType, const TArray<FName>& PropertyBindingChain)
	{
		if (PropertyBindingChain.Num() == 0)
		{
			return false;
		}
	
		const int PropertyChainLength = PropertyBindingChain.Num();
		for(int PropertyChainIndex = 0; PropertyChainIndex < PropertyChainLength - 1; PropertyChainIndex++)
		{
			if (const FStructProperty* StructProperty = FindFProperty<FStructProperty>(StructType, PropertyBindingChain[PropertyChainIndex]))
			{
				StructType = StructProperty->Struct;
				Container = StructProperty->ContainerPtrToValuePtr<void>(Container);
			}
			else if (const FObjectProperty* ObjectProperty = FindFProperty<FObjectProperty>(StructType, PropertyBindingChain[PropertyChainIndex]))
			{
				StructType = ObjectProperty->PropertyClass;
				Container = *ObjectProperty->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Container);
				if (Container == nullptr)
				{
					return false;
				}
			}
			else
			{
				// check if it's a member function
				if (const UClass* ClassType = Cast<const UClass>(StructType))
				{
					if (UFunction* Function = ClassType->FindFunctionByName(PropertyBindingChain[PropertyChainIndex]))
					{
						UObject* Object = reinterpret_cast<UObject*>(const_cast<void*>(Container));
						if (Function->IsNative())
						{
							FFrame Stack(Object, Function, nullptr, nullptr, Function->ChildProperties);
							Function->Invoke(Object, Stack, &Container);
						}
						else
						{
							Object->ProcessEvent(Function, &Container);
						}
						
						if (Container == nullptr)
						{
							return false;
						}
						else
						{
							StructType = reinterpret_cast<UObject*>(const_cast<void*>(Container))->GetClass();
						}
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
		}
	
		return true;
	}
	
#if WITH_EDITOR
	void CopyPropertyChain(const TArray<FBindingChainElement>& InBindingChain, FChooserPropertyBinding& OutPropertyBinding)
	{
		OutPropertyBinding.PropertyBindingChain.Empty();

		if (InBindingChain.Num() == 0)
		{
			OutPropertyBinding.ContextIndex = -1;
		}
		else
		{
			OutPropertyBinding.ContextIndex = InBindingChain[0].ArrayIndex;
		}

		for (int32 i = 1; i < InBindingChain.Num(); ++i)
		{
			OutPropertyBinding.PropertyBindingChain.Emplace(InBindingChain[i].Field.GetFName());
		}
	}
#endif

	
}