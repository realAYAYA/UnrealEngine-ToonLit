// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamHelpers.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "Templates/SubclassOf.h"
#include "Graph/AnimNext_LODPose.h"

namespace UE::AnimNext
{

FParamHelpers::ECopyResult FParamHelpers::Copy(const FAnimNextParamType& InSourceType, const FAnimNextParamType& InTargetType, TConstArrayView<uint8> InSourceMemory, TArrayView<uint8> InTargetMemory)
{
	if(InSourceType == InTargetType)
	{
		Copy(InSourceType, InSourceMemory, InTargetMemory);
		return ECopyResult::Succeeded;
	}

	/** TODO: mismatched/conversions etc. */
	check(false);
	return ECopyResult::Failed;
}


FParamHelpers::ECopyResult FParamHelpers::Copy(const FParamTypeHandle& InSourceTypeHandle, const FParamTypeHandle& InTargetTypeHandle, TConstArrayView<uint8> InSourceMemory, TArrayView<uint8> InTargetMemory)
{
	if(InSourceTypeHandle == InTargetTypeHandle)
	{
		Copy(InSourceTypeHandle, InSourceMemory, InTargetMemory);
		return ECopyResult::Succeeded;
	}

	/** TODO: mismatched/conversions etc. */
	check(false);
	return ECopyResult::Failed;
}

void FParamHelpers::Copy(const FAnimNextParamType& InType, TConstArrayView<uint8> InSourceMemory, TArrayView<uint8> InTargetMemory)
{
	switch (InType.ContainerType)
	{
	case FAnimNextParamType::EContainerType::Array:
	{
		const TScriptArray<FHeapAllocator>* SourceArray = reinterpret_cast<const TScriptArray<FHeapAllocator>*>(InSourceMemory.GetData());
		TScriptArray<FHeapAllocator>* TargetArray = reinterpret_cast<TScriptArray<FHeapAllocator>*>(InTargetMemory.GetData());

		const int32 NumElements = SourceArray->Num();
		const size_t ValueTypeSize = InType.GetValueTypeSize();
		const size_t ValueTypeAlignment = InType.GetValueTypeSize();

		// Reallocate target array
		TargetArray->SetNumUninitialized(NumElements, ValueTypeSize, ValueTypeAlignment);
		check(TargetArray->GetAllocatedSize(ValueTypeSize) == SourceArray->GetAllocatedSize(ValueTypeSize));

		// Perform the copy according to value type
		switch (InType.ValueType)
		{
		case FAnimNextParamType::EValueType::None:
			ensureMsgf(false, TEXT("Trying to copy parameter of type None"));
			break;
		case FAnimNextParamType::EValueType::Bool:
		case FAnimNextParamType::EValueType::Byte:
		case FAnimNextParamType::EValueType::Int32:
		case FAnimNextParamType::EValueType::Int64:
		case FAnimNextParamType::EValueType::Float:
		case FAnimNextParamType::EValueType::Double:
		case FAnimNextParamType::EValueType::Name:
		case FAnimNextParamType::EValueType::Enum:
		{
			FMemory::Memcpy(TargetArray->GetData(), SourceArray->GetData(), SourceArray->GetAllocatedSize(ValueTypeSize));
		}
		break;
		case FAnimNextParamType::EValueType::String:
		{
			const FString* SourceString = static_cast<const FString*>(SourceArray->GetData());
			FString* TargetString = static_cast<FString*>(TargetArray->GetData());
			for (int32 Index = 0; Index < NumElements; ++Index)
			{
				*TargetString = *SourceString;
				++TargetString;
				++SourceString;
			}
		}
		break;
		case FAnimNextParamType::EValueType::Text:
		{
			const FText* SourceText = static_cast<const FText*>(SourceArray->GetData());
			FText* TargetText = static_cast<FText*>(TargetArray->GetData());
			for (int32 Index = 0; Index < NumElements; ++Index)
			{
				*TargetText = *SourceText;
				++TargetText;
				++SourceText;
			}
		}
		break;
		case FAnimNextParamType::EValueType::Struct:
			if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InType.ValueTypeObject.Get()))
			{
				ScriptStruct->CopyScriptStruct(TargetArray->GetData(), SourceArray->GetData(), NumElements);
			}
			else
			{
				checkf(false, TEXT("Error: FParameterHelpers::Copy: Unknown Struct Type"));
			}
			break;
		case FAnimNextParamType::EValueType::Object:
		{
			const TObjectPtr<UObject>* SourceObjectPtr = reinterpret_cast<const TObjectPtr<UObject>*>(InSourceMemory.GetData());
			TObjectPtr<UObject>* TargetObjectPtr = reinterpret_cast<TObjectPtr<UObject>*>(InTargetMemory.GetData());
			for (int32 Index = 0; Index < NumElements; ++Index)
			{
				*TargetObjectPtr = *SourceObjectPtr;
				++TargetObjectPtr;
				++SourceObjectPtr;
			}
		}
		break;
		case FAnimNextParamType::EValueType::SoftObject:
		{
			const TSoftObjectPtr<UObject>* SourceSoftObjectPtr = reinterpret_cast<const TSoftObjectPtr<UObject>*>(InSourceMemory.GetData());
			TSoftObjectPtr<UObject>* TargetSoftObjectPtr = reinterpret_cast<TSoftObjectPtr<UObject>*>(InTargetMemory.GetData());
			for (int32 Index = 0; Index < NumElements; ++Index)
			{
				*TargetSoftObjectPtr = *SourceSoftObjectPtr;
				++TargetSoftObjectPtr;
				++SourceSoftObjectPtr;
			}
		}
		break;
		case FAnimNextParamType::EValueType::Class:
		{
			const TSubclassOf<UObject>* SourceClassPtr = reinterpret_cast<const TSubclassOf<UObject>*>(InSourceMemory.GetData());
			TSubclassOf<UObject>* TargetClassPtr = reinterpret_cast<TSubclassOf<UObject>*>(InTargetMemory.GetData());
			for (int32 Index = 0; Index < NumElements; ++Index)
			{
				*TargetClassPtr = *SourceClassPtr;
				++TargetClassPtr;
				++SourceClassPtr;
			}
		}
		break;
		case FAnimNextParamType::EValueType::SoftClass:
		{
			const TSoftClassPtr<UObject>* SourceClassPtr = reinterpret_cast<const TSoftClassPtr<UObject>*>(InSourceMemory.GetData());
			TSoftClassPtr<UObject>* TargetClassPtr = reinterpret_cast<TSoftClassPtr<UObject>*>(InTargetMemory.GetData());
			for (int32 Index = 0; Index < NumElements; ++Index)
			{
				*TargetClassPtr = *SourceClassPtr;
				++TargetClassPtr;
				++SourceClassPtr;
			}
		}
		break;
		default:
			checkf(false, TEXT("Error: FParameterHelpers::Copy of unknown type"));
			break;
		}
	}
	break;
	case FAnimNextParamType::EContainerType::None:
	{
		switch (InType.ValueType)
		{
		case FAnimNextParamType::EValueType::None:
			ensureMsgf(false, TEXT("Trying to copy parameter of type None"));
			break;
		case FAnimNextParamType::EValueType::Bool:
		case FAnimNextParamType::EValueType::Byte:
		case FAnimNextParamType::EValueType::Int32:
		case FAnimNextParamType::EValueType::Int64:
		case FAnimNextParamType::EValueType::Float:
		case FAnimNextParamType::EValueType::Double:
		case FAnimNextParamType::EValueType::Name:
		case FAnimNextParamType::EValueType::Enum:
		{
			const int32 ParamAlignment = InType.GetAlignment();
			const int32 ParamSize = InType.GetSize();
			const int32 ParamAllocSize = Align(ParamSize, ParamAlignment);
			check(InTargetMemory.Num() <= ParamAllocSize);
			check(InSourceMemory.Num() <= ParamAllocSize);
			check(IsAligned(InTargetMemory.GetData(), ParamAlignment));
			check(IsAligned(InSourceMemory.GetData(), ParamAlignment));

			FMemory::Memcpy(InTargetMemory.GetData(), InSourceMemory.GetData(), ParamAllocSize);
		}
		break;
		case FAnimNextParamType::EValueType::String:
		{
			const FString* SourceString = reinterpret_cast<const FString*>(InSourceMemory.GetData());
			FString* TargetString = reinterpret_cast<FString*>(InTargetMemory.GetData());
			*TargetString = *SourceString;
		}
		break;
		case FAnimNextParamType::EValueType::Text:
		{
			const FText* SourceText = reinterpret_cast<const FText*>(InSourceMemory.GetData());
			FText* TargetText = reinterpret_cast<FText*>(InTargetMemory.GetData());
			*TargetText = *SourceText;
		}
		break;
		case FAnimNextParamType::EValueType::Struct:
			if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InType.ValueTypeObject.Get()))
			{
				const int32 StructSize = ScriptStruct->GetStructureSize();
				check(StructSize <= InTargetMemory.Num());

				ScriptStruct->CopyScriptStruct(InTargetMemory.GetData(), InSourceMemory.GetData(), 1);
			}
			else
			{
				checkf(false, TEXT("Error: FParameterHelpers::Copy: Unknown Struct Type"));
			}
			break;
		case FAnimNextParamType::EValueType::Object:
		{
			const TObjectPtr<UObject>* SourceObjectPtr = reinterpret_cast<const TObjectPtr<UObject>*>(InSourceMemory.GetData());
			TObjectPtr<UObject>* TargetObjectPtr = reinterpret_cast<TObjectPtr<UObject>*>(InTargetMemory.GetData());
			*TargetObjectPtr = *SourceObjectPtr;
		}
		break;
		case FAnimNextParamType::EValueType::SoftObject:
		{
			const TSoftObjectPtr<UObject>* SourceSoftObjectPtr = reinterpret_cast<const TSoftObjectPtr<UObject>*>(InSourceMemory.GetData());
			TSoftObjectPtr<UObject>* TargetSoftObjectPtr = reinterpret_cast<TSoftObjectPtr<UObject>*>(InTargetMemory.GetData());
			*TargetSoftObjectPtr = *SourceSoftObjectPtr;
		}
		break;
		case FAnimNextParamType::EValueType::Class:
		{
			const TSubclassOf<UObject>* SourceClassPtr = reinterpret_cast<const TSubclassOf<UObject>*>(InSourceMemory.GetData());
			TSubclassOf<UObject>* TargetClassPtr = reinterpret_cast<TSubclassOf<UObject>*>(InTargetMemory.GetData());
			*TargetClassPtr = *SourceClassPtr;
		}
		break;
		case FAnimNextParamType::EValueType::SoftClass:
		{
			const TSoftClassPtr<UObject>* SourceClassPtr = reinterpret_cast<const TSoftClassPtr<UObject>*>(InSourceMemory.GetData());
			TSoftClassPtr<UObject>* TargetClassPtr = reinterpret_cast<TSoftClassPtr<UObject>*>(InTargetMemory.GetData());
			*TargetClassPtr = *SourceClassPtr;
		}
		break;
		default:
			checkf(false, TEXT("Error: FParameterHelpers::Copy of unknown type"));
			break;
		}
	}
	break;
	}
}

void FParamHelpers::Copy(const FParamTypeHandle& InTypeHandle, TConstArrayView<uint8> InSourceMemory, TArrayView<uint8> InTargetMemory)
{
	auto SimpleCopy = [&InTargetMemory, &InSourceMemory](int32 InParamSize, int32 InAlignment)
	{
		const int32 ParamAllocSize = Align(InParamSize, InAlignment);
		check(InTargetMemory.Num() >= ParamAllocSize);
		check(InSourceMemory.Num() >= ParamAllocSize);
		check(IsAligned(InTargetMemory.GetData(), InAlignment));
		check(IsAligned(InSourceMemory.GetData(), InAlignment));

		FMemory::Memmove(InTargetMemory.GetData(), InSourceMemory.GetData(), ParamAllocSize);
	};

	switch (InTypeHandle.GetParameterType())
	{
	case FParamTypeHandle::EParamType::None:
		ensureMsgf(false, TEXT("Trying to copy parameter of type None"));
		break;
	case FParamTypeHandle::EParamType::Bool:
		SimpleCopy(sizeof(bool), alignof(bool));
		break;
	case FParamTypeHandle::EParamType::Byte:
		SimpleCopy(sizeof(uint8), alignof(uint8));
		break;
	case FParamTypeHandle::EParamType::Int32:
		SimpleCopy(sizeof(int32), alignof(int32));
		break;
	case FParamTypeHandle::EParamType::Int64:
		SimpleCopy(sizeof(int64), alignof(int64));
		break;
	case FParamTypeHandle::EParamType::Float:
		SimpleCopy(sizeof(float), alignof(float));
		break;
	case FParamTypeHandle::EParamType::Double:
		SimpleCopy(sizeof(double), alignof(double));
		break;
	case FParamTypeHandle::EParamType::Name:
		SimpleCopy(sizeof(FName), alignof(FName));
		break;
	case FParamTypeHandle::EParamType::String:
		{
			const FString* SourceString = reinterpret_cast<const FString*>(InSourceMemory.GetData());
			FString* TargetString = reinterpret_cast<FString*>(InTargetMemory.GetData());
			*TargetString = *SourceString;
		}
		break;
	case FParamTypeHandle::EParamType::Text:
		{
			const FText* SourceText = reinterpret_cast<const FText*>(InSourceMemory.GetData());
			FText* TargetText = reinterpret_cast<FText*>(InTargetMemory.GetData());
			*TargetText = *SourceText;
		}
		break;
	case FParamTypeHandle::EParamType::Vector:
		SimpleCopy(sizeof(FVector), alignof(FVector));
		break;
	case FParamTypeHandle::EParamType::Vector4:
		SimpleCopy(sizeof(FVector4), alignof(FVector4));
		break;
	case FParamTypeHandle::EParamType::Quat:
		SimpleCopy(sizeof(FQuat), alignof(FQuat));
		break;
	case FParamTypeHandle::EParamType::Transform:
		SimpleCopy(sizeof(FTransform), alignof(FTransform));
		break;
	case FParamTypeHandle::EParamType::Object:
	case FParamTypeHandle::EParamType::CharacterMovementComponent:
	case FParamTypeHandle::EParamType::AnimNextMeshComponent:
	case FParamTypeHandle::EParamType::AnimSequence:
		SimpleCopy(sizeof(UObject*), alignof(UObject*));
		break;
	case FParamTypeHandle::EParamType::AnimNextGraphLODPose:
		SimpleCopy(sizeof(FAnimNextGraphLODPose), alignof(FAnimNextGraphLODPose));
		break;
	case FParamTypeHandle::EParamType::AnimNextGraphReferencePose:
		{
			const FAnimNextGraphReferencePose* SourceRefPose = reinterpret_cast<const FAnimNextGraphReferencePose*>(InSourceMemory.GetData());
			FAnimNextGraphReferencePose* TargetSourceRefPose = reinterpret_cast<FAnimNextGraphReferencePose*>(InTargetMemory.GetData());
			*TargetSourceRefPose = *SourceRefPose;
		}
		break;
	case FParamTypeHandle::EParamType::Custom:
		{
			FAnimNextParamType Type = InTypeHandle.GetType();
			Copy(Type, InSourceMemory, InTargetMemory);
		}
		break;
	default:
		checkf(false, TEXT("Error: FParameterHelpers::Copy of unknown type"));
		break;
	}
}

void FParamHelpers::Destroy(const FAnimNextParamType& InType, TArrayView<uint8> InMemory)
{
	switch (InType.ContainerType)
	{
	case FAnimNextParamType::EContainerType::Array:
		{
			TScriptArray<FHeapAllocator>* Array = reinterpret_cast<TScriptArray<FHeapAllocator>*>(InMemory.GetData());
			const int32 NumElements = Array->Num();

			// Perform the destroy according to value type
			switch (InType.ValueType)
			{
			case FAnimNextParamType::EValueType::None:
			case FAnimNextParamType::EValueType::Bool:
			case FAnimNextParamType::EValueType::Byte:
			case FAnimNextParamType::EValueType::Int32:
			case FAnimNextParamType::EValueType::Int64:
			case FAnimNextParamType::EValueType::Float:
			case FAnimNextParamType::EValueType::Double:
			case FAnimNextParamType::EValueType::Name:
			case FAnimNextParamType::EValueType::Enum:
				break;
			case FAnimNextParamType::EValueType::String:
				{
					FString* String = static_cast<FString*>(Array->GetData());
					for (int32 Index = 0; Index < NumElements; ++Index)
					{
						String->~FString();
						++String;
					}
				}
				break;
			case FAnimNextParamType::EValueType::Text:
				{
					FText* Text = static_cast<FText*>(Array->GetData());
					for (int32 Index = 0; Index < NumElements; ++Index)
					{
						Text->~FText();
						++Text;
					}
				}
				break;
			case FAnimNextParamType::EValueType::Struct:
				if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InType.ValueTypeObject.Get()))
				{
					ScriptStruct->DestroyStruct(Array->GetData(), NumElements);
				}
				else
				{
					checkf(false, TEXT("Error: FParameterHelpers::Destroy: Unknown Struct Type"));
				}
				break;
			case FAnimNextParamType::EValueType::Object:
				{
					TObjectPtr<UObject>* ObjectPtr = reinterpret_cast<TObjectPtr<UObject>*>(InMemory.GetData());
					for (int32 Index = 0; Index < NumElements; ++Index)
					{
						*ObjectPtr = nullptr;
						++ObjectPtr;
					}
				}
				break;
			case FAnimNextParamType::EValueType::SoftObject:
				break;
			case FAnimNextParamType::EValueType::Class:
				{
					TSubclassOf<UObject>* ClassPtr = reinterpret_cast<TSubclassOf<UObject>*>(InMemory.GetData());
					for (int32 Index = 0; Index < NumElements; ++Index)
					{
						*ClassPtr = nullptr;
						++ClassPtr;
					}
				}
				break;
			case FAnimNextParamType::EValueType::SoftClass:
				break;
			default:
				checkf(false, TEXT("Error: FParameterHelpers::Destroy of unknown type"));
				break;
			}
		}
		break;
	case FAnimNextParamType::EContainerType::None:
		{
			switch (InType.ValueType)
			{
			case FAnimNextParamType::EValueType::None:
			case FAnimNextParamType::EValueType::Bool:
			case FAnimNextParamType::EValueType::Byte:
			case FAnimNextParamType::EValueType::Int32:
			case FAnimNextParamType::EValueType::Int64:
			case FAnimNextParamType::EValueType::Float:
			case FAnimNextParamType::EValueType::Double:
			case FAnimNextParamType::EValueType::Name:
			case FAnimNextParamType::EValueType::Enum:
				break;
			case FAnimNextParamType::EValueType::String:
				{
					FString* String = reinterpret_cast<FString*>(InMemory.GetData());
					String->~FString();
				}
				break;
			case FAnimNextParamType::EValueType::Text:
				{
					FText* Text = reinterpret_cast<FText*>(InMemory.GetData());
					Text->~FText();
				}
				break;
			case FAnimNextParamType::EValueType::Struct:
				if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InType.ValueTypeObject.Get()))
				{
					ScriptStruct->DestroyStruct(InMemory.GetData(), 1);
				}
				else
				{
					checkf(false, TEXT("Error: FParameterHelpers::Destroy: Unknown Struct Type"));
				}
				break;
			case FAnimNextParamType::EValueType::Object:
				{
					TObjectPtr<UObject>* ObjectPtr = reinterpret_cast<TObjectPtr<UObject>*>(InMemory.GetData());
					*ObjectPtr = nullptr;
				}
				break;
			case FAnimNextParamType::EValueType::SoftObject:
				break;
			case FAnimNextParamType::EValueType::Class:
				{
					TSubclassOf<UObject>* ClassPtr = reinterpret_cast<TSubclassOf<UObject>*>(InMemory.GetData());
					*ClassPtr = nullptr;
				}
				break;
			case FAnimNextParamType::EValueType::SoftClass:
				break;
			default:
				checkf(false, TEXT("Error: FParameterHelpers::Destroy of unknown type"));
				break;
			}
		}
		break;
	}
}

void FParamHelpers::Destroy(const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InMemory)
{
	switch (InTypeHandle.GetParameterType())
	{
	case FParamTypeHandle::EParamType::None:
	case FParamTypeHandle::EParamType::Bool:
	case FParamTypeHandle::EParamType::Byte:
	case FParamTypeHandle::EParamType::Int32:
	case FParamTypeHandle::EParamType::Int64:
	case FParamTypeHandle::EParamType::Float:
	case FParamTypeHandle::EParamType::Double:
	case FParamTypeHandle::EParamType::Name:
		break;
	case FParamTypeHandle::EParamType::String:
		{
			FString* String = reinterpret_cast<FString*>(InMemory.GetData());
			String->~FString();
		}
		break;
	case FParamTypeHandle::EParamType::Text:
		{
			FText* Text = reinterpret_cast<FText*>(InMemory.GetData());
			Text->~FText();
		}
		break;
	case FParamTypeHandle::EParamType::Vector:
	case FParamTypeHandle::EParamType::Vector4:
	case FParamTypeHandle::EParamType::Quat:
	case FParamTypeHandle::EParamType::Transform:
	case FParamTypeHandle::EParamType::Object:
	case FParamTypeHandle::EParamType::CharacterMovementComponent:
	case FParamTypeHandle::EParamType::AnimNextMeshComponent:
	case FParamTypeHandle::EParamType::AnimSequence:
	case FParamTypeHandle::EParamType::AnimNextGraphLODPose:
		break;
	case FParamTypeHandle::EParamType::AnimNextGraphReferencePose:
		{
			FAnimNextGraphReferencePose* RefPose = reinterpret_cast<FAnimNextGraphReferencePose*>(InMemory.GetData());
			RefPose->~FAnimNextGraphReferencePose();
		}
		break;
	case FParamTypeHandle::EParamType::Custom:
		{
			FAnimNextParamType Type = InTypeHandle.GetType();
			Destroy(Type, InMemory);
		}
		break;
	default:
		checkf(false, TEXT("Error: FParameterHelpers::Destroy of unknown type"));
		break;
	}
}

}