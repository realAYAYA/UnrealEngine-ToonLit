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
#include "RigVMDefines.h"
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
	virtual void Reset(bool IsIgnoringArchetypeRef = false) override;
	virtual bool IsNativized() const override { return true; }
	virtual void Empty(FRigVMExtendedExecuteContext& Context) override { return; }
	virtual void CopyFrom(URigVM* InVM, bool bDeferCopy = false, bool bReferenceLiteralMemory = false, bool bReferenceByteCode = false, bool bCopyExternalVariables = false, bool bCopyDynamicRegisters = false) override { return; }
	virtual bool Initialize(FRigVMExtendedExecuteContext& Context) override;
	virtual ERigVMExecuteResult ExecuteVM(FRigVMExtendedExecuteContext& Context, const FName& InEntryName = NAME_None) override;
	virtual int32 AddRigVMFunction(UScriptStruct* InRigVMStruct, const FName& InMethodName) override { return INDEX_NONE; }
	virtual FString GetRigVMFunctionName(int32 InFunctionIndex) const override { return FString(); }
	virtual FRigVMMemoryStorageStruct* GetMemoryByType(FRigVMExtendedExecuteContext& Context, ERigVMMemoryType InMemoryType) override { return nullptr; }
	virtual void ClearMemory() override { return; }
	virtual const FRigVMInstructionArray& GetInstructions() override;
	virtual bool ContainsEntry(const FName& InEntryName) const override { return GetEntryNames().Contains(InEntryName); }
	virtual int32 FindEntry(const FName& InEntryName) const override { return GetEntryNames().Find(InEntryName); }
	virtual const TArray<FName>& GetEntryNames() const override { static const TArray<FName> EmptyEntries; return EmptyEntries; }
	virtual void SetInstructionIndex(FRigVMExtendedExecuteContext& Context, uint16 InInstructionIndex) override
	{
		Super::SetInstructionIndex(Context, InInstructionIndex);
#if WITH_EDITOR
		if (FRigVMInstructionVisitInfo* InstructionVisitInfo = Context.GetRigVMInstructionVisitInfo())
		{
			InstructionVisitInfo->SetInstructionVisitedDuringLastRun(InInstructionIndex);
			InstructionVisitInfo->AddInstructionIndexToVisitOrder(InInstructionIndex);
		}
#endif
	}

	UE_DEPRECATED(5.3, "Please, use Empty with Context param")
	virtual void Empty() override {}
	UE_DEPRECATED(5.3, "Please, use Initialize with Context param")
	virtual bool Initialize(TArrayView<URigVMMemoryStorage*> Memory) override { return false; }
	UE_DEPRECATED(5.4, "Please, use Initialize just with Context param")
	virtual bool Initialize(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory) { return false; }
	UE_DEPRECATED(5.4, "GetMemoryByType has been deprecated from the VM. Please, use GetWorkMemory from VMHost or the new GetMemoryByType with Context parameter.")
	virtual URigVMMemoryStorage* GetMemoryByType(ERigVMMemoryType InMemoryType, bool bCreateIfNeeded = true) override { return nullptr; }
	UE_DEPRECATED(5.3, "Please, use Execute with Context param")
	virtual ERigVMExecuteResult Execute(TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName = NAME_None) override { return ERigVMExecuteResult::Failed; }
	UE_DEPRECATED(5.4, "Please, use Execute with just Context param")
	virtual ERigVMExecuteResult Execute(FRigVMExtendedExecuteContext& Context, TArrayView<URigVMMemoryStorage*> Memory, const FName& InEntryName = NAME_None) override { return ERigVMExecuteResult::Failed; }
	UE_DEPRECATED(5.3, "Please, use SetInstructionIndex with Context param")
	virtual void SetInstructionIndex(uint16 InInstructionIndex) {}

#if WITH_EDITOR
	void SetByteCode(const FRigVMByteCode& InByteCode);
#endif
	
protected:

	template<typename ExecuteContextType = FRigVMExecuteContext>
	ExecuteContextType& UpdateContext(FRigVMExtendedExecuteContext& Context, int32 InNumberInstructions, const FName& InEntryName)
	{
		UpdateExternalVariables(Context);
	
		Context.ResetExecutionState();
		Context.VM = this;
		Context.SliceOffsets.AddZeroed(InNumberInstructions);
		Context.GetPublicData<ExecuteContextType>().EventName = InEntryName;
		return Context.GetPublicData<ExecuteContextType>();
	}

	virtual void UpdateExternalVariables(FRigVMExtendedExecuteContext& Context) {};
	
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

	void BeginSlice(FRigVMExtendedExecuteContext& Context, int32 InCount, int32 InRelativeIndex = 0)
	{
		Context.BeginSlice(InCount, InRelativeIndex);
	}

	void EndSlice(FRigVMExtendedExecuteContext& Context)
	{
		Context.EndSlice();
	}

	bool IsValidArraySize(const FRigVMExtendedExecuteContext& Context, int32 InArraySize) const
	{
		return Context.IsValidArraySize(InArraySize);
	}

	template<typename T>
	bool IsValidArrayIndex(const FRigVMExtendedExecuteContext& Context, int32& InOutIndex, const TArray<T>& InArray) const
	{
		return Context.IsValidArrayIndex(InOutIndex, InArray.Num());
	}

	template<typename T>
	static const T& GetArrayElementSafe(const TArray<T>& InArray, const int32 InIndex)
	{
		if(InArray.IsValidIndex(InIndex))
		{
			return InArray[InIndex];
		}
		static const T EmptyElement;
		return EmptyElement;
	}

	template<typename T>
	static T& GetArrayElementSafe(TArray<T>& InArray, const int32 InIndex)
	{
		if(InArray.IsValidIndex(InIndex))
		{
			return InArray[InIndex];
		}
		static T EmptyElement;
		return EmptyElement;
	}

	template<typename A, typename B>
	static void CopyUnrelatedArrays(TArray<A>& Target, const TArray<B>& Source)
	{
		const int32 Num = Source.Num();
		Target.SetNum(Num);
		for(int32 Index = 0; Index < Num; Index++)
		{
			Target[Index] = (A)Source[Index];
		}
	}

	void BroadcastExecutionReachedExit(FRigVMExtendedExecuteContext& Context)
	{
		if(Context.EntriesBeingExecuted.Num() == 1)
		{
			Context.ExecutionReachedExit().Broadcast(Context.GetPublicData<>().GetEventName());
		}
		Context.GetPublicDataSafe<>().NumExecutions++;
	}

	template<typename T>
	T& GetExternalVariableRef(FRigVMExtendedExecuteContext& Context, const FName& InExternalVariableName, const FName& InExpectedType) const
	{
		for(const FRigVMExternalVariable& ExternalVariable : GetExternalVariables(Context))
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
	T& GetOperandSlice(FRigVMExtendedExecuteContext& Context, TArray<T>& InOutArray, const T* InDefaultValue = nullptr)
	{
		const int32 SliceIndex = Context.GetSlice().GetIndex();
		if(InOutArray.Num() <= SliceIndex)
		{
			const int32 FirstInsertedItem = InOutArray.AddDefaulted(1 + SliceIndex - InOutArray.Num());
			if (InDefaultValue)
			{
				for (int32 i=FirstInsertedItem; i<InOutArray.Num();++i)
				{
					InOutArray[i] = *InDefaultValue;
				}
			}
		}
		return InOutArray[SliceIndex];
	}

	template<typename T>
	static bool ArrayOp_Iterator(const TArray<T>& InArray, T& OutElement, int32 InIndex, int32& OutCount, float& OutRatio)
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

	static bool ArrayOp_Iterator(const TArray<double>& InArray, float& OutElement, int32 InIndex, int32& OutCount, float& OutRatio)
	{
		double Element = (double)OutElement;
		const bool bResult = ArrayOp_Iterator<double>(InArray, Element, InIndex, OutCount, OutRatio);
		OutElement = (float)Element;
		return bResult;
	}

	static bool ArrayOp_Iterator(const TArray<float>& InArray, double& OutElement, int32 InIndex, int32& OutCount, float& OutRatio)
	{
		float Element = (float)OutElement;
		const bool bResult = ArrayOp_Iterator<float>(InArray, Element, InIndex, OutCount, OutRatio);
		OutElement = (double)Element;
		return bResult;
	}

	template<typename T>
	static bool ArrayOp_Find(const TArray<T>& InArray, const T& InElement, int32& OutIndex)
	{
		OutIndex = InArray.Find(InElement);
		return OutIndex != INDEX_NONE;
	}

	static bool ArrayOp_Find(const TArray<float>& InArray, const double& InElement, int32& OutIndex)
	{
		return ArrayOp_Find<float>(InArray, (float)InElement, OutIndex);
	}

	static bool ArrayOp_Find(const TArray<double>& InArray, const float& InElement, int32& OutIndex)
	{
		return ArrayOp_Find<double>(InArray, (double)InElement, OutIndex);
	}

	template<typename T>
	static void ArrayOp_Union(TArray<T>& InOutA, const TArray<T>& InB)
	{
		for(const T& Element : InB)
		{
			InOutA.AddUnique(Element);
		}
	}

	template<typename T>
	static void ArrayOp_Difference(TArray<T>& InOutA, const TArray<T>& InB)
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
	static void ArrayOp_Intersection(TArray<T>& InOutA, const TArray<T>& InB)
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
	static void ArrayOp_Reverse(TArray<T>& InOutArray)
	{
		Algo::Reverse(InOutArray);
	}

public:

	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	static T GetStructConstant(const FString& InDefaultValue)
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
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type* = nullptr
	>
	static T GetStructConstant(const FString& InDefaultValue)
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
	static TArray<T> GetStructArrayConstant(const FString& InDefaultValue)
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
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type* = nullptr
	>
	static TArray<T> GetStructArrayConstant(const FString& InDefaultValue)
	{
		TArray<T> Value;
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
	static TArray<TArray<T>> GetStructArrayArrayConstant(const FString& InDefaultValue)
	{
		TArray<TArray<T>> Value;
		if(!InDefaultValue.IsEmpty())
		{
			FErrorPipe ErrorPipe;
			TBaseStructure<T>::Get()->ImportText(*InDefaultValue, &Value, nullptr, PPF_None, &ErrorPipe, FString());
		}
		return Value;
	}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type* = nullptr
	>
	static TArray<TArray<T>> GetStructArrayArrayConstant(const FString& InDefaultValue)
	{
		TArray<TArray<T>> Value;
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
		FTransformSetter(FTransform& InOutTransform)
			: Transform(InOutTransform)
		{
		}

		void SetTranslationX(const FTransform::FReal InValue) const { SetTranslationComponent(0, InValue); }
		void SetTranslationY(const FTransform::FReal InValue) const { SetTranslationComponent(1, InValue); }
		void SetTranslationZ(const FTransform::FReal InValue) const { SetTranslationComponent(2, InValue); }
		void SetTranslationComponent(const uint8 InComponent, const FTransform::FReal InValue) const
		{
			FVector Translation = Transform.GetTranslation();
			Translation.Component(InComponent) = InValue;
			Transform.SetTranslation(Translation);
		}
		
		void SetScaleX(const FTransform::FReal InValue) const { SetScaleComponent(0, InValue); }
		void SetScaleY(const FTransform::FReal InValue) const { SetScaleComponent(1, InValue); }
		void SetScaleZ(const FTransform::FReal InValue) const { SetScaleComponent(2, InValue); }
		void SetScaleComponent(const uint8 InComponent, const FTransform::FReal InValue) const
		{
			FVector Scale = Transform.GetScale3D();
			Scale.Component(InComponent) = InValue;
			Transform.SetScale3D(Scale);
		}
		
		void SetRotationX(const FTransform::FReal InValue) const { SetRotationComponent(0, InValue); }
		void SetRotationY(const FTransform::FReal InValue) const { SetRotationComponent(1, InValue); }
		void SetRotationZ(const FTransform::FReal InValue) const { SetRotationComponent(2, InValue); }
		void SetRotationW(const FTransform::FReal InValue) const { SetRotationComponent(3, InValue); }
		void SetRotationComponent(const uint8 InComponent, const FTransform::FReal InValue) const
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

	class FMatrixSetter
	{
	public:
		FMatrixSetter(FMatrix& InOutMatrix)
			: Matrix(InOutMatrix)
		{
		}

		void SetPlane(int32 InPlaneIndex, const FPlane& InPlane)
		{
			Matrix.M[InPlaneIndex][0] = InPlane.X;
			Matrix.M[InPlaneIndex][1] = InPlane.Y;
			Matrix.M[InPlaneIndex][2] = InPlane.Z;
			Matrix.M[InPlaneIndex][3] = InPlane.W;
		}

		void SetComponent(int32 InPlaneIndex, int32 InComponentIndex, double InValue)
		{
			Matrix.M[InPlaneIndex][InComponentIndex] = InValue;
		}

	private:
		FMatrix& Matrix;
	};

	template <typename T,
			  typename TEnableIf<!TIsArray<T>::Value, int>::Type = 0>
	bool ImportDefaultValue(T& OutValue, const FProperty* Property, const FString& InBuffer)
	{
		return false;
	}

	template <typename T,
			  typename TEnableIf<TIsArray<T>::Value, int>::Type = 0>
	bool ImportDefaultValue(T& OutValue, const FProperty* Property, const FString& InBuffer)
	{
		return false;
	}

	int32 TemporaryArrayIndex;

	const FRigVMMemoryHandle& GetLazyMemoryHandle(int32 InIndex, uint8* InMemory, const FProperty* InProperty, TFunction<ERigVMExecuteResult()> InLambda);
	
	template<typename T>
	TRigVMLazyValue<T> GetLazyValue(int32 InIndex, T& InMemory, const FProperty* InProperty, TFunction<ERigVMExecuteResult()> InLambda)
	{
		const FRigVMMemoryHandle& Handle = GetLazyMemoryHandle(InIndex, (uint8*)&InMemory, InProperty, InLambda);
		return Handle.GetDataLazily<T>(false, INDEX_NONE);
	}

	void AllocateLazyMemoryHandles(int32 InCount);

	TArray<FRigVMLazyBranch> LazyMemoryBranches;
	TArray<FRigVMMemoryHandle> LazyMemoryHandles;
};
