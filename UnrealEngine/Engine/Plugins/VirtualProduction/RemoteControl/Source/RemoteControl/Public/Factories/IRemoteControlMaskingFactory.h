// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRemoteControlModule.h"

/**
 * Reference to a UObject or one of its properties for the purpose of masking.
 */
struct FRCMaskingOperation
{
	FRCMaskingOperation() = default;

	explicit FRCMaskingOperation(FRCFieldPathInfo InPathInfo, UObject* InObject)
		: OperationId(FGuid::NewGuid())
		, ObjectRef(ERCAccess::NO_ACCESS, InObject, InPathInfo)
	{
		check(InObject);
	}

	explicit FRCMaskingOperation(const FRCObjectReference& InObjectRef)
		: OperationId(FGuid::NewGuid())
		, ObjectRef(InObjectRef)
	{
	}

	bool HasMask(ERCMask InMaskBit) const
	{
		return (Masks & InMaskBit) != ERCMask::NoMask;
	}

	bool IsValid() const
	{
		return OperationId.IsValid() && ObjectRef.IsValid();
	}

	friend bool operator==(const FRCMaskingOperation& LHS, const FRCMaskingOperation& RHS)
	{
		return LHS.OperationId == RHS.OperationId && LHS.ObjectRef == RHS.ObjectRef;
	}

	friend uint32 GetTypeHash(const FRCMaskingOperation& MaskingOperation)
	{
		return HashCombine(GetTypeHash(MaskingOperation.OperationId), GetTypeHash(MaskingOperation.ObjectRef));
	}

public:

	/** Unique identifier of the operation being performed. */
	FGuid OperationId;

	/** Masks to be applied. */
	ERCMask Masks = RC_AllMasks;

	/** Holds Object reference. */
	FRCObjectReference ObjectRef;

	/** Holds the state of this RC property before applying any masking. */
	FVector4 PreMaskingCache = FVector4::Zero();
};

/**
 * Factory which is responsible for masking support for FRemoteControlProperty.
 */
class IRemoteControlMaskingFactory : public TSharedFromThis<IRemoteControlMaskingFactory>
{
public:

	/** Virtual destructor */
	virtual ~IRemoteControlMaskingFactory(){}

	/**
	 * Applies masked values to the given struct property.
	 * @param InMaskingOperation Shared reference of the masking operation to perform.
	 */
	virtual void ApplyMaskedValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation, bool bIsInteractive) = 0;

	/**
	 * Caches premasking values from the given struct property.
	 * @param InMaskingOperation Shared reference of the masking operation to perform.
	 */
	virtual void CacheRawValues(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) = 0;

	/**
	 * Whether the factory support exposed entity.
	 * @param ScriptStruct Static struct of exposed property.
	 * @return true if the script struct is supported by given factory
	 */
	virtual bool SupportsExposedEntity(UScriptStruct* ScriptStruct) const = 0;
};

