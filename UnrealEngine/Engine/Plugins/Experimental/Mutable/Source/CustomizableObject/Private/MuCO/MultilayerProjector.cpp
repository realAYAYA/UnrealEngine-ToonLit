// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/MultilayerProjector.h"

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "Templates/Tuple.h"


void FMultilayerProjectorLayer::Read(const FCustomizableObjectInstanceDescriptor& Descriptor, const FMultilayerProjector& MultilayerProjector, const int32 Index)
{
	checkCode(MultilayerProjector.CheckDescriptorParameters(Descriptor));
	check(Index >= 0 && Index < MultilayerProjector.NumLayers(Descriptor)); // Layer out of range.
	
	const FString ParamName = MultilayerProjector.ParamName.ToString();
	
	{
		const int32 ProjectorParamIndex = Descriptor.FindProjectorParameterNameIndex(ParamName);
		const FCustomizableObjectProjector& Projector = Descriptor.GetProjectorParameters()[ProjectorParamIndex].RangeValues[Index];
		Position = static_cast<FVector3d>(Projector.Position);
		Direction = static_cast<FVector3d>(Projector.Direction);
		Up = static_cast<FVector3d>(Projector.Up);
		Scale = static_cast<FVector3d>(Projector.Scale);
		Angle = Projector.Angle;
	}
	
	{
		const int32 ImageParamIndex = Descriptor.FindIntParameterNameIndex(ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX);
		Image = Descriptor.GetIntParameters()[ImageParamIndex].ParameterRangeValueNames[Index];
	}

	{
		const int32 OpacityParamIndex = Descriptor.FindFloatParameterNameIndex(ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX);
		Opacity = Descriptor.GetFloatParameters()[OpacityParamIndex].ParameterRangeValues[Index];
	}
}


void FMultilayerProjectorLayer::Write(FCustomizableObjectInstanceDescriptor& Descriptor, const FMultilayerProjector& MultilayerProjector, const int32 Index) const
{
	checkCode(MultilayerProjector.CheckDescriptorParameters(Descriptor));
	check(Index >= 0 && Index < MultilayerProjector.NumLayers(Descriptor)); // Layer out of range.

	const FString ParamName = MultilayerProjector.ParamName.ToString();

	{
		const int32 ProjectorParamIndex = Descriptor.FindProjectorParameterNameIndex(ParamName);
		FCustomizableObjectProjector& Projector = Descriptor.GetProjectorParameters()[ProjectorParamIndex].RangeValues[Index];
		Projector.Position = static_cast<FVector3f>(Position);
		Projector.Direction = static_cast<FVector3f>(Direction);
		Projector.Up = static_cast<FVector3f>(Up);
		Projector.Scale = static_cast<FVector3f>(Scale);
		Projector.Angle = Angle;
	}
	
	{
		const int32 ImageParamIndex = Descriptor.FindIntParameterNameIndex(ParamName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX);
		Descriptor.GetIntParameters()[ImageParamIndex].ParameterRangeValueNames[Index] = Image;
	}

	{
		const int32 OpacityParamIndex = Descriptor.FindFloatParameterNameIndex(ParamName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX);
		Descriptor.GetFloatParameters()[OpacityParamIndex].ParameterRangeValues[Index] = Opacity;
	}
}


uint32 GetTypeHash(const FMultilayerProjectorLayer& Key)
{
	uint32 Hash = GetTypeHash(Key.Position);

	Hash = HashCombine(Hash, GetTypeHash(Key.Direction));
	Hash = HashCombine(Hash, GetTypeHash(Key.Up));
	Hash = HashCombine(Hash, GetTypeHash(Key.Scale));
	Hash = HashCombine(Hash, GetTypeHash(Key.Angle));
	Hash = HashCombine(Hash, GetTypeHash(Key.Image));
	Hash = HashCombine(Hash, GetTypeHash(Key.Opacity));

	return Hash;
}


FMultilayerProjectorVirtualLayer::FMultilayerProjectorVirtualLayer(const FMultilayerProjectorLayer& Layer, const bool bEnabled, const int32 Order):
	FMultilayerProjectorLayer(Layer),
	bEnabled(bEnabled),
	Order(Order)
{
}


const FString FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX = FString("_NumLayers");

const FString FMultilayerProjector::OPACITY_PARAMETER_POSTFIX = FString("_Opacity");

const FString FMultilayerProjector::IMAGE_PARAMETER_POSTFIX = FString("_SelectedImages");

const FString FMultilayerProjector::POSE_PARAMETER_POSTFIX = FString("_SelectedPoses");


int32 FMultilayerProjector::NumLayers(const FCustomizableObjectInstanceDescriptor& Descriptor) const
{
	const FString NumLayersParamName = ParamName.ToString() + NUM_LAYERS_PARAMETER_POSTFIX;
	
	const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(NumLayersParamName);
	check(FloatParameterIndex != -1); // Parameter not found.

	return Descriptor.GetFloatParameters()[FloatParameterIndex].ParameterValue;
}


void FMultilayerProjector::CreateLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const int32 Index) const
{
	checkCode(CheckDescriptorParameters(Descriptor));
	check(Index >= 0 && Index <= NumLayers(Descriptor)); // Layer is non-contiguous or out of range.

	const UCustomizableObject* Object = Descriptor.GetCustomizableObject(); 
	check(Object);
	
	// Num Layers.
	{
        const FString NumLayersParamName = ParamName.ToString() + NUM_LAYERS_PARAMETER_POSTFIX;
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(NumLayersParamName);

        Descriptor.GetFloatParameters()[FloatParameterIndex].ParameterValue += 1;
    }
	// Projector Range.
	{
		const int32 ProjectorParameterIndex = Descriptor.FindProjectorParameterNameIndex(ParamName.ToString());
		
		FCustomizableObjectProjectorParameterValue& ProjectorParameter = Descriptor.GetProjectorParameters()[ProjectorParameterIndex];
		const FCustomizableObjectProjector Projector = Descriptor.GetProjectorDefaultValue(Object->FindParameter(ParamName.ToString()));
		ProjectorParameter.RangeValues.Insert(Projector, Index);
	}
	
	// Selected Image Range.
	{
		const int32 IntParameterIndex = Descriptor.FindIntParameterNameIndex(ParamName.ToString() + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX);

		FCustomizableObjectIntParameterValue& IntParameter = Descriptor.GetIntParameters()[IntParameterIndex];
		const int32 ParamIndexInObject = Object->FindParameter(IntParameter.ParameterName);

		const FString DefaultValue = Object->GetIntParameterAvailableOption(ParamIndexInObject, 0); // TODO: Define the default option in the editor instead of taking the first available, like it's currently defined for GetProjectorDefaultValue()
		IntParameter.ParameterRangeValueNames.Insert(DefaultValue, Index);
	}
	
	// Opacity Range.
	{
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(ParamName.ToString() + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX);

		FCustomizableObjectFloatParameterValue& FloatParameter = Descriptor.GetFloatParameters()[FloatParameterIndex];
		FloatParameter.ParameterRangeValues.Insert(0.5, Index); // TODO: Define the default float in the editor instead of [0.5f], like it's currently defined for GetProjectorDefaultValue()
	}
}


void FMultilayerProjector::RemoveLayerAt(FCustomizableObjectInstanceDescriptor& Descriptor, const int32 Index) const
{
	checkCode(CheckDescriptorParameters(Descriptor));
	check(Index >= 0 && Index < NumLayers(Descriptor)); // Layer out of range.
	
	// Num Layers.
	{
		const FString NumLayersParamName = ParamName.ToString() + NUM_LAYERS_PARAMETER_POSTFIX;
        const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(NumLayersParamName);
		
    	Descriptor.GetFloatParameters()[FloatParameterIndex].ParameterValue -= 1;
    }
    
	// Projector Range.
	{
		const int32 ProjectorParameterIndex = Descriptor.FindProjectorParameterNameIndex(ParamName.ToString());
		
		FCustomizableObjectProjectorParameterValue& ProjectorParameter = Descriptor.GetProjectorParameters()[ProjectorParameterIndex];
		ProjectorParameter.RangeValues.RemoveAt(Index);
	}
	
	// Selected Image Range.
	{
		const int32 IntParameterIndex = Descriptor.FindIntParameterNameIndex(ParamName.ToString() + IMAGE_PARAMETER_POSTFIX);
		
		FCustomizableObjectIntParameterValue& IntParameter = Descriptor.GetIntParameters()[IntParameterIndex];
		IntParameter.ParameterRangeValueNames.RemoveAt(Index);
	}
	
	// Opacity Range.
	{
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(ParamName.ToString() + OPACITY_PARAMETER_POSTFIX);
		
		FCustomizableObjectFloatParameterValue& FloatParameter = Descriptor.GetFloatParameters()[FloatParameterIndex];
		FloatParameter.ParameterRangeValues.RemoveAt(Index);
	}
}


FMultilayerProjectorLayer FMultilayerProjector::GetLayer(const FCustomizableObjectInstanceDescriptor& Descriptor, const int32 Index) const
{
	FMultilayerProjectorLayer MultilayerProjectorLayer;
	MultilayerProjectorLayer.Read(Descriptor, *this, Index);
	return MultilayerProjectorLayer;
}


void FMultilayerProjector::UpdateLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const int32 Index, const FMultilayerProjectorLayer& Layer) const
{
	Layer.Write(Descriptor, *this, Index);
}


TArray<FName> FMultilayerProjector::GetVirtualLayers() const
{
	TArray<FName> VirtualLayers;
	VirtualLayersMapping.GetKeys(VirtualLayers);
	return VirtualLayers;
}


void FMultilayerProjector::CreateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id)
{
	if (!VirtualLayersMapping.Contains(Id))
	{
		const int32 Index = NumLayers(Descriptor);
		
		CreateLayer(Descriptor, Index);
		VirtualLayersMapping.Add(Id, Index);
		VirtualLayersOrder.Add(Id, NEW_VIRTUAL_LAYER_ORDER);
	}
}


FMultilayerProjectorVirtualLayer FMultilayerProjector::FindOrCreateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id)
{
	FMultilayerProjectorLayer Layer;
	bool bEnabled;
	int32 Order;

	if (const int32* Index = VirtualLayersMapping.Find(Id))
	{
		if (*Index == VIRTUAL_LAYER_DISABLED)
		{
			Layer = DisableVirtualLayers[Id];
			bEnabled = false;
		}
		else
		{
			Layer = GetLayer(Descriptor, *Index);
			bEnabled = true;
		}

		Order = VirtualLayersOrder[Id];
	}
	else
	{
		const int32 NewIndex = NumLayers(Descriptor);
		constexpr int32 NewOrder = NEW_VIRTUAL_LAYER_ORDER;
		
		CreateLayer(Descriptor, NewIndex);
		VirtualLayersMapping.Add(Id, NewIndex);
		VirtualLayersOrder.Add(Id, NewOrder);

		Layer = GetLayer(Descriptor, NewIndex);
		bEnabled = true;
		Order = NewOrder;
	}

	return FMultilayerProjectorVirtualLayer(Layer, bEnabled, Order);
}


void FMultilayerProjector::RemoveVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id)
{
	const int32* Index = VirtualLayersMapping.Find(Id);
	check(Index); // Virtual Layer not created.
	
	if (*Index == VIRTUAL_LAYER_DISABLED)
	{
		DisableVirtualLayers.Remove(Id);
	}
	else
	{
		RemoveLayerAt(Descriptor, *Index);
		
		for (TMap<FName, int32>::TIterator It =  VirtualLayersMapping.CreateIterator(); It; ++It)
		{
			if (It.Key() == Id)
			{
				It.RemoveCurrent();
			}
			else if (It.Value() > *Index) // Update following Layers.
			{
				--It.Value();
			}
		}
	}

	VirtualLayersOrder.Remove(Id);
}


FMultilayerProjectorVirtualLayer FMultilayerProjector::GetVirtualLayer(const FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id) const
{
	const int32* Index = VirtualLayersMapping.Find(Id);
	check(Index); // Virtual Layer not created.

	const FMultilayerProjectorLayer Layer = GetLayer(Descriptor, *Index);
	const bool bEnabled = *Index != VIRTUAL_LAYER_DISABLED;
	const int32 Order = VirtualLayersOrder[Id];
	
	return FMultilayerProjectorVirtualLayer(Layer, bEnabled, Order);
}


void FMultilayerProjector::UpdateVirtualLayer(FCustomizableObjectInstanceDescriptor& Descriptor, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer)
{
	const int32* Index = VirtualLayersMapping.Find(Id);
	check(Index); // Virtual Layer not created.

	const bool bEnabled = *Index != VIRTUAL_LAYER_DISABLED;
	
	if (!bEnabled)
	{
		DisableVirtualLayers[Id] = static_cast<FMultilayerProjectorLayer>(Layer); // Update disabled layer.
		VirtualLayersOrder[Id] = Layer.Order;
	}
	else
	{
		int32* Order = VirtualLayersOrder.Find(Id);
		if (*Order != Layer.Order) // Order changed, check if it needs to be moved.
		{
			const int32 OldIndex = *Index;
			int32 NewIndex = CalculateVirtualLayerIndex(Id, Layer.Order);
			if (OldIndex != NewIndex) // Move required. Could be optimized by moving only the in-between values.
			{
				RemoveLayerAt(Descriptor, OldIndex);
				UpdateMappingVirtualLayerDisabled(Id, OldIndex);

				if (OldIndex < NewIndex)
				{
					NewIndex -= 1;
				}
				
				CreateLayer(Descriptor, NewIndex);
				UpdateMappingVirtualLayerEnabled(Id, NewIndex);
			}
			
			*Order = Layer.Order;
		}

		UpdateLayer(Descriptor, *Index, static_cast<FMultilayerProjectorLayer>(Layer)); // Update enabled layer.
	}
	
	// Enable or disable virtual layer.
	if (Layer.bEnabled && !bEnabled)
	{
		const int32 NewIndex = CalculateVirtualLayerIndex(Id, VirtualLayersOrder[Id]);

		CreateLayer(Descriptor, NewIndex);
		UpdateMappingVirtualLayerEnabled(Id, NewIndex);

		UpdateLayer(Descriptor, NewIndex, static_cast<FMultilayerProjectorLayer>(Layer));
		
		DisableVirtualLayers.Remove(Id);
	}
	else if (!Layer.bEnabled && bEnabled)
	{
		RemoveLayerAt(Descriptor, *Index);
		UpdateMappingVirtualLayerDisabled(Id, *Index);
		
		DisableVirtualLayers.Add(Id, Layer);
	}
}


FMultilayerProjector::FMultilayerProjector(const FName& InParamName) :
	ParamName(InParamName)
{
}


int32 FMultilayerProjector::CalculateVirtualLayerIndex(const FName& Id, const int32 InsertOrder) const
{
	int32 LayerBeforeIndex = -1;
	int32 LayerBeforeOrder = -1;
	
	for (const TTuple<FName, int>& MappingTuple : VirtualLayersMapping) // Find closest smallest layer.
	{
		if (MappingTuple.Value != VIRTUAL_LAYER_DISABLED && MappingTuple.Key != Id)
		{
			const int32 LayerOrder = VirtualLayersOrder[MappingTuple.Key];
			if (LayerOrder <= InsertOrder)
			{
				if ((LayerOrder > LayerBeforeOrder) ||
					(LayerOrder == LayerBeforeOrder && MappingTuple.Value > LayerBeforeIndex))
				{
					LayerBeforeIndex = MappingTuple.Value;
					LayerBeforeOrder = LayerOrder;
				}
			}
		}
	}
	
	return LayerBeforeIndex + 1;
}


void FMultilayerProjector::UpdateMappingVirtualLayerEnabled(const FName& Id, const int32 Index)
{
	for (TTuple<FName, int>& Tuple : VirtualLayersMapping)
	{
		if (Tuple.Key == Id)
		{
			Tuple.Value = Index;
		}
		else if (Tuple.Value >= Index) // Update following Layers.
		{
			++Tuple.Value;
		}
	}
}


void FMultilayerProjector::UpdateMappingVirtualLayerDisabled(const FName& Id, const int32 Index)
{
	for (TTuple<FName, int>& Tuple : VirtualLayersMapping)
	{
		if (Tuple.Key == Id)
    	{
			Tuple.Value = VIRTUAL_LAYER_DISABLED;
    	}
		else if (Tuple.Value > Index) // Update following Layers.
		{
			--Tuple.Value;
		}
	}
}


void FMultilayerProjector::CheckDescriptorParameters(const FCustomizableObjectInstanceDescriptor& Descriptor) const
{
	const FString ParamNameString = ParamName.ToString();

	// Num layers.
	{
		const FString NumLayersParamName = ParamNameString + NUM_LAYERS_PARAMETER_POSTFIX;
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(NumLayersParamName);
		check(FloatParameterIndex >= 0); // Descriptor Parameter does not exist.
	}
    
	// Projector.
	{
		const int32 ProjectorParameterIndex = Descriptor.FindProjectorParameterNameIndex(ParamNameString);
		check(ProjectorParameterIndex >= 0) // Descriptor Parameter does not exist.
	}
	
	// Selected Image.
	{
		const int32 IntParameterIndex = Descriptor.FindIntParameterNameIndex(ParamNameString + IMAGE_PARAMETER_POSTFIX);
		check(IntParameterIndex >= 0) // Descriptor Parameter does not exist.
	}
	
	// Opacity.
	{
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(ParamNameString + OPACITY_PARAMETER_POSTFIX);
		check(FloatParameterIndex >= 0) // Descriptor Parameter does not exist.
	}
}


bool FMultilayerProjector::AreDescriptorParametersValid(const FCustomizableObjectInstanceDescriptor& Descriptor, const FName& ParamName)
{
	const FString ParamNameString = ParamName.ToString();

	// Num layers.
	{
		const FString NumLayersParamName = ParamNameString + NUM_LAYERS_PARAMETER_POSTFIX;
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(NumLayersParamName);
		if (FloatParameterIndex < 0)
		{
			return false;
		}
	}
    
	// Projector.
	{
		const int32 ProjectorParameterIndex = Descriptor.FindProjectorParameterNameIndex(ParamNameString);
		if (ProjectorParameterIndex < 0)
		{
			return false;
		}
	}
	
	// Selected Image.
	{
		const int32 IntParameterIndex = Descriptor.FindIntParameterNameIndex(ParamNameString + IMAGE_PARAMETER_POSTFIX);
		if (IntParameterIndex < 0)
		{
			return false;
		}
	}
	
	// Opacity.
	{
		const int32 FloatParameterIndex = Descriptor.FindFloatParameterNameIndex(ParamNameString + OPACITY_PARAMETER_POSTFIX);
		if (FloatParameterIndex < 0)
		{
			return false;
		}
	}

	return true;
}


uint32 GetTypeHash(const FMultilayerProjector& Key)
{
	uint32 Hash = GetTypeHash(Key.ParamName);

	for (const TTuple<FName, int32>& Pair : Key.VirtualLayersMapping)
	{
		Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
	
	for (const TTuple<FName, int32>& Pair : Key.VirtualLayersOrder)
	{
		Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}

	for (const TTuple<FName, FMultilayerProjectorLayer>& Pair : Key.DisableVirtualLayers)
	{
		Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
	
	return Hash;
}