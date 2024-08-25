// Copyright Epic Games, Inc. All Rights Reserved.
#include "StateTreePropertyBindingCompiler.h"
#include "IPropertyAccessEditor.h"
#include "PropertyPathHelpers.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorPropertyBindings.h"
#include "Misc/EnumerateRange.h"
#include "StateTreePropertyBindings.h"
#include "StateTreePropertyRef.h"
#include "StateTreePropertyRefHelpers.h"
#include "StateTreePropertyHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreePropertyBindingCompiler)

bool FStateTreePropertyBindingCompiler::Init(FStateTreePropertyBindings& InPropertyBindings, FStateTreeCompilerLog& InLog)
{
	Log = &InLog;
	PropertyBindings = &InPropertyBindings;
	PropertyBindings->Reset();

	SourceStructs.Reset();
	
	return true;
}

bool FStateTreePropertyBindingCompiler::CompileBatch(const FStateTreeBindableStructDesc& TargetStruct, TConstArrayView<FStateTreePropertyPathBinding> BatchPropertyBindings, int32& OutBatchIndex)
{
	check(Log);
	check(PropertyBindings);
	OutBatchIndex = INDEX_NONE;

	StoreSourceStructs();

	struct FSortedBinding
	{
		FStateTreePropertyPathBinding Binding;
		TArray<FStateTreePropertyPathIndirection> TargetIndirections;
	};
	TArray<FSortedBinding> NewBindings;

	for (const FStateTreePropertyPathBinding& Binding : BatchPropertyBindings)
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}
		// Source must be in the source array
		const FStateTreeBindableStructDesc* SourceStruct = GetSourceStructDescByID(Binding.GetSourcePath().GetStructID());
		if (!SourceStruct)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find a binding source."));
			return false;
		}

		FString Error;
		TArray<FStateTreePropertyPathIndirection> SourceIndirections;
		TArray<FStateTreePropertyPathIndirection> TargetIndirections;
		
		if (!Binding.GetSourcePath().ResolveIndirections(SourceStruct->Struct, SourceIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *SourceStruct->ToString(), *Error);
			return false;
		}

		if (!Binding.GetTargetPath().ResolveIndirections(TargetStruct.Struct, TargetIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *TargetStruct.ToString(), *Error);
			return false;
		}

		FStateTreePropertyCopy DummyCopy;
		FStateTreePropertyPathIndirection LastSourceIndirection = !SourceIndirections.IsEmpty() ? SourceIndirections.Last() : FStateTreePropertyPathIndirection(SourceStruct->Struct);
		FStateTreePropertyPathIndirection LastTargetIndirection = !TargetIndirections.IsEmpty() ? TargetIndirections.Last() : FStateTreePropertyPathIndirection(TargetStruct.Struct);
		if (!PropertyBindings->ResolveCopyType(LastSourceIndirection, LastTargetIndirection, DummyCopy))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
			TEXT("Cannot copy properties between %s and %s, properties are incompatible."),
				*UE::StateTree::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()),
				*UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
			return false;
		}

		FSortedBinding& NewBinding = NewBindings.AddDefaulted_GetRef();
		NewBinding.Binding = FStateTreePropertyPathBinding(SourceStruct->DataHandle, Binding.GetSourcePath(), Binding.GetTargetPath());
		NewBinding.TargetIndirections = TargetIndirections;
	}

	if (!NewBindings.IsEmpty())
	{
		// Sort bindings base on copy target memory layout.
		NewBindings.StableSort([](const FSortedBinding& A, const FSortedBinding& B)
		{
			const int32 MaxSegments = FMath::Min(A.TargetIndirections.Num(), B.TargetIndirections.Num());
			for (int32 Index = 0; Index < MaxSegments; Index++)
			{
				// If property A is in struct before B, copy A first. 
				if (A.TargetIndirections[Index].GetPropertyOffset() < B.TargetIndirections[Index].GetPropertyOffset())
				{
					return true;
				}
				// If A and B points to the same property, choose the one that points to an earlier array item.
				// Note: this assumes that INDEX_NONE = -1, which means that binding directly to an array comes before an array access,
				// and non-array access will compare equal (both INDEX_NONE).
				if (A.TargetIndirections[Index].GetPropertyOffset() == B.TargetIndirections[Index].GetPropertyOffset()
					&& A.TargetIndirections[Index].GetArrayIndex() < B.TargetIndirections[Index].GetArrayIndex())
				{
					return true;
				}
			}
			// We get here if the common path is the same, shorter path wins.
			return A.TargetIndirections.Num() <= B.TargetIndirections.Num(); 
		});

		// Store bindings batch.
		const int32 BindingsBegin = PropertyBindings->PropertyPathBindings.Num();
		for (const FSortedBinding& NewBinding : NewBindings)
		{
			PropertyBindings->PropertyPathBindings.Add(NewBinding.Binding);
		}
		const int32 BindingsEnd = PropertyBindings->PropertyPathBindings.Num();

		FStateTreePropertyCopyBatch& Batch = PropertyBindings->CopyBatches.AddDefaulted_GetRef();
		Batch.TargetStruct = TargetStruct;
		Batch.BindingsBegin = IntCastChecked<uint16>(BindingsBegin);
		Batch.BindingsEnd = IntCastChecked<uint16>(BindingsEnd);
		OutBatchIndex = PropertyBindings->CopyBatches.Num() - 1;
	}

	return true;
}

bool FStateTreePropertyBindingCompiler::CompileReferences(const FStateTreeBindableStructDesc& TargetStruct, TConstArrayView<FStateTreePropertyPathBinding> PropertyReferenceBindings, FStateTreeDataView InstanceDataView)
{
	for (const FStateTreePropertyPathBinding& Binding : PropertyReferenceBindings)
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}

		// Source must be in the source array/
		const FStateTreeBindableStructDesc* SourceStruct = GetSourceStructDescByID(Binding.GetSourcePath().GetStructID());
		if (!SourceStruct)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find a binding source."));
			return false;
		}

		FString Error;
		TArray<FStateTreePropertyPathIndirection> SourceIndirections;
		
		if (!Binding.GetSourcePath().ResolveIndirections(SourceStruct->Struct, SourceIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *SourceStruct->ToString(), *Error);
			return false;
		}

		if (!UE::StateTree::PropertyRefHelpers::IsPropertyAccessibleForPropertyRef(SourceIndirections, *SourceStruct))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("%s cannot reference non-output %s "),
					*UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
					*UE::StateTree::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()));
			return false;
		}

		TArray<FStateTreePropertyIndirection> TargetIndirections;
		FStateTreePropertyIndirection TargetFirstIndirection;
		FStateTreePropertyPathIndirection TargetLeafIndirection;
		if (!FStateTreePropertyBindings::ResolvePath(InstanceDataView.GetStruct(), Binding.GetTargetPath(), TargetIndirections, TargetFirstIndirection, TargetLeafIndirection))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *TargetStruct.ToString(), *Error);
			return false;
		}

		if (!UE::StateTree::PropertyRefHelpers::IsPropertyRefCompatibleWithProperty(*TargetLeafIndirection.GetProperty(), *SourceIndirections.Last().GetProperty()))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("%s cannot reference %s, types are incompatible."),		
				*UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
				*UE::StateTree::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()));
			return false;
		}

		FStateTreeIndex16 ReferenceIndex;

		// Reuse the index if another PropertyRef already references the same property.
		{
			int32 IndexOfAlreadyExisting = PropertyBindings->PropertyReferencePaths.IndexOfByPredicate([&Binding](const FStateTreePropertyRefPath& RefPath)
			{
				return RefPath.GetSourcePath() == Binding.GetSourcePath();
			});

			if (IndexOfAlreadyExisting != INDEX_NONE)
			{
				ReferenceIndex = FStateTreeIndex16(IndexOfAlreadyExisting);
			}
		}

		if (!ReferenceIndex.IsValid())
		{
			// If referencing another PropertyRef, reuse it's index.
			if (UE::StateTree::PropertyRefHelpers::IsPropertyRef(*SourceIndirections.Last().GetProperty()))
			{
				const FCompiledReference* ReferencedReference = CompiledReferences.FindByPredicate([&Binding](const FCompiledReference& CompiledReference)
				{
					return CompiledReference.Path == Binding.GetSourcePath();
				});

				if (ReferencedReference)
				{
					ReferenceIndex = ReferencedReference->Index;
				}
				else
				{
					if(!UE::StateTree::PropertyHelpers::HasOptionalMetadata(*TargetLeafIndirection.GetProperty()))
					{
						Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Referenced %s is not bound"), *UE::StateTree::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()));
						return false;
					}
							
					return true;
				}
			}
		}

		if (!ReferenceIndex.IsValid())
		{
			ReferenceIndex = FStateTreeIndex16(PropertyBindings->PropertyReferencePaths.Num());
			PropertyBindings->PropertyReferencePaths.Emplace(SourceStruct->DataHandle, Binding.GetSourcePath());
		}

		// Store index in instance data.
		uint8* RawData = FStateTreePropertyBindings::GetAddress(InstanceDataView, TargetIndirections, TargetFirstIndirection, TargetLeafIndirection.GetProperty());
		check(RawData);
		reinterpret_cast<FStateTreePropertyRef*>(RawData)->RefAccessIndex = ReferenceIndex;

		FCompiledReference& CompiledReference = CompiledReferences.AddDefaulted_GetRef();
		CompiledReference.Path = Binding.GetTargetPath();
		CompiledReference.Index = ReferenceIndex;
	}

	return true;
}

void FStateTreePropertyBindingCompiler::Finalize()
{
	StoreSourceStructs();
}

int32 FStateTreePropertyBindingCompiler::AddSourceStruct(const FStateTreeBindableStructDesc& SourceStruct)
{
	const FStateTreeBindableStructDesc* ExistingStruct = SourceStructs.FindByPredicate([&SourceStruct](const FStateTreeBindableStructDesc& Struct) { return (Struct.ID == SourceStruct.ID); });
	if (ExistingStruct)
	{
		UE_LOG(LogStateTree, Error, TEXT("%s already exists as %s using ID '%s'"),
			*SourceStruct.ToString(), *ExistingStruct->ToString(), *ExistingStruct->ID.ToString());
	}
	
	UE_CLOG(!SourceStruct.DataHandle.IsValid(), LogStateTree, Error, TEXT("%s does not have a valid data handle."), *SourceStruct.ToString());
	
	SourceStructs.Add(SourceStruct);
	return SourceStructs.Num() - 1;
}

int32 FStateTreePropertyBindingCompiler::GetSourceStructIndexByID(const FGuid& ID) const
{
	return SourceStructs.IndexOfByPredicate([ID](const FStateTreeBindableStructDesc& Structs) { return (Structs.ID == ID); });
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
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
	const EStateTreePropertyAccessCompatibility Result = FStateTreePropertyBindings::GetPropertyCompatibility(FromProperty, ToProperty);
	if (Result == EStateTreePropertyAccessCompatibility::Compatible)
	{
		return EPropertyAccessCompatibility::Compatible;
	}
	if (Result == EStateTreePropertyAccessCompatibility::Promotable)
	{
		return EPropertyAccessCompatibility::Promotable;
	}
	return EPropertyAccessCompatibility::Incompatible;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

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
