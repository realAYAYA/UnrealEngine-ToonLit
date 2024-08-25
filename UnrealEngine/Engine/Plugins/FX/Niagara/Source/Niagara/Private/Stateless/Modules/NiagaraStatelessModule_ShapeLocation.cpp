// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/Modules/NiagaraStatelessModule_ShapeLocation.h"
#include "Stateless/NiagaraStatelessDrawDebugContext.h"

void UNiagaraStatelessModule_ShapeLocation::SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const
{
	FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
	if (!IsModuleEnabled())
	{
		Parameters->ShapeLocation_Mode.X		= 0;
		Parameters->ShapeLocation_Parameters0	= FVector4f::Zero();
		Parameters->ShapeLocation_Parameters1	= FVector4f::Zero();
		return;
	}

	switch (ShapePrimitive)
	{
		case ENSM_ShapePrimitive::Box:
		{
			Parameters->ShapeLocation_Mode.X		= 0;
			Parameters->ShapeLocation_Mode.Y		= bBoxSurfaceOnly ? 1 : 0;
			Parameters->ShapeLocation_Parameters0	= FVector4f(BoxSize, BoxSurfaceThicknessMax - BoxSurfaceThicknessMin);
			Parameters->ShapeLocation_Parameters1	= FVector4f(BoxSize * -0.5f, BoxSurfaceThicknessMin);
			break;
		}
		case ENSM_ShapePrimitive::Plane:
		{
			Parameters->ShapeLocation_Mode.X		= 0;
			Parameters->ShapeLocation_Parameters0	= FVector4f(PlaneSize.X, PlaneSize.Y, 0.0f, 0.0f);
			Parameters->ShapeLocation_Parameters1	= FVector4f(-PlaneSize.X * 0.5f, -PlaneSize.Y * 0.5f, 0.0f, 0.0f);
			break;
		}
		case ENSM_ShapePrimitive::Cylinder:
		{
			Parameters->ShapeLocation_Mode.X		= 1;
			Parameters->ShapeLocation_Parameters0.X = CylinderHeight;
			Parameters->ShapeLocation_Parameters0.Y = CylinderHeight * -CylinderHeightMidpoint;
			Parameters->ShapeLocation_Parameters0.Z = CylinderRadius;
			break;
		}
		case ENSM_ShapePrimitive::Ring:
		{
			const float DC = FMath::Clamp(1.0f - DiscCoverage, 0.0f, 1.0f);
			const float SDC = DC > 0.0f ? FMath::Sqrt(DC) : 0.0f;

			Parameters->ShapeLocation_Mode.X		= 2;
			Parameters->ShapeLocation_Parameters0.X = RingRadius * (1.0f - SDC);
			Parameters->ShapeLocation_Parameters0.Y = RingRadius * SDC;
			Parameters->ShapeLocation_Parameters0.Z = -UE_TWO_PI * (1.0f - RingUDistribution);
			Parameters->ShapeLocation_Parameters0.W = 0.0f;
			break;
		}
		case ENSM_ShapePrimitive::Sphere:
		{
			Parameters->ShapeLocation_Mode.X		= 3;
			Parameters->ShapeLocation_Parameters0.X = SphereMax - SphereMin;
			Parameters->ShapeLocation_Parameters0.Y = SphereMin;
			break;
		}
		default:
		{
			Parameters->ShapeLocation_Mode.X = 0;
			Parameters->ShapeLocation_Parameters0 = FVector4f::Zero();
			Parameters->ShapeLocation_Parameters1 = FVector4f::Zero();
			checkNoEntry();
		}
	}
}

#if WITH_EDITOR
void UNiagaraStatelessModule_ShapeLocation::DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const
{
	switch (ShapePrimitive)
	{
		case ENSM_ShapePrimitive::Box:
		{
			DrawDebugContext.DrawBox(FVector::ZeroVector, FVector(BoxSize * 0.5f));
			break;
		}
		case ENSM_ShapePrimitive::Plane:
		{
			DrawDebugContext.DrawBox(FVector::ZeroVector, FVector(PlaneSize.X * 0.5f, PlaneSize.Y * 0.5f, 0.0f));
			break;
		}
		case ENSM_ShapePrimitive::Cylinder:
		{
			DrawDebugContext.DrawCylinder(CylinderHeight, CylinderRadius, CylinderHeightMidpoint);
			break;
		}
		case ENSM_ShapePrimitive::Ring:
		{
			const float DC = FMath::Clamp(1.0f - DiscCoverage, 0.0f, 1.0f);
			const float SDC = DC > 0.0f ? FMath::Sqrt(DC) : 0.0f;
			DrawDebugContext.DrawCircle(FVector::ZeroVector, RingRadius);
			DrawDebugContext.DrawCircle(FVector::ZeroVector, RingRadius * SDC);
			break;
		}
		case ENSM_ShapePrimitive::Sphere:
		{
			DrawDebugContext.DrawSphere(FVector::ZeroVector, SphereMin);
			DrawDebugContext.DrawSphere(FVector::ZeroVector, SphereMax);
			break;
		}
	}
}
#endif
