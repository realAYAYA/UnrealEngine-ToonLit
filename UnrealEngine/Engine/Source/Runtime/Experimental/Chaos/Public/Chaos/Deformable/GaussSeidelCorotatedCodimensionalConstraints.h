// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Deformable/ChaosDeformableSolverTypes.h"
#include "Chaos/Vector.h"


namespace Chaos::Softs
{
	template <typename T, typename ParticleType>
	class FGaussSeidelCorotatedCodimensionalConstraints
	{

	public:
		//this one only accepts tetmesh input and mesh
		FGaussSeidelCorotatedCodimensionalConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 3>>& InMesh,
			const bool bRecordMetricIn = true,
			const T& EMesh = (T)10.0,
			const T& NuMesh = (T).3
		)
			: bRecordMetric(bRecordMetricIn), MeshConstraints(InMesh)
		{
			Measure.Init((T)0., MeshConstraints.Num());
			Lambda = EMesh * NuMesh / (((T)1. + NuMesh) * ((T)1. - (T)2. * NuMesh));
			Mu = EMesh / ((T)2. * ((T)1. + NuMesh));

			MuElementArray.Init(Mu, MeshConstraints.Num());
			LambdaElementArray.Init(Lambda, MeshConstraints.Num());

			InitializeCodimensionData(InParticles);
		}

		virtual ~FGaussSeidelCorotatedCodimensionalConstraints() {}

		PMatrix<T, 3, 3> DsInit(const int32 E, const ParticleType& InParticles)  
		{
			PMatrix<T, 3, 3> Result((T)0.);
			for (int32 i = 0; i < 3; i++) 
			{
				for (int32 c = 0; c < 3; c++) 
				{
					Result.SetAt(c, i, InParticles.X(MeshConstraints[E][i + 1])[c] - InParticles.X(MeshConstraints[E][0])[c]);
				}
			}
			return Result;
		}

		PMatrix<T, 3, 2> Ds(const int32 E, const ParticleType& InParticles) const 
		{
			if (INDEX_NONE < E && E< MeshConstraints.Num()
				&& INDEX_NONE < MeshConstraints[E][0] && MeshConstraints[E][0] < (int32)InParticles.Size()
				&& INDEX_NONE < MeshConstraints[E][1] && MeshConstraints[E][1] < (int32)InParticles.Size()
				&& INDEX_NONE < MeshConstraints[E][2] && MeshConstraints[E][2] < (int32)InParticles.Size())
			{
				const TVec3<T> P1P0 = InParticles.P(MeshConstraints[E][1]) - InParticles.P(MeshConstraints[E][0]);
				const TVec3<T> P2P0 = InParticles.P(MeshConstraints[E][2]) - InParticles.P(MeshConstraints[E][0]);

				return PMatrix<T, 3, 2>(
					P1P0[0], P1P0[1], P1P0[2],
					P2P0[0], P2P0[1], P2P0[2]);
			}
			else
			{
				return PMatrix<T, 3, 2>(
					(T)0., (T)0, (T)0,
					(T)0, (T)0, (T)0);
			}
		
		}

		PMatrix<T, 3, 2> F(const int32 E, const ParticleType& InParticles) const 
		{
			if (INDEX_NONE < E && E < DmInverse.Num())
			{
				return Ds(E, InParticles) * DmInverse[E];
			}
			else
			{
				return PMatrix<T, 3, 2>(
					(T)0., (T)0, (T)0,
					(T)0, (T)0, (T)0);
			}
		}

		TArray<TArray<int32>> GetConstraintsArray() const
		{
			TArray<TArray<int32>> Constraints;
			Constraints.SetNum(MeshConstraints.Num());
			for (int32 i = 0; i < MeshConstraints.Num(); i++)
			{
				Constraints[i].SetNum(3);
				for (int32 j = 0; j < 3; j++)
				{
					Constraints[i][j] = MeshConstraints[i][j];
				}
			}
			return Constraints;
		}

		void ComputeNodalMass(const T InDensity, const int32 NumParticles, TArray<T>& NodalMass) const
		{
			NodalMass.Init((T)0., NumParticles);
			for (int32 e = 0; e < Measure.Num(); e++)
			{
				const T TotalMass = InDensity * Measure[e];
				for (int32 j = 0; j < 3; j++)
				{
					NodalMass[MeshConstraints[e][j]] += TotalMass / (T)3.;
				}
			}
		}



	protected:

		void InitializeCodimensionData(const ParticleType& Particles)
		{
			Measure.Init((T)0., MeshConstraints.Num());
			DmInverse.Init(PMatrix<FSolverReal, 2, 2>(0.f, 0.f, 0.f), MeshConstraints.Num());
			for (int32 e = 0; e < MeshConstraints.Num(); e++)
			{
				check(MeshConstraints[e][0] < (int32)Particles.Size() && MeshConstraints[e][0] > INDEX_NONE);
				check(MeshConstraints[e][1] < (int32)Particles.Size() && MeshConstraints[e][1] > INDEX_NONE);
				check(MeshConstraints[e][2] < (int32)Particles.Size() && MeshConstraints[e][2] > INDEX_NONE);
				if (MeshConstraints[e][0] < (int32)Particles.Size() && MeshConstraints[e][0] > INDEX_NONE
					&& MeshConstraints[e][1] < (int32)Particles.Size() && MeshConstraints[e][1] > INDEX_NONE
					&& MeshConstraints[e][2] < (int32)Particles.Size() && MeshConstraints[e][2] > INDEX_NONE)
				{
					const TVec3<T> X1X0 = Particles.GetX(MeshConstraints[e][1]) - Particles.GetX(MeshConstraints[e][0]);
					const TVec3<T> X2X0 = Particles.GetX(MeshConstraints[e][2]) - Particles.GetX(MeshConstraints[e][0]);
					PMatrix<T, 2, 2> Dm((T)0., (T)0., (T)0.);
					Dm.M[0] = X1X0.Size();
					Dm.M[2] = X1X0.Dot(X2X0) / Dm.M[0];
					Dm.M[3] = Chaos::TVector<T, 3>::CrossProduct(X1X0, X2X0).Size() / Dm.M[0];
					Measure[e] = Chaos::TVector<T, 3>::CrossProduct(X1X0, X2X0).Size() / (T)2.;
					ensureMsgf(Measure[e] > (T)0., TEXT("Degenerate triangle detected"));

					PMatrix<T, 2, 2> DmInv = Dm.Inverse();
					DmInverse[e] = DmInv;
				}
			}
		}

		static T SafeRecip(const T Len, const T Fallback)
		{
			if (Len > (T)UE_SMALL_NUMBER)
			{
				return (T)1. / Len;
			}
			return Fallback;
		}

		static inline void SymSchur2D(const T Aqq, const T App, const T Apq, T& c, T& s)
		{
			// A is an n x n matrix
			// (p, q) is an index pair satisfying 1 <= p < q <= n
			//
			// This function computes a (c, s) pair such that
			//
			// B = [ c, s]^T [a_pp, a_pq] [ c, s]
			//     [-s, c]   [a_pq, a_qq] [-s, c]
			//
			// is diagonal

			if (Apq != 0) 
			{
				T Tau = T(0.5) * (Aqq - App) / Apq;
				T t;
				if (Tau >= 0)
					t = T(1) / (Tau + FMath::Sqrt((T)1. + Tau * Tau));
				else
					t = T(-1) / (-Tau + FMath::Sqrt((T)1. + Tau * Tau));
				c = T(1) / FMath::Sqrt((T)1. + t * t);
				s = t * c;
			}
			else 
			{
				c = (T)1.;
				s = (T)0.;
			}
		}


		static inline void Jacobi(const PMatrix<T, 2, 2>& B, PMatrix<T, 2, 2>& D, PMatrix<T, 2, 2>& V)
		{
			T c, s;
			SymSchur2D(B.M[3], B.M[0], B.M[1], c, s);

			V.M[0] = c;
			V.M[1] = -s;
			V.M[2] = s;
			V.M[3] = c;

			D = V.GetTransposed() * B * V;
		}

		static PMatrix<T, 3, 2> ComputeR(const PMatrix<T, 3, 2>& Fe) 
		{
			PMatrix<T, 2, 2> FTF = ComputeFTF(Fe), D((T)0., (T)0., (T)0.), U((T)0., (T)0., (T)0.);
			Jacobi(FTF, D, U);
			PMatrix<T, 2, 2> SDiag(FMath::Sqrt(D.M[0]), (T)0., (T)0., FMath::Sqrt(D.M[3]));
			PMatrix<T, 2, 2> S = U * SDiag * U.GetTransposed(), SInv = S.Inverse();;
			return Fe * S.Inverse();
		}

		static PMatrix<T, 3, 2> ComputeRSimple(const PMatrix<T, 3, 2>& InputMatrix) 
		{
			TVec3<T> Col1(InputMatrix.M[0], InputMatrix.M[1], InputMatrix.M[2]), Col2(InputMatrix.M[3], InputMatrix.M[4], InputMatrix.M[5]), a(T(0)), b(T(0));
			if (FMath::Abs(Col1[0]) < (T)UE_SMALL_NUMBER && FMath::Abs(Col1[1]) < (T)UE_SMALL_NUMBER)
			{
				a[0] = T(1);
			}
			else
			{
				a[0] = -Col1[1];
				a[1] = Col1[0];
				const T OneOveraNorm = SafeRecip(a.Length(), 0.f);
				a *= OneOveraNorm;
			}
			b = TVec3<T>::CrossProduct(a, Col2);
			const T OneOverbNorm = SafeRecip(b.Length(), 0.f);
			b *= OneOverbNorm;
			return Chaos::PMatrix<T, 3, 2>(b[0], b[1], b[2], a[0], a[1], a[2]);
		}

		//Does Gram Schmidt on a 3*2 matrix and returns an orthogonal one
		static Chaos::PMatrix<T, 3, 2> GramSchmidt(const Chaos::PMatrix<T, 3, 2>& InputMatrix)
		{
			TVec3<T> Col1(InputMatrix.M[0], InputMatrix.M[1], InputMatrix.M[2]), Col2(InputMatrix.M[3], InputMatrix.M[4], InputMatrix.M[5]);
			const T Col1Norm = Col1.Length();
			const T OneOverCol1Norm = SafeRecip(Col1Norm, 0.f);
			TVec3<T> Col1Normalized = Col1 * OneOverCol1Norm;
			TVec3<T> Col2Orthogonal = Col2 - TVec3<T>::DotProduct(Col1Normalized, Col2) * Col1Normalized;
			const T Col2OrthogonalNorm = Col2Orthogonal.Length();
			const T OneOverCol2OrthogonalNorm = SafeRecip(Col2OrthogonalNorm, 0.f);
			Col2Orthogonal *= OneOverCol2OrthogonalNorm;
			return Chaos::PMatrix<T, 3, 2>(Col1Normalized[0], Col1Normalized[1], Col1Normalized[2], Col2Orthogonal[0], Col2Orthogonal[1], Col2Orthogonal[2]);
		}

		static PMatrix<T, 2, 2> ComputeFTF(const PMatrix<T, 3, 2>& InputMatrix)
		{
			return PMatrix<T, 2, 2>(InputMatrix.M[0] * InputMatrix.M[0] + InputMatrix.M[1] * InputMatrix.M[1] + InputMatrix.M[2] * InputMatrix.M[2],
				InputMatrix.M[0] * InputMatrix.M[3] + InputMatrix.M[1] * InputMatrix.M[4] + InputMatrix.M[2] * InputMatrix.M[5],
				InputMatrix.M[3] * InputMatrix.M[3] + InputMatrix.M[4] * InputMatrix.M[4] + InputMatrix.M[5] * InputMatrix.M[5]);
		}

		//Computes det(FTF)^{1/2} * F * (FTF)^-1
		static void dJdF32(const PMatrix<T, 3, 2>& F, PMatrix<T, 3, 2>& dJ)
		{
			const PMatrix<T, 2, 2> FTF = ComputeFTF(F);
			const T J2 = FTF.Determinant();

			const PMatrix<T, 2, 2> J2invT(FTF.M[3], -FTF.M[2], -FTF.M[1], FTF.M[0]);

			if (J2 > T(0))
			{
				dJ = (T(1) / FMath::Sqrt(J2)) *  (F * J2invT);
			}
			else
			{
				dJ = PMatrix<T, 3, 2>(TVec3<T>((T)0.), TVec3<T>((T)0.));
			}
		}

	public:
		void AddHyperelasticResidualAndHessian(const ParticleType& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const T Dt, TVec3<T>& ParticleResidual, Chaos::PMatrix<T, 3, 3>& ParticleHessian)
		{
			check(ElementIndex < DmInverse.Num() && ElementIndex > INDEX_NONE);
			check(ElementIndex < MuElementArray.Num());
			check(ElementIndex < Measure.Num());
			check(ElementIndex < LambdaElementArray.Num());
			check(ElementIndexLocal < 3 && ElementIndexLocal > INDEX_NONE);
			if (ElementIndexLocal < 3 
				&& ElementIndexLocal > INDEX_NONE
				&& ElementIndex < Measure.Num()
				&& ElementIndex < MuElementArray.Num()
				&& ElementIndex < LambdaElementArray.Num()
				&& ElementIndex < DmInverse.Num() 
				&& ElementIndex > INDEX_NONE)
			{
				const Chaos::PMatrix<T, 2, 2> DmInvT = DmInverse[ElementIndex].GetTransposed(), DmInv = DmInverse[ElementIndex]; 
				const Chaos::PMatrix<T, 3, 2> Fe = F(ElementIndex, Particles);

				const PMatrix<T, 3, 2> Re = ComputeR(Fe);
				const PMatrix<T, 2, 2> FTF = ComputeFTF(Fe);
				const T J = FMath::Sqrt(FTF.Determinant());

				PMatrix<T, 3, 2> Pe(TVec3<T>((T)0.), TVec3<T>((T)0.)), ForceTerm(TVec3<T>((T)0.), TVec3<T>((T)0.));
				Pe = T(2) * MuElementArray[ElementIndex] * (Fe - Re) + LambdaElementArray[ElementIndex] * (J - T(1)) * J * Fe * ComputeFTF(Fe).Inverse(); 

				ForceTerm = -Measure[ElementIndex] * Pe * DmInverse[ElementIndex].GetTransposed();

				Chaos::TVector<T, 3> Dx((T)0.);
				if (ElementIndexLocal > 0)
				{
					for (int32 c = 0; c < 3; c++)
					{
						Dx[c] += ForceTerm.M[ElementIndexLocal * 3 - 3 + c];
					}
				}
				else
				{
					for (int32 c = 0; c < 3; c++)
					{
						for (int32 h = 0; h < 2; h++)
						{
							Dx[c] -= ForceTerm.M[h * 3 + c];
						}
					}
				}

				Dx *= Dt * Dt;

				ParticleResidual -= Dx;

				PMatrix<T, 3, 2> dJ(TVec3<T>((T)0.), TVec3<T>((T)0.));

				dJdF32(Fe, dJ);

				T Coeff = Dt * Dt * Measure[ElementIndex];

				if (ElementIndexLocal == 0) 
				{
					T DmInvsum = T(0);
					for (int32 nu = 0; nu < 2; nu++) 
					{
						T localDmsum = T(0);
						for (int32 k = 0; k < 2; k++) 
						{
							localDmsum += DmInv.M[nu * 2 + k];
						}
						DmInvsum += localDmsum * localDmsum;
					}
					for (int32 alpha = 0; alpha < 3; alpha++) 
					{
						ParticleHessian.SetAt(alpha, alpha, ParticleHessian.GetAt(alpha, alpha) + Coeff * T(2) * MuElementArray[ElementIndex] * DmInvsum);
					}

					const PMatrix<T, 3, 2> dJDmInvT = dJ * DmInvT; 
					Chaos::TVector<T, 3> L((T)0.);
					for (int32 alpha = 0; alpha < 3; alpha++) 
					{
						for (int32 k = 0; k < 2; k++) 
						{
							L[alpha] += dJDmInvT.M[3 * k + alpha]; 
						}
					}
					for (int32 alpha = 0; alpha < 3; alpha++)
					{
						ParticleHessian.SetRow(alpha, ParticleHessian.GetRow(alpha) + Coeff * LambdaElementArray[ElementIndex] * L[alpha] * L);
					}
				}
				else 
				{
					T DmInvsum = T(0);
					for (int32 nu = 0; nu < 2; nu++)
					{
						DmInvsum += DmInv.GetAt(ElementIndexLocal - 1, nu) * DmInv.GetAt(ElementIndexLocal - 1, nu);
					}
					for (int32 alpha = 0; alpha < 3; alpha++)
					{
						ParticleHessian.SetAt(alpha, alpha, ParticleHessian.GetAt(alpha, alpha) + Dt * Dt * Measure[ElementIndex] * T(2) * MuElementArray[ElementIndex] * DmInvsum);
					}

					const PMatrix<T, 3, 2> dJDmInvT = dJ * DmInvT; 

					Chaos::TVector<T, 3> L((T)0.);
					for (int32 alpha = 0; alpha < 3; alpha++) 
					{
						const int32 IndexVisited = ElementIndexLocal* 3 - 3 + alpha;
						if (IndexVisited < 6 && IndexVisited > INDEX_NONE)
						{
							L[alpha] = dJDmInvT.M[IndexVisited];
						}
					}
					for (int32 alpha = 0; alpha < 3; alpha++)
					{
						ParticleHessian.SetRow(alpha, ParticleHessian.GetRow(alpha) + Dt * Dt * Measure[ElementIndex] * LambdaElementArray[ElementIndex] * L[alpha] * L);
					}
				}
			}
		}



	protected:
		mutable TArray<FSolverMatrix22> DmInverse;

		//material constants calculated from E:
		T Mu;
		T Lambda;
		TArray<T> MuElementArray;
		TArray<T> LambdaElementArray;
		TArray<T> AlphaJArray;
		bool bRecordMetric;

		TArray<TVector<int32, 3>> MeshConstraints;
		mutable TArray<T> Measure;
	};

}  // End namespace Chaos::Softs

