// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerEditorActor.h"

class UMLDeformerComponent;
class UGeometryCacheComponent;

namespace UE::MLDeformer
{
	/**
	 * An editor actor with a geometry cache component on it.
	 * This can for example be used as ground truth viewport actors.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerGeomCacheActor
		: public FMLDeformerEditorActor
	{
	public:
		FMLDeformerGeomCacheActor(const FConstructSettings& Settings);
		virtual ~FMLDeformerGeomCacheActor() override;

		void SetGeometryCacheComponent(UGeometryCacheComponent* Component)	{ GeomCacheComponent = Component; }
		UGeometryCacheComponent* GetGeometryCacheComponent() const			{ return GeomCacheComponent; }

		// FMLDeformerEditorActor overrides.
		virtual void SetVisibility(bool bIsVisible) override;
		virtual bool IsVisible() const override;
		virtual bool HasVisualMesh() const override;
		virtual void SetPlayPosition(float TimeInSeconds, bool bAutoPause = true) override;
		virtual float GetPlayPosition() const override;
		virtual void SetPlaySpeed(float PlaySpeed) override;
		virtual void Pause(bool bPaused) override;
		virtual bool IsPlaying() const override;
		virtual FBox GetBoundingBox() const override;
		// ~END FMLDeformerEditorActor overrides.

	protected:
		/** The geometry cache component. */
		TObjectPtr<UGeometryCacheComponent> GeomCacheComponent = nullptr;
	};
}	// namespace UE::MLDeformer
