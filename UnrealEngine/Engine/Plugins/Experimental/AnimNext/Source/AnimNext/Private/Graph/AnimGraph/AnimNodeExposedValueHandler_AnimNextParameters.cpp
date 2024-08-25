// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimNodeExposedValueHandler_AnimNextParameters.h"
#include "Param/ParamStack.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSubsystem_PropertyAccess.h"
#include "Animation/AnimClassInterface.h"
#include "AnimGraphParamStackScope.h"

void FAnimNodeExposedValueHandler_AnimNextParameters::Initialize(const UClass* InClass)
{
	using namespace UE::AnimNext;

	Super::Initialize(InClass);

	if (const FAnimSubsystem_PropertyAccess* Subsystem = IAnimClassInterface::GetFromClass(InClass)->FindSubsystem<FAnimSubsystem_PropertyAccess>())
	{
		PropertyAccessLibrary = &Subsystem->GetLibrary();
	}

	for (FAnimNodeExposedValueHandler_AnimNextParameters_Entry& Entry : Entries)
	{
		Entry.ParamId = FParamId(Entry.ParameterName);
		Entry.PropertyParamTypeHandle = Entry.PropertyParamType.GetHandle();
	}
}

namespace UE::AnimNext::Private
{
	template <typename SourceType, typename DestinationType>
	static void CopyAndCastFloatingPointArray(
		const TArray<SourceType>& SourceArray,
		const FArrayProperty* DestinationArrayProperty,
		void* DestinationAddress)
	{
		checkSlow(DestinationArrayProperty);
		checkSlow(DestinationAddress);

		FScriptArrayHelper DestinationArrayHelper(DestinationArrayProperty, DestinationAddress);

		DestinationArrayHelper.Resize(SourceArray.Num());
		for (int32 i = 0; i < SourceArray.Num(); ++i)
		{
			const SourceType& SourceData = SourceArray[i];
			DestinationType* DestinationData = reinterpret_cast<DestinationType*>(DestinationArrayHelper.GetRawPtr(i));

			*DestinationData = static_cast<DestinationType>(SourceData);
		}
	}

	static void PerformCopy(EPropertyAccessCopyType InCopyType, const FProperty* InDestProperty, void* InDestAddr, const void* InSrcAddr)
	{
		switch (InCopyType)
		{
		case EPropertyAccessCopyType::None:
			break;
		case EPropertyAccessCopyType::Plain:
			checkSlow(InDestProperty->PropertyFlags & CPF_IsPlainOldData);
			FMemory::Memcpy(InDestAddr, InSrcAddr, InDestProperty->ElementSize);
			break;
		case EPropertyAccessCopyType::Complex:
			InDestProperty->CopyCompleteValue(InDestAddr, InSrcAddr);
			break;
		case EPropertyAccessCopyType::Bool:
			checkSlow(InDestProperty->IsA<FBoolProperty>());
			static_cast<const FBoolProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, *reinterpret_cast<const bool*>(InSrcAddr));
			break;
		case EPropertyAccessCopyType::Struct:
			checkSlow(InDestProperty->IsA<FStructProperty>());
			static_cast<const FStructProperty*>(InDestProperty)->Struct->CopyScriptStruct(InDestAddr, InSrcAddr);
			break;
		case EPropertyAccessCopyType::Name:
			checkSlow(InDestProperty->IsA<FNameProperty>());
			static_cast<const FNameProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, *reinterpret_cast<const FName*>(InSrcAddr));
			break;
		case EPropertyAccessCopyType::Object:
			checkSlow(InDestProperty->IsA<FObjectPropertyBase>());
			static_cast<const FObjectPropertyBase*>(InDestProperty)->SetObjectPropertyValue(InDestAddr, *reinterpret_cast<UObject* const*>(InSrcAddr));
			break;
		case EPropertyAccessCopyType::Array:
		{
			checkSlow(InDestProperty->IsA<FArrayProperty>());
			const FArrayProperty* ArrayProperty = static_cast<const FArrayProperty*>(InDestProperty);
			FScriptArrayHelper SourceArrayHelper(ArrayProperty, InSrcAddr);
			FScriptArrayHelper DestArrayHelper(ArrayProperty, InDestAddr);

			// Copy the minimum number of elements to the destination array without resizing
			const int32 MinSize = FMath::Min(SourceArrayHelper.Num(), DestArrayHelper.Num());
			for (int32 ElementIndex = 0; ElementIndex < MinSize; ++ElementIndex)
			{
				ArrayProperty->Inner->CopySingleValue(DestArrayHelper.GetRawPtr(ElementIndex), SourceArrayHelper.GetRawPtr(ElementIndex));
			}
			break;
		}
		case EPropertyAccessCopyType::PromoteBoolToByte:
			checkSlow(InDestProperty->IsA<FByteProperty>());
			static_cast<const FByteProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (uint8)(*reinterpret_cast<const bool*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteBoolToInt32:
			checkSlow(InDestProperty->IsA<FIntProperty>());
			static_cast<const FIntProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (int32)(*reinterpret_cast<const bool*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteBoolToInt64:
			checkSlow(InDestProperty->IsA<FInt64Property>());
			static_cast<const FInt64Property*>(InDestProperty)->SetPropertyValue(InDestAddr, (int64)(*reinterpret_cast<const bool*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteBoolToFloat:
			checkSlow(InDestProperty->IsA<FFloatProperty>());
			static_cast<const FFloatProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (float)(*reinterpret_cast<const bool*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteBoolToDouble:
			checkSlow(InDestProperty->IsA<FDoubleProperty>());
			static_cast<const FDoubleProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (double)(*reinterpret_cast<const bool*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteByteToInt32:
			checkSlow(InDestProperty->IsA<FIntProperty>());
			static_cast<const FIntProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (int32)(*reinterpret_cast<const uint8*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteByteToInt64:
			checkSlow(InDestProperty->IsA<FInt64Property>());
			static_cast<const FInt64Property*>(InDestProperty)->SetPropertyValue(InDestAddr, (int64)(*reinterpret_cast<const uint8*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteByteToFloat:
			checkSlow(InDestProperty->IsA<FFloatProperty>());
			static_cast<const FFloatProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (float)(*reinterpret_cast<const uint8*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteByteToDouble:
			checkSlow(InDestProperty->IsA<FDoubleProperty>());
			static_cast<const FDoubleProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (double)(*reinterpret_cast<const uint8*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteInt32ToInt64:
			checkSlow(InDestProperty->IsA<FInt64Property>());
			static_cast<const FInt64Property*>(InDestProperty)->SetPropertyValue(InDestAddr, (int64)(*reinterpret_cast<const int32*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteInt32ToFloat:
			checkSlow(InDestProperty->IsA<FFloatProperty>());
			static_cast<const FFloatProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (float)(*reinterpret_cast<const int32*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteInt32ToDouble:
			checkSlow(InDestProperty->IsA<FDoubleProperty>());
			static_cast<const FDoubleProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (double)(*reinterpret_cast<const int32*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteFloatToDouble:
			checkSlow(InDestProperty->IsA<FDoubleProperty>());
			static_cast<const FDoubleProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (double)(*reinterpret_cast<const float*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::DemoteDoubleToFloat:
			checkSlow(InDestProperty->IsA<FFloatProperty>());
			static_cast<const FFloatProperty*>(InDestProperty)->SetPropertyValue(InDestAddr, (float)(*reinterpret_cast<const double*>(InSrcAddr)));
			break;
		case EPropertyAccessCopyType::PromoteArrayFloatToDouble:
		{
			checkSlow(InDestProperty->IsA<FArrayProperty>());
			const FArrayProperty* DestArrayProperty = ExactCastField<const FArrayProperty>(InDestProperty);
			Private::CopyAndCastFloatingPointArray<float, double>(*reinterpret_cast<const TArray<float>*>(InSrcAddr), DestArrayProperty, InDestAddr);
			break;
		}
		case EPropertyAccessCopyType::DemoteArrayDoubleToFloat:
		{
			checkSlow(InDestProperty->IsA<FArrayProperty>());
			const FArrayProperty* DestArrayProperty = ExactCastField<const FArrayProperty>(InDestProperty);
			Private::CopyAndCastFloatingPointArray<double, float>(*reinterpret_cast<const TArray<double>*>(InSrcAddr), DestArrayProperty, InDestAddr);
			break;
		}
		// TODO: no map support in AnimNext parameter at present
		case EPropertyAccessCopyType::PromoteMapValueFloatToDouble:
		case EPropertyAccessCopyType::DemoteMapValueDoubleToFloat:
		default:
			check(false);
			break;
		}
	}

	static EPropertyAccessCopyType GetCopyTypeForCompatibility(EPropertyAccessCopyType InAccessType, const FParamTypeHandle& InDestTypeHandle, const FParamTypeHandle& InSrcTypeHandle)
	{
		switch (InSrcTypeHandle.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Bool:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Bool:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Byte:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Byte:
				return InAccessType;
			case FParamTypeHandle::EParamType::Int32:
				return EPropertyAccessCopyType::PromoteByteToInt32;
			case FParamTypeHandle::EParamType::Int64:
				return EPropertyAccessCopyType::PromoteByteToInt64;
			case FParamTypeHandle::EParamType::Float:
				return EPropertyAccessCopyType::PromoteByteToFloat;
			case FParamTypeHandle::EParamType::Double:
				return EPropertyAccessCopyType::PromoteByteToDouble;
			}
			break;
		case FParamTypeHandle::EParamType::Int32:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Byte:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int32:
				return InAccessType;
			case FParamTypeHandle::EParamType::Int64:
				return EPropertyAccessCopyType::PromoteInt32ToInt64;
			case FParamTypeHandle::EParamType::Float:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Double:
				return EPropertyAccessCopyType::PromoteInt32ToDouble;
			}
			break;
		case FParamTypeHandle::EParamType::Int64:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Byte:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int32:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int64:
				return InAccessType;
			case FParamTypeHandle::EParamType::Float:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Double:
				return EPropertyAccessCopyType::None;
			}
			break;
		case FParamTypeHandle::EParamType::Float:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Byte:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int32:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int64:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Float:
				return InAccessType;
			case FParamTypeHandle::EParamType::Double:
				return EPropertyAccessCopyType::PromoteFloatToDouble;
			}
			break;
		case FParamTypeHandle::EParamType::Double:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Byte:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int32:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Int64:
				return EPropertyAccessCopyType::None;
			case FParamTypeHandle::EParamType::Float:
				return EPropertyAccessCopyType::DemoteDoubleToFloat;
			case FParamTypeHandle::EParamType::Double:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Name:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Name:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::String:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::String:
				return InAccessType;;
			}
			break;
		case FParamTypeHandle::EParamType::Text:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Text:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Vector:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Vector:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Vector4:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Vector4:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Quat:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Quat:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Transform:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Transform:
				return InAccessType;
			}
			break;
		case FParamTypeHandle::EParamType::Custom:
			switch (InDestTypeHandle.GetParameterType())
			{
			case FParamTypeHandle::EParamType::Custom:
				{
					FAnimNextParamType::EValueType ValueTypeDest, ValueTypeSrc;
					FAnimNextParamType::EContainerType ContainerTypeDest, ContainerTypeSrc;
					const UObject* ValueTypeObjectDest, * ValueTypeObjectSrc;

					InDestTypeHandle.GetCustomTypeInfo(ValueTypeDest, ContainerTypeDest, ValueTypeObjectDest);
					InSrcTypeHandle.GetCustomTypeInfo(ValueTypeSrc, ContainerTypeSrc, ValueTypeObjectSrc);

					if (ContainerTypeDest == ContainerTypeSrc)
					{
						if (ContainerTypeDest == FAnimNextParamType::EContainerType::Array)
						{
							if (ValueTypeDest != ValueTypeSrc)
							{
								switch (ValueTypeDest)
								{
								case FAnimNextParamType::EValueType::Float:
									if (ValueTypeSrc == FAnimNextParamType::EValueType::Double)
									{
										return EPropertyAccessCopyType::DemoteArrayDoubleToFloat;
									}
									break;
								case FAnimNextParamType::EValueType::Double:
									if (ValueTypeSrc == FAnimNextParamType::EValueType::Float)
									{
										return EPropertyAccessCopyType::PromoteArrayFloatToDouble;
									}
									break;
								case FAnimNextParamType::EValueType::Struct:
								case FAnimNextParamType::EValueType::Object:
								case FAnimNextParamType::EValueType::SoftObject:
								case FAnimNextParamType::EValueType::Class:
								case FAnimNextParamType::EValueType::SoftClass:
									if (ValueTypeObjectDest == ValueTypeObjectSrc)
									{
										return InAccessType;
									}
									break;
								}
							}
							else
							{
								switch (ValueTypeDest)
								{
								case FAnimNextParamType::EValueType::Bool:
								case FAnimNextParamType::EValueType::Byte:
								case FAnimNextParamType::EValueType::Int32:
								case FAnimNextParamType::EValueType::Int64:
								case FAnimNextParamType::EValueType::Float:
								case FAnimNextParamType::EValueType::Double:
								case FAnimNextParamType::EValueType::Name:
								case FAnimNextParamType::EValueType::String:
								case FAnimNextParamType::EValueType::Text:
									return InAccessType;
								case FAnimNextParamType::EValueType::Enum:
								case FAnimNextParamType::EValueType::Struct:
								case FAnimNextParamType::EValueType::Object:
								case FAnimNextParamType::EValueType::SoftObject:
								case FAnimNextParamType::EValueType::Class:
								case FAnimNextParamType::EValueType::SoftClass:
									if (ValueTypeObjectDest == ValueTypeObjectSrc)
									{
										return InAccessType;
									}
								}
							}
						}
						else
						{
							if (ValueTypeDest == ValueTypeSrc)
							{
								switch (ValueTypeDest)
								{
								case FAnimNextParamType::EValueType::Enum:
									if (ValueTypeObjectDest == ValueTypeObjectSrc)
									{
										return InAccessType;
									}
									break;
								case FAnimNextParamType::EValueType::Struct:
									if (ValueTypeObjectDest == ValueTypeObjectSrc)
									{
										return InAccessType;
									}
									else if (CastChecked<UScriptStruct>(ValueTypeObjectDest)->IsChildOf(CastChecked<UScriptStruct>(ValueTypeObjectSrc)))
									{
										return InAccessType;
									}
									break;
								case FAnimNextParamType::EValueType::Object:
								case FAnimNextParamType::EValueType::SoftObject:
								case FAnimNextParamType::EValueType::Class:
								case FAnimNextParamType::EValueType::SoftClass:
									if (ValueTypeObjectDest == ValueTypeObjectSrc)
									{
										return InAccessType;
									}
									else if (CastChecked<UClass>(ValueTypeObjectDest)->IsChildOf(CastChecked<UClass>(ValueTypeObjectSrc)))
									{
										return InAccessType;
									}
									break;
								}
							}
						}
					}
				}
				break;
			}
			break;
		}

		return EPropertyAccessCopyType::None;
	}
}


void FAnimNodeExposedValueHandler_AnimNextParameters::Execute(const FAnimationBaseContext& InContext) const
{
	using namespace UE::AnimNext;

	Super::Execute(InContext);

	{
		FAnimGraphParamStackScope Scope(InContext);

		FParamStack& ParamStack = FParamStack::Get();
		UObject* Object = InContext.GetAnimInstanceObject();

		for (const FAnimNodeExposedValueHandler_AnimNextParameters_Entry& Entry : Entries)
		{
			EPropertyAccessCopyType CopyType = Entry.AccessType;
			FParamTypeHandle FoundTypeHandle;
			TConstArrayView<uint8> Value;
			FParamResult Result = ParamStack.GetParamData(Entry.ParamId, Entry.PropertyParamTypeHandle, Value, FoundTypeHandle, FParamCompatibility::IncompatibleWithDataLoss());
			if (Result.IsOfCompatibleType())
			{
				// Compatibility indicates we need to potentially modify the access type
				CopyType = Private::GetCopyTypeForCompatibility(CopyType, Entry.PropertyParamTypeHandle, FoundTypeHandle);
			}

			if(Result.IsSuccessful())
			{
				const uint8* SrcAddr = Value.GetData();

				PropertyAccess::GetAccessAddress(Object, *PropertyAccessLibrary, Entry.AccessIndex, [CopyType, SrcAddr](const FProperty* InProperty, void* InAddress)
				{
					Private::PerformCopy(CopyType, InProperty, InAddress, SrcAddr);
				});
			}
		}
	}
}