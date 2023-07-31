// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SubobjectData.h"

class UObject;
struct FSubobjectDataHandle;

/**
 * Container that represents the subobject data of a child actor component.
 */
struct SUBOBJECTDATAINTERFACE_API FInheritedSubobjectData : public FSubobjectData
{
	friend class USubobjectDataSubsystem;
	
	FInheritedSubobjectData(UObject* ContextObject, const FSubobjectDataHandle& ParentHandle, const bool bIsInheritedSCS);
	
	// FSubobjectData interface
	virtual bool IsNativeComponent() const override;
	virtual bool CanEdit() const override;
	virtual bool CanDelete() const override;
	virtual bool IsInheritedSCSNode() const override;
	// End FSubobjectData

protected:

	/** True if this SCS node is inherited from another blueprint generated class. */
	bool bIsInheritedSCS;
};