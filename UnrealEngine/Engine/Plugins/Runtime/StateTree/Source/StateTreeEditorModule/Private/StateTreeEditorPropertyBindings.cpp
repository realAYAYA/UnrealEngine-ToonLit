// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorPropertyBindings.h"
#include "PropertyPathHelpers.h"
#include "StateTreePropertyBindingCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeEditorPropertyBindings)


UStateTreeEditorPropertyBindingsOwner::UStateTreeEditorPropertyBindingsOwner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//////////////////////////////////////////////////////////////////////////

void FStateTreeEditorPropertyBindings::AddPropertyBinding(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath)
{
	RemovePropertyBindings(TargetPath);
	PropertyBindings.Add(FStateTreeEditorPropertyBinding(SourcePath, TargetPath));
}

void FStateTreeEditorPropertyBindings::RemovePropertyBindings(const FStateTreeEditorPropertyPath& TargetPath)
{
	PropertyBindings.RemoveAll([TargetPath](const FStateTreeEditorPropertyBinding& Binding)
		{
			return Binding.TargetPath == TargetPath;
		});
}

bool FStateTreeEditorPropertyBindings::HasPropertyBinding(const FStateTreeEditorPropertyPath& TargetPath) const
{
	return PropertyBindings.ContainsByPredicate([TargetPath](const FStateTreeEditorPropertyBinding& Binding)
		{
			return Binding.TargetPath == TargetPath;
		});
}

const FStateTreeEditorPropertyPath* FStateTreeEditorPropertyBindings::GetPropertyBindingSource(const FStateTreeEditorPropertyPath& TargetPath) const
{
	const FStateTreeEditorPropertyBinding* Binding = PropertyBindings.FindByPredicate([TargetPath](const FStateTreeEditorPropertyBinding& Binding)
		{
			return Binding.TargetPath == TargetPath;
		});
	return Binding ? &Binding->SourcePath : nullptr;
}

void FStateTreeEditorPropertyBindings::GetPropertyBindingsFor(const FGuid StructID, TArray<FStateTreeEditorPropertyBinding>& OutBindings) const
{
	OutBindings = PropertyBindings.FilterByPredicate([StructID](const FStateTreeEditorPropertyBinding& Binding)
		{
			return Binding.TargetPath.StructID == StructID && Binding.IsValid();
		});
}

void FStateTreeEditorPropertyBindings::RemoveUnusedBindings(const TMap<FGuid, const UStruct*>& ValidStructs)
{
	PropertyBindings.RemoveAll([ValidStructs](const FStateTreeEditorPropertyBinding& Binding)
		{
			// Remove binding if it's target struct has been removed
			if (!ValidStructs.Contains(Binding.TargetPath.StructID))
			{
				return true;
			}

			// Remove binding if the target property cannot be resolved, likely renamed property.
			// TODO: Try to use core redirect to find new name.
			const UStruct* Struct = ValidStructs.FindChecked(Binding.TargetPath.StructID);

			FStateTreeBindableStructDesc StructDesc;
			StructDesc.Struct = Struct;
			StructDesc.ID = Binding.TargetPath.StructID;
		
			TArray<FStateTreePropertySegment> Segments;
			const FProperty* LeafProperty = nullptr;
			int32 LeafArrayIndex = INDEX_NONE;
			if (!FStateTreePropertyBindingCompiler::ResolvePropertyPath(StructDesc, Binding.TargetPath, Segments, LeafProperty, LeafArrayIndex))
			{
				return true;
			}

			return false;
		});
}

//////////////////////////////////////////////////////////////////////////

FStateTreeBindingLookup::FStateTreeBindingLookup(IStateTreeEditorPropertyBindingsOwner* InBindingOwner)
	: BindingOwner(InBindingOwner)
{
}

const FStateTreeEditorPropertyPath* FStateTreeBindingLookup::GetPropertyBindingSource(const FStateTreeEditorPropertyPath& InTargetPath) const
{
	check(BindingOwner);
	FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	return EditorBindings->GetPropertyBindingSource(InTargetPath);
}

FText FStateTreeBindingLookup::GetPropertyPathDisplayName(const FStateTreeEditorPropertyPath& InPath) const
{
	check(BindingOwner);
	FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	FString Result;

	FStateTreeBindableStructDesc Struct;
	if (BindingOwner->GetStructByID(InPath.StructID, Struct))
	{
		Result = Struct.Name.ToString();
	}

	for (const FString& Segment : InPath.Path)
	{
		Result += TEXT(".") + Segment;
	}

	return FText::FromString(Result);
}

const FProperty* FStateTreeBindingLookup::GetPropertyPathLeafProperty(const FStateTreeEditorPropertyPath& InPath) const
{
	check(BindingOwner);
	FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings();
	check(EditorBindings);

	const FProperty* Result = nullptr;
	FStateTreeBindableStructDesc Struct;
	if (BindingOwner->GetStructByID(InPath.StructID, Struct))
	{
		// TODO: Could use inline allocator here, since there are usually only few segments. Needs API change in ResolvePropertyPath().
		TArray<FStateTreePropertySegment> Segments;
		const FProperty* LeafProperty = nullptr;
		int32 LeafArrayIndex = INDEX_NONE;
		if (FStateTreePropertyBindingCompiler::ResolvePropertyPath(Struct, InPath, Segments, LeafProperty, LeafArrayIndex))
		{
			Result = LeafProperty;
		}
	}

	return Result;
}


