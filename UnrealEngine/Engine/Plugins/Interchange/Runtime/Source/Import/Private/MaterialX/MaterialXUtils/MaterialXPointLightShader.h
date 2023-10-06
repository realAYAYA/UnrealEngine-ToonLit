// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "MaterialX/MaterialXUtils/MaterialXLightShaderAbstract.h"

class FMaterialXPointLightShader : public FMaterialXLightShaderAbstract
{
protected:

	template<typename T, ESPMode>
	friend class SharedPointerInternals::TIntrusiveReferenceController;

	FMaterialXPointLightShader(UInterchangeBaseNodeContainer & BaseNodeContainer);

public:

	static TSharedRef<FMaterialXBase> MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer);

	virtual void Translate(MaterialX::NodePtr PointLightShaderNode) override;

	static float GetDecayRate(MaterialX::InputPtr DecayRateInput);

	static void GetPosition(MaterialX::InputPtr PositionInput, FTransform& Transform);

	void SetDecayRate();

	void SetPosition();
};
#endif