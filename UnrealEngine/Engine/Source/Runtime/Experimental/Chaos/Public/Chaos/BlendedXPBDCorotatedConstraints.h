// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/XPBDCorotatedConstraints.h"


namespace Chaos::Softs
{
	template <typename T, typename ParticleType>
	class FBlendedXPBDCorotatedConstraints : public FXPBDCorotatedConstraints <T, ParticleType>
	{

		typedef FXPBDCorotatedConstraints<T, ParticleType> Base;
		using Base::MeshConstraints;
		using Base::LambdaArray;
		using Base::Measure;
		using Base::DmInverse;
		using Base::Lambda;
		using Base::bRecordMetric;

	public:
		//this one only accepts tetmesh input and mesh
		FBlendedXPBDCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const bool bRecordMetricIn = true,
			const T& EMesh = (T)10.0,
			const T& NuMesh = (T).3,
			const T& InZeta = (T)1.
		)
			: Base(InParticles, InMesh, bRecordMetricIn, EMesh, NuMesh), Zeta(InZeta)
		{
			if (Zeta < (T)0.) 
			{
				Zeta = (T)0.;
			}
			if (Zeta > (T)1.) 
			{
				Zeta = (T)1.;
			}

			C1Contribution.Init(TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.)), InMesh.Num());
			C2Contribution.Init(TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.)), InMesh.Num());
		}

		//this one only accepts tetmesh input and mesh
		FBlendedXPBDCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const TArray<T>& EMeshArray,
			const T& NuMesh = (T).3,
			const bool bRecordMetricIn = false,
			const T& InZeta = (T)1.
		)
			: Base(InParticles, InMesh, EMeshArray, NuMesh, bRecordMetricIn), Zeta(InZeta)
		{
			if (Zeta < (T)0.)
			{
				Zeta = (T)0.;
			}
			if (Zeta > (T)1.)
			{
				Zeta = (T)1.;
			}

			C1Contribution.Init(TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.)), InMesh.Num());
			C2Contribution.Init(TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.)), InMesh.Num());
		}

		virtual ~FBlendedXPBDCorotatedConstraints() {}


		virtual void ApplyInSerial(ParticleType& Particles, const T Dt, const int32 ElementIndex) const override
		{
			TVec4<TVector<T, 3>> PolarDelta = GetPolarDelta(Particles, Dt, ElementIndex);

			for (int i = 0; i < 4; i++)
			{
				Particles.P(MeshConstraints[ElementIndex][i]) += PolarDelta[i];
			}

			TVec4<TVector<T, 3>> DetDelta = GetDeterminantDelta(Particles, Dt, ElementIndex);

			for (int i = 0; i < 4; i++)
			{
				Particles.P(MeshConstraints[ElementIndex][i]) += DetDelta[i];
			}
		}

		virtual void Init() const override
		{
			for (T& Lambdas : LambdaArray) { Lambdas = (T)0.; }
			C1Contribution.Init(TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.)), MeshConstraints.Num());
			C2Contribution.Init(TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.)), MeshConstraints.Num());
		}

	protected:

		virtual TVec4<TVector<T, 3>> GetDeterminantDelta(const ParticleType& Particles, const T Dt, const int32 ElementIndex, const T Tol = (T)1e-3) const override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosBlendedXPBDCorotatedApplyDet);

			const PMatrix<T, 3, 3> Fe = Base::F(ElementIndex, Particles);
			PMatrix<T, 3, 3> DmInvT = Base::ElementDmInv(ElementIndex).GetTransposed();

			T J = Fe.Determinant();
			if (J - (T)1. < Tol)
			{
				return TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.));
			}

			TVec4<TVector<T, 3>> dC2 = Base::GetDeterminantGradient(Fe, DmInvT);

			T AlphaTilde = (T)2. / (Dt * Dt * Base::Lambda * Measure[ElementIndex]);

			T DLambda = (1 - J) - AlphaTilde * LambdaArray[2 * ElementIndex + 1];

			T Denom = AlphaTilde;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Denom += dC2[i][j] * Particles.InvM(MeshConstraints[ElementIndex][i]) * dC2[i][j];
				}
			}
			DLambda /= Denom;
			LambdaArray[2 * ElementIndex + 1] += DLambda;
			TVec4<TVector<T, 3>> Delta2(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Delta2[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i]) * dC2[i][j] * DLambda;
				}
			}
			TVec4<TVector<T, 3>> Delta1(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Delta1[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i]) 
						* dC2[i][j] * LambdaArray[2 * ElementIndex + 1] - C2Contribution[ElementIndex][i][j];
				}
			}
			TVec4<TVector<T, 3>> Delta(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				Delta[i] = Zeta * Delta1[i] + ((T)1. - Zeta) * Delta2[i];
			}

			for (int i = 0; i < 4; i++)
			{
				C2Contribution[ElementIndex][i] += Delta[i];
			}

			return Delta;
		}



		virtual TVec4<TVector<T, 3>> GetPolarDelta(const ParticleType& Particles, const T Dt, const int32 ElementIndex, const T Tol = (T)1e-3) const override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosBlendedXPBDCorotatedApplyPolar);
			SCOPE_CYCLE_COUNTER(STAT_ChaosXPBDCorotatedPolar);
			const PMatrix<T, 3, 3> Fe = Base::F(ElementIndex, Particles);

			PMatrix<T, 3, 3> Re((T)0.), Se((T)0.);

			Chaos::PolarDecomposition(Fe, Re, Se);

			T C1 = (T)0.;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					C1 += FMath::Square((Fe - Re).GetAt(i, j));
				}
			}
			C1 = FMath::Sqrt(C1);

			if (C1 < Tol)
			{
				return TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.));
			}

			PMatrix<T, 3, 3> DmInvT = Base::ElementDmInv(ElementIndex).GetTransposed();

			TVec4<TVector<T, 3>> dC1 = Base::GetPolarGradient(Fe, Re, DmInvT, C1);

			T AlphaTilde = (T)1. / (Dt * Dt * Base::Mu * Measure[ElementIndex]);

			T DLambda = -C1 - AlphaTilde * LambdaArray[2 * ElementIndex + 0];

			T Denom = AlphaTilde;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Denom += dC1[i][j] * Particles.InvM(MeshConstraints[ElementIndex][i]) * dC1[i][j];
				}
			}
			DLambda /= Denom;
			LambdaArray[2 * ElementIndex + 0] += DLambda;

			TVec4<TVector<T, 3>> Delta2(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Delta2[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i]) * dC1[i][j] * DLambda;
				}
			}
			TVec4<TVector<T, 3>> Delta1(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Delta1[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i])
						* dC1[i][j] * LambdaArray[2 * ElementIndex + 0] - C1Contribution[ElementIndex][i][j];
				}
			}
			TVec4<TVector<T, 3>> Delta(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				Delta[i] = Zeta * Delta1[i] + ((T)1. - Zeta) * Delta2[i];
			}

			for (int i = 0; i < 4; i++)
			{
				C1Contribution[ElementIndex][i] += Delta[i];
			}

			return Delta;

		}


	private:

		T Zeta;
		mutable TArray<TVec4<TVector<T, 3>>> C1Contribution;
		mutable TArray<TVec4<TVector<T, 3>>> C2Contribution;
	};

}  // End namespace Chaos::Softs

