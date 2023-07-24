// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "IWaveformTransformation.h"
#include "WaveformTransformationRenderLayerFactory.h"

class IWaveformTransformationRenderer;
class FWaveformTransformationDurationRenderer;
class USoundWave;
class UWaveformTransformationBase;

using FTransformationLayerConstraints = TPair<float, float>;
using FTransformationRenderLayerInfo = TPair<TSharedPtr<IWaveformTransformationRenderer>, FTransformationLayerConstraints>;
using FTransformationToPropertiesPair = TPair<TObjectPtr<UWaveformTransformationBase>, TArray<TSharedRef<IPropertyHandle>>>;
using FTransformationsToPropertiesArray = TArray<FTransformationToPropertiesPair>;

DECLARE_MULTICAST_DELEGATE(FOnRenderElementsUpdated)
DECLARE_DELEGATE_OneParam(FOnTransformationsPropertiesRequired, FTransformationsToPropertiesArray& /*Transformations Properties To Hand*/)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLayersChainGenerated, FTransformationRenderLayerInfo* /*First Layer ptr*/, const int32 /* NLayers */)
DECLARE_MULTICAST_DELEGATE_SixParams(FOnRenderDataGenerated, const uint8* /* First RawPCMData element */, const uint32 /*Num Samples*/, const uint32 /*First Edited Sample*/, const uint32 /*Last Edited Sample*/, const uint32/*Sample Rate*/, const uint16 /*Num Channels*/)


/*************************************************************************************/
/* FWaveformTransformationsRenderManager											 */
/* The Waveform Transformations render manager can produce UI to display a chain 	 */
/* of waveform transformations														 */
/*  																				 */
/* The main UI elements created are:												 */
/*  Render Data : a uint8 array containing the transformed samples of the waveform   */ 
/*  Transform Layers : an array of UI renderers for each transformation				 */ 
/*  in the chain. Renderers should be contained in dummy widgets. 					 */
/* 																					 */
/* These are created so that the entire stack of transformation is displayable.		 */
/* E.g : if a 10s long file is trimmed from second 2, seconds 0 to 1 will 			 */
/* still be present in the render data.												 */
/* UI widgets are passed a struct with information about the transformation to       */
/* display property (e.g. StartFrameOffset, Sampleduration ecc)                      */
/* 																					 */
/* UIs for different transformations are registered withand spawned by 				 */
/* FWaveformTransformationRenderLayerFactory.										 */
/* Transformations don't necessarily have a widget UI. In that case they are 		 */
/* only reflected in  the render data. 												 */
/*************************************************************************************/


class WAVEFORMTRANSFORMATIONSWIDGETS_API FWaveformTransformationsRenderManager
{
public:
	explicit FWaveformTransformationsRenderManager(
		TObjectPtr<USoundWave> InSoundWave,
		FOnTransformationsPropertiesRequired InTransformationChainPropertiesHandler
	);

	/** Used to generate the stack of transformations UI					*/
	/** Should be called when the waveform transformation chain is changed	*/
	void GenerateLayersChain();

	/** Used to generate updated render data and pass transformation info to the widgets */
	/** Should be called when the transformations parameters are changed				 */
	void UpdateRenderElements();

	TArrayView<const FTransformationRenderLayerInfo> GetTransformLayers() const;

	/** Called when a new layer chain of transformations UI is created */
	FOnLayersChainGenerated OnLayersChainGenerated;

	/** Called when new render data is generated */
	FOnRenderDataGenerated OnRenderDataGenerated;

	/** Called when the different render elements are updated */
	FOnRenderElementsUpdated OnRenderElementsUpdated;

private:
	void GenerateRenderDataInternal();

	void CreateDurationHighlightLayer();

	TArray<Audio::FTransformationPtr> CreateTransformations() const;
	const bool CanChainChangeFileLength(const TArray<Audio::FTransformationPtr>& TransformationArray) const;

	TArray<TObjectPtr<UWaveformTransformationBase>> TransformationsToRender;
	TArray<FTransformationRenderLayerInfo> RenderLayers;

	TObjectPtr<USoundWave> SoundWaveToRender = nullptr;

	TArray<uint8> RawPCMData;

	TUniquePtr<FWaveformTransformationRenderLayerFactory> LayersFactory = nullptr;
	TSharedPtr<FWaveformTransformationDurationRenderer> DurationHiglightLayer = nullptr;

	FOnTransformationsPropertiesRequired ChainPropertiesHandler;

	uint32 NumOriginalWaveformFrames = 0;
};