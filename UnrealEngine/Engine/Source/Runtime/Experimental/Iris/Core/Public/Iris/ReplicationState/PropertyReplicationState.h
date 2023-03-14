// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Templates/RefCounting.h"
 
namespace UE::Net
{

 /** 
 * ReplicationState created at runtime using a descriptor built from existing reflection data
 * StateBuffer contains storage for all properties described by the descriptor,
 * When polling data from the source object properties we will compare the value with what we have stored in the statebuffer
 * and mark the member as dirty.if the value differs
 *
 * Polling is quite expensive but we only do it once per update of the replication system
 */
class FPropertyReplicationState
{
public:
	/** Construct a new state, StateBuffer will be allocated and constructed according to data in the descriptor */
	IRISCORE_API explicit FPropertyReplicationState(const FReplicationStateDescriptor* Descriptor);

	/** Construct a new state, using already constructed state in InStateBuffer */
	IRISCORE_API FPropertyReplicationState(const FReplicationStateDescriptor* Descriptor, uint8* InStateBuffer);

	IRISCORE_API ~FPropertyReplicationState();

	/** Copy constructor, will not copy internal data */
	IRISCORE_API FPropertyReplicationState(const FPropertyReplicationState& Other);

	/** 
	 * Assignment operator, it is debatable if this should be provided or not.
	 * if we are copying to a bound state we will perform a Set which will do a full property compare to properly update dirtiness
	 * if we are copying to a loose state we will just to a copy of all properties contained in the state, changemask will also be copied
	*/
	IRISCORE_API FPropertyReplicationState& operator=(const FPropertyReplicationState& Other);

	/** Set from Other this will update dirtiness by comparing all properties */
	IRISCORE_API void Set(const FPropertyReplicationState& Other);
	
	/** Either StateBuffer is initialized and owned by this instance, or we have been injected with a state from the network system
		A default constructed state is not valid
	*/
	IRISCORE_API bool IsValid() const;

	//
	// Note: There is little to none error checking in the methods below, they are mostly intended to be used from internal code and tests
	// Expected usage for this type of ReplicationState is through the polling system
	//

	/** Set the value at the provided Index, 
		The UProperty is looked up from the descriptor using the index, if the value differs the statemask is updated
		Mostly intended for test code
		Normal use of this class is through the poll layer
		TODO: It would be nice if we could provide some validation on property types
	*/
	IRISCORE_API void SetPropertyValue(uint32 Index, const void* SrcValue);

	/** Set the value at the provided Index, written into DstValue. 
		The UProperty is looked up from the descriptor using the index, if the value differs the statemask is updated
		Mostly intended for test code
		Normal use of this class is through the poll layer
		TODO: It would be nice if we could provide some validation on property types
	*/
	IRISCORE_API void GetPropertyValue(uint32 Index, void* DstValue) const;

	/** Need array editors?, to edit arrays with statemasks */
	/** PropertyNetArrayEditor EditPropertyAsArray(uint32 Index); */
	/** const PropertyArrayReader GetPropertyAsArray(uint32 Index) */

	/** Is the property at the given index dirty, for properties with multiple bits in the statemask this will return true if any of those bits are set */
	IRISCORE_API bool IsDirty(uint32 Index) const;

	/** Explicitly mark the the property at the given Index as dirty, for properties with multiple bits in the statemask this will mark all bits dirty */
	IRISCORE_API void MarkDirty(uint32 Index);

	/** $IRIS TODO: Move Poll/Push/CallRepNotifies out of PropertyReplicationState as loose functions */

	/** Poll src data from properties in SrcData, Note: SrcData is a UClass/UStruct containing properties
		Compare and update representation in DstStateBuffer and update ChangeMask 
		This is currently used to track and update dirtiness for the properties
		$TODO: THIS requires some thought as the property offset we get in all are expressed from the base, but we do generate descriptors for all parts of the inheritance chain
		do we carry around the base pointer or should we recalculate the offsets when we build the descriptor
	*/
	IRISCORE_API void PollPropertyReplicationState(const void* RESTRICT SrcData);

	/** Push received state data to properties in DstData buffer, Note: DstData is a UClass/UStruct containing properties
		Compare and update representation in DstStateBuffer and update ChangeMask 
	*/
	IRISCORE_API void PushPropertyReplicationState(void* RESTRICT DstData, bool bPushAll = false) const;


	/**
	  * Poll data from object referencing properties in SrcData, Note: SrcData is a UObject containing properties.
	  * Behaves like PollPropertyReplicationState when it comes to dirtiness and such.
	*/
	IRISCORE_API void PollObjectReferences(const void* RESTRICT SrcData);

	/** Invoke repnotifies for all dirty members
	*/
	IRISCORE_API void CallRepNotifies(void* RESTRICT DstData, const FPropertyReplicationState* PreviousState, bool bIsInit) const;

	/** Debug output state to FString */
	IRISCORE_API FString ToString(bool bIncludeAll = true) const;

	/** Debug output state to StringBuilder */
	IRISCORE_API const TCHAR* ToString(FStringBuilderBase& StringBuilder, bool bIncludeAll = true) const;

	const FReplicationStateDescriptor* GetReplicationStateDescriptor() const { return ReplicationStateDescriptor; }
	uint8* GetStateBuffer() const { return StateBuffer; }

public:
	// These methods are for internal use only.

	// Polls source data for a single property. If the property has changed it will be marked as dirty.
	void PollProperty(const void* SrcData, uint32 Index);

	/**
	 * If state has custom conditionals then this function will check whether the condition is enabled or not.
	 * If state doesn't have custom conditionals it will return true.
	 */
	IRISCORE_API bool IsCustomConditionEnabled(uint32 Index) const;

private:
	bool IsInitState() const;
	void ConstructStateInternal();
	void DestructStateInternal();
	void InjectState(const FReplicationStateDescriptor* Descriptor, uint8* InStateBuffer);

	TRefCountPtr<const FReplicationStateDescriptor> ReplicationStateDescriptor;
	uint8* StateBuffer;
	uint32 bOwnState : 1;
};

}
