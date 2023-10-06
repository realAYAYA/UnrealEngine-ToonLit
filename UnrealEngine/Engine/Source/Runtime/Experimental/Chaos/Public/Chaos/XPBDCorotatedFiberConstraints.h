// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "Chaos/XPBDCorotatedConstraints.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"

//DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Corotated Constraint"), STAT_XPBD_Spring, STATGROUP_Chaos);

namespace Chaos::Softs
{
	template <typename T, typename ParticleType>
	class FXPBDCorotatedFiberConstraints : public FXPBDCorotatedConstraints <T, ParticleType>
	{

		typedef FXPBDCorotatedConstraints<T, ParticleType> Base;
		using Base::MeshConstraints;
		using Base::LambdaArray;
		using Base::Measure;
		using Base::DmInverse;

	public:
		//this one only accepts tetmesh input and mesh
		FXPBDCorotatedFiberConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const bool bRecordMetricIn = true,
			const T& EMesh = (T)10.0,
			const T& NuMesh = (T).3,
			const TVector<T, 3> InFiberDir = TVector<T, 3>((T)1., (T)0., (T)0.),
			const T InSigmaMax = (T)3e5
		)
			: Base(InParticles, InMesh, bRecordMetricIn, EMesh, NuMesh), SigmaMax(InSigmaMax), FiberDir(InFiberDir)
		{
			LambdaArray.Init((T)0., 3 * MeshConstraints.Num());
		}

		virtual ~FXPBDCorotatedFiberConstraints() {}
		
		TVector<T, 3> GetFiberDir() { return FiberDir; }

		void SetActivation(const T AlphaIn) { AlphaActivation = AlphaIn; }

		void SetTime(const float Time) const {
			float FinalTime = 4.0f;
			float CurrentTime = Time;
			while (CurrentTime > FinalTime)
			{
				CurrentTime -= FinalTime;
			}
			AlphaActivation = (T)1. - (T)4. / FinalTime * FMath::Abs(CurrentTime - FinalTime / (T)2.);
		}

		virtual void ApplyInSerial(ParticleType& Particles, const T Dt, const int32 ElementIndex) const override
		{
			TVec4<TVector<T, 3>> PolarDelta = Base::GetPolarDelta(Particles, Dt, ElementIndex);

			for (int i = 0; i < 4; i++)
			{
				Particles.P(MeshConstraints[ElementIndex][i]) += PolarDelta[i];
			}

			TVec4<TVector<T, 3>> DetDelta = Base::GetDeterminantDelta(Particles, Dt, ElementIndex);

			for (int i = 0; i < 4; i++)
			{
				Particles.P(MeshConstraints[ElementIndex][i]) += DetDelta[i];
			}

			TVec4<TVector<T, 3>> FiberDelta = GetFiberDelta(Particles, Dt, ElementIndex);

			for (int i = 0; i < 4; i++)
			{
				Particles.P(MeshConstraints[ElementIndex][i]) += FiberDelta[i];
			}
			

		}

		TVec4<TVector<T, 3>> GetFiberGradient(const T dFpdL, const T dFadL,const T C3, const TVec4<TVector<T, 3>>& dLdX) const 
		{
			T dC3dL = (T)0.5 * (dFpdL + AlphaActivation * dFadL);
			if (C3 == 0)
			{
				return TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.));
			}
			else
			{
				dC3dL /= C3;
			}

			TVec4<TVector<T, 3>> dC3(TVector<T, 3>((T)0.));
			for (int32 i = 0; i < 4; i++)
			{
				for (int32 j = 0; j < 3; j++)
				{
					dC3[i][j] = dC3dL * dLdX[i][j];
				}
			}

			return dC3;
		
		}


	private:

		TVec4<TVector<T, 3>> GetFiberDelta(const ParticleType& Particles, const T Dt, const int32 ElementIndex, const T Tol = 1e-3) const
		{
			const PMatrix<T, 3, 3> Fe = Base::F(ElementIndex, Particles);

			PMatrix<T, 3, 3> Re((T)0.), Se((T)0.);

			Chaos::PolarDecomposition(Fe, Re, Se);

			// l: fiber stretch, f = (vTCv)^(1/2)
			TVector<T, 3> FeV = Fe.GetTransposed() * FiberDir;
			TVector<T, 3> DmInverseV = Base::ElementDmInv(ElementIndex).GetTransposed() * FiberDir;
			T L = FeV.Size();
			TVec4<TVector<T, 3>> dLdX(TVector<T, 3>((T)0.));
			for (int32 alpha = 0; alpha < 3; alpha++)
			{
				for (int32 s = 0; s < 3; s++)
				{
					dLdX[0][alpha] -= FeV[alpha] * DmInverseV[s] / L;
				}

			}
			for (int32 ie = 1; ie < 4; ie++)
			{
				for (int32 alpha = 0; alpha < 3; alpha++)
				{
					dLdX[ie][alpha] = FeV[alpha] * DmInverseV[ie - 1] / L;
				}
			}

			T LambdaOFL = (T)1.4;
			T P1 = (T)0.05;
			T P2 = (T)6.6;
			T FpIntegral = (T)0.;
			T FaIntegral = (T)0.;

			T C3 = (T)0.;
			T AlphaTilde = LambdaOFL / (SigmaMax * Dt * Dt * Measure[ElementIndex]);
			T dFpdL = (T)0.;
			T dFadL = (T)0.;

			if (L > LambdaOFL)
			{
				FpIntegral = P1 * LambdaOFL / P2 * FMath::Exp(P2 * (L / LambdaOFL - (T)1.)) - P1 * (L - LambdaOFL);
				dFpdL = P1 * FMath::Exp(P2 * (L / LambdaOFL - (T)1.)) - P1;
			}
			if (L > (T)0.4 * LambdaOFL && L < (T)0.6 * LambdaOFL)
			{
				FaIntegral = (T)3. * LambdaOFL * FMath::Pow((L / LambdaOFL - (T)0.4), 3);
				dFadL = (T)9. * FMath::Pow((L / LambdaOFL - (T)0.4), 2);
			}
			else if (L >= (T)0.6 * LambdaOFL && L <= (T)1.4 * LambdaOFL)
			{
				FaIntegral = (T)3. * LambdaOFL * (T)0.008 + L - (T)4. / (T)3. * LambdaOFL * FMath::Pow(L / LambdaOFL - (T)1., 3) - (T)0.6 * LambdaOFL - (T)4. / (T)3. * LambdaOFL * FMath::Pow((T)0.4, 3);
				dFadL = (T)1. - (T)4. * FMath::Pow(L / LambdaOFL - (T)1., 2);
			}
			else if (L > (T)1.4 * LambdaOFL && L <= (T)1.6 * LambdaOFL)
			{
				FaIntegral = (T)3. * LambdaOFL * (T)0.008 + (T)0.8 * LambdaOFL - (T)8. / (T)3. * LambdaOFL * FMath::Pow((T)0.4, 3) + (T)3. * LambdaOFL * FMath::Pow(L / LambdaOFL - (T)1.6, 3) + (T)3. * LambdaOFL * (T)0.008;
				dFadL = (T)9. * FMath::Pow((L / LambdaOFL - (T)1.6), 2);

			}
			else if (L > (T)1.6 * LambdaOFL)
			{
				FaIntegral = (T)3. * LambdaOFL * (T)0.008 + (T)0.8 * LambdaOFL - (T)8. / (T)3. * LambdaOFL * FMath::Pow((T)0.4, 3) + (T)3. * LambdaOFL * (T)0.008;
				dFadL = (T)0.;
			}


			C3 = FMath::Sqrt(FpIntegral + AlphaActivation * FaIntegral);


			//T dC3dL = (T)0.5 * (dFpdL + AlphaActivation * dFadL);
			//if (C3 == 0)
			//{
			//	return TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.));
			//}
			//else
			//{
			//	dC3dL /= C3;
			//}

			TVec4<TVector<T, 3>> dC3 = GetFiberGradient(dFpdL,dFadL, C3, dLdX);

			//TVec4<TVector<T, 3>> dC3(TVector<T, 3>((T)0.));
			//for (int32 i = 0; i < 4; i++)
			//{
			//	for (int32 j = 0; j < 3; j++)
			//	{
			//		dC3[i][j] = dC3dL * dLdX[i][j];
			//	}
			//}

			T DLambda = -C3 - AlphaTilde * LambdaArray[2 * ElementIndex + 2];

			T Denom = AlphaTilde;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Denom += dC3[i][j] * Particles.InvM(MeshConstraints[ElementIndex][i]) * dC3[i][j];
				}
			}
			DLambda /= Denom;
			LambdaArray[2 * ElementIndex + 2] += DLambda;
			TVec4<TVector<T, 3>> Delta(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Delta[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i]) * dC3[i][j] * DLambda;
				}
			}
			return Delta;



		}

	private:

		//material constants calculated from E:
		T SigmaMax;
		mutable T AlphaActivation;
		TVector<T, 3> FiberDir;

	};

}  // End namespace Chaos::Softs

