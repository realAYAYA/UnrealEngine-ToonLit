// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitQRSVD.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/Deformable/ChaosDeformableSolverTypes.h"

DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Corotated Constraint"), STAT_ChaosXPBDCorotated, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Corotated Constraint Polar Compute"), STAT_ChaosXPBDCorotatedPolar, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Corotated Constraint Det Compute"), STAT_ChaosXPBDCorotatedDet, STATGROUP_Chaos);

namespace Chaos::Softs
{

	template <typename T, typename ParticleType>
	class FXPBDCorotatedConstraints 
	{

	public:
		//this one only accepts tetmesh input and mesh
		FXPBDCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const bool bRecordMetricIn = true,
			const T& EMesh = (T)10.0,
			const T& NuMesh = (T).3
			)
			: bRecordMetric(bRecordMetricIn), MeshConstraints(InMesh)
		{	

			LambdaArray.Init((T)0., 2 * MeshConstraints.Num());
			DmInverse.Init((T)0., 9 * MeshConstraints.Num());
			Measure.Init((T)0.,  MeshConstraints.Num());
			Lambda = EMesh * NuMesh / (((T)1. + NuMesh) * ((T)1. - (T)2. * NuMesh));
			Mu = EMesh / ((T)2. * ((T)1. + NuMesh));
			for (int e = 0; e < InMesh.Num(); e++)
			{
				PMatrix<T, 3, 3> Dm = DsInit(e, InParticles);
				PMatrix<T, 3, 3> DmInv = Dm.Inverse();
				for (int r = 0; r < 3; r++) {
					for (int c = 0; c < 3; c++) {
						DmInverse[(3 * 3) * e + 3 * r + c] = DmInv.GetAt(r, c);
					}
				}

				Measure[e] = Dm.Determinant() / (T)6.;

				//part of preprocessing: if inverted element is found, 
				//invert it so that the measure is positive
				if (Measure[e] < (T)0.)
				{

					Measure[e] = -Measure[e];
				}

			}

			InitColor(InParticles);
		}

		FXPBDCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const TArray<T>& EMeshArray,
			const T& NuMesh = (T).3,
			const bool bRecordMetricIn = false
		)
			: bRecordMetric(bRecordMetricIn), MeshConstraints(InMesh)
		{
			ensureMsgf(EMeshArray.Num() == InMesh.Num(), TEXT("Input Young Modulus Array Size is wrong"));
			LambdaArray.Init((T)0., 2 * MeshConstraints.Num());
			DmInverse.Init((T)0., 9 * MeshConstraints.Num());
			Measure.Init((T)0., MeshConstraints.Num());
			LambdaElementArray.Init((T)0., MeshConstraints.Num());
			MuElementArray.Init((T)0., MeshConstraints.Num());

			for (int e = 0; e < InMesh.Num(); e++)
			{
				for (int32 j = 0; j < 4; j++)
				{
					ensure(MeshConstraints[e][j] > -1 && MeshConstraints[e][j] < int32(InParticles.Size()));
				}
			}
			
			for (int e = 0; e < InMesh.Num(); e++)
			{
				LambdaElementArray[e] = EMeshArray[e] * NuMesh / (((T)1. + NuMesh) * ((T)1. - (T)2. * NuMesh));
				MuElementArray[e] = EMeshArray[e] / ((T)2. * ((T)1. + NuMesh));

				PMatrix<T, 3, 3> Dm = DsInit(e, InParticles);
				PMatrix<T, 3, 3> DmInv = Dm.Inverse();
				for (int r = 0; r < 3; r++) {
					for (int c = 0; c < 3; c++) {
						DmInverse[(3 * 3) * e + 3 * r + c] = DmInv.GetAt(r, c);
					}
				}

				Measure[e] = Dm.Determinant() / (T)6.;

				if (Measure[e] < (T)0.)
				{
					Measure[e] = -Measure[e];
				}
			}

			InitColor(InParticles);
		}

		FXPBDCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const TArray<T>& EMeshArray,
			const TArray<T>& NuMeshArray,
			TArray<T>&& AlphaJMeshArray,
			const FDeformableXPBDCorotatedParams& InParams,
			const T& NuMesh = (T).3,
			const bool bRecordMetricIn = false, 
			const bool bDoColoring = true
		)
			: CorotatedParams(InParams), AlphaJArray(MoveTemp(AlphaJMeshArray)), bRecordMetric(bRecordMetricIn), MeshConstraints(InMesh)
		{
			ensureMsgf(EMeshArray.Num() == InMesh.Num(), TEXT("Input Young Modulus Array Size is wrong"));
			LambdaArray.Init((T)0., 2 * MeshConstraints.Num());
			DmInverse.Init((T)0., 9 * MeshConstraints.Num());
			Measure.Init((T)0., MeshConstraints.Num());
			LambdaElementArray.Init((T)0., MeshConstraints.Num());
			MuElementArray.Init((T)0., MeshConstraints.Num());

			for (int e = 0; e < InMesh.Num(); e++)
			{
				for (int32 j = 0; j < 4; j++)
				{
					ensure(MeshConstraints[e][j] > -1 && MeshConstraints[e][j] < int32(InParticles.Size()));
					
					if (InParticles.InvM(MeshConstraints[e][j]) == (T)0.)
					{
						AlphaJArray[e] = (T)1.;
					}
				}
				
			}

			for (int e = 0; e < InMesh.Num(); e++)
			{
				LambdaElementArray[e] = EMeshArray[e] * NuMeshArray[e] / (((T)1. + NuMeshArray[e]) * ((T)1. - (T)2. * NuMeshArray[e]));
				MuElementArray[e] = EMeshArray[e] / ((T)2. * ((T)1. + NuMeshArray[e]));

				PMatrix<T, 3, 3> Dm = DsInit(e, InParticles);
				PMatrix<T, 3, 3> DmInv = Dm.Inverse();
				for (int r = 0; r < 3; r++) {
					for (int c = 0; c < 3; c++) {
						DmInverse[(3 * 3) * e + 3 * r + c] = DmInv.GetAt(r, c);
					}
				}

				Measure[e] = Dm.Determinant() / (T)6.;

				if (Measure[e] < (T)0.)
				{
					Measure[e] = -Measure[e];
				}
			}
			if (bDoColoring) {
				InitColor(InParticles);
			}
		}

		FXPBDCorotatedConstraints(
			const ParticleType& InParticles,
			const TArray<TVector<int32, 4>>& InMesh,
			const T GridN = (T).1,
			const T& EMesh = (T)10.0,
			const T& NuMesh = (T).3
		)
			: MeshConstraints(InMesh)
		{
			LambdaArray.Init((T)0., 2 * MeshConstraints.Num());
			DmInverse.Init((T)0., 9 * MeshConstraints.Num());
			Measure.Init((T)0., MeshConstraints.Num());
			Lambda = EMesh * NuMesh / (((T)1. + NuMesh) * ((T)1. - (T)2. * NuMesh));
			Mu = EMesh / ((T)2. * ((T)1. + NuMesh));
			for (int e = 0; e < InMesh.Num(); e++)
			{
				PMatrix<T, 3, 3> Dm = DsInit(e, InParticles);
				PMatrix<T, 3, 3> DmInv = Dm.Inverse();
				for (int r = 0; r < 3; r++) {
					for (int c = 0; c < 3; c++) {
						DmInverse[(3 * 3) * e + 3 * r + c] = DmInv.GetAt(r, c);
					}
				}

				Measure[e] = Dm.Determinant() / (T)6.;

				if (Measure[e] < (T)0.)
				{
					Measure[e] = -Measure[e];
				}
			}
		}

		virtual ~FXPBDCorotatedConstraints() {}

		PMatrix<T, 3, 3> DsInit(const int e, const ParticleType& InParticles) const {
			PMatrix<T, 3, 3> Result((T)0.);
			for (int i = 0; i < 3; i++) {
				for (int c = 0; c < 3; c++) {
					Result.SetAt(c, i, InParticles.GetX(MeshConstraints[e][i + 1])[c] - InParticles.GetX(MeshConstraints[e][0])[c]);
				}
			}
			return Result;
		}


		PMatrix<T, 3, 3> Ds(const int e, const ParticleType& InParticles) const {
			PMatrix<T, 3, 3> Result((T)0.);
			for (int i = 0; i < 3; i++) {
				for (int c = 0; c < 3; c++) {
					Result.SetAt(c, i, InParticles.GetP(MeshConstraints[e][i+1])[c] - InParticles.GetP(MeshConstraints[e][0])[c]);
				}
			}
			return Result;
		}


		PMatrix<T, 3, 3> F(const int e, const ParticleType& InParticles) const {
			return ElementDmInv(e) * Ds(e, InParticles);
		}

		PMatrix<T, 3, 3> ElementDmInv(const int e) const {
			PMatrix<T, 3, 3> DmInv((T)0.);
			for (int r = 0; r < 3; r++) {
				for (int c = 0; c < 3; c++) {
					DmInv.SetAt(r, c, DmInverse[(3 * 3) * e + 3 * r + c]);
				}
			}
			return DmInv;
		}
		
		virtual void Init() const 
		{
			for (T& Lambdas : LambdaArray) { Lambdas = (T)0.; }
		}

		virtual void ApplyInSerial(ParticleType& Particles, const T Dt, const int32 ElementIndex) const
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosXPBDCorotatedApplySingle);

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

		void ApplyInSerial(ParticleType& Particles, const T Dt) const
		{
			SCOPE_CYCLE_COUNTER(STAT_ChaosXPBDCorotated);
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosXPBDCorotatedApplySerial);
			const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
			for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
			{
				const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
				const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
				for (int32 Index = 0; Index < ColorSize; Index++)
				{
					const int32 ConstraintIndex = ColorStart + Index;
					ApplyInSerial(Particles, Dt, ConstraintIndex);
				}
			}


		}

		void ApplyInParallel(ParticleType& Particles, const T Dt) const
		{	
			//code for error metric:
			if (bRecordMetric)
			{
				GError.Init((T)0., 3 * Particles.Size());
				HErrorArray.Init((T)0., 2 * MeshConstraints.Num());
			}
			
			{
				SCOPE_CYCLE_COUNTER(STAT_ChaosXPBDCorotated);
				TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosXPBDCorotatedApply);
				if ((ConstraintsPerColorStartIndex.Num() > 1))//&& (MeshConstraints.Num() > Chaos_Spring_ParallelConstraintCount))
				{
					const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;

					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;

						int32 NumBatch = ColorSize / CorotatedParams.XPBDCorotatedBatchSize;
						if (ColorSize % CorotatedParams.XPBDCorotatedBatchSize != 0)
						{
							NumBatch += 1;
						}

						PhysicsParallelFor(NumBatch, [&](const int32 BatchIndex)
							{
								for (int32 BatchSubIndex = 0; BatchSubIndex < CorotatedParams.XPBDCorotatedBatchSize; BatchSubIndex++) {
									int32 TaskIndex = CorotatedParams.XPBDCorotatedBatchSize * BatchIndex + BatchSubIndex;
									const int32 ConstraintIndex = ColorStart + TaskIndex;
									if (ConstraintIndex < ColorStart + ColorSize)
									{
										ApplyInSerial(Particles, Dt, ConstraintIndex);
									}
								}
							}, NumBatch < CorotatedParams.XPBDCorotatedBatchThreshold);
					}
				}
			}
		}

		TVec4<TVector<T, 3>> GetPolarGradient(const PMatrix<T, 3, 3>& Fe, const PMatrix<T, 3, 3>& Re, const PMatrix<T, 3, 3>& DmInvT, const T C1) const
		{
			//TVector<T, 81> dRdF((T)0.);
			//Chaos::dRdFCorotated(Fe, dRdF);
			TVec4<TVector<T, 3>> dC1(TVector<T, 3>((T)0.));
			//dC1 = dC1dF * dFdX
			PMatrix<T, 3, 3> A = DmInvT * (Fe - Re);
			for (int alpha = 0; alpha < 3; alpha++) {
				for (int l = 0; l < 3; l++) {
					dC1[0][alpha] -= A.GetAt(alpha, l);
				}
			}
			for (int ie = 0; ie < 3; ie++) {
				for (int alpha = 0; alpha < 3; alpha++) {
					dC1[ie + 1][alpha] = A.GetAt(alpha, ie);
				}
			}

			if (C1 != 0)
			{
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 3; j++)
					{
						dC1[i][j] /= C1;
					}
				}
			}
			return dC1;

		}

		TVec4<TVector<T, 3>> GetDeterminantGradient(const PMatrix<T, 3, 3>& Fe, const PMatrix<T, 3, 3>& DmInvT) const
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosXPBDCorotatedGetDetGradient);
			//SCOPE_CYCLE_COUNTER(STAT_ChaosXPBDCorotatedDet);
			//const TVec4<int32>& Constraint = MeshConstraints[ElementIndex];

			//const PMatrix<T, 3, 3> Fe = F(ElementIndex, Particles);
			//PMatrix<T, 3, 3> DmInvT = ElementDmInv(ElementIndex).GetTransposed();
			TVec4<TVector<T, 3>> dC2(TVector<T, 3>((T)0.));

			PMatrix<T, 3, 3> JFinvT((T)0.);
			JFinvT.SetAt(0, 0, Fe.GetAt(1, 1) * Fe.GetAt(2, 2) - Fe.GetAt(2, 1) * Fe.GetAt(1, 2));
			JFinvT.SetAt(0, 1, Fe.GetAt(2, 0) * Fe.GetAt(1, 2) - Fe.GetAt(1, 0) * Fe.GetAt(2, 2));
			JFinvT.SetAt(0, 2, Fe.GetAt(1, 0) * Fe.GetAt(2, 1) - Fe.GetAt(2, 0) * Fe.GetAt(1, 1));
			JFinvT.SetAt(1, 0, Fe.GetAt(2, 1) * Fe.GetAt(0, 2) - Fe.GetAt(0, 1) * Fe.GetAt(2, 2));
			JFinvT.SetAt(1, 1, Fe.GetAt(0, 0) * Fe.GetAt(2, 2) - Fe.GetAt(2, 0) * Fe.GetAt(0, 2));
			JFinvT.SetAt(1, 2, Fe.GetAt(2, 0) * Fe.GetAt(0, 1) - Fe.GetAt(0, 0) * Fe.GetAt(2, 1));
			JFinvT.SetAt(2, 0, Fe.GetAt(0, 1) * Fe.GetAt(1, 2) - Fe.GetAt(1, 1) * Fe.GetAt(0, 2));
			JFinvT.SetAt(2, 1, Fe.GetAt(1, 0) * Fe.GetAt(0, 2) - Fe.GetAt(0, 0) * Fe.GetAt(1, 2));
			JFinvT.SetAt(2, 2, Fe.GetAt(0, 0) * Fe.GetAt(1, 1) - Fe.GetAt(1, 0) * Fe.GetAt(0, 1));

			PMatrix<T, 3, 3> JinvTDmInvT = DmInvT * JFinvT;

			for (int ie = 0; ie < 3; ie++) {
				for (int alpha = 0; alpha < 3; alpha++) {
					dC2[ie + 1][alpha] = JinvTDmInvT.GetAt(alpha, ie);
				}
			}
			for (int alpha = 0; alpha < 3; alpha++) {
				for (int l = 0; l < 3; l++) {
					dC2[0][alpha] -= JinvTDmInvT.GetAt(alpha, l);
				}
			}
			return dC2;
		}


	protected:

		void InitColor(const ParticleType& Particles)
		{

			{
				const TArray<TArray<int32>> ConstraintsPerColor = FGraphColoring::ComputeGraphColoring(MeshConstraints, Particles);

				// Reorder constraints based on color so each array in ConstraintsPerColor contains contiguous elements.
				TArray<TVec4<int32>> ReorderedConstraints;
				TArray<T> ReorderedMeasure;
				TArray<T> ReorderedDmInverse;
				TArray<int32> OrigToReorderedIndices; // used to reorder stiffness indices
				ReorderedConstraints.SetNumUninitialized(MeshConstraints.Num());
				ReorderedMeasure.SetNumUninitialized(Measure.Num());
				ReorderedDmInverse.SetNumUninitialized(DmInverse.Num());
				OrigToReorderedIndices.SetNumUninitialized(MeshConstraints.Num());

				ConstraintsPerColorStartIndex.Reset(ConstraintsPerColor.Num() + 1);

				int32 ReorderedIndex = 0;
				for (const TArray<int32>& ConstraintsBatch : ConstraintsPerColor)
				{
					ConstraintsPerColorStartIndex.Add(ReorderedIndex);
					for (const int32& BatchConstraint : ConstraintsBatch)
					{
						const int32 OrigIndex = BatchConstraint;
						ReorderedConstraints[ReorderedIndex] = MeshConstraints[OrigIndex];
						ReorderedMeasure[ReorderedIndex] = Measure[OrigIndex];
						for (int32 kk = 0; kk < 9; kk++)
						{
							ReorderedDmInverse[9 * ReorderedIndex + kk] = DmInverse[9 * OrigIndex + kk];
						}
						OrigToReorderedIndices[OrigIndex] = ReorderedIndex;

						++ReorderedIndex;
					}
				}
				ConstraintsPerColorStartIndex.Add(ReorderedIndex);

				MeshConstraints = MoveTemp(ReorderedConstraints);
				Measure = MoveTemp(ReorderedMeasure);
				DmInverse = MoveTemp(ReorderedDmInverse);
			}
		}


		virtual TVec4<TVector<T, 3>> GetDeterminantDelta(const ParticleType& Particles, const T Dt, const int32 ElementIndex, const T Tol = (T)1e-3) const
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosXPBDCorotatedApplyDet);
			//SCOPE_CYCLE_COUNTER(STAT_ChaosXPBDCorotatedDet);
			////const TVec4<int32>& Constraint = MeshConstraints[ElementIndex];

			const PMatrix<T, 3, 3> Fe = F(ElementIndex, Particles);
			PMatrix<T, 3, 3> DmInvT = ElementDmInv(ElementIndex).GetTransposed();
			
			T J = Fe.Determinant();
			if (J - AlphaJArray[ElementIndex] < Tol)
			{
				return TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.));
			}

			TVec4<TVector<T, 3>> dC2 = GetDeterminantGradient(Fe, DmInvT);

			//T AlphaTilde = (T)2. / (Dt * Dt * Lambda * Measure[ElementIndex]);
			T AlphaTilde = (T)2. / (Dt * Dt * LambdaElementArray[ElementIndex] * Measure[ElementIndex]);

			if (LambdaElementArray[ElementIndex] > (T)1. / (T)UE_SMALL_NUMBER)
			{
				AlphaTilde = (T)0.;
			}

			if (bRecordMetric)
			{
				HError += J - 1 + AlphaTilde * LambdaArray[2 * ElementIndex + 1];
				for (int32 ie = 0; ie < 4; ie++)
				{
					for (int32 Alpha = 0; Alpha < 3; Alpha++)
					{
						GError[MeshConstraints[ElementIndex][ie] * 3 + Alpha] -= dC2[ie][Alpha] * LambdaArray[2 * ElementIndex + 1];
					}
				}
				HErrorArray[2 * ElementIndex + 1] = J - 1 + AlphaTilde * LambdaArray[2 * ElementIndex + 1];
			}
			

			T DLambda = (AlphaJArray[ElementIndex] - J) - AlphaTilde * LambdaArray[2 * ElementIndex + 1];

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
			TVec4<TVector<T, 3>> Delta(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					Delta[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i]) * dC2[i][j] * DLambda;
				}
			}
			return Delta;
		}



		virtual TVec4<TVector<T, 3>> GetPolarDelta(const ParticleType& Particles, const T Dt, const int32 ElementIndex, const T Tol = (T)1e-3) const
		{	
			TRACE_CPUPROFILER_EVENT_SCOPE(STAT_ChaosXPBDCorotatedApplyPolar);
			SCOPE_CYCLE_COUNTER(STAT_ChaosXPBDCorotatedPolar);
			const PMatrix<T, 3, 3> Fe = F(ElementIndex, Particles);

			PMatrix<T, 3, 3> Re((T)0.), Se((T)0.);

			Chaos::PolarDecomposition(Fe, Re, Se);

			Re *= FGenericPlatformMath::Pow(AlphaJArray[ElementIndex], (T)1. / (T)3.);

			T C1 = (T)0.;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					C1 += FMath::Square((Fe - Re).GetAt(i, j));
				}
			}
			C1 = FMath::Sqrt(C1);

			if (C1 < Tol )
			{
				return TVec4<TVector<T, 3>>(TVector<T, 3>((T)0.));
			}


			PMatrix<T, 3, 3> DmInvT = ElementDmInv(ElementIndex).GetTransposed();

			TVec4<TVector<T, 3>> dC1 = GetPolarGradient(Fe, Re, DmInvT, C1);

			//T AlphaTilde = (T)1. / (Dt * Dt * Mu * Measure[ElementIndex]);
			T AlphaTilde = (T)1. / (Dt * Dt * MuElementArray[ElementIndex] * Measure[ElementIndex]);

			if (MuElementArray[ElementIndex] > (T)1./ (T)UE_SMALL_NUMBER) 
			{
				AlphaTilde = (T)0.;
			}

			if (bRecordMetric)
			{
				HError += C1 + AlphaTilde * LambdaArray[2 * ElementIndex + 0];
				for (int32 ie = 0; ie < 4; ie++)
				{
					for (int32 Alpha = 0; Alpha < 3; Alpha++)
					{
						GError[MeshConstraints[ElementIndex][ie] * 3 + Alpha] -= dC1[ie][Alpha] * LambdaArray[2 * ElementIndex + 0];
					}
				}
				HErrorArray[2 * ElementIndex + 0] = C1 + AlphaTilde * LambdaArray[2 * ElementIndex + 0];
			}

			T DLambda = -C1 - AlphaTilde* LambdaArray[2 * ElementIndex + 0];

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
			TVec4<TVector<T, 3>> Delta(TVector<T, 3>((T)0.));
			for (int i = 0; i < 4; i++) 
			{
				for (int j = 0; j < 3; j++)
				{
					Delta[i][j] = Particles.InvM(MeshConstraints[ElementIndex][i]) * dC1[i][j] * DLambda;
				}
			}
			return Delta;

		}


	protected:
		mutable TArray<T> LambdaArray;
		mutable TArray<T> DmInverse;

		//parallel data:
		FDeformableXPBDCorotatedParams CorotatedParams;

		//material constants calculated from E:
		T Mu;
		T Lambda;
		TArray<T> MuElementArray;
		TArray<T> LambdaElementArray;
		TArray<T> AlphaJArray;
		mutable T HError;
		mutable TArray<T> HErrorArray;
		bool bRecordMetric;
		bool VariableStiffness = false;

		TArray<TVector<int32, 4>> MeshConstraints;
		mutable TArray<T> Measure;
		ParticleType RestParticles;
		TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
		mutable TArray<T> GError;
};

}  // End namespace Chaos::Softs

