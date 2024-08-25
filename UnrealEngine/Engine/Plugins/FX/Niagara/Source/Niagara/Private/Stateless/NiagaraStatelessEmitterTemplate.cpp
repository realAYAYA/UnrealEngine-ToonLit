// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitterTemplate.h"
#include "Stateless/NiagaraStatelessCommon.h"
#include "Stateless/NiagaraStatelessEmitterShaders.h"
#include "NiagaraConstants.h"
#include "NiagaraModule.h"

#include "Modules/ModuleManager.h"

#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_AccelerationForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_CalculateAccurateVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_CameraOffset.h"
#include "Stateless/Modules/NiagaraStatelessModule_CurlNoiseForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_Drag.h"
#include "Stateless/Modules/NiagaraStatelessModule_GravityForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitialMeshOrientation.h"
#include "Stateless/Modules/NiagaraStatelessModule_MeshIndex.h"
#include "Stateless/Modules/NiagaraStatelessModule_MeshRotationRate.h"
#include "Stateless/Modules/NiagaraStatelessModule_RotateAroundPoint.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleColor.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleMeshSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleMeshSizeBySpeed.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSizeBySpeed.h"
#include "Stateless/Modules/NiagaraStatelessModule_ShapeLocation.h"
#include "Stateless/Modules/NiagaraStatelessModule_SolveVelocitiesAndForces.h"
#include "Stateless/Modules/NiagaraStatelessModule_SpriteFacingAndAlignment.h"
#include "Stateless/Modules/NiagaraStatelessModule_SpriteRotationRate.h"
#include "Stateless/Modules/NiagaraStatelessModule_SubUVAnimation.h"
#include "Stateless/Modules/NiagaraStatelessModule_DynamicMaterialParameters.h"

namespace NiagaraStatelessEmitterTemplateImpl
{
	TArray<TWeakObjectPtr<UNiagaraStatelessEmitterTemplate>> ObjectsToDeferredInit;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UNiagaraStatelessEmitterTemplate::PostInitProperties()
{
	Super::PostInitProperties();

	// We can end up hitting PostInitProperties before the Niagara Module has initialized bindings this needs, mark this object for deferred init and early out.
	if (FModuleManager::Get().IsModuleLoaded("Niagara") == false)
	{
		NiagaraStatelessEmitterTemplateImpl::ObjectsToDeferredInit.Add(this);
	}
	else
	{
		InitModulesAndAttributes();
	}
}

void UNiagaraStatelessEmitterTemplate::InitCDOPropertiesAfterModuleStartup()
{
	for (TWeakObjectPtr<UNiagaraStatelessEmitterTemplate>& WeakObject : NiagaraStatelessEmitterTemplateImpl::ObjectsToDeferredInit)
	{
		if (UNiagaraStatelessEmitterTemplate* EmitterObject = WeakObject.Get())
		{
			EmitterObject->InitModulesAndAttributes();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UNiagaraStatelessEmitterDefault::InitModulesAndAttributes()
{
#if WITH_EDITORONLY_DATA
	Modules =
	{
		UNiagaraStatelessModule_InitializeParticle::StaticClass(),
		UNiagaraStatelessModule_InitialMeshOrientation::StaticClass(),
		UNiagaraStatelessModule_ShapeLocation::StaticClass(),
		UNiagaraStatelessModule_CameraOffset::StaticClass(),
		UNiagaraStatelessModule_ScaleColor::StaticClass(),
		UNiagaraStatelessModule_ScaleSpriteSize::StaticClass(),
		UNiagaraStatelessModule_ScaleSpriteSizeBySpeed::StaticClass(),
		UNiagaraStatelessModule_ScaleMeshSize::StaticClass(),
		UNiagaraStatelessModule_ScaleMeshSizeBySpeed::StaticClass(),
		UNiagaraStatelessModule_MeshIndex::StaticClass(),
		UNiagaraStatelessModule_MeshRotationRate::StaticClass(),
		UNiagaraStatelessModule_AddVelocity::StaticClass(),
		UNiagaraStatelessModule_AccelerationForce::StaticClass(),
		UNiagaraStatelessModule_CurlNoiseForce::StaticClass(),
		UNiagaraStatelessModule_Drag::StaticClass(),
		UNiagaraStatelessModule_GravityForce::StaticClass(),
		UNiagaraStatelessModule_SolveVelocitiesAndForces::StaticClass(),
		UNiagaraStatelessModule_SpriteFacingAndAlignment::StaticClass(),
		UNiagaraStatelessModule_SpriteRotationRate::StaticClass(),
		UNiagaraStatelessModule_SubUVAnimation::StaticClass(),
		UNiagaraStatelessModule_DynamicMaterialParameters::StaticClass(),
	};

	const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
	OutputComponents =
	{
		StatelessGlobals.UniqueIDVariable,
		StatelessGlobals.PositionVariable,
		StatelessGlobals.CameraOffsetVariable,
		StatelessGlobals.ColorVariable,
		StatelessGlobals.DynamicMaterialParameters0Variable,
		StatelessGlobals.MeshIndexVariable,
		StatelessGlobals.MeshOrientationVariable,
		StatelessGlobals.RibbonWidthVariable,
		StatelessGlobals.ScaleVariable,
		StatelessGlobals.SpriteSizeVariable,
		StatelessGlobals.SpriteFacingVariable,
		StatelessGlobals.SpriteAlignmentVariable,
		StatelessGlobals.SpriteRotationVariable,
		StatelessGlobals.SubImageIndexVariable,
		StatelessGlobals.VelocityVariable,
		StatelessGlobals.PreviousPositionVariable,
		StatelessGlobals.PreviousCameraOffsetVariable,
		StatelessGlobals.PreviousMeshOrientationVariable,
		StatelessGlobals.PreviousRibbonWidthVariable,
		StatelessGlobals.PreviousScaleVariable,
		StatelessGlobals.PreviousSpriteSizeVariable,
		StatelessGlobals.PreviousSpriteFacingVariable,
		StatelessGlobals.PreviousSpriteAlignmentVariable,
		StatelessGlobals.PreviousSpriteRotationVariable,
		StatelessGlobals.PreviousVelocityVariable,
	};
#endif //WITH_EDITORONLY_DATA
}

const FShaderParametersMetadata* UNiagaraStatelessEmitterDefault::GetShaderParametersMetadata() const
{
	using namespace NiagaraStateless;
	return FSimulationShaderDefaultCS::FParameters::FTypeInfo::GetStructMetadata();
}

TShaderRef<NiagaraStateless::FSimulationShader> UNiagaraStatelessEmitterDefault::GetSimulationShader() const
{
	using namespace NiagaraStateless;
	return TShaderMapRef<FSimulationShaderDefaultCS>(GetGlobalShaderMap(GMaxRHIFeatureLevel));
}

//-TODO: Add a way to set them directly, we should know that the final struct is a series of ints in the order of the provided variables
void UNiagaraStatelessEmitterDefault::SetShaderParameters(uint8* ShaderParametersBase, TConstArrayView<int32> ComponentOffsets) const
{
	using namespace NiagaraStateless;
	FSimulationShaderDefaultCS::FParameters* ShaderParameters	= reinterpret_cast<FSimulationShaderDefaultCS::FParameters*>(ShaderParametersBase);
	int iComponent = 0;
	ShaderParameters->Permutation_UniqueIDComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PositionComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_CameraOffsetComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_ColorComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_DynamicMaterialParameter0Component= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_MeshIndexComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_MeshOrientationComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_RibbonWidthComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_ScaleComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SpriteSizeComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SpriteFacingComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SpriteAlignmentComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SpriteRotationComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SubImageIndexComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_VelocityComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousPositionComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousCameraOffsetComponent		= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousMeshOrientationComponent	= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousRibbonWidthComponent		= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousScaleComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousSpriteSizeComponent		= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousSpriteFacingComponent		= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousSpriteAlignmentComponent	= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousSpriteRotationComponent	= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousVelocityComponent			= ComponentOffsets[iComponent++];
}
//-TODO: Add a way to set them directly, we should know that the final struct is a series of ints in the order of the provided variables

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UNiagaraStatelessEmitterExample1::InitModulesAndAttributes()
{
#if WITH_EDITORONLY_DATA
	Modules =
	{
		UNiagaraStatelessModule_InitializeParticle::StaticClass(),
		UNiagaraStatelessModule_ShapeLocation::StaticClass(),
		UNiagaraStatelessModule_ScaleColor::StaticClass(),
		UNiagaraStatelessModule_ScaleSpriteSize::StaticClass(),
		UNiagaraStatelessModule_RotateAroundPoint::StaticClass(),
		UNiagaraStatelessModule_CalculateAccurateVelocity::StaticClass(),
	};

	const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
	OutputComponents =
	{
		StatelessGlobals.UniqueIDVariable,
		StatelessGlobals.PositionVariable,
		StatelessGlobals.ColorVariable,
		StatelessGlobals.RibbonWidthVariable,
		StatelessGlobals.ScaleVariable,
		StatelessGlobals.SpriteSizeVariable,
		StatelessGlobals.SpriteRotationVariable,
		StatelessGlobals.VelocityVariable,
		StatelessGlobals.PreviousPositionVariable,
		StatelessGlobals.PreviousRibbonWidthVariable,
		StatelessGlobals.PreviousScaleVariable,
		StatelessGlobals.PreviousSpriteSizeVariable,
		StatelessGlobals.PreviousSpriteRotationVariable,
		StatelessGlobals.PreviousVelocityVariable,
	};
#endif //WITH_EDITORONLY_DATA
}

const FShaderParametersMetadata* UNiagaraStatelessEmitterExample1::GetShaderParametersMetadata() const
{
	using namespace NiagaraStateless;
	return FSimulationShaderExample1CS::FParameters::FTypeInfo::GetStructMetadata();
}

TShaderRef<NiagaraStateless::FSimulationShader> UNiagaraStatelessEmitterExample1::GetSimulationShader() const
{
	using namespace NiagaraStateless;
	return TShaderMapRef<FSimulationShaderExample1CS>(GetGlobalShaderMap(GMaxRHIFeatureLevel));
}

//-TODO: Add a way to set them directly, we should know that the final struct is a series of ints in the order of the provided variables
void UNiagaraStatelessEmitterExample1::SetShaderParameters(uint8* ShaderParametersBase, TConstArrayView<int32> ComponentOffsets) const
{
	using namespace NiagaraStateless;
	FSimulationShaderExample1CS::FParameters* ShaderParameters		= reinterpret_cast<FSimulationShaderExample1CS::FParameters*>(ShaderParametersBase);
	int iComponent = 0;
	ShaderParameters->Permutation_UniqueIDComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PositionComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_ColorComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_RibbonWidthComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_ScaleComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SpriteSizeComponent				= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_SpriteRotationComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_VelocityComponent					= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousPositionComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousRibbonWidthComponent		= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousScaleComponent			= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousSpriteSizeComponent		= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousSpriteRotationComponent	= ComponentOffsets[iComponent++];
	ShaderParameters->Permutation_PreviousVelocityComponent			= ComponentOffsets[iComponent++];
}
//-TODO: Add a way to set them directly, we should know that the final struct is a series of ints in the order of the provided variables
