// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "Chaos/XPBDCorotatedConstraints.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/NewtonCorotatedCache.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Utilities.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/XPBDWeakConstraints.h"


namespace Chaos::Softs
{

	using Chaos::TVec3;
	template <typename T, typename ParticleType>
	struct FGaussSeidelWeakConstraints 
	{
		//TODO(Yizhou): Add unittest for Gauss Seidel Weak Constraints
		FGaussSeidelWeakConstraints(
			const TArray<TArray<int32>>& InIndices,
			const TArray<TArray<T>>& InWeights,
			const TArray<T>& InStiffness,
			const TArray<TArray<int32>>& InSecondIndices,
			const TArray<TArray<T>>& InSecondWeights,
			const FDeformableXPBDWeakConstraintParams& InParams
		)
			: Indices(InIndices), Weights(InWeights), SecondIndices(InSecondIndices), SecondWeights(InSecondWeights), Stiffness(InStiffness), DebugDrawParams(InParams)
		{
			ensureMsgf(Indices.Num() == SecondIndices.Num(), TEXT("Input Double Bindings have wrong size"));

			for (int32 i = 0; i < Indices.Num(); i++)
			{
				ensureMsgf(Indices[i].Num() + SecondIndices[i].Num() == 4, TEXT("Currently does not support non point tri pair weak constraints"));
				TSet<int32> IndicesSet = TSet<int32>(Indices[i]);
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ensureMsgf(!IndicesSet.Contains(SecondIndices[i][j]), TEXT("Indices and Second Indices overlaps. Currently not supported"));
				}
			}

			IsAnisotropic.Init(false, Indices.Num());
		}

		virtual ~FGaussSeidelWeakConstraints() {}

		void ComputeInitialWCData(const ParticleType& InParticles, const TArray<TVector<int32, 4>>& MeshConstraints, const TArray <TArray<int32>>& MeshIncidentElements, const TArray <TArray<int32>>& MeshIncidentElementsLocal, TArray<TArray<int32>>& ParticlesPerColor)
		{
			TArray<TVector<int32, 4>> ExtraConstraints;
			ExtraConstraints.Init(TVector<int32, 4>(-1), Indices.Num());
			for (int32 i = 0; i < Indices.Num(); i++)
			{
				ensureMsgf(Indices[i].Num() + SecondIndices[i].Num() == 4, TEXT("Currently does not support non point tri pair weak constraints"));
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					ExtraConstraints[i][j] = Indices[i][j];
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ExtraConstraints[i][j+Indices[i].Num()] = SecondIndices[i][j];
				}
			}
			TArray<TVector<int32, 4>> TotalConstraints = MeshConstraints;
			TotalConstraints += ExtraConstraints;
			TArray<TArray<int32>> TotalIncidentElements = MeshIncidentElements, TotalIncidentElementsLocal = MeshIncidentElementsLocal;
			WCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &WCIncidentElementsLocal);
			
			//TODO (Yizhou): Directly computing incident elements is inefficient. Optimize the total incident elements in future.
			TotalIncidentElements = Chaos::Utilities::ComputeIncidentElements(TotalConstraints, &TotalIncidentElementsLocal);
			Particle2WCIndices.Init(INDEX_NONE, InParticles.Size());
			for (int32 i = 0; i < WCIncidentElements.Num(); i++)
			{
				if (WCIncidentElements[i].Num() > 0)
				{
					int32 p = ExtraConstraints[WCIncidentElements[i][0]][WCIncidentElementsLocal[i][0]];
					Particle2WCIndices[p] = i;
				}
			}

			ParticlesPerColor = ComputeNodalColoring(TotalConstraints, InParticles, 0, InParticles.Size(), TotalIncidentElements, TotalIncidentElementsLocal);

			NodalWeights.Init({}, InParticles.Size());

			for (int32 i = 0; i < WCIncidentElements.Num(); i++) 
			{
				if (WCIncidentElements[i].Num() > 0)
				{
					int32 p = ExtraConstraints[WCIncidentElements[i][0]][WCIncidentElementsLocal[i][0]];
					NodalWeights[p].Init(T(0), 6);
					for (int32 j = 0; j < WCIncidentElements[i].Num(); j++)
					{
						int32 ConstraintIndex = WCIncidentElements[i][j];
						int32 LocalIndex = WCIncidentElementsLocal[i][j];

						T Weight = T(0);
						if (LocalIndex >= Indices[ConstraintIndex].Num())
						{
							Weight = SecondWeights[ConstraintIndex][LocalIndex - Indices[ConstraintIndex].Num()];
						}
						else
						{
							Weight = Weights[ConstraintIndex][LocalIndex];
						}

						if (IsAnisotropic[ConstraintIndex])
						{
							for (int32 alpha = 0; alpha < 3; alpha++) 
							{
								NodalWeights[p][alpha] += Normals[ConstraintIndex][alpha] * Normals[ConstraintIndex][alpha] * Weight * Weight * Stiffness[ConstraintIndex];
							}

							NodalWeights[p][3] += Normals[ConstraintIndex][0] * Normals[ConstraintIndex][1] * Weight * Weight * Stiffness[ConstraintIndex];
							NodalWeights[p][4] += Normals[ConstraintIndex][0] * Normals[ConstraintIndex][2] * Weight * Weight * Stiffness[ConstraintIndex];
							NodalWeights[p][5] += Normals[ConstraintIndex][1] * Normals[ConstraintIndex][2] * Weight * Weight * Stiffness[ConstraintIndex];
						}
						else
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += Weight * Weight * Stiffness[ConstraintIndex];
							}
						}
					}
				}
			}

			InitialWCSize = Indices.Num();
		}

		void AddWCResidual(const ParticleType& InParticles, const int32 p, const T Dt, TVec3<T>& res)
		{
			int32 WCIndex = Particle2WCIndices[p];
			if (WCIndex != INDEX_NONE)
			{
				for (int32 j = 0; j < WCIncidentElements[WCIndex].Num(); j++)
				{
					int32 ConstraintIndex = WCIncidentElements[WCIndex][j];
					int32 LocalIndex = WCIncidentElementsLocal[WCIndex][j];
					TVec3<T> SpringEdge((T)0.);
					for (int32 l = 0; l < Weights[ConstraintIndex].Num(); l++)
					{
						for (int32 beta = 0; beta < 3; beta++)
						{
							SpringEdge[beta] += Weights[ConstraintIndex][l] * InParticles.P(Indices[ConstraintIndex][l])[beta];
						}
					}
					for (int32 l = 0; l < SecondWeights[ConstraintIndex].Num(); l++)
					{
						for (int32 beta = 0; beta < 3; beta++)
						{
							SpringEdge[beta] -= SecondWeights[ConstraintIndex][l] * InParticles.P(SecondIndices[ConstraintIndex][l])[beta];
						}
					}
					T weight = T(0);
					if (LocalIndex >= Indices[ConstraintIndex].Num())
					{
						weight = -SecondWeights[ConstraintIndex][LocalIndex - Indices[ConstraintIndex].Num()];
					}
					else
					{
						weight = Weights[ConstraintIndex][LocalIndex];
					}
					if (IsAnisotropic[ConstraintIndex])
					{
						T comp = TVec3<T>::DotProduct(SpringEdge, Normals[ConstraintIndex]);
						TVec3<T> proj = Normals[ConstraintIndex] * comp;
						for (int32 alpha = 0; alpha < 3; alpha++)
						{
							res[alpha] += Dt * Dt * Stiffness[ConstraintIndex] * proj[alpha] * weight;
						}
					}
					else
					{
						for (int32 alpha = 0; alpha < 3; alpha++)
						{
							res[alpha] += Dt * Dt * Stiffness[ConstraintIndex] * SpringEdge[alpha] * weight;
						}
					}
				}
			}
		}

		void AddWCHessian(const int32 p, const T Dt, Chaos::PMatrix<T, 3, 3>& ParticleHessian)
		{
			if (NodalWeights[p].Num() > 0)
			{
				for (int32 alpha = 0; alpha < 3; alpha++)
				{
					ParticleHessian.SetAt(alpha, alpha, ParticleHessian.GetAt(alpha, alpha) + Dt * Dt * NodalWeights[p][alpha]);
				}

				ParticleHessian.SetAt(0, 1, ParticleHessian.GetAt(0, 1) + Dt * Dt * NodalWeights[p][3]);
				ParticleHessian.SetAt(0, 2, ParticleHessian.GetAt(0, 2) + Dt * Dt * NodalWeights[p][4]);
				ParticleHessian.SetAt(1, 2, ParticleHessian.GetAt(1, 2) + Dt * Dt * NodalWeights[p][5]);
				ParticleHessian.SetAt(1, 0, ParticleHessian.GetAt(1, 0) + Dt * Dt * NodalWeights[p][3]);
				ParticleHessian.SetAt(2, 0, ParticleHessian.GetAt(2, 0) + Dt * Dt * NodalWeights[p][4]);
				ParticleHessian.SetAt(2, 1, ParticleHessian.GetAt(2, 1) + Dt * Dt * NodalWeights[p][5]);
				//TODO(Yizhou): Clean up the following after debugging:
				//ParticleHessian.SetAt(0, 2) += Dt * Dt * NodalWeights[p][4];
				//ParticleHessian.SetAt(1, 2) += Dt * Dt * NodalWeights[p][5];
				//ParticleHessian.SetAt(1, 0) += Dt * Dt * NodalWeights[p][3];
				//ParticleHessian.SetAt(2, 0) += Dt * Dt * NodalWeights[p][4];
				//ParticleHessian.SetAt(2, 1) += Dt * Dt * NodalWeights[p][5];
			}
		}

		void AddExtraConstraints(const TArray<TArray<int32>>& InIndices,
								const TArray<TArray<T>>& InWeights,
								const TArray<T>& InStiffness,
								const TArray<TArray<int32>>& InSecondIndices,
								const TArray<TArray<T>>& InSecondWeights)
		{
			int32 Offset = Indices.Num();
			Indices.SetNum(Offset + InIndices.Num());
			SecondIndices.SetNum(Offset + InSecondIndices.Num());
			Stiffness.SetNum(Offset + InStiffness.Num());
			Weights.SetNum(Offset + InIndices.Num());
			SecondWeights.SetNum(Offset + InIndices.Num());

			for (int32 i = 0; i < InIndices.Num(); i++)
			{
				Indices[i + Offset] = InIndices[i];
				SecondIndices[i + Offset] = InSecondIndices[i];
				Stiffness[i + Offset] = InStiffness[i];
				Weights[i + Offset] = InWeights[i];
				SecondWeights[i + Offset] = InSecondWeights[i];
			}

			IsAnisotropic.Init(false, Indices.Num());
		}

		void VisualizeAllBindings(const FSolverParticles& InParticles, const T Dt) const
		{
#if WITH_EDITOR
			auto DoubleVert = [](Chaos::TVec3<T> V) { return FVector3d(V.X, V.Y, V.Z); };
			for (int32 i = 0; i < Indices.Num(); i++)
			{
				Chaos::TVec3<T> SourcePos((T)0.), TargetPos((T)0.);
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					SourcePos += Weights[i][j] * InParticles.P(Indices[i][j]);
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					TargetPos += SecondWeights[i][j] * InParticles.P(SecondIndices[i][j]);
				}

				float ParticleThickness = DebugDrawParams.DebugParticleWidth;
				float LineThickness = DebugDrawParams.DebugLineWidth;

				if (Indices[i].Num() == 1)
				{
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(SourcePos), FColor::Red, false, Dt, 0, ParticleThickness);
					for (int32 j = 0; j < SecondIndices[i].Num(); j++)
					{
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(InParticles.P(SecondIndices[i][j])), FColor::Green, false, Dt, 0, ParticleThickness);
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(DoubleVert(InParticles.P(SecondIndices[i][j])), DoubleVert(InParticles.P(SecondIndices[i][(j + 1) % SecondIndices[i].Num()])), FColor::Green, false, Dt, 0, LineThickness);
					}

				}

				if (SecondIndices[i].Num() == 1)
				{
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(TargetPos), FColor::Red, false, Dt, 0, ParticleThickness);
					for (int32 j = 0; j < Indices[i].Num(); j++)
					{
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(DoubleVert(InParticles.P(Indices[i][j])), FColor::Green, false, Dt, 0, ParticleThickness);
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(DoubleVert(InParticles.P(Indices[i][j])), DoubleVert(InParticles.P(Indices[i][(j + 1) % SecondIndices[i].Num()])), FColor::Green, false, Dt, 0, LineThickness);
					}
				}

				Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(DoubleVert(SourcePos), DoubleVert(TargetPos), FColor::Yellow, false, Dt, 0, LineThickness);
			}
#endif
		}

		void Init(const FSolverParticles& InParticles, const T Dt) const
		{
			if (DebugDrawParams.bVisualizeBindings)
			{
				VisualizeAllBindings(InParticles, Dt);
			}

		}

		void ComputeCollisionWCData(const ParticleType& InParticles, const TArray<TVector<int32, 4>>& MeshConstraints, const TArray <TArray<int32>>& MeshIncidentElements, const TArray <TArray<int32>>& MeshIncidentElementsLocal, TArray<TArray<int32>>& ParticlesPerColor)
		{
			TArray<TVector<int32, 4>> ExtraConstraints;
			ExtraConstraints.Init(TVector<int32, 4>(-1), Indices.Num());
			for (int32 i = InitialWCSize; i < Indices.Num(); i++)
			{
				ensureMsgf(Indices[i].Num() + SecondIndices[i].Num() == 4, TEXT("Currently does not support non point tri pair weak constraints"));
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					ExtraConstraints[i][j] = Indices[i][j];
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ExtraConstraints[i][j + Indices[i].Num()] = SecondIndices[i][j];
				}
			}

			TArray<TArray<int32>> ExtraWCIncidentElements, ExtraWCIncidentElementsLocal;
			ExtraWCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraWCIncidentElementsLocal);

			TArray<TVector<int32, 4>> TotalConstraints = NoCollisionConstraints;
			TotalConstraints += ExtraConstraints;
			//TODO (Yizhou): Make the following more efficient by computing only extra incident elements in the future. 
			WCIncidentElements = Chaos::Utilities::ComputeIncidentElements(TotalConstraints, &WCIncidentElementsLocal);

			Particle2WCIndices.Init(INDEX_NONE, InParticles.Size());
			for (int32 i = 0; i < WCIncidentElements.Num(); i++)
			{
				if (WCIncidentElements[i].Num() > 0)
				{
					int32 p = ExtraConstraints[WCIncidentElements[i][0]][WCIncidentElementsLocal[i][0]];
					Particle2WCIndices[p] = i;
				}
			}

			//TODO(Yizhou): Write the collision per timestep coloring to make the following thing much faster:
			TArray<TVector<int32, 4>> TrueAllConstraints = MeshConstraints;
			TrueAllConstraints += TotalConstraints;
			TArray<TArray<int32>> TrueAllIncidentElements, TrueAllIncidentElementsLocal;
			TrueAllIncidentElements = Chaos::Utilities::ComputeIncidentElements(TrueAllConstraints, &TrueAllIncidentElementsLocal);
			ParticlesPerColor = ComputeNodalColoring(TrueAllConstraints, InParticles, 0, InParticles.Size(), TrueAllIncidentElements, TrueAllIncidentElementsLocal);


			NodalWeights = NoCollisionNodalWeights;
			for (int32 i = 0; i < ExtraWCIncidentElements.Num(); i++) 
			{
				if (ExtraWCIncidentElements[i].Num() > 0)
				{
					int32 p = ExtraConstraints[ExtraWCIncidentElements[i][0]][ExtraWCIncidentElementsLocal[i][0]];
					if (NodalWeights[p].Num() == 0) 
					{
						NodalWeights[p].Init(T(0), 6);
					}
					for (int32 j = 0; j < ExtraWCIncidentElements[i].Num(); j++) 
					{
						int32 LocalIndex = ExtraWCIncidentElementsLocal[i][j];
						int32 ConstraintIndex = ExtraWCIncidentElements[i][j] + InitialWCSize;
						T weight = T(0);
						if (LocalIndex >= Indices[ConstraintIndex].Num()) 
						{
							weight = SecondWeights[ConstraintIndex][LocalIndex - Indices[ConstraintIndex].Num()];
						}
						else 
						{
							weight = Weights[ConstraintIndex][LocalIndex];
						}
						if (IsAnisotropic[ConstraintIndex]) 
						{
							for (int32 alpha = 0; alpha < 3; alpha++) 
							{
								NodalWeights[p][alpha] += Normals[ConstraintIndex][alpha] * Normals[ConstraintIndex][alpha] * weight * weight * Stiffness[ConstraintIndex];
							}

							NodalWeights[p][3] += Normals[ConstraintIndex][0] * Normals[ConstraintIndex][1] * weight * weight * Stiffness[ConstraintIndex];
							NodalWeights[p][4] += Normals[ConstraintIndex][0] * Normals[ConstraintIndex][2] * weight * weight * Stiffness[ConstraintIndex];
							NodalWeights[p][5] += Normals[ConstraintIndex][1] * Normals[ConstraintIndex][2] * weight * weight * Stiffness[ConstraintIndex];
						}
						else {
							for (int32 alpha = 0; alpha < 3; alpha++) 
							{
								NodalWeights[p][alpha] += weight * weight * Stiffness[ConstraintIndex];
							}
						}
					}
				}
			}

		}



		TArray<TArray<int32>> Indices;
		TArray<TArray<T>> Weights;
		TArray<TVector<T, 3>> Constraints;
		TArray<TArray<int32>> SecondIndices;
		TArray<TArray<T>> SecondWeights;
		TArray<T> Stiffness;
		TArray<bool> IsAnisotropic;
		TArray<TVector<T, 3>> Normals;
		TArray<TArray<T>> NodalWeights;
		TArray<int32> Particle2WCIndices;

		TArray<TArray<int32>> WCIncidentElements;
		TArray<TArray<int32>> WCIncidentElementsLocal;

		FDeformableXPBDWeakConstraintParams DebugDrawParams;

		int32 InitialWCSize;
		TArray<TArray<T>> NoCollisionNodalWeights;
		TArray<TVector<int32, 4>> NoCollisionConstraints;
	};


}// End namespace Chaos::Softs
