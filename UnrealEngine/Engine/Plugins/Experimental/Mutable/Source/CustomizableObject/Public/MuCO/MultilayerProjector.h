// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"

#include "MultilayerProjector.generated.h"

struct FCustomizableObjectInstanceDescriptor;
class UCustomizableObjectInstance;
struct FMultilayerProjector;


/** Data structure representing a Multilayer Projector Layer.
 *
 * This struct is not actually saved, its values are obtained from the Instance Parameters. */
USTRUCT(BlueprintType)
struct FMultilayerProjectorLayer
{
	GENERATED_BODY()
	
	/** Read the Layer from the Instance Parameters.
	 *
	 * @param Descriptor Instance Descriptor.
	 * @param MultilayerProjector Multilayer Projector helper.
	 * @param Index Index where to read the Layer in the Instance Parameters.
	 */
	void Read(const FCustomizableObjectInstanceDescriptor& Descriptor, const FMultilayerProjector& MultilayerProjector, int32 Index);

	/** Write to Layer to the Instance Parameters.
	 *
 	 * @param Descriptor Instance Descriptor.
	 * @param MultilayerProjector Multilayer Projector helper.
	 * @param Index Index where to write the Layer in the Instance Parameters.
	 */
	void Write(FCustomizableObjectInstanceDescriptor& Descriptor, const FMultilayerProjector& MultilayerProjector, int32 Index) const;
	
	/** Layer position. */
	FVector Position;

	/** Layer direction vector. */
	FVector Direction;

	/** Layer up direction vector. */
	FVector Up;

	/** Layer scale. */
	FVector Scale;

	/** Layer angle*/
	float Angle;

	/** Layer selected image. */
	FString Image;	

	/** Layer image opacity. */
	float Opacity;
};


uint32 GetTypeHash(const FMultilayerProjectorLayer& Key);


/** Data structure representing a Multilayer Projector Virtual Layer. */
USTRUCT(BlueprintType)
struct FMultilayerProjectorVirtualLayer : public FMultilayerProjectorLayer
{
	GENERATED_BODY()

	FMultilayerProjectorVirtualLayer() = default;

	FMultilayerProjectorVirtualLayer(const FMultilayerProjectorLayer& Layer, bool bEnabled, int32 Order);

	/** True if the Virtual Layer is enabled. */
	bool bEnabled;

	/** Virtual Layer absolute order. Equal values may result in an undefined behaviour. */
	int32 Order;
};


/** Multilayer Projector Helper. Eases the management of Layers and Virtual Layers.
 *
 * Layer: Management of Multilayer Projector Layers by index.
 * All layers indices has to be consecutive.
 * 
 * Virtual Layer: Management of Multilayer Projector Layers by name.
 * - Allows to enabled and disabled layers.
 * - Allows to sort layers.
 */
USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMultilayerProjector
{
	GENERATED_BODY()

	friend FCustomizableObjectInstanceDescriptor;
	friend FMultilayerProjectorLayer;
	friend uint32 GetTypeHash(const FMultilayerProjector& Key);


	// Parameters encoding
	static const FString NUM_LAYERS_PARAMETER_POSTFIX;
	static const FString OPACITY_PARAMETER_POSTFIX;
	static const FString IMAGE_PARAMETER_POSTFIX;
	static const FString POSE_PARAMETER_POSTFIX;

	// Constructors
	FMultilayerProjector() = default;

	explicit FMultilayerProjector(const FName& InParamName);

	// Layers
	
	/** Returns the number of layers. */
	int32 NumLayers(const FCustomizableObjectInstanceDescriptor& Descriptor) const;

	/** Insert the Layer at the given index moving all the following layers one position. */
	void CreateLayer(FCustomizableObjectInstanceDescriptor& Descriptor, int32 Index) const;

	/** Remove the Layer at the given index moving all the following layers one position. */
	void RemoveLayerAt(FCustomizableObjectInstanceDescriptor& Descriptor, int32 Index) const;

	/** Get the properties of the Layer at the given index. */
	FMultilayerProjectorLayer GetLayer(const FCustomizableObjectInstanceDescriptor& Descriptor, int32 Index) const;

	/** Update the Layer properties at the given index. */
	void UpdateLayer(FCustomizableObjectInstanceDescriptor& Descriptor, int32 Index, const FMultilayerProjectorLayer& Layer) const;

	// Virtual layers

	/** Get all created Virtual Layers. */
	TArray<FName> GetVirtualLayers() const;
	
	/** Given a new identifier, create a new Virtual Layer (if non-existent).*/
	void CreateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id);

	/** Given a new identifier, find or create a new Virtual Layer.*/
	FMultilayerProjectorVirtualLayer FindOrCreateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id);

	/** Given its identifier, remove the Virtual Layer. */
	void RemoveVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id);

	/** Given its identifier, get the Virtual Layer properties. */
	FMultilayerProjectorVirtualLayer GetVirtualLayer(const FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id) const;

	/** Given its identifier, update a Virtual Layer properties. */
	void UpdateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer);

private:
	/** Multilayer Projector Parameter name. */
	UPROPERTY()
	FName ParamName;
	
	/** Maps a Virtual Layer to a Layer.
	 *
 	 * All enabled Virtual Layers must have an entry pointing to the Layer index.
 	 * Disabled layers should have VIRTUAL_LAYER_DISABLED as value.
	 */
	UPROPERTY()
	TMap<FName, int32> VirtualLayersMapping;

	/** Maps a Virtual Layer to a Layer.
	 *
	 * All created Virtual Layers (enabled or disabled) must have an entry to this map.
	 */
	UPROPERTY()
	TMap<FName, int32> VirtualLayersOrder;
	
	/** Since disabling a Virtual Layers removes the Layer, its values are no longer on the Instance Parameters.
	 * Instead, they are saved in this data structure to be able to reenable it later. */
	UPROPERTY()
	TMap<FName, FMultilayerProjectorLayer> DisableVirtualLayers;

	/** Virtual Layer Disabled Index. */
	static constexpr int32 VIRTUAL_LAYER_DISABLED = -1;

	/** New Virtual Layer default order. */
	static constexpr int32 NEW_VIRTUAL_LAYER_ORDER = 0;

	/** Taking into consideration the Virtual Layer Order, Calculate where the Layer should be inserted. */
	int32 CalculateVirtualLayerIndex(const FName& Id, int32 InsertOrder) const;
	
	/** Given an inserted Virtual Layer, update the mapping of the Virtual Layers.
 	 *
 	 * @param Id Virtual Layer.
	 * @param Index Index which the layer has been enabled.
	 */
	void UpdateMappingVirtualLayerEnabled(const FName& Id, int32 Index);
	
	/** Given a removed Virtual Layer, update the mapping of the Virtual Layers.
	 *
 	 * @param Id Virtual Layer.	
	 * @param Index Index which the layer has been disabled.
	 */
	void UpdateMappingVirtualLayerDisabled(const FName& Id, int32 Index);

	/** Rise an assert if the Instance Descriptor does not contain the necessary Parameters. */
	void CheckDescriptorParameters(const FCustomizableObjectInstanceDescriptor& Descriptor) const;
	
	/** Return false if the Instance does not contain the necessary Instance Parameters. */
	static bool AreDescriptorParametersValid(const FCustomizableObjectInstanceDescriptor& Descriptor, const FName& ParamName);
};


uint32 GetTypeHash(const FMultilayerProjector& Key);

