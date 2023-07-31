// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Containers/Map.h"
#include "Delegates/IDelegateInstance.h"

class FCustomizableObjectNodeParentedMaterial;
class UCustomizableObject;
class UCustomizableObjectNode;
class UEdGraphPin;
struct FEdGraphPinReference;
struct FGuid;

/** Common functionalities from EditMaterial and ExtendMaterial.
 *
 * See must call functions section! These functions should be called in the corresponding inheritor functions. */
class FCustomizableObjectNodeUseMaterial
{
public:
	virtual ~FCustomizableObjectNodeUseMaterial() = default;
	
	/** Returns true if this node uses the given Image. */
	bool UsesImage(const FGuid& ImageId) const;

	/** Given an Image id, returns its Image pin. Can nullptr if the Image is not used. */
	const UEdGraphPin* GetUsedImagePin(const FGuid& ImageId) const;

protected:
	// Begin must call functions
	void PostBackwardsCompatibleFixupWork();
	
	bool IsNodeOutDatedAndNeedsRefreshWork();

	/** Call before SetParentNode. */
	void PreSetParentNodeWork(UCustomizableObject* Object, const FGuid NodeId);

	/** Call after SetParentNode. */
	void PostSetParentNodeWork(UCustomizableObject* Object, const FGuid NodeId);

	void PinConnectionListChangedWork(UEdGraphPin* Pin);

	void CustomRemovePinWork(UEdGraphPin& Pin);
	// End must call functions

	/** Return the node which this interface belongs to. */
	virtual UCustomizableObjectNode& GetNode() = 0;
	virtual const UCustomizableObjectNode& GetNode() const;

	/** Returns the Parented Material interface this node belongs to. */
	virtual FCustomizableObjectNodeParentedMaterial& GetNodeParentedMaterial() = 0;
	
	/** Return the PinsParameter map which this interface belongs to. */
	virtual TMap<FGuid, FEdGraphPinReference>& GetPinsParameter() = 0;
	virtual const TMap<FGuid, FEdGraphPinReference>& GetPinsParameter() const;

	/** Get the output material pin. */
	virtual UEdGraphPin* OutputPin() const = 0;

private:
	/** Handler to the reconstruct node delegate. Used to remove the previously attached delegate. */
	FDelegateHandle PostReconstructNodeDelegateHandler;
	
	/** Handler to the mutable mode changed delegate. Used to remove the previously attached delegate. */
	FDelegateHandle PostTextureParameterModeChangedDelegateHandle;
};
