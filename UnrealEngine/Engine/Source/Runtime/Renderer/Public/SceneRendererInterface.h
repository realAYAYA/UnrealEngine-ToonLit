// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneView.h"
#include "ShaderParameterStructDeclaration.h"

class FSceneUniformBuffer;

DECLARE_UNIFORM_BUFFER_STRUCT(FSceneUniformParameters, RENDERER_API)

/**
 * Public interface into FSceneRenderer. Used as the scope for scene rendering functions.
 */
class ISceneRenderer
{
public:
	virtual ~ISceneRenderer() = default;

	virtual const FSceneUniformBuffer& GetSceneUniforms() const = 0;
	virtual FSceneUniformBuffer& GetSceneUniforms() = 0;

	virtual TRDGUniformBufferRef<FSceneUniformParameters> GetSceneUniformBufferRef(FRDGBuilder& GraphBuilder) = 0;
};

inline TRDGUniformBufferRef<FSceneUniformParameters> GetSceneUniformBufferRef(FRDGBuilder& GraphBuilder, const FSceneView& View)
{
	ISceneRenderer* SceneRender = View.Family ? View.Family->GetSceneRenderer() : nullptr;
	return SceneRender ? SceneRender->GetSceneUniformBufferRef(GraphBuilder) : nullptr;
}
