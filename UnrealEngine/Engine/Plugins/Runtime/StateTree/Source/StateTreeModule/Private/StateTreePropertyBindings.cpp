// Copyright Epic Games, Inc. All Rights Reserved.
#include "StateTreePropertyBindings.h"
#include "UObject/EnumProperty.h"
#include "Misc/EnumerateRange.h"
#include "PropertyPathHelpers.h"
#include "PropertyBag.h"
#include "StateTreePropertyRef.h"

#if WITH_EDITOR
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UObject/Field.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreePropertyBindings)

namespace UE::StateTree
{
	FString GetDescAndPathAsString(const FStateTreeBindableStructDesc& Desc, const FStateTreePropertyPath& Path)
	{
		FStringBuilderBase Result;

		Result += Desc.ToString();

		if (!Path.IsPathEmpty())
		{
			Result += TEXT(" ");
			Result += Path.ToString();
		}

		return Result.ToString();
	}

#if WITH_EDITOR
	EStateTreePropertyUsage GetUsageFromMetaData(const FProperty* Property)
	{
		static const FName CategoryName(TEXT("Category"));

		if (Property == nullptr)
		{
			return EStateTreePropertyUsage::Invalid;
		}
		
		const FString Category = Property->GetMetaData(CategoryName);

		if (Category == TEXT("Input"))
		{
			return EStateTreePropertyUsage::Input;
		}
		if (Category == TEXT("Inputs"))
		{
			return EStateTreePropertyUsage::Input;
		}
		if (Category == TEXT("Output"))
		{
			return EStateTreePropertyUsage::Output;
		}
		if (Category == TEXT("Outputs"))
		{
			return EStateTreePropertyUsage::Output;
		}
		if (Category == TEXT("Context"))
		{
			return EStateTreePropertyUsage::Context;
		}

		return EStateTreePropertyUsage::Parameter;
	}
#endif

} // UE::StateTree

namespace UE::StateTree::Private
{

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreePropertyPath ConvertEditorPath(const FStateTreeEditorPropertyPath& InEditorPath)
	{
		FStateTreePropertyPath Path;
		Path.SetStructID(InEditorPath.StructID);

		for (const FString& Segment : InEditorPath.Path)
		{
			const TCHAR* PropertyNamePtr = nullptr;
			int32 PropertyNameLength = 0;
			int32 ArrayIndex = INDEX_NONE;
			PropertyPathHelpers::FindFieldNameAndArrayIndex(Segment.Len(), *Segment, PropertyNameLength, &PropertyNamePtr, ArrayIndex);
			FString PropertyNameString(PropertyNameLength, PropertyNamePtr);
			const FName PropertyName(*PropertyNameString, FNAME_Find);
			Path.AddPathSegment(PropertyName, ArrayIndex);
		}
		return Path;
	}

	FStateTreeEditorPropertyPath ConvertEditorPath(const FStateTreePropertyPath& InPath)
	{
		FStateTreeEditorPropertyPath Path;
		Path.StructID = InPath.GetStructID();
		for (const FStateTreePropertyPathSegment& Segment : InPath.GetSegments())
		{
			if (Segment.GetArrayIndex() != INDEX_NONE)
			{
				Path.Path.Add(FString::Printf(TEXT("%s[%d]"), *Segment.GetName().ToString(), Segment.GetArrayIndex()));
			}
			else
			{
				Path.Path.Add(Segment.GetName().ToString());
			}
		}
		return Path;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
} // UE::StateTree::Private 


//----------------------------------------------------------------//
//  FStateTreeBindableStructDesc
//----------------------------------------------------------------//

FString FStateTreeBindableStructDesc::ToString() const
{
	FStringBuilderBase Result;

	Result += UEnum::GetDisplayValueAsText(DataSource).ToString();
	Result += TEXT(" '");
	Result += Name.ToString();
	Result += TEXT("'");

	return Result.ToString();
}

//----------------------------------------------------------------//
//  FStateTreePropertyPathBinding
//----------------------------------------------------------------//
void FStateTreePropertyPathBinding::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (SourcePath_DEPRECATED.IsValid())
	{
		SourcePropertyPath = UE::StateTree::Private::ConvertEditorPath(SourcePath_DEPRECATED);
		SourcePath_DEPRECATED.StructID = FGuid();
		SourcePath_DEPRECATED.Path.Reset();
	}

	if (TargetPath_DEPRECATED.IsValid())
	{
		TargetPropertyPath = UE::StateTree::Private::ConvertEditorPath(TargetPath_DEPRECATED);
		TargetPath_DEPRECATED.StructID = FGuid();
		TargetPath_DEPRECATED.Path.Reset();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

}


//----------------------------------------------------------------//
//  FStateTreePropertyBindings
//----------------------------------------------------------------//

void FStateTreePropertyBindings::Reset()
{
	SourceStructs.Reset();
	CopyBatches.Reset();
	PropertyPathBindings.Reset();
	PropertyCopies.Reset();
	PropertyAccesses.Reset();
	PropertyReferencePaths.Reset();
	PropertyIndirections.Reset();
	
	bBindingsResolved = false;
}

const FStateTreeBindableStructDesc* FStateTreePropertyBindings::GetSourceDescByHandle(const FStateTreeDataHandle SourceDataHandle)
{
	TArray<FStateTreeBindableStructDesc> FoundDescs;
	for (const FStateTreeBindableStructDesc& Desc : SourceStructs)
	{
		if (Desc.DataHandle == SourceDataHandle)
		{
			FoundDescs.Add(Desc);
		}
	}

	if (FoundDescs.Num() > 1)
	{
		UE_LOG(LogStateTree, Error, TEXT("%hs: Found %d entries for handle %s."), __FUNCTION__, FoundDescs.Num(), *SourceDataHandle.Describe());
	}
	
	return SourceStructs.FindByPredicate([SourceDataHandle](const FStateTreeBindableStructDesc& Desc)
	{
		return Desc.DataHandle == SourceDataHandle;
	});
}

bool FStateTreePropertyBindings::ResolvePaths()
{
	PropertyIndirections.Reset();
	PropertyCopies.SetNum(PropertyPathBindings.Num());

	bBindingsResolved = true;

	bool bResult = true;
	
	for (const FStateTreePropertyCopyBatch& Batch : CopyBatches)
	{
		for (int32 i = Batch.BindingsBegin; i != Batch.BindingsEnd; i++)
		{
			const FStateTreePropertyPathBinding& Binding = PropertyPathBindings[i];
			
			FStateTreePropertyCopy& Copy = PropertyCopies[i];
			Copy.SourceDataHandle = Binding.GetSourceDataHandle();

			if (!Binding.GetSourceDataHandle().IsValid())
			{
				UE_LOG(LogStateTree, Error, TEXT("%hs: Invalid source struct for property binding %s."), __FUNCTION__, *Binding.GetSourcePath().ToString());
				Copy.Type = EStateTreePropertyCopyType::None;
				bBindingsResolved = false;
				bResult = false;
				continue;
			}

			const FStateTreeBindableStructDesc* SourceDesc = GetSourceDescByHandle(Copy.SourceDataHandle);
			if (!SourceDesc)
			{
				UE_LOG(LogStateTree, Error, TEXT("%hs: Could not find data source for binding %s."), __FUNCTION__, *Binding.GetSourcePath().ToString());
				Copy.Type = EStateTreePropertyCopyType::None;
				bBindingsResolved = false;
				bResult = false;
				continue;
			}

			const UStruct* SourceStruct = SourceDesc->Struct;
			const UStruct* TargetStruct = Batch.TargetStruct.Struct;
			if (!SourceStruct || !TargetStruct)
			{
				Copy.Type = EStateTreePropertyCopyType::None;
				bBindingsResolved = false;
				bResult = false;
				continue;
			}

			Copy.SourceStructType = SourceStruct;

			// Resolve paths and validate the copy. Stops on first failure.
			bool bSuccess = true;
			FStateTreePropertyPathIndirection SourceLeafIndirection;
			FStateTreePropertyPathIndirection TargetLeafIndirection;
			bSuccess = bSuccess && ResolvePath(SourceStruct, Binding.GetSourcePath(), Copy.SourceIndirection, SourceLeafIndirection);
			bSuccess = bSuccess && ResolvePath(TargetStruct, Binding.GetTargetPath(), Copy.TargetIndirection, TargetLeafIndirection);
			bSuccess = bSuccess && ResolveCopyType(SourceLeafIndirection, TargetLeafIndirection, Copy);
			if (!bSuccess) 
			{
				// Resolving or validating failed, make the copy a nop.
				Copy.Type = EStateTreePropertyCopyType::None;
				bResult = false;
			}
		}
	}

	PropertyAccesses.Reset();
	PropertyAccesses.Reserve(PropertyReferencePaths.Num());

	for (const FStateTreePropertyRefPath& ReferencePath : PropertyReferencePaths)
	{
		FStateTreePropertyAccess& PropertyAccess = PropertyAccesses.AddDefaulted_GetRef();
		
		PropertyAccess.SourceDataHandle = ReferencePath.GetSourceDataHandle();
		const FStateTreeBindableStructDesc* SourceDesc = GetSourceDescByHandle(PropertyAccess.SourceDataHandle);
		PropertyAccess.SourceStructType = SourceDesc->Struct;

		FStateTreePropertyPathIndirection SourceLeafIndirection;
		if (!ResolvePath(SourceDesc->Struct, ReferencePath.GetSourcePath(), PropertyAccess.SourceIndirection, SourceLeafIndirection))
		{
			bResult = false;
		}

		PropertyAccess.SourceLeafProperty = SourceLeafIndirection.GetProperty();
	}

	return bResult;
}

bool FStateTreePropertyBindings::ResolvePath(const UStruct* Struct, const FStateTreePropertyPath& Path, TArray<FStateTreePropertyIndirection>& OutIndirections, FStateTreePropertyIndirection& OutFirstIndirection, FStateTreePropertyPathIndirection& OutLeafIndirection)
{
	if (!Struct)
 	{
		UE_LOG(LogStateTree, Error, TEXT("%hs: '%s' Invalid source struct."), __FUNCTION__, *Path.ToString());
		return false;
	}

	FString Error;
	TArray<FStateTreePropertyPathIndirection> PathIndirections;
	if (!Path.ResolveIndirections(Struct, PathIndirections, &Error))
	{
		UE_LOG(LogStateTree, Error, TEXT("%hs: %s"), __FUNCTION__, *Error);
		return false;
	}

	// Casts array index to FStateTreeIndex16 (clamping INDEX_NONE to 0) or returns false if out if index bounds.
	auto CastArrayIndexToIndex16 = [](const int32 Index)
	{
		const int32 ClampedIndex = FMath::Max(0, Index);
		if (!FStateTreeIndex16::IsValidIndex(ClampedIndex))
		{
			return FStateTreeIndex16();
		}
		return FStateTreeIndex16(ClampedIndex);
	};

	TArray<FStateTreePropertyIndirection, TInlineAllocator<16>> TempIndirections;
	for (FStateTreePropertyPathIndirection& PathIndirection : PathIndirections)
	{
		FStateTreePropertyIndirection& Indirection = TempIndirections.AddDefaulted_GetRef();

		check(PathIndirection.GetPropertyOffset() >= MIN_uint16 && PathIndirection.GetPropertyOffset() <= MAX_uint16);

		Indirection.Offset = static_cast<uint16>(PathIndirection.GetPropertyOffset());
		Indirection.Type = PathIndirection.GetAccessType();

		if (Indirection.Type == EStateTreePropertyAccessType::IndexArray)
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(PathIndirection.GetProperty()))
			{
				Indirection.ArrayProperty = ArrayProperty;
				Indirection.ArrayIndex = CastArrayIndexToIndex16(PathIndirection.GetArrayIndex());
				if (!Indirection.ArrayIndex.IsValid())
				{
					UE_LOG(LogStateTree, Error, TEXT("%hs: Array index %d at %s, is too large."),
						__FUNCTION__, PathIndirection.GetArrayIndex(), *Path.ToString(PathIndirection.GetPathSegmentIndex(), TEXT("<"), TEXT(">")));
					return false;
				}
			}
			else
			{
				UE_LOG(LogStateTree, Error, TEXT("%hs: Expect property %s to be array property."),
					__FUNCTION__, *Path.ToString(PathIndirection.GetPathSegmentIndex(), TEXT("<"), TEXT(">")));
				return false;
			}
		}
		else if (Indirection.Type == EStateTreePropertyAccessType::StructInstance
				|| Indirection.Type == EStateTreePropertyAccessType::ObjectInstance)
		{
			if (PathIndirection.GetInstanceStruct())
			{
				Indirection.InstanceStruct = PathIndirection.GetInstanceStruct();
			}
			else
			{
				UE_LOG(LogStateTree, Error, TEXT("%hs: Expect instanced property access %s to have instance type specified."),
					__FUNCTION__, *Path.ToString(PathIndirection.GetPathSegmentIndex(), TEXT("<"), TEXT(">")));
				return false;
			}
		}
	}

	if (TempIndirections.Num() > 0)
	{
		for (int32 Index = 0; Index < TempIndirections.Num(); Index++)
		{
			FStateTreePropertyIndirection& Indirection = TempIndirections[Index];
			if ((Index + 1) < TempIndirections.Num())
			{
				const FStateTreePropertyIndirection& NextIndirection = TempIndirections[Index + 1];
				if (Indirection.Type == EStateTreePropertyAccessType::Offset
					&& NextIndirection.Type == EStateTreePropertyAccessType::Offset)
				{
					// Collapse adjacent offset indirections
					Indirection.Offset += NextIndirection.Offset;
					TempIndirections.RemoveAt(Index + 1);
					Index--;
				}
				else if (Indirection.Type == EStateTreePropertyAccessType::IndexArray
					&& NextIndirection.Type == EStateTreePropertyAccessType::Offset
					&& NextIndirection.Offset == 0)
				{
					// Remove empty offset after array indexing.
					TempIndirections.RemoveAt(Index + 1);
					Index--;
				}
				else if (Indirection.Type == EStateTreePropertyAccessType::StructInstance
					&& NextIndirection.Type == EStateTreePropertyAccessType::Offset
					&& NextIndirection.Offset == 0)
				{
					// Remove empty offset after struct indirection.
					TempIndirections.RemoveAt(Index + 1);
					Index--;
				}
				else if ((Indirection.Type == EStateTreePropertyAccessType::Object
						|| Indirection.Type == EStateTreePropertyAccessType::ObjectInstance)
					&& NextIndirection.Type == EStateTreePropertyAccessType::Offset
					&& NextIndirection.Offset == 0)
				{
					// Remove empty offset after object indirection.
					TempIndirections.RemoveAt(Index + 1);
					Index--;
				}
			}
		}

		OutLeafIndirection = PathIndirections.Last(); 

		// Store indirections
		OutFirstIndirection = TempIndirections[0];
		FStateTreePropertyIndirection* PrevIndirection = &OutFirstIndirection;
		for (int32 Index = 1; Index < TempIndirections.Num(); Index++)
		{
			const int32 IndirectionIndex = OutIndirections.Num();
			PrevIndirection->NextIndex = FStateTreeIndex16(IndirectionIndex); // Set PrevIndirection before array add, as it can invalidate the pointer.
			FStateTreePropertyIndirection& NewIndirection = OutIndirections.Add_GetRef(TempIndirections[Index]);
			PrevIndirection = &NewIndirection;
		}
	}
	else
	{
		// Indirections can be empty in case we're directly binding to source structs.
		// Zero offset will return the struct itself.
		OutFirstIndirection.Offset = 0;
		OutFirstIndirection.Type = EStateTreePropertyAccessType::Offset;

		OutLeafIndirection = FStateTreePropertyPathIndirection(Struct);
	}

	return true;
}

bool FStateTreePropertyBindings::ResolveCopyType(const FStateTreePropertyPathIndirection& SourceIndirection,const FStateTreePropertyPathIndirection& TargetIndirection, FStateTreePropertyCopy& OutCopy)
{
	// @todo: see if GetPropertyCompatibility() can be implemented as call to ResolveCopyType() instead so that we write this logic just once.
	
	const FProperty* SourceProperty = SourceIndirection.GetProperty();
	const UStruct* SourceStruct = SourceIndirection.GetContainerStruct();
	
	const FProperty* TargetProperty = TargetIndirection.GetProperty();
	const UStruct* TargetStruct = TargetIndirection.GetContainerStruct();

	if (!SourceStruct || !TargetStruct)
	{
		return false;
	}

	OutCopy.SourceLeafProperty = SourceProperty;
	OutCopy.TargetLeafProperty = TargetProperty;
	OutCopy.CopySize = 0;
	OutCopy.Type = EStateTreePropertyCopyType::None;
	
	if (SourceProperty == nullptr)
	{
		// Copy directly from the source struct, target must be.
		if (const FStructProperty* TargetStructProperty = CastField<FStructProperty>(TargetProperty))
		{
			if (TargetStructProperty->Struct == SourceStruct)
			{
				OutCopy.Type = EStateTreePropertyCopyType::CopyStruct;
				return true;
			}
		}
		else if (const FObjectPropertyBase* TargetObjectProperty = CastField<FObjectPropertyBase>(TargetProperty))
		{
			if (SourceStruct->IsChildOf(TargetObjectProperty->PropertyClass))
			{
				OutCopy.Type = EStateTreePropertyCopyType::CopyObject;
				return true;
			}
		}

		return false;
	}

	// Handle FStateTreeStructRef
	if (const FStructProperty* TargetStructProperty = CastField<const FStructProperty>(TargetProperty))
	{
		if (TargetStructProperty->Struct == TBaseStructure<FStateTreeStructRef>::Get())
		{
			if (const FStructProperty* SourceStructProperty = CastField<const FStructProperty>(SourceProperty))
			{
				// FStateTreeStructRef to FStateTreeStructRef is copied as usual.
				if (SourceStructProperty->Struct != TBaseStructure<FStateTreeStructRef>::Get())
				{
					OutCopy.Type = EStateTreePropertyCopyType::StructReference;
					return true;
				}
			}
		}
	}

	const EStateTreePropertyAccessCompatibility Compatibility = FStateTreePropertyBindings::GetPropertyCompatibility(SourceProperty, TargetProperty);

	// Extract underlying types for enums
	if (const FEnumProperty* EnumPropertyA = CastField<const FEnumProperty>(SourceProperty))
	{
		SourceProperty = EnumPropertyA->GetUnderlyingProperty();
	}

	if (const FEnumProperty* EnumPropertyB = CastField<const FEnumProperty>(TargetProperty))
	{
		TargetProperty = EnumPropertyB->GetUnderlyingProperty();
	}

	if (Compatibility == EStateTreePropertyAccessCompatibility::Compatible)
	{
		if (CastField<FNameProperty>(TargetProperty))
		{
			OutCopy.Type = EStateTreePropertyCopyType::CopyName;
			return true;
		}
		else if (CastField<FBoolProperty>(TargetProperty))
		{
			OutCopy.Type = EStateTreePropertyCopyType::CopyBool;
			return true;
		}
		else if (CastField<FStructProperty>(TargetProperty))
		{
			OutCopy.Type = EStateTreePropertyCopyType::CopyStruct;
			return true;
		}
		else if (CastField<FObjectPropertyBase>(TargetProperty))
		{
			if (SourceProperty->IsA<FSoftObjectProperty>()
				&& TargetProperty->IsA<FSoftObjectProperty>())
			{
				// Use CopyComplex when copying soft object to another soft object so that we do not try to dereference the object (just copies the path).
				// This handles soft class too.
				OutCopy.Type = EStateTreePropertyCopyType::CopyComplex;
			}
			else
			{
				OutCopy.Type = EStateTreePropertyCopyType::CopyObject;
			}
			return true;
		}
		else if (CastField<FArrayProperty>(TargetProperty) && TargetProperty->HasAnyPropertyFlags(CPF_EditFixedSize))
		{
			// only apply array copying rules if the destination array is fixed size, otherwise it will be 'complex'
			OutCopy.Type = EStateTreePropertyCopyType::CopyFixedArray;
			return true;
		}
		else if (TargetProperty->PropertyFlags & CPF_IsPlainOldData)
		{
			OutCopy.Type = EStateTreePropertyCopyType::CopyPlain;
			OutCopy.CopySize = SourceProperty->ElementSize * SourceProperty->ArrayDim;
			return true;
		}
		else
		{
			OutCopy.Type = EStateTreePropertyCopyType::CopyComplex;
			return true;
		}
	}
	else if (Compatibility == EStateTreePropertyAccessCompatibility::Promotable)
	{
		if (SourceProperty->IsA<FBoolProperty>())
		{
			if (TargetProperty->IsA<FByteProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteBoolToByte;
				return true;
			}
			else if (TargetProperty->IsA<FIntProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteBoolToInt32;
				return true;
			}
			else if (TargetProperty->IsA<FUInt32Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteBoolToUInt32;
				return true;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteBoolToInt64;
				return true;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteBoolToFloat;
				return true;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteBoolToDouble;
				return true;
			}
		}
		else if (SourceProperty->IsA<FByteProperty>())
		{
			if (TargetProperty->IsA<FIntProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteByteToInt32;
				return true;
			}
			else if (TargetProperty->IsA<FUInt32Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteByteToUInt32;
				return true;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteByteToInt64;
				return true;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteByteToFloat;
				return true;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteByteToDouble;
				return true;
			}
		}
		else if (SourceProperty->IsA<FIntProperty>())
		{
			if (TargetProperty->IsA<FInt64Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteInt32ToInt64;
				return true;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteInt32ToFloat;
				return true;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteInt32ToDouble;
				return true;
			}
		}
		else if (SourceProperty->IsA<FUInt32Property>())
		{
			if (TargetProperty->IsA<FInt64Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteUInt32ToInt64;
				return true;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteUInt32ToFloat;
				return true;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteUInt32ToDouble;
				return true;
			}
		}
		else if (SourceProperty->IsA<FFloatProperty>())
		{
			if (TargetProperty->IsA<FIntProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteFloatToInt32;
				return true;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteFloatToInt64;
				return true;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::PromoteFloatToDouble;
				return true;
			}
		}
		else if (SourceProperty->IsA<FDoubleProperty>())
		{
			if (TargetProperty->IsA<FIntProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::DemoteDoubleToInt32;
				return true;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::DemoteDoubleToInt64;
				return true;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				OutCopy.Type = EStateTreePropertyCopyType::DemoteDoubleToFloat;
				return true;
			}
		}
	}

	return false;
}

EStateTreePropertyAccessCompatibility FStateTreePropertyBindings::GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty)
{
	if (FromProperty == ToProperty)
	{
		return EStateTreePropertyAccessCompatibility::Compatible;
	}

	if (FromProperty == nullptr || ToProperty == nullptr)
	{
		return EStateTreePropertyAccessCompatibility::Incompatible;
	}

	// Special case for object properties since InPropertyA->SameType(InPropertyB) requires both properties to be of the exact same class.
	// In our case we want to be able to bind a source property if its class is a child of the target property class.
	if (FromProperty->IsA<FObjectPropertyBase>() && ToProperty->IsA<FObjectPropertyBase>())
	{
		const FObjectPropertyBase* SourceProperty = CastField<FObjectPropertyBase>(FromProperty);
		const FObjectPropertyBase* TargetProperty = CastField<FObjectPropertyBase>(ToProperty);
		return (SourceProperty->PropertyClass->IsChildOf(TargetProperty->PropertyClass)) ? EStateTreePropertyAccessCompatibility::Compatible : EStateTreePropertyAccessCompatibility::Incompatible;
	}

	// When copying to an enum property, expect FromProperty to be the same enum.
	auto GetPropertyEnum = [](const FProperty* Property) -> const UEnum*
	{
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			return ByteProperty->GetIntPropertyEnum();
		}
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum();
		}
		return nullptr;
	};
	
	if (const UEnum* ToPropertyEnum = GetPropertyEnum(ToProperty))
	{
		const UEnum* FromPropertyEnum = GetPropertyEnum(FromProperty);
		return (ToPropertyEnum == FromPropertyEnum) ? EStateTreePropertyAccessCompatibility::Compatible : EStateTreePropertyAccessCompatibility::Incompatible;
	}
	
	// Allow source enums to be promoted to numbers.
	if (const FEnumProperty* EnumPropertyA = CastField<const FEnumProperty>(FromProperty))
	{
		FromProperty = EnumPropertyA->GetUnderlyingProperty();
	}

	if (FromProperty->SameType(ToProperty))
	{
		return EStateTreePropertyAccessCompatibility::Compatible;
	}
	else
	{
		// Not directly compatible, check for promotions
		if (FromProperty->IsA<FBoolProperty>())
		{
			if (ToProperty->IsA<FByteProperty>()
				|| ToProperty->IsA<FIntProperty>()
				|| ToProperty->IsA<FUInt32Property>()
				|| ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FByteProperty>())
		{
			if (ToProperty->IsA<FIntProperty>()
				|| ToProperty->IsA<FUInt32Property>()
				|| ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FIntProperty>())
		{
			if (ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FUInt32Property>())
		{
			if (ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FFloatProperty>())
		{
			if (ToProperty->IsA<FIntProperty>()
				|| ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FDoubleProperty>())
		{
			if (ToProperty->IsA<FIntProperty>()
				|| ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>())
			{
				return EStateTreePropertyAccessCompatibility::Promotable;
			}
		}
	}

	return EStateTreePropertyAccessCompatibility::Incompatible;
}

uint8* FStateTreePropertyBindings::GetAddress(FStateTreeDataView InStructView, TConstArrayView<FStateTreePropertyIndirection> Indirections, const FStateTreePropertyIndirection& FirstIndirection, const FProperty* LeafProperty)
{
	uint8* Address = InStructView.GetMutableMemory();
	if (Address == nullptr)
	{
		// Failed indirection, will be reported by caller.
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
		case EStateTreePropertyAccessType::ObjectInstance:
		{
			check(Indirection->InstanceStruct);
			UObject* Object = *reinterpret_cast<UObject**>(Address + Indirection->Offset);
			if (Object
				&& Object->GetClass()->IsChildOf(Indirection->InstanceStruct))
			{
				Address = reinterpret_cast<uint8*>(Object);
			}
			else
			{
				// Failed indirection, will be reported by caller.
				return nullptr;
			}
			break;
		}
		case EStateTreePropertyAccessType::StructInstance:
		{
			check(Indirection->InstanceStruct);
			FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(Address + Indirection->Offset);
			const UScriptStruct* InstanceType = InstancedStruct.GetScriptStruct(); 
			if (InstanceType != nullptr
				&& InstanceType->IsChildOf(Indirection->InstanceStruct))
			{
				Address = InstancedStruct.GetMutableMemory();
			}
			else
			{
				// Failed indirection, will be reported by caller.
				return nullptr;
			}
			break;
		}
		case EStateTreePropertyAccessType::IndexArray:
		{
			check(Indirection->ArrayProperty);
			FScriptArrayHelper Helper(Indirection->ArrayProperty, Address + Indirection->Offset);
			if (Helper.IsValidIndex(Indirection->ArrayIndex.Get()))
			{
				Address = Helper.GetRawPtr(Indirection->ArrayIndex.Get());
			}
			else
			{
				// Failed indirection, will be reported by caller.
				return nullptr;
			}
			break;
		}
		default:
			ensureMsgf(false, TEXT("FStateTreePropertyBindings::GetAddress: Unhandled indirection type %s for '%s'"),
				*StaticEnum<EStateTreePropertyAccessType>()->GetValueAsString(Indirection->Type), *LeafProperty->GetNameCPP());
		}

		Indirection = Indirection->NextIndex.IsValid() ? &Indirections[Indirection->NextIndex.Get()] : nullptr;
	}

	return Address;
}

void FStateTreePropertyBindings::PerformCopy(const FStateTreePropertyCopy& Copy, uint8* SourceAddress, uint8* TargetAddress) const
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

bool FStateTreePropertyBindings::CopyProperty(const FStateTreePropertyCopy& Copy, FStateTreeDataView SourceStructView, FStateTreeDataView TargetStructView) const
{
	// This is made ensure so that the programmers have the change to catch it (it's usually programming error not to call ResolvePaths(), and it wont spam log for others.
	if (!ensureMsgf(bBindingsResolved, TEXT("Bindings must be resolved successfully before copying. See ResolvePaths()")))
	{
		return false;
	}

	// Copies that fail to be resolved (i.e. property path does not resolve, types changed) will be marked as None, skip them.
	if (Copy.Type == EStateTreePropertyCopyType::None)
	{
		return true;
	}

	bool bResult = true;
	
	if (SourceStructView.IsValid() && TargetStructView.IsValid())
	{
		check(SourceStructView.GetStruct() == Copy.SourceStructType
			|| (SourceStructView.GetStruct() && SourceStructView.GetStruct()->IsChildOf(Copy.SourceStructType)));
			
		uint8* SourceAddress = GetAddress(SourceStructView, Copy.SourceIndirection, Copy.SourceLeafProperty);
		uint8* TargetAddress = GetAddress(TargetStructView, Copy.TargetIndirection, Copy.TargetLeafProperty);
		
		if (SourceAddress != nullptr && TargetAddress != nullptr)
		{
			PerformCopy(Copy, SourceAddress, TargetAddress);
		}
		else
		{
			bResult = false;
		}
	}
	else
	{
		bResult = false;
	}

	return bResult;
}

void FStateTreePropertyBindings::PerformResetObjects(const FStateTreePropertyCopy& Copy, uint8* TargetAddress) const
{
	// Source property can be null
	check(Copy.TargetLeafProperty);
	check(TargetAddress);

	switch (Copy.Type)
	{
	case EStateTreePropertyCopyType::CopyComplex:
		Copy.TargetLeafProperty->InitializeValue(TargetAddress);
		break;
	case EStateTreePropertyCopyType::CopyStruct:
		static_cast<const FStructProperty*>(Copy.TargetLeafProperty)->Struct->ClearScriptStruct(TargetAddress);
		break;
	case EStateTreePropertyCopyType::CopyObject:
		static_cast<const FObjectPropertyBase*>(Copy.TargetLeafProperty)->SetObjectPropertyValue(TargetAddress, nullptr);
		break;
	case EStateTreePropertyCopyType::StructReference:
		reinterpret_cast<FStateTreeStructRef*>(TargetAddress)->Set(FStructView());
		break;
	case EStateTreePropertyCopyType::CopyName:
		break;
	case EStateTreePropertyCopyType::CopyFixedArray:
	{
		// Copy into fixed sized array (EditFixedSize). Resizable arrays are copied as Complex, and regular fixed sizes arrays via the regular copies (dim specifies array size).
		const FArrayProperty* TargetArrayProperty = static_cast<const FArrayProperty*>(Copy.TargetLeafProperty);
		FScriptArrayHelper TargetArrayHelper(TargetArrayProperty, TargetAddress);
		for (int32 ElementIndex = 0; ElementIndex < TargetArrayHelper.Num(); ++ElementIndex)
		{
			TargetArrayProperty->Inner->InitializeValue(TargetArrayHelper.GetRawPtr(ElementIndex));
		}
		break;
	}
	default:
		break;
	}
}

const FStateTreePropertyAccess* FStateTreePropertyBindings::GetPropertyAccess(const FStateTreePropertyRef& PropertyRef) const
{
	if (!PropertyRef.GetRefAccessIndex().IsValid())
	{
		return nullptr;
	}

	if (!ensure(PropertyAccesses.IsValidIndex(PropertyRef.GetRefAccessIndex().Get())))
	{
		return nullptr;
	}

	return &PropertyAccesses[PropertyRef.GetRefAccessIndex().Get()];
}

bool FStateTreePropertyBindings::ResetObjects(const FStateTreeIndex16 TargetBatchIndex, FStateTreeDataView TargetStructView) const
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
	const FStateTreePropertyCopyBatch& Batch = CopyBatches[TargetBatchIndex.Get()];

	check(TargetStructView.IsValid());
	check(TargetStructView.GetStruct() == Batch.TargetStruct.Struct);

	bool bResult = true;
	
	for (int32 i = Batch.BindingsBegin; i != Batch.BindingsEnd; i++)
	{
		const FStateTreePropertyCopy& Copy = PropertyCopies[i];
		// Copies that fail to be resolved (i.e. property path does not resolve, types changed) will be marked as None, skip them.
		if (Copy.Type == EStateTreePropertyCopyType::None)
		{
			continue;
		}
		
		uint8* TargetAddress = GetAddress(TargetStructView, Copy.TargetIndirection, Copy.TargetLeafProperty);
		check(TargetAddress != nullptr);
		PerformResetObjects(Copy, TargetAddress);
	}

	return bResult;
}

bool FStateTreePropertyBindings::ContainsAnyStruct(const TSet<const UStruct*>& Structs)
{
	for (FStateTreeBindableStructDesc& SourceStruct : SourceStructs)
	{
		if (Structs.Contains(SourceStruct.Struct))
		{
			return true;
		}
	}

	for (FStateTreePropertyCopyBatch& CopyBatch : CopyBatches)
	{
		if (Structs.Contains(CopyBatch.TargetStruct.Struct))
		{
			return true;
		}
	}

	auto PathContainsStruct = [&Structs](const FStateTreePropertyPath& PropertyPath)
	{
		for (const FStateTreePropertyPathSegment& Segment : PropertyPath.GetSegments())
		{
			if (Structs.Contains(Segment.GetInstanceStruct()))
			{
				return true;
			}
		}
		return false;
	};
	
	for (FStateTreePropertyPathBinding& PropertyPathBinding : PropertyPathBindings)
	{
		if (PathContainsStruct(PropertyPathBinding.GetSourcePath()))
		{
			return true;
		}
		if (PathContainsStruct(PropertyPathBinding.GetTargetPath()))
		{
			return true;
		}
	}
	return false;
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
	for (const FStateTreePropertyCopyBatch& CopyBatch : CopyBatches)
	{
		OutString += FString::Printf(TEXT("  | %-40s | %-40s | %8s [%3d:%-3d[ |\n"),
									 CopyBatch.TargetStruct.Struct ? *CopyBatch.TargetStruct.Struct->GetName() : TEXT("null"),
									 *CopyBatch.TargetStruct.Name.ToString(),
									 TEXT(""), CopyBatch.BindingsBegin, CopyBatch.BindingsEnd);
	}

	/** Array of property bindings, resolved into arrays of copies before use. */
	OutString += FString::Printf(TEXT("\nPropertyPathBindings (%d)\n"), PropertyPathBindings.Num());
	for (const FStateTreePropertyPathBinding& PropertyBinding : PropertyPathBindings)
	{
		OutString += FString::Printf(TEXT("\n  Source: %s | Target: %s"),
					*PropertyBinding.GetSourcePath().ToString(), *PropertyBinding.GetSourcePath().ToString());
	}

	/** Array of property copies */
	OutString += FString::Printf(TEXT("\nPropertyCopies (%d)\n  [ %-7s | %-4s | %-4s | %-10s | %-7s | %-4s | %-4s | %-10s | %-10s | %-20s | %-4s ]\n"), PropertyCopies.Num(),
		TEXT("Src Idx"), TEXT("Off."), TEXT("Next"), TEXT("Type"),
		TEXT("Tgt Idx"), TEXT("Off."), TEXT("Next"), TEXT("Type"),
		TEXT("Source"), TEXT("Copy Type"), TEXT("Size"));
	for (const FStateTreePropertyCopy& PropertyCopy : PropertyCopies)
	{
		OutString += FString::Printf(TEXT("  | %7d | %4d | %4d | %-10s | %7d | %4d | %4d | %-10s | %10s | %-20s | %4d |\n"),
					PropertyCopy.SourceIndirection.ArrayIndex.Get(),
					PropertyCopy.SourceIndirection.Offset,
					PropertyCopy.SourceIndirection.NextIndex.Get(),
					*UEnum::GetDisplayValueAsText(PropertyCopy.SourceIndirection.Type).ToString(),
					PropertyCopy.TargetIndirection.ArrayIndex.Get(),
					PropertyCopy.TargetIndirection.Offset,
					PropertyCopy.TargetIndirection.NextIndex.Get(),
					*UEnum::GetDisplayValueAsText(PropertyCopy.TargetIndirection.Type).ToString(),
					*PropertyCopy.SourceDataHandle.Describe(),
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


//----------------------------------------------------------------//
//  FStateTreePropertyPath
//----------------------------------------------------------------//

bool FStateTreePropertyPath::FromString(const FString& InPath)
{
	Segments.Reset();
	
	if (InPath.IsEmpty())
	{
		return true;
	}
	
	bool bResult = true;
	TArray<FString> PathSegments;
	InPath.ParseIntoArray(PathSegments, TEXT("."), /*InCullEmpty=*/false);
	
	for (const FString& Segment : PathSegments)
	{
		if (Segment.IsEmpty())
		{
			bResult = false;
			break;
		}

		int32 FirstBracket = INDEX_NONE;
		int32 LastBracket = INDEX_NONE;
		if (Segment.FindChar(TEXT('['), FirstBracket)
			&& Segment.FindLastChar(TEXT(']'), LastBracket))
		{
			const int32 NameStringLength = FirstBracket;
			const int32 IndexStringLength = LastBracket - FirstBracket - 1;
			if (NameStringLength < 1
				|| IndexStringLength <= 0)
			{
				bResult = false;
				break;
			}

			const FString NameString = Segment.Left(FirstBracket);
			const FString IndexString = Segment.Mid(FirstBracket + 1, IndexStringLength);
			int32 ArrayIndex = INDEX_NONE;
			LexFromString(ArrayIndex, *IndexString);
			if (ArrayIndex < 0)
			{
				bResult = false;
				break;
			}
			
			AddPathSegment(FName(NameString), ArrayIndex);
		}
		else
		{
			AddPathSegment(FName(Segment));
		}
	}

	if (!bResult)
	{
		Segments.Reset();
	}
	
	return bResult;
}

bool FStateTreePropertyPath::UpdateSegments(const UStruct* BaseStruct, FString* OutError)
{
	return UpdateSegmentsFromValue(FStateTreeDataView(BaseStruct, nullptr), OutError);
}

bool FStateTreePropertyPath::UpdateSegmentsFromValue(const FStateTreeDataView BaseValueView, FString* OutError)
{
	TArray<FStateTreePropertyPathIndirection> Indirections;
	if (!ResolveIndirectionsWithValue(BaseValueView, Indirections, OutError, /*bHandleRedirects*/true))
	{
		return false;
	}

	for (FStateTreePropertyPathSegment& Segment : Segments)
	{
		Segment.SetInstanceStruct(nullptr);
	}
	
	for (const FStateTreePropertyPathIndirection& Indirection : Indirections)
	{
		if (Indirection.InstanceStruct != nullptr)
		{
			Segments[Indirection.PathSegmentIndex].SetInstanceStruct(Indirection.InstanceStruct);
		}
#if WITH_EDITORONLY_DATA		
		if (!Indirection.GetRedirectedName().IsNone())
		{
			Segments[Indirection.PathSegmentIndex].SetName(Indirection.GetRedirectedName());
		}
		Segments[Indirection.PathSegmentIndex].SetPropertyGuid(Indirection.GetPropertyGuid());
#endif			
	}

	return true;
}

FString FStateTreePropertyPath::ToString(const int32 HighlightedSegment, const TCHAR* HighlightPrefix, const TCHAR* HighlightPostfix, const bool bOutputInstances) const
{
	FString Result;
	for (TEnumerateRef<const FStateTreePropertyPathSegment> Segment : EnumerateRange(Segments))
	{
		if (Segment.GetIndex() > 0)
		{
			Result += TEXT(".");
		}
		if (Segment.GetIndex() == HighlightedSegment && HighlightPrefix)
		{
			Result += HighlightPrefix;
		}

		if (bOutputInstances && Segment->GetInstanceStruct())
		{
			Result += FString::Printf(TEXT("(%s)"), *GetNameSafe(Segment->GetInstanceStruct()));
		}

		if (Segment->GetArrayIndex() >= 0)
		{
			Result += FString::Printf(TEXT("%s[%d]"), *Segment->GetName().ToString(), Segment->GetArrayIndex());
		}
		else
		{
			Result += Segment->GetName().ToString();
		}

		if (Segment.GetIndex() == HighlightedSegment && HighlightPostfix)
		{
			Result += HighlightPostfix;
		}
	}
	return Result;
}


bool FStateTreePropertyPath::ResolveIndirections(const UStruct* BaseStruct, TArray<FStateTreePropertyPathIndirection>& OutIndirections, FString* OutError, bool bHandleRedirects) const
{
	return ResolveIndirectionsWithValue(FStateTreeDataView(BaseStruct, nullptr), OutIndirections, OutError, bHandleRedirects);
}

bool FStateTreePropertyPath::ResolveIndirectionsWithValue(const FStateTreeDataView BaseValueView, TArray<FStateTreePropertyPathIndirection>& OutIndirections, FString* OutError, bool bHandleRedirects) const
{
	OutIndirections.Reset();
	if (OutError)
	{
		OutError->Reset();
	}
	
	// Nothing to do for an empty path.
	if (IsPathEmpty())
	{
		return true;
	}

	const uint8* CurrentAddress = BaseValueView.GetMemory();
	const UStruct* CurrentStruct = BaseValueView.GetStruct();
	
	for (const TEnumerateRef<const FStateTreePropertyPathSegment> Segment : EnumerateRange(Segments))
	{
		if (CurrentStruct == nullptr)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Malformed path '%s'."),
					*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
			}
			OutIndirections.Reset();
			return false;
		}

		const FProperty* Property = CurrentStruct->FindPropertyByName(Segment->GetName());
		const bool bWithValue = CurrentAddress != nullptr;

#if WITH_EDITORONLY_DATA
		FName RedirectedName;
		FGuid PropertyGuid = Segment->GetPropertyGuid();

		// Try to fix the path in editor.
		if (bHandleRedirects)
		{
			
			// Check if there's a core redirect for it.
			if (!Property)
			{
				// Try to match by property ID (Blueprint or User Defined Struct).
				if (Segment->GetPropertyGuid().IsValid())
				{
					if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(CurrentStruct))
					{
						if (const FName* Name = BlueprintClass->PropertyGuids.FindKey(Segment->GetPropertyGuid()))
						{
							RedirectedName = *Name;
							Property = CurrentStruct->FindPropertyByName(RedirectedName);
						}
					}
					else if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
					{
						if (FProperty* FoundProperty = FStructureEditorUtils::GetPropertyByGuid(UserDefinedStruct, Segment->GetPropertyGuid()))
						{
							RedirectedName = FoundProperty->GetFName();
							Property = FoundProperty;
						}
					}
					else if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(CurrentStruct))
					{
						if (const FPropertyBagPropertyDesc* Desc = PropertyBag->FindPropertyDescByID(Segment->GetPropertyGuid()))
						{
							if (Desc->CachedProperty)
							{
								RedirectedName = Desc->CachedProperty->GetFName();
								Property = Desc->CachedProperty;
							}
						}
					}
				}
				else
				{
					// Try core redirect
					const FCoreRedirectObjectName OldPropertyName(Segment->GetName(), CurrentStruct->GetFName(), *CurrentStruct->GetOutermost()->GetPathName());
					const FCoreRedirectObjectName NewPropertyName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, OldPropertyName);
					if (OldPropertyName != NewPropertyName)
					{
						// Cached the result for later use.
						RedirectedName = NewPropertyName.ObjectName;

						Property = CurrentStruct->FindPropertyByName(RedirectedName);
					}
				}
			}

			// Update PropertyGuid 
			if (Property)
			{
				const FName PropertyName = !RedirectedName.IsNone() ? RedirectedName : Segment->GetName();
				if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(CurrentStruct))
				{
					if (const FGuid* VarGuid = BlueprintClass->PropertyGuids.Find(PropertyName))
					{
						PropertyGuid = *VarGuid;
					}
				}
				else if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
				{
					// Parse Guid from UDS property name.
					PropertyGuid = FStructureEditorUtils::GetGuidFromPropertyName(PropertyName);
				}
				else if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(CurrentStruct))
				{
					if (const FPropertyBagPropertyDesc* Desc = PropertyBag->FindPropertyDescByPropertyName(PropertyName))
					{
						PropertyGuid = Desc->ID;
					}
				}
			}
		}
#endif // WITH_EDITORONLY_DATA

		if (!Property)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Malformed path '%s', could not find property '%s%s::%s'."),
					*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")),
					CurrentStruct->GetPrefixCPP(), *CurrentStruct->GetName(), *Segment->GetName().ToString());
			}
			OutIndirections.Reset();
			return false;
		}

		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		int ArrayIndex = 0;
		int32 Offset = 0;
		if (ArrayProperty && Segment->GetArrayIndex() != INDEX_NONE)
		{
			FStateTreePropertyPathIndirection& Indirection = OutIndirections.AddDefaulted_GetRef();
			Indirection.Property = Property;
			Indirection.ContainerAddress = CurrentAddress;
			Indirection.ContainerStruct = CurrentStruct;
			Indirection.InstanceStruct = nullptr;
			Indirection.ArrayIndex = Segment->GetArrayIndex();
			Indirection.PropertyOffset = ArrayProperty->GetOffset_ForInternal();
			Indirection.PathSegmentIndex = Segment.GetIndex();
			Indirection.AccessType = EStateTreePropertyAccessType::IndexArray;
#if WITH_EDITORONLY_DATA
			Indirection.RedirectedName = RedirectedName;
			Indirection.PropertyGuid = PropertyGuid;
#endif
			
			ArrayIndex = 0;
			Offset = 0;
			Property = ArrayProperty->Inner;

			if (bWithValue)
			{
				FScriptArrayHelper Helper(ArrayProperty, CurrentAddress + ArrayProperty->GetOffset_ForInternal());
				if (!Helper.IsValidIndex(Segment->GetArrayIndex()))
				{
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("Index %d out of range (num elements %d) trying to access dynamic array '%s'."),
							Segment->GetArrayIndex(), Helper.Num(), *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
					}
					OutIndirections.Reset();
					return false;
				}
				CurrentAddress = Helper.GetRawPtr(Segment->GetArrayIndex());
			}
		}
		else
		{
			if (Segment->GetArrayIndex() > Property->ArrayDim)
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Index %d out of range %d trying to access static array '%s'."),
						Segment->GetArrayIndex(), Property->ArrayDim, *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
				}
				OutIndirections.Reset();
				return false;
			}
			ArrayIndex = FMath::Max(0, Segment->GetArrayIndex());
			Offset = Property->GetOffset_ForInternal() + Property->ElementSize * ArrayIndex;
		}

		FStateTreePropertyPathIndirection& Indirection = OutIndirections.AddDefaulted_GetRef();
		Indirection.Property = Property;
		Indirection.ContainerAddress = CurrentAddress;
		Indirection.ContainerStruct = CurrentStruct;
		Indirection.ArrayIndex = ArrayIndex;
		Indirection.PropertyOffset = Offset;
		Indirection.PathSegmentIndex = Segment.GetIndex();
		Indirection.AccessType = EStateTreePropertyAccessType::Offset; 
#if WITH_EDITORONLY_DATA
		Indirection.RedirectedName = RedirectedName;
		Indirection.PropertyGuid = PropertyGuid;
#endif
		const bool bLastSegment = Segment.GetIndex() == (Segments.Num() - 1);

		if (!bLastSegment)
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (bWithValue)
				{
					if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
					{
						// The property path is pointing into the instanced struct, it must be present.
						// @TODO:	We could potentially check the BaseStruct metadata in editor (for similar behavior as objects)
						//			Omitting for now to have matching functionality in editor and runtime.
						const FInstancedStruct& InstancedStruct = *reinterpret_cast<const FInstancedStruct*>(CurrentAddress + Offset);
						if (!InstancedStruct.IsValid())
						{
							if (OutError)
							{
								*OutError = FString::Printf(TEXT("Expecting valid instanced struct value at path '%s'."),
									*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
							}
							OutIndirections.Reset();
							return false;
						}
						const UScriptStruct* ValueInstanceStructType = InstancedStruct.GetScriptStruct();

						CurrentAddress = InstancedStruct.GetMemory();
						CurrentStruct = ValueInstanceStructType;
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EStateTreePropertyAccessType::StructInstance; 
					}
					else
					{
						CurrentAddress = CurrentAddress + Offset;
						CurrentStruct = StructProperty->Struct;
						Indirection.AccessType = EStateTreePropertyAccessType::Offset;
					}
				}
				else
				{
					if (Segment->GetInstanceStruct())
					{
						CurrentStruct = Segment->GetInstanceStruct();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EStateTreePropertyAccessType::StructInstance;
					}
					else
					{
						CurrentStruct = StructProperty->Struct;
						Indirection.AccessType = EStateTreePropertyAccessType::Offset;
					}
				}
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const UObject* Object = *reinterpret_cast<UObject* const*>(CurrentAddress + Offset);
					CurrentAddress = reinterpret_cast<const uint8*>(Object);
					
					// The property path is pointing into the object, if the object is present use it's specific type, otherwise use the type of the pointer.
					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EStateTreePropertyAccessType::ObjectInstance;
					}
					else
					{
						CurrentStruct = ObjectProperty->PropertyClass;
						Indirection.AccessType = EStateTreePropertyAccessType::Object;
					}
				}
				else
				{
					if (Segment->GetInstanceStruct())
					{
						CurrentStruct = Segment->GetInstanceStruct();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EStateTreePropertyAccessType::ObjectInstance;
					}
					else
					{
						CurrentStruct = ObjectProperty->PropertyClass;
						Indirection.AccessType = EStateTreePropertyAccessType::Object;
					}
				}
			}
			// Check to see if this is a simple weak object property (eg. not an array of weak objects).
			else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const TWeakObjectPtr<UObject>& WeakObjectPtr = *reinterpret_cast<const TWeakObjectPtr<UObject>*>(CurrentAddress + Offset);
					const UObject* Object = WeakObjectPtr.Get();
					CurrentAddress = reinterpret_cast<const uint8*>(Object);

					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
					}
				}
				else
				{
					CurrentStruct = WeakObjectProperty->PropertyClass;
				}
				
				Indirection.AccessType = EStateTreePropertyAccessType::WeakObject;
			}
			// Check to see if this is a simple soft object property (eg. not an array of soft objects).
			else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const FSoftObjectPtr& SoftObjectPtr = *reinterpret_cast<const FSoftObjectPtr*>(CurrentAddress + Offset);
					const UObject* Object = SoftObjectPtr.Get();
					CurrentAddress = reinterpret_cast<const uint8*>(Object);

					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
					}			
				}
				else
				{			
					CurrentStruct = SoftObjectProperty->PropertyClass;
				}

				Indirection.AccessType = EStateTreePropertyAccessType::SoftObject;
			}
			else
			{
				// We get here if we encounter a property type that is not supported for indirection (e.g. Map or Set).
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Unsupported property indirection type %s in path '%s'."),
						*Property->GetCPPType(), *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
				}
				OutIndirections.Reset();
				return false;
			}
		}
	}

	return true;
}

bool FStateTreePropertyPath::operator==(const FStateTreePropertyPath& RHS) const
{
#if WITH_EDITORONLY_DATA
	if (StructID != RHS.StructID)
	{
		return false;
	}
#endif // WITH_EDITORONLY_DATA
	if (Segments.Num() != RHS.Segments.Num())
	{
		return false;
	}

	for (TEnumerateRef<const FStateTreePropertyPathSegment> Segment : EnumerateRange(Segments))
	{
		if (*Segment != RHS.Segments[Segment.GetIndex()])
		{
			return false;
		}
	}

	return true;
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

PRAGMA_DISABLE_DEPRECATION_WARNINGS
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
PRAGMA_ENABLE_DEPRECATION_WARNINGS
