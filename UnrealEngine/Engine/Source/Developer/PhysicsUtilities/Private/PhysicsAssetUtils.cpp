// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetUtils.h"
#include "Modules/ModuleManager.h"
#include "MeshUtilitiesCommon.h"
#include "MeshUtilitiesEngine.h"
#include "ConvexDecompTool.h"
#include "Logging/MessageLog.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/SkinnedLevelSetElem.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PreviewScene.h"
#include "Misc/ScopedSlowTask.h"
#include "SkinnedBoneTriangleCache.h"
#include "SkinnedLevelSetBuilder.h"
#include "LevelSetHelpers.h"
#include "Chaos/Levelset.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/NamePermissionList.h"

namespace FPhysicsAssetUtils
{
	static const float	DefaultPrimSize = 15.0f;
	static const float	MinPrimSize = 0.5f;

	// Forward declarations
	bool CreateCollisionFromBoneInternal(UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info, const FSkinnedBoneTriangleCache& TriangleCache);

/** Returns INDEX_NONE if no children in the visual asset or if more than one parent */
static int32 GetChildIndex(int32 BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	int32 ChildIndex = INDEX_NONE;

	for(int32 i=0; i<SkelMesh->GetRefSkeleton().GetRawBoneNum(); i++)
	{
		int32 ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(i);

		if (ParentIndex == BoneIndex && Infos[i].Positions.Num() > 0)
		{
			if(ChildIndex != INDEX_NONE)
			{
				return INDEX_NONE; // if we already have a child, this bone has more than one so return INDEX_NONE.
			}
			else
			{
				ChildIndex = i;
			}
		}
	}

	return ChildIndex;
}

static float CalcBoneInfoLength(const FBoneVertInfo& Info)
{
	FBox BoneBox(ForceInit);
	for(int32 j=0; j<Info.Positions.Num(); j++)
	{
		BoneBox += (FVector)Info.Positions[j];
	}

	if(BoneBox.IsValid)
	{
		FVector BoxExtent = BoneBox.GetExtent();
		return BoxExtent.Size();
	}
	else
	{
		return 0.f;
	}
}

/**
 * For all bones below the give bone index, find each ones minimum box dimension, and return the maximum over those bones.
 * This is used to decide if we should create physics for a bone even if its small, because there are good-sized bones below it.
 */
static float GetMaximalMinSizeBelow(int32 BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	check( Infos.Num() == SkelMesh->GetRefSkeleton().GetRawBoneNum() );

	UE_LOG(LogPhysics, Log, TEXT("-------------------------------------------------"));

	float MaximalMinBoxSize = 0.f;

	// For all bones that are children of the supplied one...
	for(int32 i=BoneIndex; i<SkelMesh->GetRefSkeleton().GetRawBoneNum(); i++)
	{
		if( SkelMesh->GetRefSkeleton().BoneIsChildOf(i, BoneIndex) )
		{
			float MinBoneDim = CalcBoneInfoLength( Infos[i] );
			
			UE_LOG(LogPhysics, Log,  TEXT("Parent: %s Bone: %s Size: %f"), *SkelMesh->GetRefSkeleton().GetBoneName(BoneIndex).ToString(), *SkelMesh->GetRefSkeleton().GetBoneName(i).ToString(), MinBoneDim );

			MaximalMinBoxSize = FMath::Max(MaximalMinBoxSize, MinBoneDim);
		}
	}

	return MaximalMinBoxSize;
}

void AddInfoToParentInfo(const FTransform& LocalToParentTM, const FBoneVertInfo& ChildInfo, FBoneVertInfo& ParentInfo)
{
	ParentInfo.Positions.Reserve(ParentInfo.Positions.Num() + ChildInfo.Positions.Num());
	ParentInfo.Positions.Reserve(ParentInfo.Normals.Num() + ChildInfo.Normals.Num());

	for(const FVector3f& Pos : ChildInfo.Positions)
	{
		ParentInfo.Positions.Add((FVector3f)LocalToParentTM.TransformPosition((FVector)Pos));
	}

	for (const FVector3f& Normal : ChildInfo.Normals)
	{
		ParentInfo.Normals.Add((FVector3f)LocalToParentTM.TransformVectorNoScale((FVector)Normal));
	}
}

void CreateWeightedLevelSetBody(FSkinnedLevelSetBuilder& Builder, UBodySetup* BodySetup, UPhysicsAsset* PhysAsset, FName RootBoneName, const FPhysAssetCreateParams& Params)
{
	check(BodySetup->BoneName == RootBoneName);

	BodySetup->RemoveSimpleCollision();
	BodySetup->AggGeom.SkinnedLevelSetElems.Add(Builder.CreateSkinnedLevelSetElem());
	BodySetup->InvalidatePhysicsData(); // update GUID
}

int32 CalculateCommonRootBoneIndex(const FReferenceSkeleton& RefSkeleton, const TArray<int32>& BoneIndices)
{
	// This is copied from ClothSimulationModel

	// Starts at root
	int32 RootBoneIndex = 0;

	// List of valid paths to the root bone from each bone
	TArray<TArray<int32>> PathsToRoot;
	PathsToRoot.Reserve(BoneIndices.Num());

	for (const int32 BoneIndex : BoneIndices)
	{
		TArray<int32>& Path = PathsToRoot.AddDefaulted_GetRef();

		int32 CurrentBone = BoneIndex;
		Path.Add(CurrentBone);

		while (CurrentBone != 0 && CurrentBone != INDEX_NONE)
		{
			CurrentBone = RefSkeleton.GetParentIndex(CurrentBone);
			Path.Add(CurrentBone);
		}
	}

	// Paths are from leaf->root, we want the other way
	for (TArray<int32>& Path : PathsToRoot)
	{
		Algo::Reverse(Path);
	}

	// the last common bone in all is the root
	const int32 NumPaths = PathsToRoot.Num();
	if (NumPaths > 0)
	{
		TArray<int32>& FirstPath = PathsToRoot[0];

		const int32 FirstPathSize = FirstPath.Num();
		for (int32 PathEntryIndex = 0; PathEntryIndex < FirstPathSize; ++PathEntryIndex)
		{
			const int32 CurrentQueryIndex = FirstPath[PathEntryIndex];
			bool bValidRoot = true;

			for (int32 PathIndex = 1; PathIndex < NumPaths; ++PathIndex)
			{
				if (!PathsToRoot[PathIndex].Contains(CurrentQueryIndex))
				{
					bValidRoot = false;
					break;
				}
			}

			if (bValidRoot)
			{
				RootBoneIndex = CurrentQueryIndex;
			}
			else
			{
				// Once we fail to find a valid root we're done.
				break;
			}
		}
	}
	else
	{
		// Just use the root
		RootBoneIndex = 0;
	}
	return RootBoneIndex;
}

bool CreateFromSkeletalMeshInternal(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, const FPhysAssetCreateParams& Params, const FSkinnedBoneTriangleCache& TriangleCache, bool bShowProgress)
{
	// For each bone, get the vertices most firmly attached to it.
	TArray<FBoneVertInfo> Infos;
	FMeshUtilitiesEngine::CalcBoneVertInfos(SkelMesh, Infos, (Params.VertWeight == EVW_DominantWeight));
	check(Infos.Num() == SkelMesh->GetRefSkeleton().GetRawBoneNum());

	PhysicsAsset->CollisionDisableTable.Empty();

	//Given the desired min body size we work from the children up to "merge" bones together. We go from leaves up because usually fingers, toes, etc... are small bones that should be merged
	//The strategy is as follows:
	//If bone is big enough, make a body
	//If not, add bone to parent for possible merge

	const TArray<FTransform> LocalPose = SkelMesh->GetRefSkeleton().GetRefBonePose();
	typedef TArray<int32> FMergedBoneIndices;

	TMap<int32, TPair<FMergedBoneIndices,FBoneVertInfo>> BoneToMergedBones;
	const int32 NumBones = Infos.Num();

	TArray<float> MergedSizes;
	MergedSizes.AddZeroed(NumBones);
	for(int32 BoneIdx = NumBones-1; BoneIdx >=0; --BoneIdx)
	{
		const float MyMergedSize = MergedSizes[BoneIdx] += CalcBoneInfoLength(Infos[BoneIdx]);

		if(MyMergedSize < Params.MinBoneSize && MyMergedSize >= Params.MinWeldSize)
		{
			//Too small to make a body for, so let's merge with parent bone. TODO: use a merge threshold
			const int32 ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(BoneIdx);
			if(ParentIndex != INDEX_NONE)
			{
				MergedSizes[ParentIndex] += MyMergedSize;
				TPair<FMergedBoneIndices,FBoneVertInfo>& ParentMergedBones = BoneToMergedBones.FindOrAdd(ParentIndex);	//Add this bone to its parent merged bones
				ParentMergedBones.Get<0>().Add(BoneIdx);

				const FTransform LocalTM = LocalPose[BoneIdx];

				AddInfoToParentInfo(LocalTM, Infos[BoneIdx], ParentMergedBones.Get<1>());

				if(TPair<FMergedBoneIndices, FBoneVertInfo>* MyMergedBones = BoneToMergedBones.Find(BoneIdx))
				{
					//make sure any bones merged to this bone get merged into the parent
					ParentMergedBones.Get<0>().Append(MyMergedBones->Get<0>());
					AddInfoToParentInfo(LocalTM, MyMergedBones->Get<1>(), ParentMergedBones.Get<1>());

					BoneToMergedBones.Remove(BoneIdx);
				}
			}
		}
	}

	//We must ensure that there is a single root body no matter how small.
	int32 ForcedRootBoneIndex = INDEX_NONE;
	int32 FirstParentBoneIndex = INDEX_NONE;
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (MergedSizes[BoneIndex] > Params.MinBoneSize)
		{
			const int32 ParentBoneIndex = SkelMesh->GetRefSkeleton().GetParentIndex(BoneIndex);
			if(ParentBoneIndex == INDEX_NONE)
			{
				break;	//We already have a single root body, so don't worry about it
			}
			else if(FirstParentBoneIndex == INDEX_NONE)
			{
				FirstParentBoneIndex = ParentBoneIndex;	//record first parent to see if we have more than one root
			}
			else if(ParentBoneIndex == FirstParentBoneIndex)
			{
				ForcedRootBoneIndex = ParentBoneIndex;	//we have two "root" bodies so take their parent as the real root body
				break;
			}
		}
	}

	auto ShouldMakeBone = [&Params, &MergedSizes, ForcedRootBoneIndex](int32 BoneIndex)
	{
		// If desired - make a body for EVERY bone
		if (Params.bBodyForAll)
		{
			return true;
		}
		else if (MergedSizes[BoneIndex] > Params.MinBoneSize)
		{
			// If bone is big enough - create physics.
			return true;
		}
		else if (BoneIndex == ForcedRootBoneIndex)
		{
			// If the bone is a forced root body we must create they body no matter how small
			return true;
		}

		return false;
	};

	int32 RootBoneIndex = 0;
	if (Params.GeomType == EFG_SkinnedLevelSet)
	{
		// Calculate common root bone from all merged bones
		if (!Params.bBodyForAll)
		{
			TArray<int32> MergedBones;
			MergedBones.Reserve(NumBones);
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				if (ShouldMakeBone(BoneIndex))
				{
					MergedBones.Add(BoneIndex);
				}
			}
			RootBoneIndex = CalculateCommonRootBoneIndex(SkelMesh->GetRefSkeleton(), MergedBones);
		}
	}

	FSkinnedLevelSetBuilder LatticeBuilder(*SkelMesh, TriangleCache, RootBoneIndex);
	if (Params.GeomType == EFG_SkinnedLevelSet)
	{
		check(RootBoneIndex != INDEX_NONE);
		TArray<int32> AllBoneIndices;
		const int32 RawBoneNum = SkelMesh->GetRefSkeleton().GetRawBoneNum();
		AllBoneIndices.Reserve(RawBoneNum - RootBoneIndex);
		for (int32 BoneIndex = RootBoneIndex; BoneIndex < RawBoneNum; ++BoneIndex)
		{
			AllBoneIndices.Emplace(BoneIndex);
		}
		TArray<uint32> OrigIndicesUnused;
		if (!LatticeBuilder.InitializeSkinnedLevelset(Params, AllBoneIndices, OrigIndicesUnused))
		{
			return false;
		}
	}

	// Finally, iterate through all the bones and create bodies when needed

	const bool bCanCreateConstraints = CanCreateConstraints();
	FScopedSlowTask SlowTask((float)NumBones * 2, FText(), bShowProgress&& IsInGameThread());
	if (bShowProgress && IsInGameThread())
	{
		SlowTask.MakeDialog();
	}

	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		// Determine if we should create a physics body for this bone
		const bool bMakeBone = ShouldMakeBone(BoneIndex);

		if (bMakeBone)
		{
			// Go ahead and make this bone physical.
			FName BoneName = SkelMesh->GetRefSkeleton().GetBoneName(BoneIndex);

			if (bShowProgress && IsInGameThread())
			{
				SlowTask.EnterProgressFrame(1.0f, FText::Format(NSLOCTEXT("PhysicsAssetEditor", "ResetCollsionStepInfo", "Generating collision for {0}"), FText::FromName(BoneName)));
			}


			//Construct the info - in the case of merged bones we append all the data
			FBoneVertInfo Info = Infos[BoneIndex];
			FMergedBoneIndices BoneIndices;
			BoneIndices.Add(BoneIndex);
			if (const TPair<FMergedBoneIndices, FBoneVertInfo>* MergedBones = BoneToMergedBones.Find(BoneIndex))
			{
				//Don't need to convert into parent space since this was already done
				BoneIndices.Append(MergedBones->Get<0>());
				Info.Normals.Append(MergedBones->Get<1>().Normals);
				Info.Positions.Append(MergedBones->Get<1>().Positions);
			}

			if (Params.GeomType == EFG_SkinnedLevelSet)
			{
				LatticeBuilder.AddBoneInfluence(BoneIndex, BoneIndices);
			}
			else
			{
				const int32 NewBodyIndex = CreateNewBody(PhysicsAsset, BoneName, Params);
				UBodySetup* NewBodySetup = PhysicsAsset->SkeletalBodySetups[NewBodyIndex];
				check(NewBodySetup->BoneName == BoneName);

				// Fill in collision info for this bone.
				const bool bSuccess = CreateCollisionFromBoneInternal(NewBodySetup, SkelMesh, BoneIndex, Params, Info, TriangleCache);
				if (bSuccess)
				{
					// create joint to parent body
					if (Params.bCreateConstraints && bCanCreateConstraints)
					{
						int32 ParentIndex = BoneIndex;
						int32 ParentBodyIndex = INDEX_NONE;
						FName ParentName;

						do
						{
							//Travel up the hierarchy to find a parent which has a valid body
							ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(ParentIndex);
							if (ParentIndex != INDEX_NONE)
							{
								ParentName = SkelMesh->GetRefSkeleton().GetBoneName(ParentIndex);
								ParentBodyIndex = PhysicsAsset->FindBodyIndex(ParentName);
							}
							else
							{
								//no more parents so just stop
								break;
							}

						} while (ParentBodyIndex == INDEX_NONE);

						if (ParentBodyIndex != INDEX_NONE)
						{
							//found valid parent body so create joint
							int32 NewConstraintIndex = CreateNewConstraint(PhysicsAsset, BoneName);
							UPhysicsConstraintTemplate* CS = PhysicsAsset->ConstraintSetup[NewConstraintIndex];

							// set angular constraint mode
							CS->DefaultInstance.SetAngularSwing1Motion(Params.AngularConstraintMode);
							CS->DefaultInstance.SetAngularSwing2Motion(Params.AngularConstraintMode);
							CS->DefaultInstance.SetAngularTwistMotion(Params.AngularConstraintMode);

							// Place joint at origin of child
							CS->DefaultInstance.ConstraintBone1 = BoneName;
							CS->DefaultInstance.ConstraintBone2 = ParentName;
							CS->DefaultInstance.SnapTransformsToDefault(EConstraintTransformComponentFlags::All, PhysicsAsset);

							CS->SetDefaultProfile(CS->DefaultInstance);

							// Disable collision between constrained bodies by default.
							PhysicsAsset->DisableCollision(NewBodyIndex, ParentBodyIndex);
						}
					}
				}
				else
				{
					DestroyBody(PhysicsAsset, NewBodyIndex);
				}
			}
		}
	}

	if (Params.GeomType == EFG_SkinnedLevelSet)
	{
		// Finish Building WeightedLattice
		const FName RootBoneName = SkelMesh->GetRefSkeleton().GetBoneName(RootBoneIndex);
		const int32 NewBodyIndex = CreateNewBody(PhysicsAsset, RootBoneName, Params);
		UBodySetup* NewBodySetup = PhysicsAsset->SkeletalBodySetups[NewBodyIndex];
		CreateWeightedLevelSetBody(LatticeBuilder, NewBodySetup, PhysicsAsset, RootBoneName, Params);
	}

	//Go through and ensure any overlapping bodies are marked as disable collision
	FPreviewScene TmpScene;
	UWorld* TmpWorld = TmpScene.GetWorld();
	ASkeletalMeshActor* SkeletalMeshActor = TmpWorld->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform::Identity);
	SkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
	USkeletalMeshComponent* SKC = SkeletalMeshActor->GetSkeletalMeshComponent();
	SKC->SetPhysicsAsset(PhysicsAsset);
	SkeletalMeshActor->RegisterAllComponents();
	

	const TArray<FBodyInstance*> Bodies = SKC->Bodies;
	const int32 NumBodies = Bodies.Num();
	for(int32 BodyIdx = 0; BodyIdx < NumBodies; ++BodyIdx)
	{
		FBodyInstance* BodyInstance = Bodies[BodyIdx];
		if(BodyInstance && BodyInstance->BodySetup.IsValid())
		{
			if (bShowProgress && IsInGameThread())
			{
				SlowTask.EnterProgressFrame(1.0f, FText::Format(NSLOCTEXT("PhysicsAssetEditor", "ResetCollsionStepInfoOverlaps", "Fixing overlaps for {0}"), FText::FromName(BodyInstance->BodySetup->BoneName)));
			}

			FTransform BodyTM = BodyInstance->GetUnrealWorldTransform();

			for(int32 OtherBodyIdx = BodyIdx + 1; OtherBodyIdx < NumBodies; ++OtherBodyIdx)
			{
				FBodyInstance* OtherBodyInstance = Bodies[OtherBodyIdx];
				if(OtherBodyInstance && OtherBodyInstance->BodySetup.IsValid())
				{
					if(BodyInstance->OverlapTestForBody(BodyTM.GetLocation(), BodyTM.GetRotation(), OtherBodyInstance))
					{
						PhysicsAsset->DisableCollision(BodyIdx, OtherBodyIdx);
					}
				}
			}
		}
	}

	return NumBodies > 0;
}

bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, const FPhysAssetCreateParams& Params, FText& OutErrorMessage, bool bSetToMesh /*= true*/, bool bShowProgress /*= true */)
{
	PhysicsAsset->PreviewSkeletalMesh = SkelMesh;

	check(SkelMesh);
	check(SkelMesh->GetResourceForRendering());

	FSkinnedBoneTriangleCache TriangleCache(*SkelMesh, Params);

	if (Params.GeomType == EFG_SingleConvexHull || Params.GeomType == EFG_MultiConvexHull || Params.GeomType == EFG_LevelSet || Params.GeomType == EFG_SkinnedLevelSet)
	{
		TriangleCache.BuildCache();
	}

	bool bSuccess = CreateFromSkeletalMeshInternal(PhysicsAsset, SkelMesh, Params, TriangleCache, bShowProgress);
	if (!bSuccess)
	{
		// try lower minimum bone size 
		FPhysAssetCreateParams LocalParams = Params;
		LocalParams.MinBoneSize = 1.f;

		bSuccess = CreateFromSkeletalMeshInternal(PhysicsAsset, SkelMesh, LocalParams, TriangleCache, bShowProgress);

		if(!bSuccess)
		{
			OutErrorMessage = FText::Format(NSLOCTEXT("CreatePhysicsAsset", "CreatePhysicsAssetLinkFailed", "The bone size is too small to create Physics Asset '{0}' from Skeletal Mesh '{1}'. You will have to create physics asset manually."), FText::FromString(PhysicsAsset->GetName()), FText::FromString(SkelMesh->GetName()));
		}
	}

	if(bSuccess && bSetToMesh)
	{
		SkelMesh->SetPhysicsAsset(PhysicsAsset);
		SkelMesh->MarkPackageDirty();
	}

	return bSuccess;
}

FMatrix ComputeCovarianceMatrix(const FBoneVertInfo& VertInfo)
{
	if (VertInfo.Positions.Num() == 0)
	{
		return FMatrix::Identity;
	}

	const TArray<FVector3f> & Positions = VertInfo.Positions;

	//get average
	const float N = Positions.Num();
	FVector U = FVector::ZeroVector;
	for (int32 i = 0; i < N; ++i)
	{
		U += (FVector)Positions[i];
	}

	U = U / N;

	//compute error terms
	TArray<FVector> Errors;
	Errors.AddUninitialized(N);

	for (int32 i = 0; i < N; ++i)
	{
		Errors[i] = (FVector)Positions[i] - U;
	}

	FMatrix Covariance = FMatrix::Identity;
	for (int32 j = 0; j < 3; ++j)
	{
		FVector Axis = FVector::ZeroVector;
		FVector::FReal* Cj = &Axis.X;
		for (int32 k = 0; k < 3; ++k)
		{
			float Cjk = 0.f;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector::FReal* error = &Errors[i].X;
				Cj[k] += error[j] * error[k];
			}
			Cj[k] /= N;
		}

		Covariance.SetAxis(j, Axis);
	}

	return Covariance;
}

FVector ComputeEigenVector(const FMatrix& A)
{
	//using the power method: this is ok because we only need the dominate eigenvector and speed is not critical: http://en.wikipedia.org/wiki/Power_iteration
	FVector Bk = FVector(0, 0, 1);
	for (int32 i = 0; i < 32; ++i)
	{
		float Length = Bk.Size();
		if (Length > 0.f)
		{
			Bk = A.TransformVector(Bk) / Length;
		}
	}

	return Bk.GetSafeNormal();
}

bool CreateCollisionFromBoneInternal(UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info, const FSkinnedBoneTriangleCache& TriangleCache)
{
#if WITH_EDITOR
	// multi convex hull can fail so wait to clear it ( will be called in DecomposeMeshToHulls() )
	const bool bCanFail = (Params.GeomType == EFG_SingleConvexHull || Params.GeomType == EFG_MultiConvexHull || Params.GeomType == EFG_LevelSet);
	if (!bCanFail)
	{
		// Empty any existing collision.
		bs->RemoveSimpleCollision();
	}
#endif // WITH_EDITOR

	// Calculate orientation of to use for collision primitive.
	FMatrix ElemTM = FMatrix::Identity;
	bool ComputeFromVerts = false;

	if (Params.bAutoOrientToBone)
	{
		// Compute covariance matrix for verts of this bone
		// Then use axis with largest variance for orienting bone box
		const FMatrix CovarianceMatrix = ComputeCovarianceMatrix(Info);
		FVector ZAxis = ComputeEigenVector(CovarianceMatrix);
		FVector XAxis, YAxis;
		ZAxis.FindBestAxisVectors(YAxis, XAxis);
		ElemTM = FMatrix(XAxis, YAxis, ZAxis, FVector::ZeroVector);
	}

	// convert to FTransform now
	// Matrix inverse doesn't handle well when DET == 0, so 
	// convert to FTransform and use that data
	FTransform ElementTransform(ElemTM);
	// Get the (Unreal scale) bounding box for this bone using the rotation.
	FBox BoneBox(ForceInit);
	for (int32 j = 0; j < Info.Positions.Num(); j++)
	{
		BoneBox += ElementTransform.InverseTransformPosition((FVector)Info.Positions[j]);
	}

	FVector BoxCenter(0, 0, 0), BoxExtent(0, 0, 0);

	FBox TransformedBox = BoneBox;
	if (BoneBox.IsValid)
	{
		// make sure to apply scale to the box size
		FMatrix BoneMatrix = skelMesh->GetComposedRefPoseMatrix(BoneIndex);
		TransformedBox = BoneBox.TransformBy(FTransform(BoneMatrix));
		BoneBox.GetCenterAndExtents(BoxCenter, BoxExtent);
	}

	float MinRad = TransformedBox.GetExtent().GetMin();
	float MinAllowedSize = MinPrimSize;

	// If the primitive is going to be too small - just use some default numbers and let the user tweak.
	if (MinRad < MinAllowedSize)
	{
		// change min allowed size to be min, not DefaultPrimSize
		BoxExtent = FVector(MinAllowedSize, MinAllowedSize, MinAllowedSize);
	}

	FVector BoneOrigin = ElementTransform.TransformPosition(BoxCenter);
	ElementTransform.SetTranslation(BoneOrigin);

	if (Params.GeomType == EFG_Box)
	{
		// Add a new box geometry to this body the size of the bounding box.
		FKBoxElem BoxElem;

		BoxElem.SetTransform(ElementTransform);

		BoxElem.X = BoxExtent.X * 2.0f * 1.01f; // Side Lengths (add 1% to avoid graphics glitches)
		BoxElem.Y = BoxExtent.Y * 2.0f * 1.01f;
		BoxElem.Z = BoxExtent.Z * 2.0f * 1.01f;

		bs->AggGeom.BoxElems.Add(BoxElem);
	}
	else if (Params.GeomType == EFG_Sphere)
	{
		FKSphereElem SphereElem;

		SphereElem.Center = ElementTransform.GetTranslation();
		SphereElem.Radius = BoxExtent.GetMax() * 1.01f;

		bs->AggGeom.SphereElems.Add(SphereElem);
	}
	else if (Params.GeomType == EFG_SingleConvexHull || Params.GeomType == EFG_MultiConvexHull)
	{
		TArray<FVector3f> Verts;
		TArray<uint32> Indices;
		TriangleCache.GetVerticesAndIndicesForBone(BoneIndex, Verts, Indices);

		if (Verts.Num())
		{
			const int32 HullCount = Params.GeomType == EFG_SingleConvexHull ? 1 : Params.HullCount;
			DecomposeMeshToHulls(bs, Verts, Indices, HullCount, Params.MaxHullVerts);
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(NSLOCTEXT("PhysicsAssetUtils", "ConvexNoPositions", "Unable to create a convex hull for the given bone as there are no vertices associated with the bone."));
			EditorErrors.Open();
			return false;
		}
	}
	else if (Params.GeomType == EFG_Sphyl)
	{

		FKSphylElem SphylElem;

		if (BoxExtent.X > BoxExtent.Z && BoxExtent.X > BoxExtent.Y)
		{
			//X is the biggest so we must rotate X-axis into Z-axis
			SphylElem.SetTransform(FTransform(FQuat(FVector(0, 1, 0), -PI * 0.5f)) * ElementTransform);
			SphylElem.Radius = FMath::Max(BoxExtent.Y, BoxExtent.Z) * 1.01f;
			SphylElem.Length = BoxExtent.X * 1.01f;

		}
		else if (BoxExtent.Y > BoxExtent.Z && BoxExtent.Y > BoxExtent.X)
		{
			//Y is the biggest so we must rotate Y-axis into Z-axis
			SphylElem.SetTransform(FTransform(FQuat(FVector(1, 0, 0), PI * 0.5f)) * ElementTransform);
			SphylElem.Radius = FMath::Max(BoxExtent.X, BoxExtent.Z) * 1.01f;
			SphylElem.Length = BoxExtent.Y * 1.01f;
		}
		else
		{
			//Z is the biggest so use transform as is
			SphylElem.SetTransform(ElementTransform);

			SphylElem.Radius = FMath::Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
			SphylElem.Length = BoxExtent.Z * 1.01f;
		}

		bs->AggGeom.SphylElems.Add(SphylElem);
	}
	else if (Params.GeomType == EFG_TaperedCapsule)
	{
		FKTaperedCapsuleElem TaperedCapsuleElem;

		if (BoxExtent.X > BoxExtent.Z && BoxExtent.X > BoxExtent.Y)
		{
			//X is the biggest so we must rotate X-axis into Z-axis
			TaperedCapsuleElem.SetTransform(FTransform(FQuat(FVector(0, 1, 0), -PI * 0.5f)) * ElementTransform);
			TaperedCapsuleElem.Radius0 = FMath::Max(BoxExtent.Y, BoxExtent.Z) * 1.01f;
			TaperedCapsuleElem.Radius1 = FMath::Max(BoxExtent.Y, BoxExtent.Z) * 1.01f;
			TaperedCapsuleElem.Length = BoxExtent.X * 1.01f;
		}
		else if (BoxExtent.Y > BoxExtent.Z && BoxExtent.Y > BoxExtent.X)
		{
			//Y is the biggest so we must rotate Y-axis into Z-axis
			TaperedCapsuleElem.SetTransform(FTransform(FQuat(FVector(1, 0, 0), PI * 0.5f)) * ElementTransform);
			TaperedCapsuleElem.Radius0 = FMath::Max(BoxExtent.X, BoxExtent.Z) * 1.01f;
			TaperedCapsuleElem.Radius1 = FMath::Max(BoxExtent.X, BoxExtent.Z) * 1.01f;
			TaperedCapsuleElem.Length = BoxExtent.Y * 1.01f;
		}
		else
		{
			//Z is the biggest so use transform as is
			TaperedCapsuleElem.SetTransform(ElementTransform);
			TaperedCapsuleElem.Radius0 = FMath::Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
			TaperedCapsuleElem.Radius1 = FMath::Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
			TaperedCapsuleElem.Length = BoxExtent.Z * 1.01f;
		}

		bs->AggGeom.TaperedCapsuleElems.Add(TaperedCapsuleElem);
	}
	else if (Params.GeomType == EFG_LevelSet)
	{
		TArray<FVector3f> Verts;
		TArray<uint32> Indices;
		TriangleCache.GetVerticesAndIndicesForBone(BoneIndex, Verts, Indices);

		if (Verts.Num())
		{
			const bool bOK = LevelSetHelpers::CreateLevelSetForBone(bs, Verts, Indices, Params.LevelSetResolution);
			if (!bOK)
			{
				FMessageLog EditorErrors("EditorErrors");
				EditorErrors.Warning(NSLOCTEXT("PhysicsAssetUtils", "LevelSetError", "An error occurred creating a level set for the given bone."));
				EditorErrors.Open();
				return false;
			}
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(NSLOCTEXT("PhysicsAssetUtils", "LevelSetNoPositions", "Unable to create a level set for the given bone as there are no vertices associated with the bone."));
			EditorErrors.Open();
			return false;
		}
	}

	return true;
}

bool CreateCollisionFromBone(UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info)
{
	check(skelMesh);

	FSkinnedBoneTriangleCache TriangleCache(*skelMesh, Params);

	if (Params.GeomType == EFG_SingleConvexHull || Params.GeomType == EFG_MultiConvexHull || Params.GeomType == EFG_LevelSet)
	{
		TriangleCache.BuildCache();
	}

	return CreateCollisionFromBoneInternal(bs, skelMesh, BoneIndex, Params, Info, TriangleCache);
}

bool CreateCollisionFromBones(UBodySetup* bs, USkeletalMesh* skelMesh, const TArray<int32>& BoneIndices, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info)
{
	check(skelMesh);

	FSkinnedBoneTriangleCache TriangleCache(*skelMesh, Params);

	if (Params.GeomType == EFG_SingleConvexHull || Params.GeomType == EFG_MultiConvexHull || Params.GeomType == EFG_LevelSet)
	{
		TriangleCache.BuildCache();
	}

	bool bAllSuccessful = true;
	for ( int Index = 0; Index < BoneIndices.Num(); ++Index )
	{
		if ( !CreateCollisionFromBoneInternal(bs, skelMesh, BoneIndices[Index], Params, Info, TriangleCache) )
		{
			bAllSuccessful = false;
		}
	}

	return bAllSuccessful;
}

bool CreateCollisionsFromBones(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, const TArray<int32>& BodyIndices, const FPhysAssetCreateParams& Params, const TArray<FBoneVertInfo>& Info, TArray<int32>& OutSuccessfulBodyIndices)
{
	check(SkelMesh);
	check(PhysicsAsset);

	FSkinnedBoneTriangleCache TriangleCache(*SkelMesh, Params);
	if (Params.GeomType == EFG_SingleConvexHull || Params.GeomType == EFG_MultiConvexHull || Params.GeomType == EFG_LevelSet || Params.GeomType == EFG_SkinnedLevelSet)
	{
		TriangleCache.BuildCache();
	}

	OutSuccessfulBodyIndices.Reset();
	bool bAllSuccessful = true;

	// Destroying bodies causes the indices to change. Use names instead.
	TArray<FName> BodySetupBoneNames;
	BodySetupBoneNames.Reserve(BodyIndices.Num());
	TArray<FName> SubBoneNames;
	for (int32 Index = 0; Index < BodyIndices.Num(); ++Index)
	{
		const UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndices[Index]];
		BodySetupBoneNames.Add(BodySetup->BoneName);

		// Get sub-bones for any skinned levelsets
		for (const FKSkinnedLevelSetElem& SkinnedLevelSetElem : BodySetup->AggGeom.SkinnedLevelSetElems)
		{
			if (const Chaos::TWeightedLatticeImplicitObject<Chaos::FLevelSet>* SkinnedLevelSet = SkinnedLevelSetElem.WeightedLevelSet().GetReference())
			{
				SubBoneNames.Reserve(SubBoneNames.Num() + SkinnedLevelSet->GetUsedBones().Num());
				for (const FName& SubBoneName : SkinnedLevelSet->GetUsedBones())
				{
					SubBoneNames.AddUnique(SubBoneName);
				}
			}
		}
	}

	int32 RootBoneIndex = INDEX_NONE;
	TArray<int32> AllBoneIndices;
	if (Params.GeomType == EFG_SkinnedLevelSet)
	{
		AllBoneIndices.Reserve(BodySetupBoneNames.Num());
		for (int32 Index = 0; Index < BodySetupBoneNames.Num(); ++Index)
		{
			const int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(BodySetupBoneNames[Index]);
			AllBoneIndices.Add(BoneIndex);
		}
		RootBoneIndex = CalculateCommonRootBoneIndex(SkelMesh->GetRefSkeleton(), AllBoneIndices);
	}

	FSkinnedLevelSetBuilder LatticeBuilder(*SkelMesh, TriangleCache, RootBoneIndex);
	if (Params.GeomType == EFG_SkinnedLevelSet)
	{
		TArray<uint32> OrigIndices;
		if (!LatticeBuilder.InitializeSkinnedLevelset(Params, AllBoneIndices, OrigIndices))
		{
			return false;
		}

		if (Params.VertWeight == EVW_AnyWeight)
		{
			// Add additional sub-bones that contribute to the vertices
			TSet<int32> SubBoneSet;
			LatticeBuilder.GetInfluencingBones(OrigIndices, SubBoneSet);

			for (int32 AddedSubBone : SubBoneSet)
			{
				SubBoneNames.AddUnique(SkelMesh->GetRefSkeleton().GetBoneName(AddedSubBone));
			}
		}
	}

	FScopedSlowTask SlowTask((float)BodySetupBoneNames.Num() + (float)SubBoneNames.Num());
	if (IsInGameThread())
	{
		SlowTask.MakeDialog();
	}


	TArray<FName> SuccessfulBodySetupNames;
	for (const FName& BoneName : BodySetupBoneNames)
	{
		if (IsInGameThread())
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(NSLOCTEXT("PhysicsAssetEditor", "ResetCollsionStepInfo", "Generating collision for {0}"), FText::FromName(BoneName)));
		}

		const int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
		check(BodyIndex != INDEX_NONE);

		UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
		BodySetup->Modify();
		check(BodySetup);
		const int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(BodySetup->BoneName);
		check(BoneIndex != INDEX_NONE);

		if (Params.GeomType == EFG_SkinnedLevelSet)
		{
			if (BoneIndex != RootBoneIndex)
			{
				// This bone is now getting merged into the RootBoneIndex's body
				DestroyBody(PhysicsAsset, BodyIndex);
			}

			TArray<int32> BoneIndexArray;
			BoneIndexArray.Add(BoneIndex);
			LatticeBuilder.AddBoneInfluence(BoneIndex, BoneIndexArray);
		}
		else
		{
			if (CreateCollisionFromBoneInternal(BodySetup, SkelMesh, BoneIndex, Params, Info[BoneIndex], TriangleCache))
			{
				SuccessfulBodySetupNames.AddUnique(BoneName);
			}
			else
			{
				bAllSuccessful = false;
				DestroyBody(PhysicsAsset, BodyIndex);
			}
		}
	}

	for (const FName& BoneName : SubBoneNames)
	{
		if (IsInGameThread())
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(NSLOCTEXT("PhysicsAssetEditor", "ResetCollsionStepInfo", "Generating collision for {0}"), FText::FromName(BoneName)));
		}

		if (BodySetupBoneNames.Find(BoneName) != INDEX_NONE)
		{
			// We've already dealt with this above.
			continue;
		}

		if (Params.GeomType == EFG_SkinnedLevelSet)
		{
			const int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(BoneName);
			TArray<int32> BoneIndexArray;
			BoneIndexArray.Add(BoneIndex);
			LatticeBuilder.AddBoneInfluence(BoneIndex, BoneIndexArray);
		}
		else
		{
			if (PhysicsAsset->FindBodyIndex(BoneName) != INDEX_NONE)
			{
				// We already have an existing bone. Don't replace it.
				continue;
			}
			const int32 NewBodyIndex = CreateNewBody(PhysicsAsset, BoneName, Params);
			UBodySetup* NewBodySetup = PhysicsAsset->SkeletalBodySetups[NewBodyIndex];
			check(NewBodySetup->BoneName == BoneName);
			const int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(NewBodySetup->BoneName);
			if (CreateCollisionFromBoneInternal(NewBodySetup, SkelMesh, BoneIndex, Params, Info[BoneIndex], TriangleCache))
			{
				SuccessfulBodySetupNames.AddUnique(BoneName);
			}
			else
			{
				bAllSuccessful = false;
				DestroyBody(PhysicsAsset, NewBodyIndex);
			}
		}
	}

	OutSuccessfulBodyIndices.Reserve(SuccessfulBodySetupNames.Num());
	for (const FName& BoneName : SuccessfulBodySetupNames)
	{
		const int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
		check(BodyIndex != INDEX_NONE);
		OutSuccessfulBodyIndices.Add(BodyIndex);
	}

	if (Params.GeomType == EFG_SkinnedLevelSet)
	{
		// Finish Building WeightedLattice
		const FName RootBoneName = SkelMesh->GetRefSkeleton().GetBoneName(RootBoneIndex);
		const int32 BodyIndex = CreateNewBody(PhysicsAsset, RootBoneName, Params);
		UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
		CreateWeightedLevelSetBody(LatticeBuilder, BodySetup, PhysicsAsset, RootBoneName, Params);
		OutSuccessfulBodyIndices.Add(BodyIndex);
	}
	return bAllSuccessful;
}

void WeldBodies(UPhysicsAsset* PhysAsset, int32 BaseBodyIndex, int32 AddBodyIndex, USkeletalMeshComponent* SkelComp)
{
	if(BaseBodyIndex == INDEX_NONE || AddBodyIndex == INDEX_NONE)
		return;

	if (SkelComp == NULL || SkelComp->GetSkeletalMeshAsset() == NULL)
	{
		return;
	}

	UBodySetup* Body1 = PhysAsset->SkeletalBodySetups[BaseBodyIndex];
	int32 Bone1Index = SkelComp->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(Body1->BoneName);
	check(Bone1Index != INDEX_NONE);
	FTransform Bone1TM = SkelComp->GetBoneTransform(Bone1Index);
	Bone1TM.RemoveScaling();

	UBodySetup* Body2 = PhysAsset->SkeletalBodySetups[AddBodyIndex];
	int32 Bone2Index = SkelComp->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(Body2->BoneName);
	check(Bone2Index != INDEX_NONE);
	FTransform Bone2TM = SkelComp->GetBoneTransform(Bone2Index);
	Bone2TM.RemoveScaling();

	FTransform Bone2ToBone1TM = Bone2TM.GetRelativeTransform(Bone1TM);

	// First copy all collision info over.
	for(int32 i=0; i<Body2->AggGeom.SphereElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.SphereElems.Add( Body2->AggGeom.SphereElems[i] );
		Body1->AggGeom.SphereElems[NewPrimIndex].Center = Bone2ToBone1TM.TransformPosition( Body2->AggGeom.SphereElems[i].Center ); // Make transform relative to body 1 instead of body 2
	}

	for(int32 i=0; i<Body2->AggGeom.BoxElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.BoxElems.Add( Body2->AggGeom.BoxElems[i] );
		Body1->AggGeom.BoxElems[NewPrimIndex].SetTransform( Body2->AggGeom.BoxElems[i].GetTransform() * Bone2ToBone1TM );
	}

	for(int32 i=0; i<Body2->AggGeom.SphylElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.SphylElems.Add( Body2->AggGeom.SphylElems[i] );
		Body1->AggGeom.SphylElems[NewPrimIndex].SetTransform( Body2->AggGeom.SphylElems[i].GetTransform() * Bone2ToBone1TM );
	}

	for(int32 i=0; i<Body2->AggGeom.ConvexElems.Num(); i++)
	{
		FKConvexElem& Elem2 = Body2->AggGeom.ConvexElems[i];
		FTransform Elem2TM = Elem2.GetTransform() * Bone2TM;
		FTransform Elem2ToBone1TM = Elem2TM.GetRelativeTransform(Bone1TM);

		// No transform on new element - we transform all the vertices into the new ref frame instead.
		int32 NewPrimIndex = Body1->AggGeom.ConvexElems.Add( Body2->AggGeom.ConvexElems[i] );
		FKConvexElem* cElem= &Body1->AggGeom.ConvexElems[NewPrimIndex];

		for(int32 j=0; j<cElem->VertexData.Num(); j++)
		{
			cElem->VertexData[j] = Elem2ToBone1TM.TransformPosition( cElem->VertexData[j] );
		}

		// Update face data.
		cElem->UpdateElemBox();
	}

	// After changing collision, need to recreate meshes
	Body1->InvalidatePhysicsData();
	Body1->CreatePhysicsMeshes();

	// We need to update the collision disable table to shift any pairs that included body2 to include body1 instead.
	// We remove any pairs that include body2 & body1.

	for(int32 i=0; i<PhysAsset->SkeletalBodySetups.Num(); i++)
	{
		if(i == AddBodyIndex) 
			continue;

		FRigidBodyIndexPair Key(i, AddBodyIndex);

		if( PhysAsset->CollisionDisableTable.Find(Key) )
		{
			PhysAsset->CollisionDisableTable.Remove(Key);

			// Only re-add pair if its not between 'base' and 'add' bodies.
			if(i != BaseBodyIndex)
			{
				FRigidBodyIndexPair NewKey(i, BaseBodyIndex);
				PhysAsset->CollisionDisableTable.Add(NewKey, 0);
			}
		}
	}

	// Make a sensible guess for the other flags
	ECollisionEnabled::Type NewCollisionEnabled = FMath::Min(Body1->DefaultInstance.GetCollisionEnabled(), Body2->DefaultInstance.GetCollisionEnabled());
	Body1->DefaultInstance.SetCollisionEnabled(NewCollisionEnabled);

	// if different
	if (Body1->PhysicsType != Body2->PhysicsType)
	{
		// i don't think this is necessarily good, but I think better than default
		Body1->PhysicsType = FMath::Max(Body1->PhysicsType, Body2->PhysicsType);
	}

	// Then deal with any constraints.

	TArray<int32>	Body2Constraints;
	PhysAsset->BodyFindConstraints(AddBodyIndex, Body2Constraints);

	while( Body2Constraints.Num() > 0 )
	{
		int32 ConstraintIndex = Body2Constraints[0];
		FConstraintInstance& Instance = PhysAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;

		FName OtherBodyName;
		if( Instance.ConstraintBone1 == Body2->BoneName )
			OtherBodyName = Instance.ConstraintBone2;
		else
			OtherBodyName = Instance.ConstraintBone1;

		// If this is a constraint between the two bodies we are welding, we just destroy it.
		if(OtherBodyName == Body1->BoneName)
		{
			DestroyConstraint(PhysAsset, ConstraintIndex);
		}
		else // Otherwise, we reconnect it to body1 (the 'base' body) instead of body2 (the 'weldee').
		{
			if(Instance.ConstraintBone2 == Body2->BoneName)
			{
				Instance.ConstraintBone2 = Body1->BoneName;

				FTransform ConFrame = Instance.GetRefFrame(EConstraintFrame::Frame2);
				Instance.SetRefFrame(EConstraintFrame::Frame2, ConFrame * FTransform(Bone2ToBone1TM));
			}
			else
			{
				Instance.ConstraintBone1 = Body1->BoneName;

				FTransform ConFrame = Instance.GetRefFrame(EConstraintFrame::Frame1);
				Instance.SetRefFrame(EConstraintFrame::Frame1, ConFrame * FTransform(Bone2ToBone1TM));
			}
		}

		// See if we have any more constraints to body2.
		PhysAsset->BodyFindConstraints(AddBodyIndex, Body2Constraints);
	}

	// Finally remove the body
	DestroyBody(PhysAsset, AddBodyIndex);
}

int32 CreateNewConstraint(UPhysicsAsset* PhysAsset, FName InConstraintName, UPhysicsConstraintTemplate* InConstraintSetup)
{
	// constraintClass must be a subclass of UPhysicsConstraintTemplate
	int32 ConstraintIndex = PhysAsset->FindConstraintIndex(InConstraintName);
	if(ConstraintIndex != INDEX_NONE)
	{
		return ConstraintIndex;
	}

	if (!ensure(CanCreateConstraints()))
	{
		return INDEX_NONE;
	}

	UPhysicsConstraintTemplate* NewConstraintSetup = NewObject<UPhysicsConstraintTemplate>(PhysAsset, NAME_None, RF_Transactional);
	if(InConstraintSetup)
	{
		NewConstraintSetup->DefaultInstance.CopyConstraintParamsFrom( &InConstraintSetup->DefaultInstance );
	}

	int32 ConstraintSetupIndex = PhysAsset->ConstraintSetup.Add( NewConstraintSetup );
	NewConstraintSetup->DefaultInstance.JointName = InConstraintName;

	return ConstraintSetupIndex;
}

void DestroyConstraint(UPhysicsAsset* PhysAsset, int32 ConstraintIndex)
{
	check(PhysAsset);
	PhysAsset->ConstraintSetup.RemoveAt(ConstraintIndex);
}


int32 CreateNewBody(UPhysicsAsset* PhysAsset, FName InBodyName, const FPhysAssetCreateParams& Params)
{
	check(PhysAsset);

	int32 BodyIndex = PhysAsset->FindBodyIndex(InBodyName);
	if(BodyIndex != INDEX_NONE)
	{
		return BodyIndex; // if we already have one for this name - just return that.
	}

	USkeletalBodySetup* NewBodySetup = NewObject<USkeletalBodySetup>(PhysAsset, NAME_None, RF_Transactional);
	// make default to be use complex as simple 
	NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	// newly created bodies default to simulating
	NewBodySetup->PhysicsType = PhysType_Default;

	int32 BodySetupIndex = PhysAsset->SkeletalBodySetups.Add( NewBodySetup );
	NewBodySetup->BoneName = InBodyName;

	PhysAsset->UpdateBodySetupIndexMap();
	PhysAsset->UpdateBoundsBodiesArray();

	if (Params.bDisableCollisionsByDefault)
	{
		for (int i = 0; i < PhysAsset->SkeletalBodySetups.Num(); ++i)
		{
			PhysAsset->DisableCollision(i, BodySetupIndex);
		}
	}
	// Return index of new body.
	return BodySetupIndex;
}

void DestroyBody(UPhysicsAsset* PhysAsset, int32 bodyIndex)
{
	check(PhysAsset);

	// First we must correct the CollisionDisableTable.
	// All elements which refer to bodyIndex are removed.
	// All elements which refer to a body with index >bodyIndex are adjusted. 

	TMap<FRigidBodyIndexPair,bool> NewCDT;
	for(int32 i=1; i<PhysAsset->SkeletalBodySetups.Num(); i++)
	{
		for(int32 j=0; j<i; j++)
		{
			FRigidBodyIndexPair Key(j,i);

			// If there was an entry for this pair, and it doesn't refer to the removed body, we need to add it to the new CDT.
			if( PhysAsset->CollisionDisableTable.Find(Key) )
			{
				if(i != bodyIndex && j != bodyIndex)
				{
					int32 NewI = (i > bodyIndex) ? i-1 : i;
					int32 NewJ = (j > bodyIndex) ? j-1 : j;

					FRigidBodyIndexPair NewKey(NewJ, NewI);
					NewCDT.Add(NewKey, 0);
				}
			}
		}
	}

	PhysAsset->CollisionDisableTable = NewCDT;

	// Now remove any constraints that were attached to this body.
	// This is a bit yuck and slow...
	TArray<int32> Constraints;
	PhysAsset->BodyFindConstraints(bodyIndex, Constraints);

	while(Constraints.Num() > 0)
	{
		DestroyConstraint( PhysAsset, Constraints[0] );
		PhysAsset->BodyFindConstraints(bodyIndex, Constraints);
	}

	// Remove pointer from array. Actual objects will be garbage collected.
	PhysAsset->SkeletalBodySetups.RemoveAt(bodyIndex);

	PhysAsset->UpdateBodySetupIndexMap();
	// Update body indices.
	PhysAsset->UpdateBoundsBodiesArray();
}

bool CanCreateConstraints()
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	TSharedPtr<FPathPermissionList> AssetClassPermissionList = AssetTools.GetAssetClassPathPermissionList(EAssetClassAction::CreateAsset);
	if (UPhysicsConstraintTemplate::StaticClass() && AssetClassPermissionList && AssetClassPermissionList->HasFiltering())
	{
		return AssetClassPermissionList->PassesFilter(UPhysicsConstraintTemplate::StaticClass()->GetPathName());
	}
	return true;
}

void SanitizeRestrictedContent(UPhysicsAsset* PhysAsset)
{
	check(PhysAsset);
}

}; // namespace FPhysicsAssetUtils

IMPLEMENT_MODULE(FDefaultModuleImpl, PhysicsAssetUtils)
