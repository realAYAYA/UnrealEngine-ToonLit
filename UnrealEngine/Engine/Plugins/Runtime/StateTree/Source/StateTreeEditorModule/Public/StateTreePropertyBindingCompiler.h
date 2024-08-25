// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreePropertyBindings.h"
#include "StateTreePropertyBindingCompiler.generated.h"

enum class EPropertyAccessCompatibility;
struct FStateTreeCompilerLog;
struct FStateTreePropertyPathBinding;

/**
 * Helper class to compile editor representation of property bindings into runtime representation.
 * TODO: Better error reporting, something that can be shown in the UI.
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreePropertyBindingCompiler
{
	GENERATED_BODY()

	/**
	  * Initializes the compiler to compile copies to specified Property Bindings.
	  * @param PropertyBindings - Reference to the Property Bindings where all the batches will be stored.
	  * @return true on success.
	  */
	[[nodiscard]] bool Init(FStateTreePropertyBindings& InPropertyBindings, FStateTreeCompilerLog& InLog);

	/**
	  * Compiles a batch of property copies.
	  * @param TargetStruct - Description of the structs which contains the target properties.
	  * @param PropertyBindings - Array of bindings to compile, all bindings that point to TargetStructs will be added to the batch.
	  * @param OutBatchIndex - Resulting batch index, if index is INDEX_NONE, no bindings were found and no batch was generated.
	  * @return True on success, false on failure.
	 */
	[[nodiscard]] bool CompileBatch(const FStateTreeBindableStructDesc& TargetStruct, TConstArrayView<FStateTreePropertyPathBinding> PropertyBindings, int32& OutBatchIndex);

	/**
	  * Compiles references for selected struct
	  * @param TargetStruct - Description of the structs which contains the target properties.
	  * @param PropertyReferenceBindings - Array of bindings to compile, all bindings that point to TargetStructs will be added.
	  * @param InstanceDataView - view to the instance data
	  * @return True on success, false on failure.
	 */
	[[nodiscard]] bool CompileReferences(const FStateTreeBindableStructDesc& TargetStruct, TConstArrayView<FStateTreePropertyPathBinding> PropertyReferenceBindings, FStateTreeDataView InstanceDataView);

	/** Finalizes compilation, should be called once all batches are compiled. */
	void Finalize();

	/**
	  * Adds source struct. When compiling a batch, the bindings can be between any of the defined source structs, and the target struct.
	  * Source structs can be added between calls to Compilebatch().
	  * @param SourceStruct - Description of the source struct to add.
	  * @return Source struct index.
	  */
	int32 AddSourceStruct(const FStateTreeBindableStructDesc& SourceStruct);

	/** @return Index of a source struct by specified ID, or INDEX_NONE if not found. */
	UE_DEPRECATED(5.4, "Use GetSourceStructDescByID() instead.")
	int32 GetSourceStructIndexByID(const FGuid& ID) const;

	/** @return Reference to a source struct based on ID. */
	UE_DEPRECATED(5.4, "Use GetSourceStructDescByID() instead.")
	const FStateTreeBindableStructDesc& GetSourceStructDesc(const int32 Index) const
	{
		return SourceStructs[Index];
	}

	const FStateTreeBindableStructDesc* GetSourceStructDescByID(const FGuid& ID) const
	{
		return SourceStructs.FindByPredicate([ID](const FStateTreeBindableStructDesc& Structs) { return (Structs.ID == ID); });
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * Resolves a string based property path in specified struct into segments of property names and access types.
	 * If logging is required both, Log and LogContextStruct needs to be non-null.
	 * @param InStructDesc Description of the struct in which the property path is valid.
	 * @param InPath The property path in string format.
	 * @param OutSegments The resolved property access path as segments.
	 * @param OutLeafProperty The leaf property of the resolved path.
	 * @param OutLeafArrayIndex The left array index (or INDEX_NONE if not applicable) of the resolved path.
	 * @param Log Pointer to compiler log, or null if no logging needed.
	 * @param LogContextStruct Pointer to bindable struct desc where the property path belongs to.
	 * @return True of the property was solved successfully.
	 */
	UE_DEPRECATED(5.3, "Use FStateTreePropertyPath resolve indirection methods instead.")
	[[nodiscard]] static bool ResolvePropertyPath(const FStateTreeBindableStructDesc& InStructDesc, const FStateTreeEditorPropertyPath& InPath,
												  TArray<FStateTreePropertySegment>& OutSegments, const FProperty*& OutLeafProperty, int32& OutLeafArrayIndex,
												  FStateTreeCompilerLog* Log = nullptr, const FStateTreeBindableStructDesc* LogContextStruct = nullptr);

	/**
	 * Checks if two property types can are compatible for copying.
	 * @param FromProperty Property to copy from.
	 * @param ToProperty Property to copy to.
	 * @return Incompatible if the properties cannot be copied, Compatible if they are trivially copyable, or Promotable if numeric values can be promoted to another numeric type.
	 */
	UE_DEPRECATED(5.3, "Use FStateTreePropertyBindings::GetPropertyCompatibility instead.")
	static EPropertyAccessCompatibility GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:

	void StoreSourceStructs();

	UPROPERTY()
	TArray<FStateTreeBindableStructDesc> SourceStructs;

	/**
	 * Representation of compiled reference.
	 */
	struct FCompiledReference
	{
		FStateTreePropertyPath Path;
		FStateTreeIndex16 Index;
	};

	TArray<FCompiledReference> CompiledReferences;

	FStateTreePropertyBindings* PropertyBindings = nullptr;

	FStateTreeCompilerLog* Log = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "IPropertyAccessEditor.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorPropertyBindings.h"
#include "UObject/Interface.h"
#endif
