// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/NiagaraStatelessDrawDebugContext.h"

void UNiagaraStatelessModule_AddVelocity::BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const
{
	if (!IsModuleEnabled())
	{
		return;
	}

	NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
	if (VelocityType == ENSM_VelocityType::Linear)
	{
		const FNiagaraStatelessRangeVector3 VelocityRange = LinearVelocityDistribution.CalculateRange(FVector3f::ZeroVector);
		PhysicsBuildData.VelocityRange.Min += VelocityRange.Min * LinearVelocityScale;
		PhysicsBuildData.VelocityRange.Max += VelocityRange.Max * LinearVelocityScale;
	}
	else if (VelocityType == ENSM_VelocityType::FromPoint)
	{
		ensureMsgf(PhysicsBuildData.bPointVelocity == false, TEXT("Only a single point force is supported at the moment."));

		PhysicsBuildData.bPointVelocity = true;
		PhysicsBuildData.PointVelocityRange = PointVelocityDistribution.CalculateRange(0.0f);
		PhysicsBuildData.PointOrigin = PointOrigin;
	}
	else if (VelocityType == ENSM_VelocityType::InCone)
	{
		ensureMsgf(PhysicsBuildData.bConeVelocity == false, TEXT("Only a single cone force is supported at the moment."));

		PhysicsBuildData.bConeVelocity = true;
		PhysicsBuildData.ConeQuat = FQuat4f(ConeRotation.Quaternion());
		PhysicsBuildData.ConeVelocityRange = ConeVelocityDistribution.CalculateRange(0.0f);
		PhysicsBuildData.ConeOuterAngle = ConeAngle;
		PhysicsBuildData.ConeInnerAngle = InnerCone;
		PhysicsBuildData.ConeVelocityFalloff = bSpeedFalloffFromConeAxisEnabled ? FMath::Clamp(SpeedFalloffFromConeAxis, 0.0f, 1.0f) : 0.0f;
	}
}

#if WITH_EDITOR
void UNiagaraStatelessModule_AddVelocity::DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const
{
	switch (VelocityType)
	{
		case ENSM_VelocityType::Linear:
		{
			const FNiagaraStatelessRangeVector3 VelocityRange = LinearVelocityDistribution.CalculateRange(FVector3f::ZeroVector);
			const FVector MinDir = FVector(VelocityRange.Min * LinearVelocityScale);
			const FVector MaxDir = FVector(VelocityRange.Max * LinearVelocityScale);
			DrawDebugContext.DrawArrow(FVector::ZeroVector, MinDir);

			if (!FMath::IsNearlyEqual(MinDir.Length(), MaxDir.Length()))
			{
				DrawDebugContext.DrawArrow(FVector::ZeroVector, MaxDir);
			}
			break;
		}

		case ENSM_VelocityType::InCone:
		{
			const FQuat Quat = ConeRotation.Quaternion();
			const float ConeHAngle = ConeAngle / 2.0f;

			TOptional<float> InnerConeHAngle;
			if (InnerCone > 0.0f && !FMath::IsNearlyEqual(ConeAngle, InnerCone))
			{
				InnerConeHAngle = InnerCone / 2.0f;
			}

			const FNiagaraStatelessRangeFloat ConeVelocityRange = ConeVelocityDistribution.CalculateRange(0.0f);
			DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, ConeHAngle, ConeVelocityRange.Min);
			if ( InnerConeHAngle.IsSet() )
			{
				DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, InnerConeHAngle.GetValue(), ConeVelocityRange.Min);
			}

			if (!FMath::IsNearlyEqual(ConeVelocityRange.Min, ConeVelocityRange.Max))
			{
				DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, ConeHAngle, ConeVelocityRange.Max);
				if (InnerConeHAngle.IsSet())
				{
					DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, InnerConeHAngle.GetValue(), ConeVelocityRange.Max);
				}
			}
			break;
		}

		case ENSM_VelocityType::FromPoint:
		{
			const FNiagaraStatelessRangeFloat PointVelocityRange = PointVelocityDistribution.CalculateRange(0.0f);
			if (!FMath::IsNearlyEqual(PointVelocityRange.Min, 0.0f))
			{
				DrawDebugContext.DrawSphere(FVector(PointOrigin), PointVelocityRange.Min);
			}
			if (!FMath::IsNearlyEqual(PointVelocityRange.Min, PointVelocityRange.Max))
			{
				DrawDebugContext.DrawSphere(FVector(PointOrigin), PointVelocityRange.Max);
			}
			break;
		}
	}
}
#endif
