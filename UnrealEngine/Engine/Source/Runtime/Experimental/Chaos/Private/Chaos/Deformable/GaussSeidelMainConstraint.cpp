// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Deformable/GaussSeidelMainConstraint.h"
#include "Chaos/Math/Krylov.h"

namespace Chaos::Softs
{
	int32 MaxItCG = 50;

	FAutoConsoleVariableRef CVarClothMaxItCG(TEXT("p.Chaos.Cloth.MaxItCG"), MaxItCG, TEXT("Max iter for CG [def: 50]"));

	FSolverReal CGTol = 1e-4f;

	FAutoConsoleVariableRef CVarClothCGTol(TEXT("p.Chaos.Cloth.CGTol"), MaxItCG, TEXT("CG Tolerance [def: 1e-4]"));

	template <typename T, typename ParticleType>
	void FGaussSeidelMainConstraint<T, ParticleType>::AddStaticConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal)
	{	
		if (!IsClean(ExtraConstraints, ExtraIncidentElements, ExtraIncidentElementsLocal))
		{
			ExtraIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraIncidentElementsLocal);
		}

		int32 Offset = StaticConstraints.Num();
		StaticConstraints += ExtraConstraints;
		for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
		{
			if (ExtraIncidentElements[i].Num() > 0)
			{
				TArray<int32> ExtraIncidentElementsWithOffset = ExtraIncidentElements[i];
				for (int32 j = 0; j < ExtraIncidentElementsWithOffset.Num(); j++)
				{
					ExtraIncidentElementsWithOffset[j] += Offset;
				}
				StaticIncidentElements[i] += ExtraIncidentElementsWithOffset;
				StaticIncidentElementsLocal[i] += ExtraIncidentElementsLocal[i];
			}
		}
		if (StaticIncidentElementsOffsets.Num() > 0)
		{
			StaticIncidentElementsOffsets.RemoveAt(StaticIncidentElementsOffsets.Num() - 1);
		}
		StaticIncidentElementsOffsets.Add(Offset);
		StaticIncidentElementsOffsets.Add(StaticConstraints.Num());
	}

	template <typename T, typename ParticleType>
	void FGaussSeidelMainConstraint<T, ParticleType>::AddDynamicConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal, bool CheckIncidentElements)
	{
		if (CheckIncidentElements)
		{
			if (!IsClean(ExtraConstraints, ExtraIncidentElements, ExtraIncidentElementsLocal))
			{
				ExtraIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraIncidentElementsLocal);
			}
		}

		int32 Offset = DynamicConstraints.Num();
		DynamicConstraints += ExtraConstraints;
		for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
		{
			if (ExtraIncidentElements[i].Num() > 0)
			{
				TArray<int32> ExtraIncidentElementsWithOffset = ExtraIncidentElements[i];
				for (int32 j = 0; j < ExtraIncidentElementsWithOffset.Num(); j++)
				{
					ExtraIncidentElementsWithOffset[j] += Offset;
				}
				DynamicIncidentElements[i] += ExtraIncidentElementsWithOffset;
				DynamicIncidentElementsLocal[i] += ExtraIncidentElementsLocal[i];
			}
		}
		if (DynamicIncidentElementsOffsets.Num() > 0)
		{
			DynamicIncidentElementsOffsets.RemoveAt(StaticIncidentElementsOffsets.Num() - 1);
		}
		DynamicIncidentElementsOffsets.Add(Offset);
		DynamicIncidentElementsOffsets.Add(DynamicConstraints.Num());
	
	}

	template <typename T, typename ParticleType>
	TArray<TVec3<T>> FGaussSeidelMainConstraint<T, ParticleType>::ComputeNewtonResiduals(const ParticleType& Particles, const T Dt, const bool Write2File, TArray<PMatrix<T, 3, 3>>* AllParticleHessian)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelComputeNewtonResidual);

		TArray<TVec3<T>> NewtonResidual;
		NewtonResidual.Init(TVec3<T>((T)0.), xtilde.Num());

		if (AllParticleHessian)
		{
			AllParticleHessian->Init(PMatrix<T, 3, 3>((T)0.), Particles.Size());
		}

		for (int32 k = 0; k < ParticlesPerColor.Num(); k++)
		{
			PhysicsParallelFor(ParticlesPerColor[k].Num(), [this, &AllParticleHessian, Dt, k, &NewtonResidual, &Particles](const int32 j)
				{
					const int32 p = ParticlesPerColor[k][j];

					if (Particles.InvM(p) != (T)0.)
					{
						int32 ConstraintIndex = 0;
						Chaos::PMatrix<T, 3, 3> ParticleHessian((T)0., (T)0., (T)0.);

						this->ComputeInitialResidualAndHessian(Particles, p, Dt, NewtonResidual[p], ParticleHessian);

						for (int32 i = 0; i < StaticIncidentElements[p].Num(); i++)
						{
							while (StaticIncidentElements[p][i] >= StaticIncidentElementsOffsets[ConstraintIndex + 1] && ConstraintIndex < StaticIncidentElementsOffsets.Num() - 1)
							{
								ConstraintIndex += 1;
							}

							this->AddStaticConstraintResidualAndHessian[ConstraintIndex](Particles, StaticIncidentElements[p][i] - StaticIncidentElementsOffsets[ConstraintIndex], StaticIncidentElementsLocal[p][i], Dt, NewtonResidual[p], ParticleHessian);
						}

						ConstraintIndex = 0;

						for (int32 i = 0; i < DynamicIncidentElements[p].Num(); i++)
						{
							while (DynamicIncidentElements[p][i] >= DynamicIncidentElementsOffsets[ConstraintIndex + 1] && ConstraintIndex < DynamicIncidentElementsOffsets.Num() - 1)
							{
								ConstraintIndex += 1;
							}

							this->AddDynamicConstraintResidualAndHessian[ConstraintIndex](Particles, DynamicIncidentElements[p][i] - DynamicIncidentElementsOffsets[ConstraintIndex], DynamicIncidentElementsLocal[p][i], Dt, NewtonResidual[p], ParticleHessian);
						}

						if (AllParticleHessian)
						{
							(*AllParticleHessian)[p] = ParticleHessian;
						}
					}
			}, ParticlesPerColor[k].Num() < 1000);
		}

		T NewtonNorm = (T)0.;
		for (int32 p = 0; p < NewtonResidual.Num(); p++)
		{
			NewtonNorm += NewtonResidual[p].SizeSquared();
		}

		NewtonNorm = FMath::Sqrt(NewtonNorm);

		UE_LOG(LogTemp, Warning, TEXT("Current Iteration: %d"), CurrentIt);

		UE_LOG(LogTemp, Warning, TEXT("Newton Residual is %f"), NewtonNorm);

		PassedIters++;


		if (Write2File)
		{
			FString file = FPaths::ProjectDir();
			file.Append(TEXT("/DebugOutput/NewtonResidual.txt"));
			if (PassedIters == 0)
			{
				FFileHelper::SaveStringToFile(FString(TEXT("Newton Norm\r\n")), *file);
			}
			FFileHelper::SaveStringToFile((FString::SanitizeFloat(NewtonNorm) + FString(TEXT(",\r\n"))), *file, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
		}

		return NewtonResidual;

	}

	template <typename T, typename ParticleType>
	void FGaussSeidelMainConstraint<T, ParticleType>::ApplyCG(ParticleType& Particles, const T Dt)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosGaussSeidelApplyCG);
		TFunction<void(TArray<FSolverVec3>&)> ProjectBCs = [&Particles](TArray<FSolverVec3>& y)
		{
			for (int32 i = 0; i < y.Num(); i++)
			{
				if (Particles.InvM(i) == 0.f)
				{
					y[i] = FSolverVec3(0.f);
				}
			}
		};

		int32 max_it_cg = MaxItCG;
		FSolverReal cg_tol = CGTol;
		int32 MaxNewtonIt = 1;

		for (int32 It = 0; It < MaxNewtonIt; It++)
		{
			const TArray<FSolverVec3> Residual = ComputeNewtonResiduals(Particles, Dt, false, nullptr);

			auto multiply = [this, &ProjectBCs, &Dt, &Particles](TArray<FSolverVec3>& y, const TArray<FSolverVec3>& x) 
			{
				TArray<FSolverVec3> XProj = x;
				ProjectBCs(XProj);

				y.Init(FSolverVec3((FSolverReal)0.), XProj.Num());

				for (int32 i = 0; i < this->AddInternalForceDifferentials.Num(); i++)
				{
					this->AddInternalForceDifferentials[i](Particles, XProj, y);
				}

				PhysicsParallelFor(y.Num(), 
					[&y, &Particles, &XProj, Dt](const int32 i) 
					{
						y[i] = Particles.M(i) * XProj[i] + Dt * Dt * y[i];
					});

				ProjectBCs(y);
			};

			TArray<FSolverVec3> Deltax;
			Deltax.Init(FSolverVec3(0.f), Particles.Size());

			Chaos::LanczosCG<FSolverReal>(multiply, Deltax, Residual, MaxItCG, CGTol, use_list.Get()); 

			PhysicsParallelFor(Particles.Size(), [&Particles, &Deltax](const int32 i) 
				{
					Particles.P(i) -= Deltax[i];
				});
		}
	}
}


template class Chaos::Softs::FGaussSeidelMainConstraint<Chaos::Softs::FSolverReal, Chaos::Softs::FSolverParticles>;

template CHAOS_API void Chaos::Softs::FGaussSeidelMainConstraint<Chaos::FRealDouble, Chaos::TDynamicParticles<Chaos::FRealDouble, 3>>::AddStaticConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal);

template CHAOS_API void Chaos::Softs::FGaussSeidelMainConstraint<Chaos::FRealDouble, Chaos::TDynamicParticles<Chaos::FRealDouble, 3>>::AddDynamicConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal, bool CheckIncidentElements);
