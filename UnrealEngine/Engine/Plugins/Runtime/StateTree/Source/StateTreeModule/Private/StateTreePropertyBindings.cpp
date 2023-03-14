// Copyright Epic Games, Inc. All Rights Reserved.
#include "StateTreePropertyBindings.h"
#include "StateTreeTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreePropertyBindings)

//----------------------------------------------------------------//
//  FStateTreePropertyBindings
//----------------------------------------------------------------//

void FStateTreePropertyBindings::Reset()
{
	SourceStructs.Reset();
	CopyBatches.Reset();
	PropertyBindings.Reset();
	PropertySegments.Reset();
	PropertyCopies.Reset();
	PropertyIndirections.Reset();
	bBindingsResolved = false;
}

bool FStateTreePropertyBindings::ResolvePaths()
{
	PropertyIndirections.Reset();
	PropertyCopies.SetNum(PropertyBindings.Num());

	bBindingsResolved = true;

	bool bResult = true;
	
	for (const FStateTreePropCopyBatch& Batch : CopyBatches)
	{
		for (int32 i = Batch.BindingsBegin; i != Batch.BindingsEnd; i++)
		{
			const FStateTreePropertyBinding& Binding = PropertyBindings[i];
			FStateTreePropCopy& Copy = PropertyCopies[i];

			Copy.SourceStructIndex = Binding.SourceStructIndex;
			Copy.Type = Binding.CopyType;

			const UStruct* SourceStruct = SourceStructs[Binding.SourceStructIndex.Get()].Struct;
			const UStruct* TargetStruct = Batch.TargetStruct.Struct;
			if (!SourceStruct || !TargetStruct)
			{
				Copy.Type = EStateTreePropertyCopyType::None;
				bBindingsResolved = false;
				bResult = false;
				continue;
			}

			// Resolve paths and validate the copy. Stops on first failure.
			bool bSuccess = true;
			bSuccess = bSuccess && ResolvePath(SourceStruct, Binding.SourcePath, Copy.SourceIndirection, Copy.SourceLeafProperty);
			bSuccess = bSuccess && ResolvePath(TargetStruct, Binding.TargetPath, Copy.TargetIndirection, Copy.TargetLeafProperty);
			bSuccess = bSuccess && ValidateCopy(Copy);
			if (!bSuccess)
			{
				// Resolving or validating failed, make the copy a nop.
				Copy.Type = EStateTreePropertyCopyType::None;
				bResult = false;
			}
		}
	}

	return bResult;
}

bool FStateTreePropertyBindings::ResolvePath(const UStruct* Struct, const FStateTreePropertySegment& FirstPathSegment, FStateTreePropertyIndirection& OutFirstIndirection, const FProperty*& OutLeafProperty)
{
	if (!Struct)
	{
		UE_LOG(LogStateTree, Error, TEXT("%s: '%s' Invalid source struct."), ANSI_TO_TCHAR(__FUNCTION__), *GetPathAsString(FirstPathSegment));
		return false;
	}

	const UStruct* CurrentStruct = Struct;
	const FProperty* LeafProperty = nullptr;
	TArray<FStateTreePropertyIndirection, TInlineAllocator<16>> TempIndirections;
	const FStateTreePropertySegment* Segment = &FirstPathSegment;

	while (Segment != nullptr && !Segment->IsEmpty())
	{
		FStateTreePropertyIndirection& Indirection = TempIndirections.AddDefaulted_GetRef();

		const bool bFinalSegment = Segment->NextIndex.IsValid() == false;;

		if (!ensure(CurrentStruct))
		{
			UE_LOG(LogStateTree, Error, TEXT("%s: '%s' Invalid struct."), ANSI_TO_TCHAR(__FUNCTION__), *GetPathAsString(FirstPathSegment, Segment, TEXT("<"), TEXT(">")));
			return false;
		}
		FProperty* Property = CurrentStruct->FindPropertyByName(Segment->Name);
		if (!Property)
		{
			// TODO: use core redirects to fix up the name.
			UE_LOG(LogStateTree, Error, TEXT("%s: Malformed path '%s', could not to find property '%s%s.%s'."),
				ANSI_TO_TCHAR(__FUNCTION__), *GetPathAsString(FirstPathSegment, Segment, TEXT("<"), TEXT(">")),
				CurrentStruct->GetPrefixCPP(), *CurrentStruct->GetName(), *Segment->Name.ToString());
			return false;
		}
		Indirection.ArrayIndex = FStateTreeIndex16(Segment->ArrayIndex.IsValid() ? Segment->ArrayIndex.Get() : 0);
		Indirection.Type = Segment->Type;
		Indirection.Offset = Property->GetOffset_ForInternal() + Property->ElementSize * Indirection.ArrayIndex.Get();

		// Check to see if it is an array access first.
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (const FStructProperty* ArrayOfStructsProperty = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				CurrentStruct = ArrayOfStructsProperty->Struct;
			}
			else if (const FObjectPropertyBase* ArrayOfObjectsProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
			{
				CurrentStruct = ArrayOfObjectsProperty->PropertyClass;
			}
			Indirection.ArrayProperty = ArrayProperty;
		}
		// Leaf segments all get treated the same, plain, struct or object
		else if (bFinalSegment)
		{
			CurrentStruct = nullptr;
		}
		// Check to see if this is a simple structure (eg. not an array of structures)
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			CurrentStruct = StructProperty->Struct;
		}
		// Check to see if this is a simple object (eg. not an array of objects)
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			CurrentStruct = ObjectProperty->PropertyClass;
		}
		// Check to see if this is a simple weak object property (eg. not an array of weak objects).
		else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
		{
			CurrentStruct = WeakObjectProperty->PropertyClass;
		}
		// Check to see if this is a simple soft object property (eg. not an array of soft objects).
		else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		{
			CurrentStruct = SoftObjectProperty->PropertyClass;
		}
		else
		{
			UE_LOG(LogStateTree, Error, TEXT("%s: Unsupported segment %s in path '%s'."),
			    ANSI_TO_TCHAR(__FUNCTION__), *Property->GetCPPType(), *GetPathAsString(FirstPathSegment, Segment, TEXT("<"), TEXT(">")));
		}

		if (bFinalSegment)
		{
			LeafProperty = Property;
		}

		Segment = Segment->NextIndex.IsValid() ? &PropertySegments[Segment->NextIndex.Get()] : nullptr;
	}

	if (TempIndirections.Num() > 0)
	{
		// Collapse adjacent offset indirections
		for (int32 Index = 0; Index < TempIndirections.Num(); Index++)
		{
			FStateTreePropertyIndirection& Indirection = TempIndirections[Index];
			if (Indirection.Type == EStateTreePropertyAccessType::Offset && (Index + 1) < TempIndirections.Num())
			{
				const FStateTreePropertyIndirection& NextIndirection = TempIndirections[Index + 1];
				if (NextIndirection.Type == EStateTreePropertyAccessType::Offset)
				{
					Indirection.Offset += NextIndirection.Offset;
					TempIndirections.RemoveAt(Index + 1);
					Index--;
				}
			}
		}


		// Store indirections
		OutFirstIndirection = TempIndirections[0];
		FStateTreePropertyIndirection* PrevIndirection = &OutFirstIndirection;
		for (int32 Index = 1; Index < TempIndirections.Num(); Index++)
		{
			const int32 IndirectionIndex = PropertyIndirections.Num();
			FStateTreePropertyIndirection& NewIndirection = PropertyIndirections.Add_GetRef(TempIndirections[Index]);
			PrevIndirection->NextIndex = FStateTreeIndex16(IndirectionIndex);
			PrevIndirection = &NewIndirection;
		}
	}
	else
	{
		// Indirections can be empty in case we're directly binding to source structs.
		// Zero offset will return the struct itself.
		OutFirstIndirection.Offset = 0;
		OutFirstIndirection.Type = EStateTreePropertyAccessType::Offset;
	}
	
	OutLeafProperty = LeafProperty;

	return true;
}

bool FStateTreePropertyBindings::ValidateCopy(FStateTreePropCopy& Copy) const
{
	const FProperty* SourceProperty = Copy.SourceLeafProperty;
	const FProperty* TargetProperty = Copy.TargetLeafProperty;

	if (!TargetProperty)
	{
		return false;
	}

	// If source property is nullptr, we're binding directly to the source.
	if (SourceProperty == nullptr)
	{
		return TargetProperty->IsA<FStructProperty>() || TargetProperty->IsA<FObjectPropertyBase>();
	}

	// Extract underlying types for enums
	if (const FEnumProperty* EnumPropertyA = CastField<const FEnumProperty>(SourceProperty))
	{
		SourceProperty = EnumPropertyA->GetUnderlyingProperty();
	}

	if (const FEnumProperty* EnumPropertyB = CastField<const FEnumProperty>(TargetProperty))
	{
		TargetProperty = EnumPropertyB->GetUnderlyingProperty();
	}

	bool bResult = true;
	switch (Copy.Type)
	{
	case EStateTreePropertyCopyType::CopyPlain:
		Copy.CopySize = SourceProperty->ElementSize * SourceProperty->ArrayDim;
		bResult = (SourceProperty->PropertyFlags & CPF_IsPlainOldData) != 0 && (TargetProperty->PropertyFlags & CPF_IsPlainOldData) != 0;
		break;
	case EStateTreePropertyCopyType::CopyComplex:
		bResult = true;
		break;
	case EStateTreePropertyCopyType::CopyBool:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FBoolProperty>();
		break;
	case EStateTreePropertyCopyType::CopyStruct:
		bResult = SourceProperty->IsA<FStructProperty>() && TargetProperty->IsA<FStructProperty>();
		break;
	case EStateTreePropertyCopyType::CopyObject:
		bResult = SourceProperty->IsA<FObjectPropertyBase>() && TargetProperty->IsA<FObjectPropertyBase>();
		break;
	case EStateTreePropertyCopyType::CopyName:
		bResult = SourceProperty->IsA<FNameProperty>() && TargetProperty->IsA<FNameProperty>();
		break;
	case EStateTreePropertyCopyType::CopyFixedArray:
		bResult = SourceProperty->IsA<FArrayProperty>() && TargetProperty->IsA<FArrayProperty>();
		break;
	case EStateTreePropertyCopyType::StructReference:
	{
		const FStructProperty* SourceStructProperty = CastField<const FStructProperty>(SourceProperty);
		const FStructProperty* TargetStructProperty = CastField<const FStructProperty>(TargetProperty);

		bResult = SourceStructProperty != nullptr
			&& TargetStructProperty != nullptr
			&& SourceStructProperty->Struct != TBaseStructure<FStateTreeStructRef>::Get()
			&& TargetStructProperty->Struct == TBaseStructure<FStateTreeStructRef>::Get();
		break;
	}

	// Bool promotions		
	case EStateTreePropertyCopyType::PromoteBoolToByte:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FByteProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteBoolToInt32:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FIntProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteBoolToUInt32:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FUInt32Property>();
		break;
	case EStateTreePropertyCopyType::PromoteBoolToInt64:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FInt64Property>();
		break;
	case EStateTreePropertyCopyType::PromoteBoolToFloat:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FFloatProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteBoolToDouble:
		bResult = SourceProperty->IsA<FBoolProperty>() && TargetProperty->IsA<FDoubleProperty>();
		break;

	// Byte promotions
	case EStateTreePropertyCopyType::PromoteByteToInt32:
		bResult = SourceProperty->IsA<FByteProperty>() && TargetProperty->IsA<FIntProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteByteToUInt32:
		bResult = SourceProperty->IsA<FByteProperty>() && TargetProperty->IsA<FUInt32Property>();
		break;
	case EStateTreePropertyCopyType::PromoteByteToInt64:
		bResult = SourceProperty->IsA<FByteProperty>() && TargetProperty->IsA<FInt64Property>();
		break;
	case EStateTreePropertyCopyType::PromoteByteToFloat:
		bResult = SourceProperty->IsA<FByteProperty>() && TargetProperty->IsA<FFloatProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteByteToDouble:
		bResult = SourceProperty->IsA<FByteProperty>() && TargetProperty->IsA<FDoubleProperty>();
		break;

	// Int32 promotions
	case EStateTreePropertyCopyType::PromoteInt32ToInt64:
		bResult = SourceProperty->IsA<FIntProperty>() && TargetProperty->IsA<FInt64Property>();
		break;
	case EStateTreePropertyCopyType::PromoteInt32ToFloat:
		bResult = SourceProperty->IsA<FIntProperty>() && TargetProperty->IsA<FFloatProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteInt32ToDouble:
		bResult = SourceProperty->IsA<FIntProperty>() && TargetProperty->IsA<FDoubleProperty>();
		break;

	// Uint32 promotions
	case EStateTreePropertyCopyType::PromoteUInt32ToInt64:
		bResult = SourceProperty->IsA<FUInt32Property>() && TargetProperty->IsA<FInt64Property>();
		break;
	case EStateTreePropertyCopyType::PromoteUInt32ToFloat:
		bResult = SourceProperty->IsA<FUInt32Property>() && TargetProperty->IsA<FFloatProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteUInt32ToDouble:
		bResult = SourceProperty->IsA<FUInt32Property>() && TargetProperty->IsA<FDoubleProperty>();
		break;

	// Float promotions
	case EStateTreePropertyCopyType::PromoteFloatToInt32:
		bResult = SourceProperty->IsA<FFloatProperty>() && TargetProperty->IsA<FIntProperty>();
		break;
	case EStateTreePropertyCopyType::PromoteFloatToInt64:
		bResult = SourceProperty->IsA<FFloatProperty>() && TargetProperty->IsA<FInt64Property>();
		break;
	case EStateTreePropertyCopyType::PromoteFloatToDouble:
		bResult = SourceProperty->IsA<FFloatProperty>() && TargetProperty->IsA<FDoubleProperty>();
		break;

	// Double promotions
	case EStateTreePropertyCopyType::DemoteDoubleToInt32:
		bResult = SourceProperty->IsA<FDoubleProperty>() && TargetProperty->IsA<FIntProperty>();
		break;
	case EStateTreePropertyCopyType::DemoteDoubleToInt64:
		bResult = SourceProperty->IsA<FDoubleProperty>() && TargetProperty->IsA<FInt64Property>();
		break;
	case EStateTreePropertyCopyType::DemoteDoubleToFloat:
		bResult = SourceProperty->IsA<FDoubleProperty>() && TargetProperty->IsA<FFloatProperty>();
		break;
	default:
		UE_LOG(LogStateTree, Error, TEXT("FStateTreePropertyBindings::ValidateCopy: Unhandled copy type %s between '%s' and '%s'"),
			*StaticEnum<EStateTreePropertyCopyType>()->GetValueAsString(Copy.Type), *SourceProperty->GetNameCPP(), *TargetProperty->GetNameCPP());
		bResult = false;
		break;
	}

	UE_CLOG(!bResult, LogStateTree, Error, TEXT("FStateTreePropertyBindings::ValidateCopy: Failed to validate copy type %s between '%s' and '%s'"),
		*StaticEnum<EStateTreePropertyCopyType>()->GetValueAsString(Copy.Type), *SourceProperty->GetNameCPP(), *TargetProperty->GetNameCPP());
	
	return bResult;
}

uint8* FStateTreePropertyBindings::GetAddress(FStateTreeDataView InStructView, const FStateTreePropertyIndirection& FirstIndirection, const FProperty* LeafProperty) const
{
	uint8* Address = InStructView.GetMutableMemory();
	if (Address == nullptr)
	{
		return nullptr;
	}

	const FStateTreePropertyIndirection* Indirection = &FirstIndirection;

	while (Indirection != nullptr && Address != nullptr)
	{
		switch (Indirection->Type)
		{
		case EStateTreePropertyAccessType::Offset:
		{
			Address = Address + Indirection->Offset;
			break;
		}
		case EStateTreePropertyAccessType::Object:
		{
			UObject* Object = *reinterpret_cast<UObject**>(Address + Indirection->Offset);
			Address = reinterpret_cast<uint8*>(Object);
			break;
		}
		case EStateTreePropertyAccessType::WeakObject:
		{
			TWeakObjectPtr<UObject>& WeakObjectPtr = *reinterpret_cast<TWeakObjectPtr<UObject>*>(Address + Indirection->Offset);
			UObject* Object = WeakObjectPtr.Get();
			Address = reinterpret_cast<uint8*>(Object);
			break;
		}
		case EStateTreePropertyAccessType::SoftObject:
		{
			FSoftObjectPtr& SoftObjectPtr = *reinterpret_cast<FSoftObjectPtr*>(Address + Indirection->Offset);
			UObject* Object = SoftObjectPtr.Get();
			Address = reinterpret_cast<uint8*>(Object);
			break;
		}
		case EStateTreePropertyAccessType::IndexArray:
		{
			check(Indirection->ArrayProperty);
			FScriptArrayHelper Helper(Indirection->ArrayProperty, Address + Indirection->Offset);
			if (Helper.IsValidIndex(Indirection->ArrayIndex.Get()))
			{
				Address = reinterpret_cast<uint8*>(Helper.GetRawPtr(Indirection->ArrayIndex.Get()));
			}
			else
			{
				Address = nullptr;
			}
			break;
		}
		default:
			ensureMsgf(false, TEXT("FStateTreePropertyBindings::GetAddress: Unhandled indirection type %s for '%s'"),
				*StaticEnum<EStateTreePropertyAccessType>()->GetValueAsString(Indirection->Type), *LeafProperty->GetNameCPP());
		}

		Indirection = Indirection->NextIndex.IsValid() ? &PropertyIndirections[Indirection->NextIndex.Get()] : nullptr;
	}

	return Address;
}

void FStateTreePropertyBindings::PerformCopy(const FStateTreePropCopy& Copy, uint8* SourceAddress, uint8* TargetAddress) const
{
	// Source property can be null
	check(SourceAddress);
	check(Copy.TargetLeafProperty);
	check(TargetAddress);

	switch (Copy.Type)
	{
	case EStateTreePropertyCopyType::CopyPlain:
		FMemory::Memcpy(TargetAddress, SourceAddress, Copy.CopySize);
		break;
	case EStateTreePropertyCopyType::CopyComplex:
		Copy.TargetLeafProperty->CopyCompleteValue(TargetAddress, SourceAddress);
		break;
	case EStateTreePropertyCopyType::CopyBool:
		static_cast<const FBoolProperty*>(Copy.TargetLeafProperty)->SetPropertyValue(TargetAddress, static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress));
		break;
	case EStateTreePropertyCopyType::CopyStruct:
		// If SourceProperty == nullptr (pointing to the struct source directly), the GetAddress() did the right thing and is pointing the the beginning of the struct. 
		static_cast<const FStructProperty*>(Copy.TargetLeafProperty)->Struct->CopyScriptStruct(TargetAddress, SourceAddress);
		break;
	case EStateTreePropertyCopyType::CopyObject:
		if (Copy.SourceLeafProperty == nullptr)
		{
			// Source is pointing at object directly.
			static_cast<const FObjectPropertyBase*>(Copy.TargetLeafProperty)->SetObjectPropertyValue(TargetAddress, (UObject*)SourceAddress);
		}
		else
		{
			static_cast<const FObjectPropertyBase*>(Copy.TargetLeafProperty)->SetObjectPropertyValue(TargetAddress, static_cast<const FObjectPropertyBase*>(Copy.SourceLeafProperty)->GetObjectPropertyValue(SourceAddress));
		}
		break;
	case EStateTreePropertyCopyType::CopyName:
		static_cast<const FNameProperty*>(Copy.TargetLeafProperty)->SetPropertyValue(TargetAddress, static_cast<const FNameProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress));
		break;
	case EStateTreePropertyCopyType::CopyFixedArray:
	{
		// Copy into fixed sized array (EditFixedSize). Resizable arrays are copied as Complex, and regular fixed sizes arrays via the regular copies (dim specifies array size).
		const FArrayProperty* SourceArrayProperty = static_cast<const FArrayProperty*>(Copy.SourceLeafProperty);
		const FArrayProperty* TargetArrayProperty = static_cast<const FArrayProperty*>(Copy.TargetLeafProperty);
		FScriptArrayHelper SourceArrayHelper(SourceArrayProperty, SourceAddress);
		FScriptArrayHelper TargetArrayHelper(TargetArrayProperty, TargetAddress);
			
		const int32 MinSize = FMath::Min(SourceArrayHelper.Num(), TargetArrayHelper.Num());
		for (int32 ElementIndex = 0; ElementIndex < MinSize; ++ElementIndex)
		{
			TargetArrayProperty->Inner->CopySingleValue(TargetArrayHelper.GetRawPtr(ElementIndex), SourceArrayHelper.GetRawPtr(ElementIndex));
		}
		break;
	}
	case EStateTreePropertyCopyType::StructReference:
	{
		const FStructProperty* SourceStructProperty = static_cast<const FStructProperty*>(Copy.SourceLeafProperty);
		FStateTreeStructRef* Target = (FStateTreeStructRef*)TargetAddress;
		Target->Set(FStructView(SourceStructProperty->Struct, SourceAddress));
		break;
	}
	// Bool promotions
	case EStateTreePropertyCopyType::PromoteBoolToByte:
		*reinterpret_cast<uint8*>(TargetAddress) = (uint8)static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteBoolToInt32:
		*reinterpret_cast<int32*>(TargetAddress) = (int32)static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteBoolToUInt32:
		*reinterpret_cast<uint32*>(TargetAddress) = (uint32)static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteBoolToInt64:
		*reinterpret_cast<int64*>(TargetAddress) = (int64)static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteBoolToFloat:
		*reinterpret_cast<float*>(TargetAddress) = (float)static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteBoolToDouble:
		*reinterpret_cast<double*>(TargetAddress) = (double)static_cast<const FBoolProperty*>(Copy.SourceLeafProperty)->GetPropertyValue(SourceAddress);
		break;
		
	// Byte promotions	
	case EStateTreePropertyCopyType::PromoteByteToInt32:
		*reinterpret_cast<int32*>(TargetAddress) = (int32)*reinterpret_cast<const uint8*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteByteToUInt32:
		*reinterpret_cast<uint32*>(TargetAddress) = (uint32)*reinterpret_cast<const uint8*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteByteToInt64:
		*reinterpret_cast<int64*>(TargetAddress) = (int64)*reinterpret_cast<const uint8*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteByteToFloat:
		*reinterpret_cast<float*>(TargetAddress) = (float)*reinterpret_cast<const uint8*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteByteToDouble:
		*reinterpret_cast<double*>(TargetAddress) = (double)*reinterpret_cast<const uint8*>(SourceAddress);
		break;

	// Int32 promotions
	case EStateTreePropertyCopyType::PromoteInt32ToInt64:
		*reinterpret_cast<int64*>(TargetAddress) = (int64)*reinterpret_cast<const int32*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteInt32ToFloat:
		*reinterpret_cast<float*>(TargetAddress) = (float)*reinterpret_cast<const int32*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteInt32ToDouble:
		*reinterpret_cast<double*>(TargetAddress) = (double)*reinterpret_cast<const int32*>(SourceAddress);
		break;

	// Uint32 promotions
	case EStateTreePropertyCopyType::PromoteUInt32ToInt64:
		*reinterpret_cast<int64*>(TargetAddress) = (int64)*reinterpret_cast<const uint32*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteUInt32ToFloat:
		*reinterpret_cast<float*>(TargetAddress) = (float)*reinterpret_cast<const uint32*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteUInt32ToDouble:
		*reinterpret_cast<double*>(TargetAddress) = (double)*reinterpret_cast<const uint32*>(SourceAddress);
		break;

	// Float promotions
	case EStateTreePropertyCopyType::PromoteFloatToInt32:
		*reinterpret_cast<int32*>(TargetAddress) = (int32)*reinterpret_cast<const float*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteFloatToInt64:
		*reinterpret_cast<int64*>(TargetAddress) = (int64)*reinterpret_cast<const float*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::PromoteFloatToDouble:
		*reinterpret_cast<double*>(TargetAddress) = (double)*reinterpret_cast<const float*>(SourceAddress);
		break;

	// Double promotions
	case EStateTreePropertyCopyType::DemoteDoubleToInt32:
		*reinterpret_cast<int32*>(TargetAddress) = (int32)*reinterpret_cast<const double*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::DemoteDoubleToInt64:
		*reinterpret_cast<int64*>(TargetAddress) = (int64)*reinterpret_cast<const double*>(SourceAddress);
		break;
	case EStateTreePropertyCopyType::DemoteDoubleToFloat:
		*reinterpret_cast<float*>(TargetAddress) = (float)*reinterpret_cast<const double*>(SourceAddress);
		break;

	default:
		ensureMsgf(false, TEXT("FStateTreePropertyBindings::PerformCopy: Unhandled copy type %s between '%s' and '%s'"),
			*StaticEnum<EStateTreePropertyCopyType>()->GetValueAsString(Copy.Type), *Copy.SourceLeafProperty->GetNameCPP(), *Copy.TargetLeafProperty->GetNameCPP());
		break;
	}
}

bool FStateTreePropertyBindings::CopyTo(TConstArrayView<FStateTreeDataView> SourceStructViews, const FStateTreeIndex16 TargetBatchIndex, FStateTreeDataView TargetStructView) const
{
	// This is made ensure so that the programmers have the change to catch it (it's usually programming error not to call ResolvePaths(), and it wont spam log for others.
	if (!ensureMsgf(bBindingsResolved, TEXT("Bindings must be resolved successfully before copying. See ResolvePaths()")))
	{
		return false;
	}

	if (TargetBatchIndex.IsValid() == false)
	{
		return false;
	}

	check(CopyBatches.IsValidIndex(TargetBatchIndex.Get()));
	const FStateTreePropCopyBatch& Batch = CopyBatches[TargetBatchIndex.Get()];

	check(TargetStructView.IsValid());
	check(TargetStructView.GetStruct() == Batch.TargetStruct.Struct);

	bool bResult = true;
	
	for (int32 i = Batch.BindingsBegin; i != Batch.BindingsEnd; i++)
	{
		const FStateTreePropCopy& Copy = PropertyCopies[i];
		// Copies that fail to be resolved (i.e. property path does not resolve, types changed) will be marked as None, skip them.
		if (Copy.Type == EStateTreePropertyCopyType::None)
		{
			continue;
		}
		
		const FStateTreeDataView SourceStructView = SourceStructViews[Copy.SourceStructIndex.Get()];
		if (SourceStructView.IsValid())
		{
			check(SourceStructView.GetStruct() == SourceStructs[Copy.SourceStructIndex.Get()].Struct
				|| (SourceStructView.GetStruct() && SourceStructView.GetStruct()->IsChildOf(SourceStructs[Copy.SourceStructIndex.Get()].Struct)));
				
			uint8* SourceAddress = GetAddress(SourceStructView, Copy.SourceIndirection, Copy.SourceLeafProperty);
			uint8* TargetAddress = GetAddress(TargetStructView, Copy.TargetIndirection, Copy.TargetLeafProperty);
			
			check (SourceAddress != nullptr && TargetAddress != nullptr);
			PerformCopy(Copy, SourceAddress, TargetAddress);
		}
		else
		{
			bResult = false;
		}
	}

	return bResult;
}

void FStateTreePropertyBindings::DebugPrintInternalLayout(FString& OutString) const
{
	/** Array of expected source structs. */
	OutString += FString::Printf(TEXT("\nBindableStructDesc (%d)\n  [ %-40s | %-40s ]\n"), SourceStructs.Num(), TEXT("Type"), TEXT("Name"));
	for (const FStateTreeBindableStructDesc& BindableStructDesc : SourceStructs)
	{
		OutString += FString::Printf(TEXT("  | %-40s | %-40s |\n"),
									 BindableStructDesc.Struct ? *BindableStructDesc.Struct->GetName() : TEXT("null"),
									 *BindableStructDesc.Name.ToString());
	}

	/** Array of copy batches. */
	OutString += FString::Printf(TEXT("\nCopyBatches (%d)\n  [ %-40s | %-40s | %-8s [%-3s:%-3s[ ]\n"), CopyBatches.Num(),
		TEXT("Target Type"), TEXT("Target Name"), TEXT("Bindings"), TEXT("Beg"), TEXT("End"));
	for (const FStateTreePropCopyBatch& CopyBatch : CopyBatches)
	{
		OutString += FString::Printf(TEXT("  | %-40s | %-40s | %8s [%3d:%-3d[ |\n"),
									 CopyBatch.TargetStruct.Struct ? *CopyBatch.TargetStruct.Struct->GetName() : TEXT("null"),
									 *CopyBatch.TargetStruct.Name.ToString(),
									 TEXT(""), CopyBatch.BindingsBegin, CopyBatch.BindingsEnd);
	}

	/** Array of property bindings, resolved into arrays of copies before use. */
	OutString += FString::Printf(TEXT("\nPropertyBindings (%d)\n  [ %-20s | %-4s | %-4s | %-10s | %-20s | %-4s | %-7s | %-20s | %-7s | %-20s ]\n"),
		PropertyBindings.Num(),
		TEXT("Source Name"), TEXT("Arr#"), TEXT("Next"), TEXT("Access"),
		TEXT("Target Name"), TEXT("Arr#"), TEXT("Next"), TEXT("Access"),
		TEXT("Struct#"), TEXT("Copy Type"));
	for (const FStateTreePropertyBinding& PropertyBinding : PropertyBindings)
	{
		OutString += FString::Printf(TEXT("  | %-20s | %4d | %4d | %-10s | %-20s | %4d | %4d | %-10s | %7d | %-20s | \n"),
									 *PropertyBinding.SourcePath.Name.ToString(),
									 PropertyBinding.SourcePath.ArrayIndex.Get(),
									 PropertyBinding.SourcePath.NextIndex.Get(),
									 *UEnum::GetDisplayValueAsText(PropertyBinding.TargetPath.Type).ToString(),
									 *PropertyBinding.TargetPath.Name.ToString(),
									 PropertyBinding.TargetPath.ArrayIndex.Get(),
									 PropertyBinding.TargetPath.NextIndex.Get(),
									 *UEnum::GetDisplayValueAsText(PropertyBinding.TargetPath.Type).ToString(),
									 PropertyBinding.SourceStructIndex.Get(),
									 *UEnum::GetValueAsString(PropertyBinding.CopyType));
	}

	/** Array of property segments, indexed by property paths. */
	OutString += FString::Printf(TEXT("\nPropertySegments (%d)\n  [ %-20s | %4s | %4s | %-10s ]\n"), PropertySegments.Num(),
		TEXT("Name"), TEXT("Arr#"), TEXT("Next"), TEXT("Access"));
	for (const FStateTreePropertySegment& PropertySegment : PropertySegments)
	{
		OutString += FString::Printf(TEXT("  | %-20s | %4d | %4d | %-10s |\n"),
									 *PropertySegment.Name.ToString(),
									 PropertySegment.ArrayIndex.Get(),
									 PropertySegment.NextIndex.Get(),
									 *UEnum::GetDisplayValueAsText(PropertySegment.Type).ToString());
	}

	/** Array of property copies */
	OutString += FString::Printf(TEXT("\nPropertyCopies (%d)\n  [ %-7s | %-4s | %-4s | %-10s | %-7s | %-4s | %-4s | %-10s | %-7s | %-20s | %-4s ]\n"), PropertyCopies.Num(),
		TEXT("Src Idx"), TEXT("Off."), TEXT("Next"), TEXT("Type"),
		TEXT("Tgt Idx"), TEXT("Off."), TEXT("Next"), TEXT("Type"),
		TEXT("Struct"), TEXT("Copy Type"), TEXT("Size"));
	for (const FStateTreePropCopy& PropertyCopy : PropertyCopies)
	{
		OutString += FString::Printf(TEXT("  | %7d | %4d | %4d | %-10s | %7d | %4d | %4d | %-10s | %7d | %-20s | %4d |\n"),
					PropertyCopy.SourceIndirection.ArrayIndex.Get(),
					PropertyCopy.SourceIndirection.Offset,
					PropertyCopy.SourceIndirection.NextIndex.Get(),
					*UEnum::GetDisplayValueAsText(PropertyCopy.SourceIndirection.Type).ToString(),
					PropertyCopy.TargetIndirection.ArrayIndex.Get(),
					PropertyCopy.TargetIndirection.Offset,
					PropertyCopy.TargetIndirection.NextIndex.Get(),
					*UEnum::GetDisplayValueAsText(PropertyCopy.TargetIndirection.Type).ToString(),
					PropertyCopy.SourceStructIndex.Get(),
					*UEnum::GetDisplayValueAsText(PropertyCopy.Type).ToString(),
					PropertyCopy.CopySize);
	}

	/** Array of property indirections, indexed by accesses*/
	OutString += FString::Printf(TEXT("\nPropertyIndirections (%d)\n  [ %-4s | %-4s | %-4s | %-10s ] \n"), PropertyIndirections.Num(),
		TEXT("Idx"), TEXT("Off."), TEXT("Next"), TEXT("Access Type"));
	for (const FStateTreePropertyIndirection& PropertyIndirection : PropertyIndirections)
	{
		OutString += FString::Printf(TEXT("  | %4d | %4d | %4d | %-10s |\n"),
					PropertyIndirection.ArrayIndex.Get(),
					PropertyIndirection.Offset,
					PropertyIndirection.NextIndex.Get(),
					*UEnum::GetDisplayValueAsText(PropertyIndirection.Type).ToString());
	}
}

FString FStateTreePropertyBindings::GetPathAsString(const FStateTreePropertySegment& FirstPathSegment, const FStateTreePropertySegment* HighlightedSegment, const TCHAR* HighlightPrefix, const TCHAR* HighlightPostfix)
{
	FString Result;
	const FStateTreePropertySegment* Segment = &FirstPathSegment;
	while (Segment != nullptr)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT(".");
		}
		
		if (Segment == HighlightedSegment && HighlightPrefix)
		{
			Result += HighlightPrefix;
		}

		Result += Segment->Name.ToString();

		if (Segment == HighlightedSegment && HighlightPostfix)
		{
			Result += HighlightPostfix;
		}

		Segment = Segment->NextIndex.IsValid() ? &PropertySegments[Segment->NextIndex.Get()] : nullptr;
	}
	return Result;
}


//----------------------------------------------------------------//
//  FStateTreeEditorPropertyPath
//----------------------------------------------------------------//

FString FStateTreeEditorPropertyPath::ToString(const int32 HighlightedSegment, const TCHAR* HighlightPrefix, const TCHAR* HighlightPostfix) const
{
	FString Result;
	for (int32 i = 0; i < Path.Num(); i++)
	{
		if (i > 0)
		{
			Result += TEXT(".");
		}
		if (i == HighlightedSegment && HighlightPrefix)
		{
			Result += HighlightPrefix;
		}

		Result += Path[i];

		if (i == HighlightedSegment && HighlightPostfix)
		{
			Result += HighlightPostfix;
		}
	}
	return Result;
}

bool FStateTreeEditorPropertyPath::operator==(const FStateTreeEditorPropertyPath& RHS) const
{
	if (StructID != RHS.StructID)
	{
		return false;
	}

	if (Path.Num() != RHS.Path.Num())
	{
		return false;
	}

	for (int32 i = 0; i < Path.Num(); i++)
	{
		if (Path[i] != RHS.Path[i])
		{
			return false;
		}
	}

	return true;
}

