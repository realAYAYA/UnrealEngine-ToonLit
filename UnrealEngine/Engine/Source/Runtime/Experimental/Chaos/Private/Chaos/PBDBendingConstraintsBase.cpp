// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDBendingConstraintsBase.h"

namespace Chaos::Softs 
{
	namespace Private
	{
		void Calculate3DRestAngles(
			const FSolverParticles& InParticles,
			const TArray<TVec4<int32>>& Constraints,
			TArray<FSolverReal>& RestAngles)
		{
			RestAngles.Reset(Constraints.Num());
			for (const TVec4<int32>& Constraint : Constraints)
			{
				const FSolverVec3& P1 = InParticles.X(Constraint[0]);
				const FSolverVec3& P2 = InParticles.X(Constraint[1]);
				const FSolverVec3& P3 = InParticles.X(Constraint[2]);
				const FSolverVec3& P4 = InParticles.X(Constraint[3]);
				RestAngles.Add(FMath::Clamp(FPBDBendingConstraintsBase::CalcAngle(P1, P2, P3, P4), -UE_PI, UE_PI));
			}
		}

		void CalculateFlatnessRestAngles(
			const FSolverParticles& InParticles,
			int32 InParticleOffset,
			int32 InParticleCount,
			const TConstArrayView<FRealSingle>& RestAngleMap,
			const FSolverVec2& RestAngleValue,
			const TArray<TVec4<int32>>& Constraints,
			TArray<FSolverReal>& RestAngles)
		{
			if (RestAngleMap.Num() != InParticleCount && RestAngleValue[0] == 1.f)
			{
				// Special case where Flatness = 1, i.e., all RestAngles are 0.
				RestAngles.SetNumZeroed(Constraints.Num());
				return;
			}

			auto FlatnessValue = [InParticleOffset, InParticleCount, &RestAngleMap, &RestAngleValue](const TVec4<int32>& Constraint)->FSolverReal
			{
				if (RestAngleMap.Num() == InParticleCount)
				{
					return RestAngleValue[0] + (RestAngleValue[1] - RestAngleValue[0]) * 
						(RestAngleMap[Constraint[0] - InParticleOffset] + RestAngleMap[Constraint[1] - InParticleOffset]) * (FSolverReal).5f;
				}
				else
				{
					return RestAngleValue[0];
				}
			};

			RestAngles.Reset(Constraints.Num());
			for (const TVec4<int32>& Constraint : Constraints)
			{
				const FSolverVec3& P1 = InParticles.X(Constraint[0]);
				const FSolverVec3& P2 = InParticles.X(Constraint[1]);
				const FSolverVec3& P3 = InParticles.X(Constraint[2]);
				const FSolverVec3& P4 = InParticles.X(Constraint[3]);
				const FSolverReal Flatness = FMath::Clamp(FlatnessValue(Constraint), (FSolverReal)0.f, (FSolverReal)1.f);
				RestAngles.Add(FMath::Clamp(((FSolverReal)1.f - Flatness) * FPBDBendingConstraintsBase::CalcAngle(P1, P2, P3, P4), -UE_PI, UE_PI));
			}
		}

		void CalculateExplicitRestAngles(
			int32 InParticleOffset,
			int32 InParticleCount,
			const TConstArrayView<FRealSingle>& RestAngleMap,
			const FSolverVec2& RestAngleValue,
			const TArray<TVec4<int32>>& Constraints,
			TArray<FSolverReal>& RestAngles)
		{
			auto MapAngleRadians = [InParticleOffset, InParticleCount, &RestAngleMap, &RestAngleValue](const TVec4<int32>& Constraint)->FSolverReal
			{
				if (RestAngleMap.Num() == InParticleCount)
				{
					const FSolverReal RestAngle0 = FMath::UnwindRadians(FMath::DegreesToRadians(
						RestAngleValue[0] + (RestAngleValue[1] - RestAngleValue[0]) * (RestAngleMap[Constraint[0] - InParticleOffset])));
					const FSolverReal RestAngle1 = FMath::UnwindRadians(FMath::DegreesToRadians(
						RestAngleValue[0] + (RestAngleValue[1] - RestAngleValue[0]) * (RestAngleMap[Constraint[1] - InParticleOffset])));

					return FMath::Abs(RestAngle0) < FMath::Abs(RestAngle1) ? RestAngle0 : RestAngle1;
				}
				else
				{
					return FMath::UnwindRadians(FMath::DegreesToRadians(RestAngleValue[0]));
				}
			};

			RestAngles.Reset(Constraints.Num());
			for (const TVec4<int32>& Constraint : Constraints)
			{
				RestAngles.Add(FMath::Clamp(MapAngleRadians(Constraint), -UE_PI, UE_PI));
			}
		}
	}

	void FPBDBendingConstraintsBase::CalculateRestAngles(
		const FSolverParticles& InParticles,
		int32 InParticleOffset,
		int32 InParticleCount,
		const TConstArrayView<FRealSingle>& RestAngleMap,
		const FSolverVec2& RestAngleValue,
		ERestAngleConstructionType RestAngleConstructionType)
	{
		switch (RestAngleConstructionType)
		{
		default:
		case ERestAngleConstructionType::Use3DRestAngles:
			Private::Calculate3DRestAngles(InParticles, Constraints, RestAngles);
			break;
		case ERestAngleConstructionType::FlatnessRatio:
			Private::CalculateFlatnessRestAngles(InParticles, InParticleOffset, InParticleCount, RestAngleMap, RestAngleValue, Constraints, RestAngles);
			break;
		case ERestAngleConstructionType::ExplicitRestAngles:
			Private::CalculateExplicitRestAngles(InParticleOffset, InParticleCount, RestAngleMap, RestAngleValue, Constraints, RestAngles);
		}
	}
}  // End namespace Chaos::Softs
