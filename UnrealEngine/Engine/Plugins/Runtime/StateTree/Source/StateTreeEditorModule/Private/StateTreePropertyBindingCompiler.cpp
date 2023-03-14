// Copyright Epic Games, Inc. All Rights Reserved.
#include "StateTreePropertyBindingCompiler.h"
#include "IPropertyAccessEditor.h"
#include "PropertyPathHelpers.h"
#include "StateTreeTypes.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreePropertyBindingCompiler)

bool FStateTreePropertyBindingCompiler::Init(FStateTreePropertyBindings& InPropertyBindings, FStateTreeCompilerLog& InLog)
{
	Log = &InLog;
	PropertyBindings = &InPropertyBindings;
	PropertyBindings->Reset();
	SourceStructs.Reset();
	return true;
}

bool FStateTreePropertyBindingCompiler::CompileBatch(const FStateTreeBindableStructDesc& TargetStruct, TConstArrayView<FStateTreeEditorPropertyBinding> EditorPropertyBindings, int32& OutBatchIndex)
{
	check(Log);
	check(PropertyBindings);
	OutBatchIndex = INDEX_NONE;

	StoreSourceStructs();

	TArray<FStateTreePropertySegment> SourceSegments;
	TArray<FStateTreePropertySegment> TargetSegments;
	
	const int32 BindingsBegin = PropertyBindings->PropertyBindings.Num();

	for (const FStateTreeEditorPropertyBinding& EditorBinding : EditorPropertyBindings)
	{
		if (EditorBinding.TargetPath.StructID != TargetStruct.ID)
		{
			continue;
		}
		// Source must be in the source array
		const FGuid SourceStructID = EditorBinding.SourcePath.StructID;
		const int32 SourceStructIdx = SourceStructs.IndexOfByPredicate([SourceStructID](const FStateTreeBindableStructDesc& Struct)
			{
				return (Struct.ID == SourceStructID);
			});
		if (SourceStructIdx == INDEX_NONE)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find a binding source."));
			return false;
		}
		const FStateTreeBindableStructDesc& SourceStruct = SourceStructs[SourceStructIdx];

		FStateTreePropertyBinding& NewBinding = PropertyBindings->PropertyBindings.AddDefaulted_GetRef();

		SourceSegments.Reset();
		TargetSegments.Reset();

		// Resolve paths
		const FProperty* SourceLeafProperty = nullptr;
		int32 SourceLeafArrayIndex = INDEX_NONE;
		if (!ResolvePropertyPath(SourceStruct, EditorBinding.SourcePath, SourceSegments, SourceLeafProperty, SourceLeafArrayIndex, Log, &TargetStruct))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not resolve path to '%s:%s'."),
				*SourceStruct.Name.ToString(), *EditorBinding.SourcePath.ToString());
			return false;
		}

		// Destination container is set to 0, it is assumed to be passed in when doing the batch copy.
		const FProperty* TargetLeafProperty = nullptr;
		int32 TargetLeafArrayIndex = INDEX_NONE;
		if (!ResolvePropertyPath(TargetStruct, EditorBinding.TargetPath, TargetSegments, TargetLeafProperty, TargetLeafArrayIndex, Log, &TargetStruct))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not resolve path to '%s:%s'."),
				*TargetStruct.Name.ToString(), *EditorBinding.TargetPath.ToString());
			return false;
		}

		auto StorePropertyPath = [this, &TargetStruct](FStateTreePropertySegment& FirstPathSegment, TArray<FStateTreePropertySegment>& Segments) -> bool
		{
			// The path is empty when directly bound to the target struct.
			if (Segments.IsEmpty())
			{
				return true;
			}
			
			FirstPathSegment = Segments[0];
			FStateTreePropertySegment* PrevSegment = &FirstPathSegment; 
			for (int32 Index = 1; Index < Segments.Num(); Index++)
			{
				const int32 SegmentIndex = PropertyBindings->PropertySegments.Num();
				FStateTreePropertySegment& NewSegment = PropertyBindings->PropertySegments.Add_GetRef(Segments[Index]);

				if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(SegmentIndex); Validation.DidFail())
				{
					Validation.Log(*Log, TEXT("SegmentIndex"), TargetStruct);
					return false;
				}
				PrevSegment->NextIndex = FStateTreeIndex16(SegmentIndex);
				PrevSegment = &NewSegment;
			}
			return true;
		};

		if (!StorePropertyPath(NewBinding.SourcePath, SourceSegments))
		{
			return false;
		}
		if (!StorePropertyPath(NewBinding.TargetPath, TargetSegments))
		{
			return false;
		}

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(SourceStructIdx); Validation.DidFail())
		{
			Validation.Log(*Log, TEXT("SourceStructIdx"), TargetStruct);
			return false;
		}
		NewBinding.SourceStructIndex = FStateTreeIndex16(SourceStructIdx);
		
		NewBinding.CopyType = GetCopyType(SourceStruct.Struct, SourceLeafProperty, SourceLeafArrayIndex,
			TargetStruct.Struct, TargetLeafProperty, TargetLeafArrayIndex);

		if (NewBinding.CopyType == EStateTreePropertyCopyType::None)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not resolve copy type for properties '%s:%s' and '%s:%s'."),
				*SourceStruct.Name.ToString(), *EditorBinding.SourcePath.ToString(),
				*TargetStruct.Name.ToString(), *EditorBinding.TargetPath.ToString());
			return false;
		}
	}

	const int32 BindingsEnd = PropertyBindings->PropertyBindings.Num();
	if (BindingsBegin != BindingsEnd)
	{
		FStateTreePropCopyBatch& Batch = PropertyBindings->CopyBatches.AddDefaulted_GetRef();
		Batch.TargetStruct = TargetStruct;
		Batch.BindingsBegin = BindingsBegin;
		Batch.BindingsEnd = BindingsEnd;
		OutBatchIndex = PropertyBindings->CopyBatches.Num() - 1;
	}

	return true;
}

void FStateTreePropertyBindingCompiler::Finalize()
{
	StoreSourceStructs();
}

int32 FStateTreePropertyBindingCompiler::AddSourceStruct(const FStateTreeBindableStructDesc& SourceStruct)
{
	SourceStructs.Add(SourceStruct);
	return SourceStructs.Num() - 1;
}

int32 FStateTreePropertyBindingCompiler::GetSourceStructIndexByID(const FGuid& ID) const
{
	return SourceStructs.IndexOfByPredicate([ID](const FStateTreeBindableStructDesc& Structs) { return (Structs.ID == ID); });
}

EStateTreePropertyCopyType FStateTreePropertyBindingCompiler::GetCopyType(const UStruct* SourceStruct, const FProperty* SourceProperty, const int32 SourceArrayIndex,
																		  const UStruct* TargetStruct, const FProperty* TargetProperty, const int32 TargetArrayIndex) const
{
	if (SourceProperty == nullptr)
	{
		// Copy directly from the source struct, target must be.
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(TargetProperty))
		{
			if (StructProperty->Struct == SourceStruct)
			{
				return EStateTreePropertyCopyType::CopyStruct;
			}
		}
		else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(TargetProperty))
		{
			if (SourceStruct->IsChildOf(ObjectProperty->PropertyClass))
			{
				return EStateTreePropertyCopyType::CopyObject;
			}
		}
		
		return EStateTreePropertyCopyType::None;
	}
	
	if (const FArrayProperty* SourceArrayProperty = CastField<FArrayProperty>(SourceProperty))
	{
		// use the array's inner property if we are not trying to copy the whole array
		if (SourceArrayIndex != INDEX_NONE)
		{
			SourceProperty = SourceArrayProperty->Inner;
		}
	}

	if (const FArrayProperty* TargetArrayProperty = CastField<FArrayProperty>(TargetProperty))
	{
		// use the array's inner property if we are not trying to copy the whole array
		if (TargetArrayIndex != INDEX_NONE)
		{
			TargetProperty = TargetArrayProperty->Inner;
		}
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
					return EStateTreePropertyCopyType::StructReference;
				}
			}
		}
	}

	const EPropertyAccessCompatibility Compatibility = GetPropertyCompatibility(SourceProperty, TargetProperty);

	// Extract underlying types for enums
	if (const FEnumProperty* EnumPropertyA = CastField<const FEnumProperty>(SourceProperty))
	{
		SourceProperty = EnumPropertyA->GetUnderlyingProperty();
	}

	if (const FEnumProperty* EnumPropertyB = CastField<const FEnumProperty>(TargetProperty))
	{
		TargetProperty = EnumPropertyB->GetUnderlyingProperty();
	}

	if (Compatibility == EPropertyAccessCompatibility::Compatible)
	{
		if (CastField<FNameProperty>(TargetProperty))
		{
			return EStateTreePropertyCopyType::CopyName;
		}
		else if (CastField<FBoolProperty>(TargetProperty))
		{
			return EStateTreePropertyCopyType::CopyBool;
		}
		else if (CastField<FStructProperty>(TargetProperty))
		{
			return EStateTreePropertyCopyType::CopyStruct;
		}
		else if (CastField<FObjectPropertyBase>(TargetProperty))
		{
			return EStateTreePropertyCopyType::CopyObject;
		}
		else if (CastField<FArrayProperty>(TargetProperty) && TargetProperty->HasAnyPropertyFlags(CPF_EditFixedSize))
		{
			// only apply array copying rules if the destination array is fixed size, otherwise it will be 'complex'
			return EStateTreePropertyCopyType::CopyFixedArray;
		}
		else if (TargetProperty->PropertyFlags & CPF_IsPlainOldData)
		{
			return EStateTreePropertyCopyType::CopyPlain;
		}
		else
		{
			return EStateTreePropertyCopyType::CopyComplex;
		}
	}
	else if (Compatibility == EPropertyAccessCompatibility::Promotable)
	{
		if (SourceProperty->IsA<FBoolProperty>())
		{
			if (TargetProperty->IsA<FByteProperty>())
			{
				return EStateTreePropertyCopyType::PromoteBoolToByte;
			}
			else if (TargetProperty->IsA<FIntProperty>())
			{
				return EStateTreePropertyCopyType::PromoteBoolToInt32;
			}
			else if (TargetProperty->IsA<FUInt32Property>())
			{
				return EStateTreePropertyCopyType::PromoteBoolToUInt32;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				return EStateTreePropertyCopyType::PromoteBoolToInt64;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				return EStateTreePropertyCopyType::PromoteBoolToFloat;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyCopyType::PromoteBoolToDouble;
			}
		}
		else if (SourceProperty->IsA<FByteProperty>())
		{
			if (TargetProperty->IsA<FIntProperty>())
			{
				return EStateTreePropertyCopyType::PromoteByteToInt32;
			}
			else if (TargetProperty->IsA<FUInt32Property>())
			{
				return EStateTreePropertyCopyType::PromoteByteToUInt32;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				return EStateTreePropertyCopyType::PromoteByteToInt64;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				return EStateTreePropertyCopyType::PromoteByteToFloat;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyCopyType::PromoteByteToDouble;
			}
		}
		else if (SourceProperty->IsA<FIntProperty>())
		{
			if (TargetProperty->IsA<FInt64Property>())
			{
				return EStateTreePropertyCopyType::PromoteInt32ToInt64;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				return EStateTreePropertyCopyType::PromoteInt32ToFloat;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyCopyType::PromoteInt32ToDouble;
			}
		}
		else if (SourceProperty->IsA<FUInt32Property>())
		{
			if (TargetProperty->IsA<FInt64Property>())
			{
				return EStateTreePropertyCopyType::PromoteUInt32ToInt64;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				return EStateTreePropertyCopyType::PromoteUInt32ToFloat;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyCopyType::PromoteUInt32ToDouble;
			}
		}
		else if (SourceProperty->IsA<FFloatProperty>())
		{
			if (TargetProperty->IsA<FIntProperty>())
			{
				return EStateTreePropertyCopyType::PromoteFloatToInt32;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				return EStateTreePropertyCopyType::PromoteFloatToInt64;
			}
			else if (TargetProperty->IsA<FDoubleProperty>())
			{
				return EStateTreePropertyCopyType::PromoteFloatToDouble;
			}
		}
		else if (SourceProperty->IsA<FDoubleProperty>())
		{
			if (TargetProperty->IsA<FIntProperty>())
			{
				return EStateTreePropertyCopyType::DemoteDoubleToInt32;
			}
			else if (TargetProperty->IsA<FInt64Property>())
			{
				return EStateTreePropertyCopyType::DemoteDoubleToInt64;
			}
			else if (TargetProperty->IsA<FFloatProperty>())
			{
				return EStateTreePropertyCopyType::DemoteDoubleToFloat;
			}
		}
	}
	
	ensureMsgf(false, TEXT("Couldnt determine property copy type (%s -> %s)"), *SourceProperty->GetNameCPP(), *TargetProperty->GetNameCPP());

	return EStateTreePropertyCopyType::None;
}

bool FStateTreePropertyBindingCompiler::ResolvePropertyPath(const FStateTreeBindableStructDesc& InStructDesc, const FStateTreeEditorPropertyPath& InPath,
															TArray<FStateTreePropertySegment>& OutSegments, const FProperty*& OutLeafProperty, int32& OutLeafArrayIndex,
															FStateTreeCompilerLog* InLog, const FStateTreeBindableStructDesc* InLogContextStruct)
{
	if (!InPath.IsValid())
	{
		if (InLog != nullptr && InLogContextStruct != nullptr)
		{
			InLog->Reportf(EMessageSeverity::Error, *InLogContextStruct,
					TEXT("Invalid path '%s:%s'."),
					*InStructDesc.Name.ToString(), *InPath.ToString());
		}
		return false;
	}

	// If the path is empty, we're pointing directly at the source struct.
	if (InPath.Path.Num() == 0)
	{
		OutLeafProperty = nullptr;
		OutLeafArrayIndex = INDEX_NONE;
		return true;
	}

	const UStruct* CurrentStruct = InStructDesc.Struct;
	const FProperty* LeafProperty = nullptr;
	int32 LeafArrayIndex = INDEX_NONE;
	bool bResult = true;

	for (int32 SegmentIndex = 0; SegmentIndex < InPath.Path.Num(); SegmentIndex++)
	{
		const FString& SegmentString = InPath.Path[SegmentIndex];
		const TCHAR* PropertyNamePtr = nullptr;
		int32 PropertyNameLength = 0;
		int32 ArrayIndex = INDEX_NONE;
		PropertyPathHelpers::FindFieldNameAndArrayIndex(SegmentString.Len(), *SegmentString, PropertyNameLength, &PropertyNamePtr, ArrayIndex);
		ensure(PropertyNamePtr != nullptr);
		FString PropertyNameString(PropertyNameLength, PropertyNamePtr);
		const FName PropertyName = FName(*PropertyNameString, FNAME_Find);

		const bool bFinalSegment = SegmentIndex == (InPath.Path.Num() - 1);

		if (CurrentStruct == nullptr)
		{
			if (InLog != nullptr && InLogContextStruct != nullptr)
			{
				InLog->Reportf(EMessageSeverity::Error, *InLogContextStruct,
						TEXT("Malformed path '%s:%s'."),
						*InStructDesc.Name.ToString(), *InPath.ToString(SegmentIndex, TEXT("<"), TEXT(">")));
			}
			bResult = false;
			break;
		}

		const FProperty* Property = CurrentStruct->FindPropertyByName(PropertyName);
		if (Property == nullptr)
		{
			// TODO: use core redirects to fix up the name.
			if (InLog != nullptr && InLogContextStruct != nullptr)
			{
				InLog->Reportf(EMessageSeverity::Error, *InLogContextStruct,
						TEXT("Malformed path '%s:%s', could not find property '%s%s.%s'."),
						*InStructDesc.Name.ToString(), *InPath.ToString(SegmentIndex, TEXT("<"), TEXT(">")),
						CurrentStruct->GetPrefixCPP(), *CurrentStruct->GetName(), *PropertyName.ToString());
			}
			bResult = false;
			break;
		}

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(ArrayIndex); Validation.DidFail())
		{
			if (InLog != nullptr)
			{
				Validation.Log(*InLog, TEXT("ArrayIndex"), InStructDesc);
			}
			return false;
		}

		FStateTreePropertySegment& Segment = OutSegments.AddDefaulted_GetRef();
		Segment.Name = PropertyName;
		Segment.ArrayIndex = FStateTreeIndex16(ArrayIndex);

		// Check to see if it is an array access first
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		if (ArrayProperty != nullptr && ArrayIndex != INDEX_NONE)
		{
			// It is an array, now check to see if this is an array of structures
			if (const FStructProperty* ArrayOfStructsProperty = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				Segment.Type = EStateTreePropertyAccessType::IndexArray;
				CurrentStruct = ArrayOfStructsProperty->Struct;
			}
			// if it's not an array of structs, maybe it's an array of objects
			else if (const FObjectPropertyBase* ArrayOfObjectsProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
			{
				Segment.Type = EStateTreePropertyAccessType::IndexArray;
				CurrentStruct = ArrayOfObjectsProperty->PropertyClass;

				if (!bFinalSegment)
				{
					// Object arrays need an object dereference adding if non-leaf
					FStateTreePropertySegment& ExtraSegment = OutSegments.AddDefaulted_GetRef();
					ExtraSegment.ArrayIndex = FStateTreeIndex16(0);
					ExtraSegment.Type = EStateTreePropertyAccessType::Object;
					const FProperty* InnerProperty = ArrayProperty->Inner;
					if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InnerProperty))
					{
						ExtraSegment.Type = EStateTreePropertyAccessType::Object;
					}
					else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(InnerProperty))
					{
						ExtraSegment.Type = EStateTreePropertyAccessType::WeakObject;
					}
					else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InnerProperty))
					{
						ExtraSegment.Type = EStateTreePropertyAccessType::SoftObject;
					}
				}
			}
			else
			{
				Segment.Type = EStateTreePropertyAccessType::IndexArray;
				Segment.ArrayIndex = FStateTreeIndex16(ArrayIndex);
				CurrentStruct = nullptr;
			}
		}
		// Leaf segments all get treated the same, plain, array, struct or object. Copy type is figured out separately.
		else if (bFinalSegment)
		{
			Segment.Type = EStateTreePropertyAccessType::Offset;
			CurrentStruct = nullptr;
		}
		// Check to see if this is a simple structure (eg. not an array of structures)
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			Segment.Type = EStateTreePropertyAccessType::Offset;
			CurrentStruct = StructProperty->Struct;
		}
		// Check to see if this is a simple object (eg. not an array of objects)
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			Segment.Type = EStateTreePropertyAccessType::Object;
			CurrentStruct = ObjectProperty->PropertyClass;
		}
		// Check to see if this is a simple weak object property (eg. not an array of weak objects).
		else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
		{
			Segment.Type = EStateTreePropertyAccessType::WeakObject;
			CurrentStruct = WeakObjectProperty->PropertyClass;
		}
		// Check to see if this is a simple soft object property (eg. not an array of soft objects).
		else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		{
			Segment.Type = EStateTreePropertyAccessType::SoftObject;
			CurrentStruct = SoftObjectProperty->PropertyClass;
		}
		else
		{
			if (InLog != nullptr && InLogContextStruct != nullptr)
			{
				InLog->Reportf(EMessageSeverity::Error, *InLogContextStruct,
						TEXT("Unsupported segment %s in path '%s:%s'."),
						*InStructDesc.Name.ToString(), *InPath.ToString(SegmentIndex, TEXT("<"), TEXT(">")),
						*Property->GetCPPType(), *InStructDesc.Name.ToString(), *InPath.ToString(SegmentIndex, TEXT("<"), TEXT(">")));
			}
			bResult = false;
			break;
		}

		if (bFinalSegment)
		{
			LeafProperty = Property;
			LeafArrayIndex = ArrayIndex;
		}
	}

	if (!bResult)
	{
		return false;
	}

	OutLeafProperty = LeafProperty;
	OutLeafArrayIndex = LeafArrayIndex;

	return true;
}

EPropertyAccessCompatibility FStateTreePropertyBindingCompiler::GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty)
{
	if (FromProperty == ToProperty)
	{
		return EPropertyAccessCompatibility::Compatible;
	}

	if (FromProperty == nullptr || ToProperty == nullptr)
	{
		return EPropertyAccessCompatibility::Incompatible;
	}

	// Special case for object properties since InPropertyA->SameType(InPropertyB) requires both properties to be of the exact same class.
	// In our case we want to be able to bind a source property if its class is a child of the target property class.
	if (FromProperty->IsA<FObjectPropertyBase>() && ToProperty->IsA<FObjectPropertyBase>())
	{
		const FObjectPropertyBase* SourceProperty = CastField<FObjectPropertyBase>(FromProperty);
		const FObjectPropertyBase* TargetProperty = CastField<FObjectPropertyBase>(ToProperty);
		return (SourceProperty->PropertyClass->IsChildOf(TargetProperty->PropertyClass)) ? EPropertyAccessCompatibility::Compatible : EPropertyAccessCompatibility::Incompatible;
	}

	// Extract underlying types for enums
	if (const FEnumProperty* EnumPropertyA = CastField<const FEnumProperty>(FromProperty))
	{
		FromProperty = EnumPropertyA->GetUnderlyingProperty();
	}

	if (const FEnumProperty* EnumPropertyB = CastField<const FEnumProperty>(ToProperty))
	{
		ToProperty = EnumPropertyB->GetUnderlyingProperty();
	}

	if (FromProperty->SameType(ToProperty))
	{
		return EPropertyAccessCompatibility::Compatible;
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
				return EPropertyAccessCompatibility::Promotable;
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
				return EPropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FIntProperty>())
		{
			if (ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EPropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FUInt32Property>())
		{
			if (ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EPropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FFloatProperty>())
		{
			if (ToProperty->IsA<FIntProperty>()
				|| ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FDoubleProperty>())
			{
				return EPropertyAccessCompatibility::Promotable;
			}
		}
		else if (FromProperty->IsA<FDoubleProperty>())
		{
			if (ToProperty->IsA<FIntProperty>()
				|| ToProperty->IsA<FInt64Property>()
				|| ToProperty->IsA<FFloatProperty>())
			{
				return EPropertyAccessCompatibility::Promotable;
			}
		}
	}

	return EPropertyAccessCompatibility::Incompatible;
}

void FStateTreePropertyBindingCompiler::StoreSourceStructs()
{
	// Check that existing structs are compatible
	check(PropertyBindings->SourceStructs.Num() <= SourceStructs.Num());
	for (int32 i = 0; i < PropertyBindings->SourceStructs.Num(); i++)
	{
		check(PropertyBindings->SourceStructs[i] == SourceStructs[i]);
	}

	// Add new
	if (SourceStructs.Num() > PropertyBindings->SourceStructs.Num())
	{
		for (int32 i = PropertyBindings->SourceStructs.Num(); i < SourceStructs.Num(); i++)
		{
			PropertyBindings->SourceStructs.Add(SourceStructs[i]);
		}
	}
}

