// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessCommon.h"

#include "Stateless/NiagaraStatelessEmitterTemplate.h"

namespace NiagaraStatelessCommon
{
	static FNiagaraStatelessGlobals GGlobals;

	void Initialize()
	{
		GGlobals.CameraOffsetVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CameraOffset"));
		GGlobals.ColorVariable						= FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color"));
		GGlobals.DynamicMaterialParameters0Variable = FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("DynamicMaterialParameter"));
		GGlobals.MeshIndexVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), TEXT("MeshIndex"));
		GGlobals.MeshOrientationVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetQuatDef(), TEXT("MeshOrientation"));
		GGlobals.PositionVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Position"));
		GGlobals.RibbonWidthVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("RibbonWidth"));
		GGlobals.ScaleVariable						= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale"));
		GGlobals.SpriteAlignmentVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SpriteAlignment"));
		GGlobals.SpriteFacingVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SpriteFacing"));
		GGlobals.SpriteSizeVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec2Def(), TEXT("SpriteSize"));
		GGlobals.SpriteRotationVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SpriteRotation"));
		GGlobals.SubImageIndexVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SubImageIndex"));
		GGlobals.UniqueIDVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), TEXT("UniqueID"));
		GGlobals.VelocityVariable					= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity"));

		GGlobals.PreviousCameraOffsetVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Previous.CameraOffset"));
		//GGlobals.PreviousColorVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetColorDef(), TEXT("Previous.Color"));
		//GGlobals.PreviousDynamicMaterialParameters0Variable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Previous.DynamicMaterialParameter"));
		GGlobals.PreviousMeshOrientationVariable	= FNiagaraVariableBase(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Previous.MeshOrientation"));
		GGlobals.PreviousPositionVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Previous.Position"));
		GGlobals.PreviousRibbonWidthVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Previous.RibbonWidth"));
		GGlobals.PreviousScaleVariable				= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.Scale"));
		GGlobals.PreviousSpriteAlignmentVariable	= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.SpriteAlignment"));
		GGlobals.PreviousSpriteFacingVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.SpriteFacing"));
		GGlobals.PreviousSpriteSizeVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Previous.SpriteSize"));
		GGlobals.PreviousSpriteRotationVariable		= FNiagaraVariableBase(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Previous.SpriteRotation"));
		GGlobals.PreviousVelocityVariable			= FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Previous.Velocity"));

		UNiagaraStatelessEmitterTemplate::InitCDOPropertiesAfterModuleStartup();
	}
} //NiagaraStatelessCommon

const FNiagaraStatelessGlobals& FNiagaraStatelessGlobals::Get()
{
	return NiagaraStatelessCommon::GGlobals;
}
