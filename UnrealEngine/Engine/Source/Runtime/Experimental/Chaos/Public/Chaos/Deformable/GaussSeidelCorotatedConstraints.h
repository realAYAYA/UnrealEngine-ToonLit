// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "Chaos/XPBDCorotatedConstraints.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/NewtonCorotatedCache.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Deformable/GaussSeidelWeakConstraints.h"


namespace Chaos::Softs
{
	template <typename T, typename ParticleType>
	class FGaussSeidelCorotatedConstraints : public FXPBDCorotatedConstraints <T, ParticleType>
	{

		typedef FXPBDCorotatedConstraints<T, ParticleType> Base;
		using Base::MeshConstraints;
		using Base::LambdaArray;
		using Base::Measure;
		using Base::DmInverse;
		using Base::MuElementArray;
		using Base::LambdaElementArray;
		using Base::CorotatedParams;
		using Base::F;

	public:
		FGaussSeidelCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const TArray<T>& EMeshArray,
			const TArray<T>& NuMeshArray,
			TArray<T>&& AlphaJMeshArray,
			TArray<TArray<int32>>&& IncidentElementsIn,
			TArray<TArray<int32>>&& IncidentElementsLocalIn,
			const int32 ParticleStartIndexIn,
			const int32 ParticleEndIndexIn,
			const bool bDoQuasistaticsIn = false,
			const bool bDoSORIn = true,
			const T InOmegaSOR = (T)1.6,
			const FDeformableXPBDCorotatedParams& InParams = FDeformableXPBDCorotatedParams(),
			const T& NuMesh = (T).3,
			const bool bRecordMetricIn = false
		)
			: Base(InParticles, InMesh, EMeshArray, NuMeshArray, MoveTemp(AlphaJMeshArray), InParams, NuMesh, bRecordMetricIn, false), IncidentElements(IncidentElementsIn), IncidentElementsLocal(IncidentElementsLocalIn),
			ParticleStartIndex(ParticleStartIndexIn), ParticleEndIndex(ParticleEndIndexIn), bDoQuasistatics(bDoQuasistaticsIn), bDoSOR(bDoSORIn), OmegaSOR(InOmegaSOR)
		{
			Base::LambdaArray.SetNum(0);
			Particle2Incident.Init(INDEX_NONE, ParticleEndIndexIn - ParticleStartIndexIn);
			for (int32 i = 0; i < IncidentElements.Num(); i++)
			{
				Particle2Incident[i] = i;
			}
			for (int32 e = 0; e < InMesh.Num(); e++)
			{
				for (int32 r = 0; r < 3; r++) {
					for (int32 c = 0; c < 3; c++) {
						DmInverse[(3 * 3) * e + 3 * r + c] *= Base::AlphaJArray[e];
					}
				}
			}
			if (!bDoQuasistatics)
			{
				xtilde.Init(TVector<T, 3>(T(0.)), ParticleEndIndex - ParticleStartIndex);
			}
			if (bDoSOR)
			{
				X_k_1.Init(TVector<T, 3>(T(0.)), ParticleEndIndex - ParticleStartIndex);
				X_k.Init(TVector<T, 3>(T(0.)), ParticleEndIndex - ParticleStartIndex);
			}
			InitColor(InParticles);
			InitializeCorotatedLambdas();
		}

		virtual ~FGaussSeidelCorotatedConstraints() {}


		void Apply(ParticleType& Particles, const T Dt) const 
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelApply);
			for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
			{
				int32 NumBatch = ParticlesPerColor[i].Num() / CorotatedParams.XPBDCorotatedBatchSize;
				if (ParticlesPerColor[i].Num() % CorotatedParams.XPBDCorotatedBatchSize != 0)
				{
					NumBatch += 1;
				}

				PhysicsParallelFor(NumBatch, [&](const int32 BatchIndex)
					{
						for (int32 BatchSubIndex = 0; BatchSubIndex < CorotatedParams.XPBDCorotatedBatchSize; BatchSubIndex++) {
							int32 TaskIndex = CorotatedParams.XPBDCorotatedBatchSize * BatchIndex + BatchSubIndex;
							if (TaskIndex < ParticlesPerColor[i].Num())
							{
								int32 ParticleIndex = ParticlesPerColor[i][TaskIndex];
								Chaos::TVector<T, 3> Dx = ComputeDeltax(ParticleIndex, Particle2Incident[ParticleIndex - ParticleStartIndex], Particles,
									Dt);
								Particles.P(ParticleIndex) += Dx;
							}
						}
					}, NumBatch < CorotatedParams.XPBDCorotatedBatchThreshold);

			}
			ApplySOR(Particles, Dt);
			CurrentIt += 1;
		}

		void Init(const T Dt, const ParticleType& Particles) const 
		{
			if (!bDoQuasistatics)
			{
				for (int32 i = 0; i < ParticleEndIndex - ParticleStartIndex; i++)
				{
					xtilde[i] = Particles.X(ParticleStartIndex + i) + Dt * Particles.V(ParticleStartIndex + i);
				}
			}
			CurrentIt = 0;
		}

		void Init() const override {}

		TArray<TArray<int32>>& GetIncidentElements() { return IncidentElements; }
		TArray<TArray<int32>>& GetIncidentElementsLocal() { return IncidentElementsLocal; }
		TArray<TVector<int32, 4>> GetMeshConstraints() const { return MeshConstraints; }
		void SetParticlesPerColor(TArray<TArray<int32>>&& InParticlesPerColor) {ParticlesPerColor = MoveTemp(InParticlesPerColor); }

		TArray<TArray<int32>> GetMeshArray() const 
		{
			TArray<TArray<int32>> MeshArray;
			MeshArray.SetNum(MeshConstraints.Num());
			for (int32 i = 0; i < MeshConstraints.Num(); i++)
			{
				MeshArray[i].SetNum(4);
				for (int32 ie = 0; ie < 4; ie++)
				{
					MeshArray[i][ie] = MeshConstraints[i][ie];
				}
			}
			return MeshArray;
		}

		// Apply Successive Over Relaxation (SOR) to accelerate convergence rate
		void ApplySOR(ParticleType& Particles, const T Dt) const
		{
			if (bDoSOR)
			{
				PhysicsParallelFor(ParticleEndIndex - ParticleStartIndex, [&](const int32 i)
					{
						int32 ParticleIndex = i + ParticleStartIndex;
						if (Particles.InvM(ParticleIndex) != T(0) && CurrentIt > 3)
						{
							Particles.P(ParticleIndex) = OmegaSOR * (Particles.P(ParticleIndex) - X_k_1[i]) + X_k_1[i];
						}
						X_k_1[i] = X_k[i];
						X_k[i] = Particles.P(ParticleIndex);
					}, ParticleEndIndex - ParticleStartIndex < 1000);
			}
		}

		Chaos::TVector<T, 3> ComputePerParticleResidual(const int32 p, const int32 IncidentIndex, const ParticleType& Particles,
			const T Dt, const bool AddMass = true) const
		{

			Chaos::TVector<T, 3> res(T(0));
			if (AddMass) {
				for (int32 alpha = 0; alpha < 3; alpha++) {
					res[alpha] = Particles.M(p) * (Particles.P(p)[alpha] - xtilde[p - ParticleStartIndex][alpha]);
				}
			}
			for (int32 j = 0; j < IncidentElements[IncidentIndex].Num(); j++) {
				int32 local_index = IncidentElementsLocal[IncidentIndex][j];
				int32 e = IncidentElements[IncidentIndex][j];
				Chaos::PMatrix<T, 3, 3> DmInvT = Base::ElementDmInv(e).GetTransposed();
				Chaos::PMatrix<T, 3, 3> Fe = Base::F(e, Particles);
				Chaos::PMatrix<T, 3, 3> P((T)0.);

				ComputeStress(Fe, MuElementArray[e], LambdaElementArray[e], P);

				Chaos::PMatrix<T, 3, 3> force_term = -Measure[e] * DmInvT * P;
				Chaos::TVector<T, 3> dx((T)0.);
				if (local_index > 0)
				{
					for (int32 c = 0; c < 3; c++)
					{
						dx[c] += force_term.GetAt(c, local_index - 1);
					}
				}
				else
				{
					for (int32 c = 0; c < 3; c++)
					{
						for (int32 h = 0; h < 3; h++)
						{
							dx[c] -= force_term.GetAt(c, h);
						}
					}
				}

				for (int32 ll = 0; ll < 3; ll++) {
					dx[ll] *= Dt * Dt;
				}

				for (int32 alpha = 0; alpha < 3; alpha++) {
					res[alpha] -= dx[alpha];
				}
			}
			return res;
		}


		Chaos::PMatrix<T, 3, 3> ComputePerParticleCorotatedHessianSimple(const int32 p, const int32 IncidentIndex, const ParticleType& Particles,
			const T Dt, const bool AddMass = true) const
		{
			Chaos::PMatrix<T, 3, 3> final_hessian = Chaos::PMatrix<T, 3, 3>::Zero;
			if (AddMass) {
				for (int32 alpha = 0; alpha < 3; alpha++) {
					final_hessian.SetAt(alpha, alpha, Particles.M(p));
				}
			}

			for (int32 j = 0; j < IncidentElements[IncidentIndex].Num(); j++) {
				int32 local_index = IncidentElementsLocal[IncidentIndex][j];
				int32 ElementIndex = IncidentElements[IncidentIndex][j];
				Chaos::PMatrix<T, 3, 3> DmInv = Base::ElementDmInv(ElementIndex);
				Chaos::PMatrix<T, 3, 3> Fe = Base::F(ElementIndex, Particles);

				T Coeff = Dt * Dt * Measure[ElementIndex];

				ComputeHessianHelper(Fe, DmInv, MuElementArray[ElementIndex], LambdaElementArray[ElementIndex], local_index, Coeff, final_hessian);

			}
			return final_hessian;
		}


		void AddHyperelasticResidualAndHessian (const ParticleType& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const T Dt, TVec3<T>& ParticleResidual, Chaos::PMatrix<T, 3, 3>& ParticleHessian)
		{
			Chaos::PMatrix<T, 3, 3> DmInvT = Base::ElementDmInv(ElementIndex).GetTransposed();
			Chaos::PMatrix<T, 3, 3> Fe = F(ElementIndex, Particles);
			Chaos::PMatrix<T, 3, 3> P((T)0.);

			this->ComputeStress(Fe, MuElementArray[ElementIndex], LambdaElementArray[ElementIndex], P);

			Chaos::PMatrix<T, 3, 3> force_term = -Measure[ElementIndex] * DmInvT * P;
			Chaos::TVector<T, 3> dx((T)0.);
			if (ElementIndexLocal > 0)
			{
				for (int32 c = 0; c < 3; c++)
				{
					dx[c] += force_term.GetAt(c, ElementIndexLocal - 1);
				}
			}
			else
			{
				for (int32 c = 0; c < 3; c++)
				{
					for (int32 h = 0; h < 3; h++)
					{
						dx[c] -= force_term.GetAt(c, h);
					}
				}
			}

			for (int32 ll = 0; ll < 3; ll++) {
				dx[ll] *= Dt * Dt;
			}

			for (int32 alpha = 0; alpha < 3; alpha++) {
				ParticleResidual[alpha] -= dx[alpha];
			}

			ComputeHessianHelper(Fe, Base::ElementDmInv(ElementIndex), MuElementArray[ElementIndex], LambdaElementArray[ElementIndex], ElementIndexLocal, Dt * Dt * Measure[ElementIndex], ParticleHessian);

		}

	protected:
		 void InitColor(const ParticleType& Particles)
		{
			ParticlesPerColor = ComputeNodalColoring(MeshConstraints, Particles, ParticleStartIndex, ParticleEndIndex, IncidentElements, IncidentElementsLocal);
		}

		 void InitializeCorotatedLambdas()
		 {
			 ComputeStress = [](const Chaos::PMatrix<T, 3, 3>& Fe, const T mu, const T lambda, Chaos::PMatrix<T, 3, 3>& P)
			 {
				 PCorotated(Fe, mu, lambda, P);
			 };
			 ComputeHessianHelper = [](const Chaos::PMatrix<T, 3, 3>& Fe, const Chaos::PMatrix<T, 3, 3>& DmInv, const T mu, const T lambda, const int32 local_index, const T Coeff, Chaos::PMatrix<T, 3, 3>& final_hessian) 
			 {
				 Chaos::PMatrix<T, 3, 3> JFinvT((T)0.);
				 JFinvT.SetAt(0, 0, Fe.GetAt(1, 1) * Fe.GetAt(2, 2) - Fe.GetAt(2, 1) * Fe.GetAt(1, 2));
				 JFinvT.SetAt(0, 1, Fe.GetAt(2, 0) * Fe.GetAt(1, 2) - Fe.GetAt(1, 0) * Fe.GetAt(2, 2));
				 JFinvT.SetAt(0, 2, Fe.GetAt(1, 0) * Fe.GetAt(2, 1) - Fe.GetAt(2, 0) * Fe.GetAt(1, 1));
				 JFinvT.SetAt(1, 0, Fe.GetAt(2, 1) * Fe.GetAt(0, 2) - Fe.GetAt(0, 1) * Fe.GetAt(2, 2));
				 JFinvT.SetAt(1, 1, Fe.GetAt(0, 0) * Fe.GetAt(2, 2) - Fe.GetAt(2, 0) * Fe.GetAt(0, 2));
				 JFinvT.SetAt(1, 2, Fe.GetAt(2, 0) * Fe.GetAt(0, 1) - Fe.GetAt(0, 0) * Fe.GetAt(2, 1));
				 JFinvT.SetAt(2, 0, Fe.GetAt(0, 1) * Fe.GetAt(1, 2) - Fe.GetAt(1, 1) * Fe.GetAt(0, 2));
				 JFinvT.SetAt(2, 1, Fe.GetAt(1, 0) * Fe.GetAt(0, 2) - Fe.GetAt(0, 0) * Fe.GetAt(1, 2));
				 JFinvT.SetAt(2, 2, Fe.GetAt(0, 0) * Fe.GetAt(1, 1) - Fe.GetAt(1, 0) * Fe.GetAt(0, 1));

				 Chaos::PMatrix<T, 3, 3> JFinv = JFinvT.GetTransposed();

				 if (local_index == 0) {
					 T DmInvsum = T(0);
					 for (int32 nu = 0; nu < 3; nu++) {
						 T localDmsum = T(0);
						 for (int32 k = 0; k < 3; k++) {
							 localDmsum += DmInv.GetAt(k, nu);
						 }
						 DmInvsum += localDmsum * localDmsum;
					 }
					 for (int32 alpha = 0; alpha < 3; alpha++) {
						 final_hessian.SetAt(alpha, alpha, final_hessian.GetAt(alpha, alpha) + Coeff * T(2) * mu * DmInvsum);
					 }

					 Chaos::PMatrix<T, 3, 3> DmInvJFinv = JFinv * DmInv;
					 Chaos::TVector<T, 3> l((T)0.);
					 for (int32 alpha = 0; alpha < 3; alpha++) {
						 for (int32 k = 0; k < 3; k++) {
							 l[alpha] += DmInvJFinv.GetAt(k, alpha);
						 }
					 }
					 for (int32 alpha = 0; alpha < 3; alpha++)
					 {
						 final_hessian.SetRow(alpha, final_hessian.GetRow(alpha) + Coeff * lambda * l[alpha] * l);
					 }

				 }
				 else {
					 T DmInvsum = T(0);
					 for (int32 nu = 0; nu < 3; nu++)
					 {
						 DmInvsum += DmInv.GetAt(local_index - 1, nu) * DmInv.GetAt(local_index - 1, nu);
					 }
					 for (int32 alpha = 0; alpha < 3; alpha++)
					 {
						 final_hessian.SetAt(alpha, alpha, final_hessian.GetAt(alpha, alpha) + Coeff * T(2) * mu * DmInvsum);
					 }

					 Chaos::PMatrix<T, 3, 3> DmInvJFinv = JFinv * DmInv;
					 Chaos::TVector<T, 3> l((T)0.);
					 for (int32 alpha = 0; alpha < 3; alpha++) {
						 l[alpha] = DmInvJFinv.GetAt(local_index - 1, alpha);
					 }
					 for (int32 alpha = 0; alpha < 3; alpha++)
					 {
						 final_hessian.SetRow(alpha, final_hessian.GetRow(alpha) + Coeff * lambda * l[alpha] * l);
					 }
				 }
			 };

			 AddAdditionalRes = [](const ParticleType& InParticles, const int32 p, const T Dt, TVec3<T>& res) {};
			 AddAdditionalHessian = [](const ParticleType& InParticles, const int32 p, const T Dt, Chaos::PMatrix<T, 3, 3>& hessian) {};
		 }

	private:

		TVector<T, 3> ComputeDeltax(const int32 p, const int32 IncidentIndex, const ParticleType& Particles,
			const T Dt) const
		{
			Chaos::TVector<T, 3> ParticleResidual = ComputePerParticleResidual(p, Particle2Incident[p], Particles, Dt, !bDoQuasistatics);

			AddAdditionalRes(Particles, p, Dt, ParticleResidual);

			if (ParticleResidual.Size() > LocalNewtonTol)
			{
				Chaos::PMatrix<T, 3, 3> SimplifiedHessian = ComputePerParticleCorotatedHessianSimple(p, Particle2Incident[p], Particles, Dt, !bDoQuasistatics);
				AddAdditionalHessian(Particles, p, Dt, SimplifiedHessian);
				
				T HessianDet = SimplifiedHessian.Determinant();
				if (HessianDet > UE_SMALL_NUMBER)
				{
					Chaos::PMatrix<T, 3, 3> HessianInv = SimplifiedHessian.SymmetricCofactorMatrix();
					HessianInv *= T(1) / HessianDet;
					return HessianInv.GetTransposed() * (-ParticleResidual);
				}
			}
			return TVector<T, 3>((T)0.);
		}

		

	protected:	

		TArray<int32> Particle2Incident;
		TArray<TArray<int32>> IncidentElements;
		TArray<TArray<int32>> IncidentElementsLocal;
		T LocalNewtonTol = T(1e-5);
		TArray<TArray<int32>> ParticlesPerColor;
		int32 ParticleStartIndex;
		int32 ParticleEndIndex;
		mutable TArray<Chaos::TVector<T, 3>> xtilde;
		mutable TArray<Chaos::TVector<T, 3>> X_k_1;
		mutable TArray<Chaos::TVector<T, 3>> X_k;
		bool bDoQuasistatics = false;
		bool bDoSOR = true;
		//Parameter to control the behavior of SOR. 1-1.7 is the usual range. Larger paramater leads to potentially more convergence but less stable behavior. 
		T OmegaSOR = T(1.6);
		mutable int32 CurrentIt = 0;
		TFunction<void(const Chaos::PMatrix<T, 3, 3>&, const T, const T, Chaos::PMatrix<T, 3, 3>& )> ComputeStress;
		TFunction<void(const Chaos::PMatrix<T, 3, 3>&, const Chaos::PMatrix<T, 3, 3>&, const T, const T, const int32, const T, Chaos::PMatrix<T, 3, 3>&)> ComputeHessianHelper;
		
	public:
		TFunction<void(const ParticleType&, const int32, const T, TVec3<T>&)> AddAdditionalRes;
		TFunction<void(const ParticleType&, const int32, const T, Chaos::PMatrix<T, 3, 3>&)> AddAdditionalHessian;
		TUniquePtr<TArray<int32>> ParticleColors;
	};


}// End namespace Chaos::Softs
