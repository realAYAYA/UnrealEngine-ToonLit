// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraVertexFactory.h"
#include "ShaderParameterUtils.h"
#include "MeshMaterialShader.h"

IMPLEMENT_TYPE_LAYOUT(FNiagaraVertexFactoryShaderParametersBase);

void FNiagaraVertexFactoryShaderParametersBase::Bind(const FShaderParameterMap& ParameterMap)
{

}

void FNiagaraVertexFactoryShaderParametersBase::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType VertexStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	const FNiagaraVertexFactoryBase* NiagaraVF = static_cast<const FNiagaraVertexFactoryBase*>(VertexFactory);
}
