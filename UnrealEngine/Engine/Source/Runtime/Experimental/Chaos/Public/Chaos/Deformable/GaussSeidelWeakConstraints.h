// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/BoundingVolumeHierarchy.h"
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
#include "Chaos/Triangle.h"
#include "Chaos/TriangleCollisionPoint.h"
#include "Chaos/TriangleMesh.h"

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
				TSet<int32> IndicesSet = TSet<int32>(Indices[i]);
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ensureMsgf(!IndicesSet.Contains(SecondIndices[i][j]), TEXT("Indices and Second Indices overlaps. Currently not supported"));
				}
			}

			IsAnisotropic.Init(false, Indices.Num());
		}

		virtual ~FGaussSeidelWeakConstraints() {}

		void ComputeInitialWCData(const ParticleType& InParticles, const TArray<TArray<int32>>& MeshConstraints, const TArray <TArray<int32>>& MeshIncidentElements, const TArray <TArray<int32>>& MeshIncidentElementsLocal, TArray<TArray<int32>>& ParticlesPerColor)
		{
			TArray<TArray<int32>> ExtraConstraints;
			ExtraConstraints.Init(TArray<int32>(), Indices.Num());
			for (int32 i = 0; i < Indices.Num(); i++)
			{
				ExtraConstraints[i].SetNum(Indices[i].Num() + SecondIndices[i].Num());
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					ExtraConstraints[i][j] = Indices[i][j];
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ExtraConstraints[i][j+Indices[i].Num()] = SecondIndices[i][j];
				}
			}
			WCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &WCIncidentElementsLocal);

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
			NoCollisionNodalWeights = NodalWeights;
			NoCollisionConstraints = ExtraConstraints;
			InitialWCSize = Indices.Num();

			NoCollisionWCIncidentElements = WCIncidentElements;
			NoCollisionWCIncidentElementsLocal = WCIncidentElementsLocal;

			StaticConstraints = MeshConstraints;
			StaticConstraints += NoCollisionConstraints;
			StaticIncidentElements = MeshIncidentElements;
			for (int32 i = 0; i < NoCollisionWCIncidentElements.Num(); i++)
			{
				if (NoCollisionWCIncidentElements[i].Num() > 0)
				{
					StaticIncidentElements[i] += NoCollisionWCIncidentElements[i];
				}
			}
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
			Normals.SetNum(Indices.Num());
		}

		void Resize(int32 Size)
		{
			Indices.SetNum(Size);
			SecondIndices.SetNum(Size);
			Stiffness.SetNum(Size);
			Weights.SetNum(Size);
			SecondWeights.SetNum(Size);
			IsAnisotropic.SetNum(Size);
			Normals.SetNum(Size);
		}

		void UpdatePointTriangleCollisionWCData(const FSolverParticles& Particles)
		{	
			TArray<TArray<int32>> IndicesTemp = Indices;
			TArray<TArray<int32>> SecondIndicesTemp = SecondIndices;
			TArray<T> StiffnessTemp = Stiffness;
			TArray<TArray<T>> WeightsTemp = Weights;
			TArray<TArray<T>> SecondWeightsTemp = SecondWeights;
			TArray<bool> IsAnisotropicTemp = IsAnisotropic;
			TArray<TVector<T, 3>> NormalsTemp = Normals;

			Indices.SetNum(InitialWCSize);
			SecondIndices.SetNum(InitialWCSize);
			Stiffness.SetNum(InitialWCSize);
			Weights.SetNum(InitialWCSize);
			SecondWeights.SetNum(InitialWCSize);
			IsAnisotropic.SetNum(InitialWCSize);
			Normals.SetNum(InitialWCSize);

			for (int32 i = InitialWCSize; i < IndicesTemp.Num(); i++)
			{
				ensureMsgf(IndicesTemp[i].Num() == 3, TEXT("Collision format is not point-triangle"));
				ensureMsgf(SecondIndicesTemp[i].Num() == 1, TEXT("Collision format is not point-triangle"));
				Chaos::TVector<float, 3> TriPos0(Particles.P(IndicesTemp[i][0])), TriPos1(Particles.P(IndicesTemp[i][1])), TriPos2(Particles.P(IndicesTemp[i][2])), ParticlePos(Particles.P(SecondIndicesTemp[i][0]));
				Chaos::TVector<T, 3> Normal = FVector3f::CrossProduct(TriPos1 - TriPos0, TriPos2 - TriPos0);
				if (FVector3f::DotProduct(ParticlePos - TriPos0, Normal) < 0.f) //not resolved, keep the spring
				{
					Indices.Add(IndicesTemp[i]);
					SecondIndices.Add(SecondIndicesTemp[i]);
					Stiffness.Add(StiffnessTemp[i]);
					Weights.Add(WeightsTemp[i]);
					SecondWeights.Add(SecondWeightsTemp[i]);
					IsAnisotropic.Add(IsAnisotropicTemp[i]);
					Normals.Add(NormalsTemp[i]);
				}
			}
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

		void UpdateBoundaryVertices(const TArray<TVec3<int32>> Elements)
		{
			for (int32 i = 0; i < Elements.Num(); i++)
			{
				for (int32 j = 0; j < 3; j++)
				{
					BoundaryVertices.AddUnique(Elements[i][j]);
				}
			}
		}

		//CollisionDetectionSpatialHash should be faster than CollisionDetectionBVH
		void CollisionDetectionBVH(const FSolverParticles& Particles, const TArray<TVec3<int32>>& SurfaceElements, const TArray<int32>& ComponentIndex, float DetectRadius = 1.f, float PositionTargetStiffness = 10000.f, bool UseAnisotropicSpring = true)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelWeakConstraintsCollisionDetection);
			Resize(InitialWCSize);

			TArray<Chaos::TVector<int32, 3>> SurfaceElementsArray;
			for (int32 i = 0; i < SurfaceElements.Num(); i++)
			{
				Chaos::TVector<int32, 3> CurrentSurfaceElements(0);
				for (int32 j = 0; j < 3; j++)
				{
					CurrentSurfaceElements[j] = SurfaceElements[i][j];
				}
				if (CurrentSurfaceElements[0] != INDEX_NONE
					&& CurrentSurfaceElements[1] != INDEX_NONE
					&& CurrentSurfaceElements[2] != INDEX_NONE)
				{
					SurfaceElementsArray.Emplace(CurrentSurfaceElements);
				}
			}
			TArray<TArray<int32>> LocalIndex;
			TArray<TArray<int32>>* LocalIndexPtr = &LocalIndex;
			TArray<TArray<int>> GlobalIndex = Chaos::Utilities::ComputeIncidentElements(SurfaceElementsArray, LocalIndexPtr);
			int32 ActualParticleCount = 0;
			for (int32 l = 0; l < GlobalIndex.Num(); l++)
			{
				if (GlobalIndex[l].Num() > 0)
				{
					ActualParticleCount += 1;
				}
			}
			TArray<Chaos::TVector<float, 3>> SurfaceElementsPositions;
			SurfaceElementsPositions.SetNum(ActualParticleCount);
			TArray<int32> SurfaceElementsMap;
			SurfaceElementsMap.SetNum(ActualParticleCount);
			int32 CurrentParticleIndex = 0;
			for (int32 i = 0; i < GlobalIndex.Num(); i++)
			{
				if (GlobalIndex[i].Num() > 0)
				{
					SurfaceElementsPositions[CurrentParticleIndex] = Particles.P(SurfaceElements[GlobalIndex[i][0]][LocalIndex[i][0]]);
					SurfaceElementsMap[CurrentParticleIndex] = SurfaceElements[GlobalIndex[i][0]][LocalIndex[i][0]];
					CurrentParticleIndex += 1;
				}
			}

			TArray<Chaos::TSphere<Chaos::FReal, 3>*> VertexSpherePtrs;
			TArray<Chaos::TSphere<Chaos::FReal, 3>> VertexSpheres;

			VertexSpheres.Init(Chaos::TSphere<Chaos::FReal, 3>(Chaos::TVec3<Chaos::FReal>(0), DetectRadius), SurfaceElementsPositions.Num());
			VertexSpherePtrs.SetNum(SurfaceElementsPositions.Num());

			for (int32 i = 0; i < SurfaceElementsPositions.Num(); i++)
			{
				Chaos::TVec3<Chaos::FReal> SphereCenter(SurfaceElementsPositions[i]);
				Chaos::TSphere<Chaos::FReal, 3> VertexSphere(SphereCenter, DetectRadius);
				VertexSpheres[i] = Chaos::TSphere<Chaos::FReal, 3>(SphereCenter, DetectRadius);
				VertexSpherePtrs[i] = &VertexSpheres[i];
			}
			Chaos::TBoundingVolumeHierarchy<
				TArray<Chaos::TSphere<Chaos::FReal, 3>*>,
				TArray<int32>,
				Chaos::FReal,
				3> VertexBVH(VertexSpherePtrs);

			for (int32 i = 0; i < SurfaceElements.Num(); i++)
			{
				TArray<int32> TriangleIntersections0 = VertexBVH.FindAllIntersections(Particles.P(SurfaceElements[i][0]));
				TArray<int32> TriangleIntersections1 = VertexBVH.FindAllIntersections(Particles.P(SurfaceElements[i][1]));
				TArray<int32> TriangleIntersections2 = VertexBVH.FindAllIntersections(Particles.P(SurfaceElements[i][2]));
				TriangleIntersections0.Sort();
				TriangleIntersections1.Sort();
				TriangleIntersections2.Sort();

				TArray<int32> TriangleIntersections({});
				for (int32 k = 0; k < TriangleIntersections0.Num(); k++)
				{
					if (TriangleIntersections1.Contains(TriangleIntersections0[k])
						&& TriangleIntersections2.Contains(TriangleIntersections0[k]))
					{
						TriangleIntersections.Emplace(TriangleIntersections0[k]);
					}
				}

				int32 TriangleIndex = ComponentIndex[SurfaceElements[i][0]];
				int32 MinIndex = INDEX_NONE;
				float MinDis = DetectRadius;
				Chaos::TVector<float, 3> ClosestBary(0.f);
				Chaos::TVector<float, 3> FaceNormal;
				for (int32 j = 0; j < TriangleIntersections.Num(); j++)
				{
					if (ComponentIndex[SurfaceElementsMap[TriangleIntersections[j]]] >= 0 && TriangleIndex >= 0 && ComponentIndex[SurfaceElementsMap[TriangleIntersections[j]]] != TriangleIndex)
					{
						Chaos::TVector<float, 3> Bary, TriPos0(Particles.P(SurfaceElements[i][0])), TriPos1(Particles.P(SurfaceElements[i][1])), TriPos2(Particles.P(SurfaceElements[i][2])), ParticlePos(Particles.P(SurfaceElementsMap[TriangleIntersections[j]]));
						Chaos::TVector<Chaos::FRealSingle, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
						Chaos::FRealSingle CurrentDistance = (Particles.P(SurfaceElementsMap[TriangleIntersections[j]]) - ClosestPoint).Size();
						if (CurrentDistance < MinDis)
						{
							Chaos::TVector<T, 3> Normal = FVector3f::CrossProduct(TriPos2 - TriPos0, TriPos1 - TriPos0); //The normal needs to point outwards of the geometry
							if (FVector3f::DotProduct(ParticlePos - TriPos0, Normal) < 0.f)
							{
								Normal.SafeNormalize(1e-8f);
								MinDis = CurrentDistance;
								MinIndex = SurfaceElementsMap[TriangleIntersections[j]];
								ClosestBary = Bary;
								FaceNormal = Normal;
							}
						}

					}
				}
				if (MinIndex != INDEX_NONE
					&& MinIndex != SurfaceElements[i][0]
					&& MinIndex != SurfaceElements[i][1]
					&& MinIndex != SurfaceElements[i][2])
				{
					Indices.SetNum(Indices.Num() + 1);
					SecondIndices.SetNum(SecondIndices.Num() + 1);
					Weights.SetNum(Weights.Num() + 1);
					SecondWeights.SetNum(SecondWeights.Num() + 1);
					Indices[Indices.Num() - 1].Add(SurfaceElements[i][0]);
					Indices[Indices.Num() - 1].Add(SurfaceElements[i][1]);
					Indices[Indices.Num() - 1].Add(SurfaceElements[i][2]);
					SecondIndices[SecondIndices.Num() - 1].Add(MinIndex);
					Weights[Weights.Num() - 1].Add(ClosestBary[0]);
					Weights[Weights.Num() - 1].Add(ClosestBary[1]);
					Weights[Weights.Num() - 1].Add(ClosestBary[2]);
					SecondWeights[SecondWeights.Num() - 1].Add(1.f);

					float SpringStiffness = 0.f;
					for (int32 k = 0; k < 3; k++)
					{
						SpringStiffness += ClosestBary[k] * PositionTargetStiffness * Particles.M(SurfaceElements[i][k]);
					}
					SpringStiffness += PositionTargetStiffness * Particles.M(MinIndex);
					Stiffness.Add(SpringStiffness);
					IsAnisotropic.Add(UseAnisotropicSpring);
					Normals.Add(FaceNormal);
				}
			}
		}

		template<typename SpatialAccelerator>
		void CollisionDetectionSpatialHash(const FSolverParticles& Particles, const FTriangleMesh& TriangleMesh, const TArray<int32>& ComponentIndex, const SpatialAccelerator& Spatial, float DetectRadius = 1.f, float PositionTargetStiffness = 10000.f, bool UseAnisotropicSpring = true)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelWeakConstraintsCollisionDetectionSpatialHash);
			constexpr int32 MaxConnectionsPerPoint = 1;
			Resize(InitialWCSize + Particles.Size() * MaxConnectionsPerPoint);
			
			std::atomic<int32> ConstraintIndex(InitialWCSize);
			const TArray<TVec3<int32>>& Elements = TriangleMesh.GetSurfaceElements();
			PhysicsParallelFor(BoundaryVertices.Num(),
				[this, &Spatial, &Particles, &ConstraintIndex, MaxConnectionsPerPoint, &TriangleMesh, &Elements, &DetectRadius, &ComponentIndex, &PositionTargetStiffness, &UseAnisotropicSpring](int32 i)
				{
					const int32 Index = BoundaryVertices[i];

					TArray< TTriangleCollisionPoint<FSolverReal> > Result;
					if (TriangleMesh.PointProximityQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), Index, Particles.GetX(Index), DetectRadius, DetectRadius,
						[this, &ComponentIndex, &Elements](const int32 PointIndex, const int32 TriangleIndex)->bool
						{
							const TVector<int32, 3>& Elem = Elements[TriangleIndex];

							if (ComponentIndex[PointIndex] == ComponentIndex[Elements[TriangleIndex][0]])
							{
								return false;
							}

							return true;
						},
						Result))
					{

						if (Result.Num() > MaxConnectionsPerPoint)
						{
							// TODO: once we have a PartialSort, use that instead here.
							Result.Sort(
								[](const TTriangleCollisionPoint<FSolverReal>& First, const TTriangleCollisionPoint<FSolverReal>& Second)->bool
								{
									return First.Phi < Second.Phi;
								}
							);
							Result.SetNum(MaxConnectionsPerPoint, EAllowShrinking::No);
						}

						for (const TTriangleCollisionPoint<FSolverReal>& CollisionPoint : Result)
						{
							const TVector<int32, 3>& Elem = Elements[CollisionPoint.Indices[1]];
						

							// NOTE: CollisionPoint.Normal has already been flipped to point toward the Point, so need to recalculate here.
							const TTriangle<FSolverReal> Triangle(Particles.GetX(Elem[0]), Particles.GetX(Elem[1]), Particles.GetX(Elem[2]));
							if ((Particles.GetX(Index) - CollisionPoint.Location).Dot(-Triangle.GetNormal()) < 0) //Is point inside boundary? Normal should point outwards
							{
								const int32 IndexToWrite = ConstraintIndex.fetch_add(1);

								Indices[IndexToWrite] = { Elem[0], Elem[1] ,Elem[2] };
								SecondIndices[IndexToWrite] = { Index };
								Weights[IndexToWrite] = { CollisionPoint.Bary[1], CollisionPoint.Bary[2], CollisionPoint.Bary[3] };
								SecondWeights[IndexToWrite] = { 1.f };
								/*
								Indices[IndexToWrite].Add(Elem[0]);
								Indices[IndexToWrite].Add(Elem[1]);
								Indices[IndexToWrite].Add(Elem[2]);
								SecondIndices[IndexToWrite].Add(Index);
								Weights[IndexToWrite].Add(ClosestBary[0]);
								Weights[IndexToWrite].Add(ClosestBary[1]);
								Weights[IndexToWrite].Add(ClosestBary[2]);
								SecondWeights[SecondWeights.Num() - 1].Add(1.f);
								*/
								float SpringStiffness = 0.f;
								for (int32 k = 0; k < 3; k++)
								{
									SpringStiffness += Weights[IndexToWrite][k] * PositionTargetStiffness * Particles.M(Elem[k]);
								}
								SpringStiffness += PositionTargetStiffness * Particles.M(Index);
								Stiffness[IndexToWrite] = SpringStiffness;
								IsAnisotropic[IndexToWrite] = UseAnisotropicSpring;
								Normals[IndexToWrite] = Triangle.GetNormal();
							}							
						}
					}
				}
			);

			// Shrink the arrays to the actual number of found constraints.
			const int32 ConstraintNum = ConstraintIndex.load();
			Resize(ConstraintNum);
		}

		void ComputeCollisionWCData(const ParticleType& InParticles, const TArray<TArray<int32>>& MeshConstraints, const TArray <TArray<int32>>& MeshIncidentElements, const TArray <TArray<int32>>& MeshIncidentElementsLocal, TArray<TArray<int32>>& ParticlesPerColor)
		{
			ensureMsgf(Indices.Num() >= InitialWCSize, TEXT("The size of Indices is smaller than InitialWCSize"));
			TArray<TArray<int32>> ExtraConstraints;
			ExtraConstraints.Init(TArray<int32>(), Indices.Num());
			for (int32 i = InitialWCSize; i < Indices.Num(); i++)
			{
				ExtraConstraints[i - InitialWCSize].SetNum(Indices[i].Num() + SecondIndices[i].Num());
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j] = Indices[i][j];
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j + Indices[i].Num()] = SecondIndices[i][j];
				}
			}

			TArray<TArray<int32>> ExtraWCIncidentElements, ExtraWCIncidentElementsLocal;
			ExtraWCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraWCIncidentElementsLocal);

			//TArray<TVector<int32, 4>> TotalConstraints = NoCollisionConstraints;
			//TotalConstraints += ExtraConstraints;
			//TODO (Yizhou): Make the following more efficient by computing only extra incident elements in the future. 
			//WCIncidentElements = Chaos::Utilities::ComputeIncidentElements(TotalConstraints, &WCIncidentElementsLocal);

			//TArray<TVector<int32, 4>> TotalConstraints = NoCollisionConstraints;
			//TotalConstraints += ExtraConstraints;
			//TODO (Yizhou): Make the following more efficient by computing only extra incident elements in the future. 
			//TArray<TArray<int32>> WCIncidentElementsLocalTemp;
			////WCIncidentElements = Chaos::Utilities::ComputeIncidentElements(TotalConstraints, &WCIncidentElementsLocalTemp);
			//WCIncidentElementsLocal = WCIncidentElementsLocalTemp;
			WCIncidentElements = NoCollisionWCIncidentElements;
			WCIncidentElementsLocal = NoCollisionWCIncidentElementsLocal;

			if (NoCollisionWCIncidentElements.Num() < ExtraWCIncidentElements.Num())
			{
				WCIncidentElements.SetNum(ExtraWCIncidentElements.Num());
				WCIncidentElementsLocal.SetNum(ExtraWCIncidentElementsLocal.Num());
			}

			for (int32 i = 0; i < ExtraWCIncidentElements.Num(); i++)
			{
				if (ExtraWCIncidentElements[i].Num() > 0)
				{
					TArray<int32> ExtraWCIncidentElementsTemp = ExtraWCIncidentElements[i];
					for (int32 j = 0; j < ExtraWCIncidentElements[i].Num(); j++)
					{
						ExtraWCIncidentElementsTemp[j] += InitialWCSize;
					}
					WCIncidentElements[i] += ExtraWCIncidentElementsTemp;
					WCIncidentElementsLocal[i] += ExtraWCIncidentElementsLocal[i];
				}
			}

			//TODO(Yizhou): Check if the following variable is really necessary:
			Particle2WCIndices.Init(INDEX_NONE, InParticles.Size());
			for (int32 i = 0; i < WCIncidentElements.Num(); i++)
			{
				if (WCIncidentElements[i].Num() > 0)
				{
					int32 p = ExtraConstraints[WCIncidentElements[i][0]][WCIncidentElementsLocal[i][0]];
					Particle2WCIndices[p] = i;
				}
			}

			Chaos::ComputeExtraNodalColoring(StaticConstraints, ExtraConstraints, InParticles, StaticIncidentElements, ExtraWCIncidentElements, ParticleColors, ParticlesPerColor);

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
						else 
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += weight * weight * Stiffness[ConstraintIndex];
							}
						}
					}
				}
			}
		}


		void ComputeCollisionWCDataSimplified(TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraWCIncidentElements, TArray<TArray<int32>>& ExtraWCIncidentElementsLocal)
		{
			ensureMsgf(Indices.Num() >= InitialWCSize, TEXT("The size of Indices is smaller than InitialWCSize"));

			ExtraConstraints.Init(TArray<int32>(), Indices.Num() - InitialWCSize);
			for (int32 i = InitialWCSize; i < Indices.Num(); i++)
			{
				ExtraConstraints[i - InitialWCSize].SetNum(Indices[i].Num() + SecondIndices[i].Num());
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j] = Indices[i][j];
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j + Indices[i].Num()] = SecondIndices[i][j];
				}
			}

			ExtraWCIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraWCIncidentElementsLocal);

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
						else
						{
							for (int32 alpha = 0; alpha < 3; alpha++)
							{
								NodalWeights[p][alpha] += weight * weight * Stiffness[ConstraintIndex];
							}
						}
					}
				}
			}
		}


		TArray<TArray<int32>> GetStaticConstraintArrays(TArray<TArray<int32>>& IncidentElements, TArray<TArray<int32>> IncidentElementsLocal)
		{
			IncidentElements = NoCollisionWCIncidentElements;
			IncidentElementsLocal = NoCollisionWCIncidentElementsLocal;
			return NoCollisionConstraints;
		}

		TArray<TArray<int32>> GetDynamicConstraintArrays(TArray<TArray<int32>>& IncidentElements, TArray<TArray<int32>> IncidentElementsLocal)
		{
			TArray<TArray<int32>> ExtraConstraints;
			ExtraConstraints.Init(TArray<int32>(), Indices.Num());
			for (int32 i = InitialWCSize; i < Indices.Num(); i++)
			{
				ExtraConstraints[i - InitialWCSize].SetNum(Indices[i].Num() + SecondIndices[i].Num());
				for (int32 j = 0; j < Indices[i].Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j] = Indices[i][j];
				}
				for (int32 j = 0; j < SecondIndices[i].Num(); j++)
				{
					ExtraConstraints[i - InitialWCSize][j + Indices[i].Num()] = SecondIndices[i][j];
				}
			}

			IncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &IncidentElementsLocal);

			return ExtraConstraints;
		}

		void AddWCResidualAndHessian(const ParticleType& InParticles, const int32 ConstraintIndex, const int32 LocalIndex, const T Dt, TVec3<T>& ParticleResidual, Chaos::PMatrix<T, 3, 3>& ParticleHessian)
		{
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
					ParticleResidual[alpha] += Dt * Dt * Stiffness[ConstraintIndex] * proj[alpha] * weight;
				}
			}
			else
			{
				for (int32 alpha = 0; alpha < 3; alpha++)
				{
					ParticleResidual[alpha] += Dt * Dt * Stiffness[ConstraintIndex] * SpringEdge[alpha] * weight;
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
		//TArray<TVector<int32, 4>> NoCollisionConstraints;
		//For debugging
		bool Detected = false;
		TArray<int32> BoundaryVertices;

		TArray<TArray<int32>> NoCollisionConstraints;
		TArray<TArray<int32>> NoCollisionWCIncidentElements;
		TArray<TArray<int32>> NoCollisionWCIncidentElementsLocal;

		TArray<TArray<int32>> StaticConstraints;
		TArray<TArray<int32>> StaticIncidentElements;

		TArray<int32> ParticleColors; 
	};


}// End namespace Chaos::Softs
