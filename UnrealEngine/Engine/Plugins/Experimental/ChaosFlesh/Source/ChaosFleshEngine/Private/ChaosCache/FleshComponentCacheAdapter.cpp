// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCache/FleshComponentCacheAdapter.h"

#include "Chaos/ChaosCache.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/PBDEvolution.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/FleshComponent.h"

#if USE_USD_SDK && DO_USD_CACHING

#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

#include "ChaosCachingUSD/Operations.h"
#include "USDConversionUtils.h" // for GetPrimPathForObject()

#endif // USE_USD_SDK && DO_USD_CACHING

DEFINE_LOG_CATEGORY(LogChaosFleshCache)

namespace Chaos
{
	FFleshCacheAdapterCVarParams CVarParams;

#if USE_USD_SDK && DO_USD_CACHING

	FAutoConsoleVariableRef CVarDeformableFleshCacheWriteBinaryUSD(
		TEXT("p.Chaos.Caching.USD.WriteBinary"),
		CVarParams.bWriteBinary,
		TEXT("Write binary (usdc) cache files. [def: true]"));
	FAutoConsoleVariableRef CVarDeformableFleshCacheNoClobberUSD(
		TEXT("p.Chaos.Caching.USD.NoClobber"),
		CVarParams.bNoClobber,
		TEXT("Rename rather than over write existing cach files. [def: true]"));
	FAutoConsoleVariableRef CVarDeformableFleshCacheSaveFrequency(
		TEXT("p.Chaos.Caching.USD.SaveFrequency"),
		CVarParams.SaveFrequency,
		TEXT("Interval in frames to flush USD data to disk. 2 saves every other frame, 1 saves every frame, 0 caches in memory until complete. [def: 10]"));

#endif // USE_USD_SDK && DO_USD_CACHING

	FFleshCacheAdapter::~FFleshCacheAdapter()
	{
#if USE_USD_SDK && DO_USD_CACHING
		if (MonolithStage)
		{
			UE::ChaosCachingUSD::CloseStage(MonolithStage);
		}
#endif // USE_USD_SDK && DO_USD_CACHING
	}

	FComponentCacheAdapter::SupportType FFleshCacheAdapter::SupportsComponentClass(UClass* InComponentClass) const
	{
		const UClass* Desired = GetDesiredClass();
		if(InComponentClass == Desired)
		{
			return FComponentCacheAdapter::SupportType::Direct;
		}
		else if(InComponentClass->IsChildOf(Desired))
		{
			return FComponentCacheAdapter::SupportType::Derived;
		}

		return FComponentCacheAdapter::SupportType::None;
	}

	UClass* FFleshCacheAdapter::GetDesiredClass() const
	{
		return UFleshComponent::StaticClass();
	}

	uint8 FFleshCacheAdapter::GetPriority() const
	{
		return EngineAdapterPriorityBegin;
	}

	void FFleshCacheAdapter::Record_PostSolve(UPrimitiveComponent* InComponent, const FTransform& InRootTransform, FPendingFrameWrite& OutFrame, Chaos::FReal InTime) const
	{
		if( FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess(Solver, Chaos::Softs::FPhysicsThreadAccessor());

			if (FEvolution* Evolution = PhysicsThreadAccess.GetEvolution())
			{
				const FParticles& Particles = Evolution->Particles();

				const uint32 NumParticles = Particles.Size();
				if (NumParticles > 0)
				{
#if USE_USD_SDK && DO_USD_CACHING
					FScopedUsdAllocs UsdAllocs; // Use USD memory allocator

					// Update time range.
					MinTime = FMath::Min(InTime, MinTime);
					MaxTime = FMath::Max(InTime, MaxTime);

					if (MonolithStage)
					{
						if (!UE::ChaosCachingUSD::WritePoints(MonolithStage, PrimPath, InTime, Particles.XArray(), Particles.GetV()))
						{
							UE_LOG(LogChaosFleshCache, Error,
								TEXT("Failed to write points '%s' at time %g to file: '%s'"),
								*PrimPath, InTime, *MonolithStage.GetRootLayer().GetDisplayName());
							return;
						}

						uint64 NumTimeSamples = UE::ChaosCachingUSD::GetNumTimeSamples(MonolithStage, PrimPath, UE::ChaosCachingUSD::GetPointsAttrName());
						if (CVarParams.SaveFrequency >= 1 && NumTimeSamples % CVarParams.SaveFrequency == 0)
						{
							if (!UE::ChaosCachingUSD::SaveStage(MonolithStage, MinTime, MaxTime))
							{
								UE_LOG(LogChaosFleshCache, Error,
									TEXT("Failed to save file: '%s'"),
									*MonolithStage.GetRootLayer().GetDisplayName());
							}
						}
					}
#else // USE_USD_SDK && DO_USD_CACHING
					const Softs::FSolverVec3* ParticleXs = &Particles.X(0);
					const Softs::FSolverVec3* ParticleVs = &Particles.V(0);

					TArray<float> PendingVX, PendingVY, PendingVZ, PendingPX, PendingPY, PendingPZ;
					TArray<int32>& PendingID = OutFrame.PendingChannelsIndices;

					PendingID.Reserve(NumParticles);
					PendingVX.Reserve(NumParticles);
					PendingVY.Reserve(NumParticles);
					PendingVZ.Reserve(NumParticles);
					PendingPX.Reserve(NumParticles);
					PendingPY.Reserve(NumParticles);
					PendingPZ.Reserve(NumParticles);

					for (uint32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
					{
						const Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
						const Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];

						// Adding the vertices relative position to the particle write datas
						PendingID.Add(ParticleIndex);
						PendingVX.Add(ParticleV.X);
						PendingVY.Add(ParticleV.Y);
						PendingVZ.Add(ParticleV.Z);

						PendingPX.Add(ParticleX.X);
						PendingPY.Add(ParticleX.Y);
						PendingPZ.Add(ParticleX.Z);
					}

					OutFrame.PendingChannelsData.Add(VelocityXName, PendingVX);
					OutFrame.PendingChannelsData.Add(VelocityYName, PendingVY);
					OutFrame.PendingChannelsData.Add(VelocityZName, PendingVZ);
					OutFrame.PendingChannelsData.Add(PositionXName, PendingPX);
					OutFrame.PendingChannelsData.Add(PositionYName, PendingPY);
					OutFrame.PendingChannelsData.Add(PositionZName, PendingPZ);

#endif // USE_USD_SDK && DO_USD_CACHING
				}
			}
		}
	}

	void FFleshCacheAdapter::Playback_PreSolve(UPrimitiveComponent* InComponent, UChaosCache* InCache, Chaos::FReal InTime, FPlaybackTickRecord& TickRecord, TArray<TPBDRigidParticleHandle<Chaos::FReal, 3>*>& OutUpdatedRigids) const
	{
		if (FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess(Solver, Softs::FPhysicsThreadAccessor());

			if (FEvolution* Evolution = PhysicsThreadAccess.GetEvolution())
			{
#if USE_USD_SDK && DO_USD_CACHING
				if (MonolithStage)
				{
					FScopedUsdAllocs UEAllocs; // Use USD memory allocator
					pxr::VtArray<pxr::GfVec3f> Points0, Points1;
					pxr::VtArray<pxr::GfVec3f> Vels0, Vels1;

					double TargetTime = InTime;// TickRecord.GetTime();
					double Prev = -TNumericLimits<double>::Max();
					double Next = -TNumericLimits<double>::Max();
					double PrevV = -TNumericLimits<double>::Max();
					double NextV = -TNumericLimits<double>::Max();
					if (!UE::ChaosCachingUSD::GetBracketingTimeSamples(
							MonolithStage, PrimPath, UE::ChaosCachingUSD::GetPointsAttrName(), TargetTime, &Prev, &Next) ||
						!UE::ChaosCachingUSD::GetBracketingTimeSamples(
							MonolithStage, PrimPath, UE::ChaosCachingUSD::GetVelocityAttrName(), TargetTime, &PrevV, &NextV) ||
						Prev != PrevV ||
						Next != NextV)
					{
						UE_LOG(LogChaosFleshCache, Error,
							TEXT("Inconsistent bracketing time samples for attributes '%s' and '%s' at frame %g from file: '%s'"),
							*UE::ChaosCachingUSD::GetPointsAttrName(),
							*UE::ChaosCachingUSD::GetVelocityAttrName(),
							TargetTime, *MonolithStage.GetRootLayer().GetDisplayName());
						return;
					}

					if (!UE::ChaosCachingUSD::ReadPoints(MonolithStage, PrimPath, Prev, Points0, Vels0) ||
						Points0.size() != Vels0.size())
					{
						UE_LOG(LogChaosFleshCache, Error,
							TEXT("Failed to read points '%s' at time %g from file: '%s'"),
							*PrimPath, Prev, *MonolithStage.GetRootLayer().GetDisplayName());
						return;
					}

					FParticles& Particles = Evolution->Particles();
					const int32 NumParticles = Particles.Size();
					Softs::FSolverVec3* ParticleXs = &Particles.X(0);
					Softs::FSolverVec3* ParticleVs = &Particles.V(0);

					int32 NumCachedParticles = static_cast<int32>(Points0.size());
					if (NumCachedParticles > NumParticles)
					{
						// Cached particles doesn't match solver particles.  Truncate.
						NumCachedParticles = NumParticles;
					}

					// < time range start, > time range end, or exact hit
					if (FMath::IsNearlyEqual(Prev, Next))
					{
						// Directly set the result of the cache into the solver particles
						for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
						{
							// Note that VtArray::operator[] is non-const access and will cause trigger 
							// the copy-on-write memcopy!  VtArray::cdata() avoids that.
							const pxr::GfVec3f& P0 = Points0.cdata()[CachedIndex];
							const pxr::GfVec3f& V0 = Vels0.cdata()[CachedIndex];
							ParticleXs[CachedIndex].Set(P0[0], P0[1], P0[2]);
							ParticleVs[CachedIndex].Set(V0[0], V0[1], V0[2]);
						}
						return;
					}

					if (!UE::ChaosCachingUSD::ReadPoints(MonolithStage, PrimPath, Next, Points1, Vels1) ||
						Points1.size() != Vels1.size() ||
						Points0.size() != Points1.size())
					{
						UE_LOG(LogChaosFleshCache, Error,
							TEXT("Failed to read points '%s' at time %g from file: '%s'"),
							*PrimPath, Next, *MonolithStage.GetRootLayer().GetDisplayName());
						return;
					}
					double Duration = Next - Prev;
					double Alpha = Duration > UE_SMALL_NUMBER ? (TargetTime - Prev) / Duration : 0.5;
					for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
					{
						// Note that VtArray::operator[] is non-const access and will cause trigger 
						// the copy-on-write memcopy!  VtArray::cdata() avoids that.
						const pxr::GfVec3f& P0 = Points0.cdata()[CachedIndex];
						const pxr::GfVec3f& P1 = Points1.cdata()[CachedIndex];
						const pxr::GfVec3f& V0 = Vels0.cdata()[CachedIndex];
						const pxr::GfVec3f& V1 = Vels1.cdata()[CachedIndex];
						pxr::GfVec3f Pos = (1.0 - Alpha) * P0 + Alpha * P1;
						pxr::GfVec3f Vel = (1.0 - Alpha) * V0 + Alpha * V1;
						ParticleXs[CachedIndex].Set(Pos[0], Pos[1], Pos[2]);
						ParticleVs[CachedIndex].Set(Vel[0], Vel[1], Vel[2]);
					}
				}
#else // USE_USD_SDK && DO_USD_CACHING
				FCacheEvaluationContext Context(TickRecord);
				Context.bEvaluateTransform = false;
				Context.bEvaluateCurves = false;
				Context.bEvaluateEvents = false;
				Context.bEvaluateChannels = true;

				// The evaluated result are already in world space since we are passing the tickrecord.spacetransform in the context
				FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);

				const TArray<float>* PendingVX = EvaluatedResult.Channels.Find(VelocityXName);
				const TArray<float>* PendingVY = EvaluatedResult.Channels.Find(VelocityYName);
				const TArray<float>* PendingVZ = EvaluatedResult.Channels.Find(VelocityZName);
				const TArray<float>* PendingPX = EvaluatedResult.Channels.Find(PositionXName);
				const TArray<float>* PendingPY = EvaluatedResult.Channels.Find(PositionYName);
				const TArray<float>* PendingPZ = EvaluatedResult.Channels.Find(PositionZName);

				const int32 NumCachedParticles = EvaluatedResult.ParticleIndices.Num();

				FParticles& Particles = Evolution->Particles();

				const int32 NumParticles = Particles.Size();
				if (NumCachedParticles > 0)
				{
					// Directly set the result of the cache into the solver particles

					Softs::FSolverVec3* ParticleXs = &Particles.X(0);
					Softs::FSolverVec3* ParticleVs = &Particles.V(0);

					for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
					{
						const int32 ParticleIndex = EvaluatedResult.ParticleIndices[CachedIndex];
						if (ensure(ParticleIndex < NumParticles))
						{
							Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
							Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];
							ParticleV.X = (*PendingVX)[CachedIndex];
							ParticleV.Y = (*PendingVY)[CachedIndex];
							ParticleV.Z = (*PendingVZ)[CachedIndex];
							ParticleX.X = (*PendingPX)[CachedIndex];
							ParticleX.Y = (*PendingPY)[CachedIndex];
							ParticleX.Z = (*PendingPZ)[CachedIndex];
						}
					}
				}

#endif // USE_USD_SDK && DO_USD_CACHING
			}
		}
	}

	FGuid FFleshCacheAdapter::GetGuid() const
	{
		FGuid NewGuid;
		checkSlow(FGuid::Parse(TEXT("2C054706CB7441B582377B0EDACD12EE"), NewGuid));
		return NewGuid;
	}

	bool FFleshCacheAdapter::ValidForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache) const
	{
		// If we have a flesh mesh we can play back any cache as long as it has one or more tracks
		const UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent);
#if USE_USD_SDK && DO_USD_CACHING
		return FleshComp != nullptr;
#else
		return FleshComp && InCache->ChannelCurveToParticle.Num() > 0;
#endif // USE_USD_SDK && DO_USD_CACHING
	}

	Chaos::Softs::FDeformableSolver* FFleshCacheAdapter::GetDeformableSolver(UPrimitiveComponent* InComponent) const
	{
		if(InComponent)
		{
			if(UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent) )
			{
				if(FleshComp->GetDeformableSolver())
				{
					return FleshComp->GetDeformableSolver()->Solver.Get();
				}
			}
		}
		return nullptr;
	}

	Chaos::FPhysicsSolver* FFleshCacheAdapter::GetComponentSolver(UPrimitiveComponent* InComponent) const
	{
		return nullptr;
	}
	
	Chaos::FPhysicsSolverEvents* FFleshCacheAdapter::BuildEventsSolver(UPrimitiveComponent* InComponent) const
	{
		if(UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
		{
			// Initialize the physics solver at the beginning of the play/record
			FleshComp->RecreatePhysicsState();
			return GetDeformableSolver(InComponent);
		}
		return nullptr;
	}
	
	void FFleshCacheAdapter::SetRestState(UPrimitiveComponent* InComponent, UChaosCache* InCache, const FTransform& InRootTransform, Chaos::FReal InTime) const
	{
#if !USE_USD_SDK && DO_USD_CACHING
		if (!InCache || InCache->GetDuration() == 0.0f)
		{
			return;
		} 
#endif // USE_USD_SDK && DO_USD_CACHING

		if (UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
		{
#if USE_USD_SDK && DO_USD_CACHING
			FleshComp->ResetDynamicCollection();
			if (UFleshDynamicAsset* DynamicCollection = FleshComp->GetDynamicCollection())
			{
				TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetPositions();
				//TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetVelocities();
				const int32 NumDynamicVertex = DynamicVertex.Num();

				if (MonolithStage)
				{
					FScopedUsdAllocs UEAllocs; // Use USD memory allocator
					pxr::VtArray<pxr::GfVec3f> Points;
					pxr::VtArray<pxr::GfVec3f> Vels;

					if (!UE::ChaosCachingUSD::ReadPoints(MonolithStage, PrimPath, -TNumericLimits<double>::Max(), Points, Vels))
					{
						UE_LOG(LogChaosFleshCache, Error, 
							TEXT("Failed to read points '%s' at time 'default' from file: '%s'"), 
							*PrimPath, *MonolithStage.GetRootLayer().GetDisplayName());
						return;
					}

					int32 NumCachedParticles = static_cast<int32>(Points.size());
					if (NumDynamicVertex == NumCachedParticles)
					{
						for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
						{
							// Note that VtArray::operator[] is non-const access and will cause trigger 
							// the copy-on-write memcopy!  VtArray::cdata() avoids that.
							const pxr::GfVec3f& P = Points.cdata()[CachedIndex];
							DynamicVertex[CachedIndex].Set(P[0], P[1], P[2]);

							auto UEVertd = [](Chaos::FVec3 V) { return FVector3d(V.X, V.Y, V.Z); };
							auto UEVertf = [](FVector3d V) { return FVector3f((float)V.X, (float)V.Y, (float)V.Z); };
							DynamicVertex[CachedIndex] =
								UEVertf(FleshComp->GetComponentTransform().InverseTransformPosition(
									UEVertd(DynamicVertex[CachedIndex])));
						}
					}
				}
			}

#else // USE_USD_SDK && DO_USD_CACHING

			FPlaybackTickRecord TickRecord;
			TickRecord.SetLastTime(InTime);

			FCacheEvaluationContext Context(TickRecord);
			Context.bEvaluateTransform = false;
			Context.bEvaluateCurves = false;
			Context.bEvaluateEvents = false;
			Context.bEvaluateChannels = true;

			FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);

			const TArray<float>* PendingVX = EvaluatedResult.Channels.Find(VelocityXName);
			const TArray<float>* PendingVY = EvaluatedResult.Channels.Find(VelocityYName);
			const TArray<float>* PendingVZ = EvaluatedResult.Channels.Find(VelocityZName);
			const TArray<float>* PendingPX = EvaluatedResult.Channels.Find(PositionXName);
			const TArray<float>* PendingPY = EvaluatedResult.Channels.Find(PositionYName);
			const TArray<float>* PendingPZ = EvaluatedResult.Channels.Find(PositionZName);

			const int32 NumCachedParticles = EvaluatedResult.ParticleIndices.Num();

			const bool bHasPositions = PendingPX && PendingPY && PendingPZ;
			const bool bHasVelocities = PendingVX && PendingVY && PendingVZ;


			FleshComp->ResetDynamicCollection();

			if (bHasPositions && NumCachedParticles > 0)
			{
				if (UFleshDynamicAsset* DynamicCollection = FleshComp->GetDynamicCollection())
				{
					TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetPositions();
					//TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetVelocities();
					const int32 NumDynamicVertex = DynamicVertex.Num();
					if (NumDynamicVertex == NumCachedParticles)
					{
						for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
						{
							const int32 ParticleIndex = EvaluatedResult.ParticleIndices[CachedIndex];

							if (ensure(ParticleIndex < NumDynamicVertex))
							{
								DynamicVertex[ParticleIndex].X = (*PendingPX)[CachedIndex];
								DynamicVertex[ParticleIndex].Y = (*PendingPY)[CachedIndex];
								DynamicVertex[ParticleIndex].Z = (*PendingPZ)[CachedIndex];

								auto UEVertd = [](Chaos::FVec3 V) { return FVector3d(V.X, V.Y, V.Z); };
								auto UEVertf = [](FVector3d V) { return FVector3f((float)V.X, (float)V.Y, (float)V.Z); };
								DynamicVertex[ParticleIndex] = UEVertf(FleshComp->GetComponentTransform().InverseTransformPosition(UEVertd(DynamicVertex[ParticleIndex])));
							}
						}
					}
				}
			}

#endif // USE_USD_SDK && DO_USD_CACHING
		}
	}

	FString GetCacheDirectory(const FObservedComponent& InObserved)
	{
		// USDCacheDirectory is relative to the content dir, but with "/Game" rather than just "/" or some relative path.
		FString CacheDir = InObserved.USDCacheDirectory.Path;
		if (CacheDir.IsEmpty())
		{
			CacheDir = FString(TEXT("SimCache"));
		}
		FPaths::NormalizeDirectoryName(CacheDir);
		if (CacheDir.StartsWith(FString(TEXT("/Game"))))
		{
			CacheDir = FPaths::Combine(FPaths::ProjectContentDir(), CacheDir.RightChop(5));
		}
		return CacheDir;
	}

	FString GetCacheFileName(const UFleshComponent* FleshComp)
	{
		const UObject* CurrObject = FleshComp;
		const AActor* Actor = nullptr;
		do {
			CurrObject = CurrObject->GetOuter();
			Actor = Cast<AActor>(CurrObject);
		} while (!Actor);
		const UObject* ActorParent = Actor ? Actor->GetOuter() : nullptr;

		FString CompName;
		if (ActorParent)
		{
			CompName = FleshComp->GetPathName(ActorParent);
		}
		else
		{
			FleshComp->GetName(CompName);
		}
		if (!CompName.Len())
		{
			CompName = FString(TEXT("FleshCache"));
		}
		return CompName;
	}

	bool FFleshCacheAdapter::InitializeForRecord(UPrimitiveComponent* InComponent, FObservedComponent& InObserved)
	{
		if( FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FGameThreadAccess GameThreadAccess(Solver, Softs::FGameThreadAccessor());
			GameThreadAccess.SetEnableSolver(true);
			if (UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
			{
				FleshComp->ResetDynamicCollection();

#if USE_USD_SDK && DO_USD_CACHING
				//
				// USD caching
				//

				FString CompName = GetCacheFileName(FleshComp);
				FString CacheDir = GetCacheDirectory(InObserved);
				FPlatformFileManager& FileManager = FPlatformFileManager::Get();
				IPlatformFile& PlatformFile = FileManager.GetPlatformFile();
				if (!PlatformFile.DirectoryExists(*CacheDir))
				{
					if (!PlatformFile.CreateDirectoryTree(*CacheDir))
					{
						UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to create output directory: '%s'"), *CacheDir);
						return false;
					}
				}

				const UFleshAsset* RestCollectionAsset = FleshComp->GetRestCollection();
				const FFleshCollection* RestCollection = RestCollectionAsset ? RestCollectionAsset->GetCollection() : nullptr;
				if (!RestCollection)
				{
					UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to get rest collection from flesh component: '%s'"), *FleshComp->GetName());
					return false;
				}

				PrimPath = UsdUtils::GetPrimPathForObject(FleshComp);
				if (bUseMonolith)
				{
					bReadOnly = false;
					FString Ext = CVarParams.bWriteBinary ? FString(TEXT("usd")) : FString(TEXT("usda"));
					FString FileName = FString::Printf(TEXT("%s.%s"), *CompName, *Ext);
					FilePath = FPaths::Combine(CacheDir, FileName);
					if (CVarParams.bNoClobber)
					{
						if (PlatformFile.FileExists(*FilePath))
						{
							// Rename the file to 'path/to/file.usd' to 'path/to/file_#.usd', where '#' 
							// is a unique version number.
							FString UniqueFilePath;
							int32 i = 1;
							do {
								FString UniqueCompName = FString::Printf(TEXT("%s_%d.%s"), *CompName, i++, *Ext);
								UniqueFilePath = FPaths::Combine(CacheDir, UniqueCompName);
							} while (PlatformFile.FileExists(*UniqueFilePath));

							if (!PlatformFile.MoveFile(*UniqueFilePath, *FilePath))
							{
								UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to rename file from '%s' to '%s'."), *FilePath, *UniqueFilePath);
								return false;
							}
						}
					}
					else
					{
						if (!PlatformFile.DeleteFile(*FilePath))
						{
							UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to remove existing cache file: '%s'"), *FilePath);
							return false;
						}
					}

					if (MonolithStage)
					{
						UE::ChaosCachingUSD::CloseStage(MonolithStage);
					}
					if (!UE::ChaosCachingUSD::NewStage(FilePath, MonolithStage))
					{
						UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to create new USD file: '%s'"), *FilePath);
						return false;
					}
					if (!UE::ChaosCachingUSD::WriteTetMesh(MonolithStage, PrimPath, *RestCollection))
					{
						UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to write tetrahedron mesh '%s' to USD file: '%s'"), *PrimPath, *FilePath);
						return false;
					}
				}

#endif // USE_USD_SDK && DO_USD_CACHING

			}
		}
		return true;
	}

	bool FFleshCacheAdapter::InitializeForPlayback(UPrimitiveComponent* InComponent, FObservedComponent& InObserved, float InTime)
	{
		EnsureIsInGameThreadContext();
		
		if (FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FGameThreadAccess GameThreadAccess(Solver, Softs::FGameThreadAccessor());
			GameThreadAccess.SetEnableSolver(false);
			if (UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
			{
				FleshComp->ResetDynamicCollection();

#if USE_USD_SDK && DO_USD_CACHING
				//
				// USD caching
				//

				FString CacheDir = GetCacheDirectory(InObserved);
				FString CompName = GetCacheFileName(FleshComp);
				PrimPath = UsdUtils::GetPrimPathForObject(FleshComp);
				if (bUseMonolith)
				{
					bReadOnly = true;
					FString Ext = CVarParams.bWriteBinary ? FString(TEXT("usd")) : FString(TEXT("usda"));
					FString FileName = FString::Printf(TEXT("%s.%s"), *CompName, *Ext);
					FilePath = FPaths::Combine(CacheDir, FileName);

					FPlatformFileManager& FileManager = FPlatformFileManager::Get();
					IPlatformFile& PlatformFile = FileManager.GetPlatformFile();
					if (PlatformFile.FileExists(*FilePath))
					{
						if (MonolithStage)
						{
							UE::ChaosCachingUSD::CloseStage(MonolithStage);
						}
						if (!UE::ChaosCachingUSD::OpenStage(FilePath, MonolithStage))
						{
							UE_LOG(LogChaosFleshCache, Error, TEXT("Failed to open USD cache file: '%s'"), *FilePath);
							return false;
						}
					}
					else
					{
						UE_LOG(LogChaosFleshCache, Error, TEXT("USD cache file not found: '%s'"), *FilePath);
						return false;
					}
				}
				if (!bUseMonolith)
				{
					UE_LOG(LogChaosFleshCache, Warning, TEXT("No USD file structure selected (bMonolith)."));
				}
#endif // USE_USD_SDK && DO_USD_CACHING

			}
		}
		return true;
	}

	void FFleshCacheAdapter::Finalize()
	{
#if USE_USD_SDK && DO_USD_CACHING
		// Detach shared memory arrays.
		if (MonolithStage)
		{
			if (!bReadOnly)
			{
				UE::ChaosCachingUSD::SaveStage(MonolithStage, MinTime, MaxTime);
			}
			UE::ChaosCachingUSD::CloseStage(MonolithStage);
			MonolithStage = UE::FUsdStage();
		}

		PrimPath.Empty();
		FilePath.Empty();
		MinTime = TNumericLimits<double>::Max();
		MaxTime = -TNumericLimits<double>::Max();
#endif // USE_USD_SDK && DO_USD_CACHING
	}

}    // namespace Chaos
