// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "StateTreeTypes.h"
#include "StateTreeLinker.generated.h"

UENUM()
enum class EStateTreeLinkerStatus : uint8
{
	Succeeded,
	Failed,
};

/**
 * The StateTree linker is used to resolved references to various StateTree data at load time.
 * @see TStateTreeExternalDataHandle<> for example usage.
 */
struct FStateTreeLinker
{
	explicit FStateTreeLinker(const UStateTreeSchema* InSchema) : Schema(InSchema) {}
	
	/** Sets base index for all external data handles. */
	void SetExternalDataBaseIndex(const int32 InExternalDataBaseIndex) { ExternalDataBaseIndex = InExternalDataBaseIndex; }

	/** Sets currently linked item's instance data type and index. */ 
	void SetCurrentInstanceDataType(const UStruct* Struct, const int32 Index)
	{
		CurrentInstanceStruct = Struct;
		CurrentInstanceIndex = Index;
	}

	EStateTreeLinkerStatus GetStatus() const { return Status; }
	
	/**
	 * Links reference to an external UObject.
	 * @param Handle Reference to TStateTreeExternalDataHandle<> with UOBJECT type to link to.
	 */
	template <typename T>
	typename TEnableIf<TIsDerivedFrom<typename T::DataType, UObject>::IsDerived, void>::Type LinkExternalData(T& Handle)
	{
		LinkExternalData(Handle, T::DataType::StaticClass(), T::DataRequirement);
	}

	/**
	 * Links reference to an external UObject.
	 * @param Handle Reference to TStateTreeExternalDataHandle<> with USTRUCT type to link to.
	 */
	template <typename T>
	typename TEnableIf<!TIsDerivedFrom<typename T::DataType, UObject>::IsDerived, void>::Type LinkExternalData(T& Handle)
	{
		LinkExternalData(Handle, T::DataType::StaticStruct(), T::DataRequirement);
	}

	/**
	 * Links reference to an external Object or Struct.
	 * This function should only be used when TStateTreeExternalDataHandle<> cannot be used, i.e. the Struct is based on some data.
	 * @param Handle Reference to link to.
	 * @param Struct Expected type of the Object or Struct to link to.
	 * @param Requirement Describes if the external data is expected to be required or optional.
	 */
	void LinkExternalData(FStateTreeExternalDataHandle& Handle, const UStruct* Struct, const EStateTreeExternalDataRequirement Requirement)
	{
		if (Schema != nullptr && !Schema->IsExternalItemAllowed(*Struct))
		{
			UE_LOG(LogStateTree, Error,
				TEXT("External data of type '%s' used by current node is not allowed by schema '%s' (i.e. rejected by IsExternalItemAllowed)"),
				*Struct->GetName(),
				*Schema->GetClass()->GetName());

			Handle = FStateTreeExternalDataHandle();
			Status = EStateTreeLinkerStatus::Failed;
			return;
		}
		
		const FStateTreeExternalDataDesc Desc(Struct, Requirement);
		int32 Index = ExternalDataDescs.Find(Desc);
		if (Index == INDEX_NONE)
		{
			Index = ExternalDataDescs.Add(Desc);
			check(FStateTreeExternalDataHandle::IsValidIndex(Index + ExternalDataBaseIndex));
			ExternalDataDescs[Index].Handle.DataViewIndex = FStateTreeIndex8(Index + ExternalDataBaseIndex);
		}
		Handle.DataViewIndex = FStateTreeIndex8(Index + ExternalDataBaseIndex);
	}

	/** @return linked external data descriptors. */
	TConstArrayView<FStateTreeExternalDataDesc> GetExternalDataDescs() const { return ExternalDataDescs; }

protected:

	const UStateTreeSchema* Schema = nullptr;
	EStateTreeLinkerStatus Status = EStateTreeLinkerStatus::Succeeded;
	const UStruct* CurrentInstanceStruct = nullptr;
	int32 CurrentInstanceIndex = INDEX_NONE;
	int32 ExternalDataBaseIndex = 0;
	TArray<FStateTreeExternalDataDesc> ExternalDataDescs;
};
