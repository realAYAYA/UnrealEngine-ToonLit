// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "MaterialX/MaterialXUtils/MaterialXSurfaceShaderAbstract.h"

class FMaterialXSurfaceUnlitShader : public FMaterialXSurfaceShaderAbstract
{
protected:
	
	template<typename T, ESPMode>
	friend class SharedPointerInternals::TIntrusiveReferenceController;

	FMaterialXSurfaceUnlitShader(UInterchangeBaseNodeContainer & BaseNodeContainer);

public:

	static TSharedRef<FMaterialXBase> MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer);

	virtual void Translate(MaterialX::NodePtr SurfaceUnlitNode) override;
};
#endif