// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


struct FGuid;

class UCustomizableObject;
class UCustomizableObjectNode;

/** Interface for a node that saves a parent node. */
class ICustomizableObjectNodeParentedNode
{
public:
	virtual ~ICustomizableObjectNodeParentedNode() {};

	/** Save the parent node reference. */
	virtual void SetParentNode(UCustomizableObject* Object, FGuid NodeId);
	
	/** Return the saved parent material node. Can return nullptr. */
	UCustomizableObjectNode* GetParentNode() const;

protected:
	/** Save the parent node reference. Do not call directly, use SetParentNode instead. */
	virtual void SaveParentNode(UCustomizableObject* Object, FGuid NodeId) = 0;

	virtual FGuid GetParentNodeId() const = 0;

	virtual UCustomizableObject* GetParentObject() const = 0;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "HAL/Platform.h"
#include "Misc/Guid.h"
#endif
