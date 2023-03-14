// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Reverse.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Logging/LogVerbosity.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/OutputDevice.h"
#include "RigVM.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMCore/RigVMMemoryCommon.h"
#include "RigVMCore/RigVMTraits.h"
#include "Templates/EnableIf.h"
#include "Templates/Models.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"

#include "RigVMNativized.generated.h"

class FArchive;
class UObject;
class URigVMMemoryStorage;
struct FRigVMInstructionArray;

UCLASS(BlueprintType, Abstract)
class RIGVM_API URigVMNativized : public URigVM
{
	GENERATED_BODY()

public:

	URigVMNativized();
	virtual ~URigVMNativized();

	// URigVM interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override { return; }
	virtual void Reset(bool IsIgnoringArchetypeRef = false) override { return; }
	virtual bool IsNativized() const override { return true; }
	virtual void Empty() override { return; }
	virtual void CopyFrom(URigVM* InVM, bool bDeferCopy = false, bool bReferenceLiteralMemory = false, bool bReferenceByteCode = false, bool bCopyExternalVariables = false, bool bCopyDynamicRegisters = false) override { return; }
	virtual bool Initialize(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, bool bInitializeMemory = true) override;
	virtual bool Execute(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName = NAME_None) override;
	virtual int32 AddRigVMFunction(UScriptStruct* InRigVMStruct, const FName& InMethodName) override { return INDEX_NONE; }
	virtual FString GetRigVMFunctionName(int32 InFunctionIndex) const override { return FString(); }
	virtual URigVMMemoryStorage* GetMemoryByType(ERigVMMemoryType InMemoryType, bool bCreateIfNeeded = true) override { return nullptr; }
	virtual void ClearMemory() override { return; }
	virtual const FRigVMInstructionArray& GetInstructions() override;
	virtual bool ContainsEntry(const FName& InEntryName) const override { return GetEntryNames().Contains(InEntryName); }
	virtual int32 FindEntry(const FName& InEntryName) const override { return GetEntryNames().Find(InEntryName); }
	virtual const TArray<FName>& GetEntryNames() const override { static const TArray<FName> EmptyEntries; return EmptyEntries; }
	
protected:

	const FRigVMExecuteContext& UpdateContext(TArrayView<void*> AdditionalArguments, int32 InNumberInstructions, const FName& InEntryName);
	virtual void UpdateExternalVariables() {};
	
	class FErrorPipe : public FOutputDevice
	{
	public:

		int32 NumErrors;

		FErrorPipe()
			: FOutputDevice()
			, NumErrors(0)
		{
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
		{
			NumErrors++;
		}
	};

	FORCEINLINE void BeginSlice(int32 InCount, int32 InRelativeIndex = 0)
	{
		Context.BeginSlice(InCount, InRelativeIndex);
	}

	FORCEINLINE void EndSlice()
	{
		Context.EndSlice();
	}

	FORCEINLINE bool IsValidArraySize(int32 InArraySize) const
	{
		return Context.IsValidArraySize(InArraySize);
	}

	template<typename T>
	FORCEINLINE bool IsValidArrayIndex(int32& InOutIndex, const TArray<T>& InArray) const
	{
		return Context.IsValidArrayIndex(InOutIndex, InArray.Num());
	}

	template<typename T>
	FORCEINLINE static const T& GetArrayElementSafe(const TArray<T>& InArray, const int32 InIndex)
	{
		if(InArray.IsValidIndex(InIndex))
		{
			return InArray[InIndex];
		}
		static const T EmptyElement;
		return EmptyElement;
	}

	template<typename T>
	FORCEINLINE static T& GetArrayElementSafe(TArray<T>& InArray, const int32 InIndex)
	{
		if(InArray.IsValidIndex(InIndex))
		{
			return InArray[InIndex];
		}
		static T EmptyElement;
		return EmptyElement;
	}

	template<typename A, typename B>
	FORCEINLINE static void CopyUnrelatedArrays(TArray<A>& Target, const TArray<B>& Source)
	{
		const int32 Num = Source.Num();
		Target.SetNum(Num);
		for(int32 Index = 0; Index < Num; Index++)
		{
			Target[Index] = (A)Source[Index];
		}
	}

	FORCEINLINE void BroadcastExecutionReachedExit()
	{
		if(EntriesBeingExecuted.Num() == 1)
		{
			ExecutionReachedExit().Broadcast(Context.PublicData.GetEventName());
		}
	}

	template<typename T>
	FORCEINLINE T& GetExternalVariableRef(const FName& InExternalVariableName, const FName& InExpectedType) const
	{
		for(const FRigVMExternalVariable& ExternalVariable : GetExternalVariables())
		{
			if(ExternalVariable.Name == InExternalVariableName && ExternalVariable.GetExtendedCPPType() == InExpectedType)
			{
				if(ExternalVariable.IsValid(false))
				{
					return *(T*)ExternalVariable.Memory;
				}
			}
		}
		static T EmptyValue;
		return EmptyValue;
	}

	template<typename T>
	FORCEINLINE T& GetOperandSlice(TArray<T>& InOutArray)
	{
		const int32 SliceIndex = Context.GetSlice().GetIndex();
		if(InOutArray.Num() <= SliceIndex)
		{
			InOutArray.AddDefaulted(1 + SliceIndex - InOutArray.Num());
		}
		return InOutArray[SliceIndex];
	}

	template<typename T>
	FORCEINLINE static bool ArrayOp_Iterator(const TArray<T>& InArray, T& OutElement, int32 InIndex, int32& OutCount, float& OutRatio)
	{
		OutCount = InArray.Num();
		const bool bContinue = InIndex >=0 && InIndex < OutCount;

		if((OutCount <= 0) || !bContinue)
		{
			OutRatio = 0.f;
		}
		else
		{
			OutRatio = float(InIndex) / float(OutCount - 1);
			OutElement = InArray[InIndex];
		}
		
		return bContinue;
	}

	FORCEINLINE static bool ArrayOp_Iterator(const TArray<double>& InArray, float& OutElement, int32 InIndex, int32& OutCount, float& OutRatio)
	{
		double Element = (double)OutElement;
		const bool bResult = ArrayOp_Iterator<double>(InArray, Element, InIndex, OutCount, OutRatio);
		OutElement = (float)Element;
		return bResult;
	}

	FORCEINLINE static bool ArrayOp_Iterator(const TArray<float>& InArray, double& OutElement, int32 InIndex, int32& OutCount, float& OutRatio)
	{
		float Element = (float)OutElement;
		const bool bResult = ArrayOp_Iterator<float>(InArray, Element, InIndex, OutCount, OutRatio);
		OutElement = (double)Element;
		return bResult;
	}

	template<typename T>
	FORCEINLINE static bool ArrayOp_Find(const TArray<T>& InArray, const T& InElement, int32& OutIndex)
	{
		OutIndex = InArray.Find(InElement);
		return OutIndex != INDEX_NONE;
	}

	FORCEINLINE static bool ArrayOp_Find(const TArray<float>& InArray, const double& InElement, int32& OutIndex)
	{
		return ArrayOp_Find<float>(InArray, (float)InElement, OutIndex);
	}

	FORCEINLINE static bool ArrayOp_Find(const TArray<double>& InArray, const float& InElement, int32& OutIndex)
	{
		return ArrayOp_Find<double>(InArray, (double)InElement, OutIndex);
	}

	template<typename T>
	FORCEINLINE static void ArrayOp_Union(TArray<T>& InOutA, const TArray<T>& InB)
	{
		for(const T& Element : InB)
		{
			InOutA.AddUnique(Element);
		}
	}

	template<typename T>
	FORCEINLINE static void ArrayOp_Difference(TArray<T>& InOutA, const TArray<T>& InB)
	{
		TArray<T> Temp;
		Swap(InOutA, Temp);

		for(const T& Element : Temp)
		{
			if(!InB.Contains(Element))
			{
				InOutA.Add(Element);
			}
		}
		for(const T& Element : InB)
		{
			if(!Temp.Contains(Element))
			{
				InOutA.Add(Element);
			}
		}
	}

	template<typename T>
	FORCEINLINE static void ArrayOp_Intersection(TArray<T>& InOutA, const TArray<T>& InB)
	{
		TArray<T> Temp;
		Swap(InOutA, Temp);

		for(const T& Element : Temp)
		{
			if(InB.Contains(Element))
			{
				InOutA.Add(Element);
			}
		}
	}

	template<typename T>
	FORCEINLINE static void ArrayOp_Reverse(TArray<T>& InOutArray)
	{
		Algo::Reverse(InOutArray);
	}

public:

	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE static T GetStructConstant(const FString& InDefaultValue)
	{
		T Value;
		if(!InDefaultValue.IsEmpty())
		{
			FErrorPipe ErrorPipe;
			TBaseStructure<T>::Get()->ImportText(*InDefaultValue, &Value, nullptr, PPF_None, &ErrorPipe, FString());
		}
		return Value;
	}

	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type* = nullptr
	>
	FORCEINLINE static T GetStructConstant(const FString& InDefaultValue)
	{
		T Value;
		if(!InDefaultValue.IsEmpty())
		{
			FErrorPipe ErrorPipe;
			T::StaticStruct()->ImportText(*InDefaultValue, &Value, nullptr, PPF_None, &ErrorPipe, FString());
		}
		return Value;
	}

	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE static TArray<T> GetStructArrayConstant(const FString& InDefaultValue)
	{
		TArray<T> Value;
		if(!InDefaultValue.IsEmpty())
		{
			FErrorPipe ErrorPipe;
			TBaseStructure<T>::Get()->ImportText(*InDefaultValue, &Value, nullptr, PPF_None, &ErrorPipe, FString());
		}
		return Value;
	}

	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type* = nullptr
	>
	FORCEINLINE static TArray<T> GetStructArrayConstant(const FString& InDefaultValue)
	{
		TArray<T> Value;
		if(!InDefaultValue.IsEmpty())
		{
			FErrorPipe ErrorPipe;
			T::StaticStruct()->ImportText(*InDefaultValue, &Value, nullptr, PPF_None, &ErrorPipe, FString());
		}
		return Value;
	}

protected:
	
	class FTransformSetter
	{
	public:
		FORCEINLINE FTransformSetter(FTransform& InOutTransform)
			: Transform(InOutTransform)
		{
		}

		FORCEINLINE void SetTranslationX(const FTransform::FReal InValue) const { SetTranslationComponent(0, InValue); }
		FORCEINLINE void SetTranslationY(const FTransform::FReal InValue) const { SetTranslationComponent(1, InValue); }
		FORCEINLINE void SetTranslationZ(const FTransform::FReal InValue) const { SetTranslationComponent(2, InValue); }
		FORCEINLINE void SetTranslationComponent(const uint8 InComponent, const FTransform::FReal InValue) const
		{
			FVector Translation = Transform.GetTranslation();
			Translation.Component(InComponent) = InValue;
			Transform.SetTranslation(Translation);
		}
		
		FORCEINLINE void SetScaleX(const FTransform::FReal InValue) const { SetScaleComponent(0, InValue); }
		FORCEINLINE void SetScaleY(const FTransform::FReal InValue) const { SetScaleComponent(1, InValue); }
		FORCEINLINE void SetScaleZ(const FTransform::FReal InValue) const { SetScaleComponent(2, InValue); }
		FORCEINLINE void SetScaleComponent(const uint8 InComponent, const FTransform::FReal InValue) const
		{
			FVector Scale = Transform.GetScale3D();
			Scale.Component(InComponent) = InValue;
			Transform.SetScale3D(Scale);
		}
		
		FORCEINLINE void SetRotationX(const FTransform::FReal InValue) const { SetRotationComponent(0, InValue); }
		FORCEINLINE void SetRotationY(const FTransform::FReal InValue) const { SetRotationComponent(1, InValue); }
		FORCEINLINE void SetRotationZ(const FTransform::FReal InValue) const { SetRotationComponent(2, InValue); }
		FORCEINLINE void SetRotationW(const FTransform::FReal InValue) const { SetRotationComponent(3, InValue); }
		FORCEINLINE void SetRotationComponent(const uint8 InComponent, const FTransform::FReal InValue) const
		{
			FQuat Rotation = Transform.GetRotation();
			switch(InComponent)
			{
				case 0:
				{
					Rotation.X = InValue;
					break;
				}
				case 1:
				{
					Rotation.Y = InValue;
					break;
				}
				case 2:
				{
					Rotation.Z = InValue;
					break;
				}
				case 3:
				default:
				{
					Rotation.W = InValue;
					break;
				}
			}
			Rotation.Normalize();
			Transform.SetRotation(Rotation);
		}

	private:
		FTransform& Transform;
	};

	int32 TemporaryArrayIndex;
};
