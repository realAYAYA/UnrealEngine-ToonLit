// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnSkeletalComponent.cpp: Actor component implementation.
=============================================================================*/

#include "Components/SkeletalMeshComponent.h"
#include "Misc/App.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimStats.h"
#include "AnimationRuntime.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationSettings.h"
#include "Engine/SkeletalMeshSocket.h"
#include "AI/NavigationSystemHelpers.h"
#include "PhysicsPublic.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimBlueprint.h"
#include "SkeletalRender.h"
#include "HAL/LowLevelMemTracker.h"
#include "SkinnedAssetCompiler.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalDebugRendering.h"

#include "Logging/MessageLog.h"
#include "Animation/AnimNode_LinkedInputPose.h"

#include "PhysXIncludes.h"
#include "ClothingSimulationFactory.h"
#include "ClothingSimulationInterface.h"
#include "ClothingSimulationInteractor.h"
#include "Features/IModularFeatures.h"
#include "Misc/RuntimeErrors.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "SkeletalRenderPublic.h"
#include "ContentStreaming.h"
#include "Animation/AnimTrace.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemStats.h"
#include "HAL/IConsoleManager.h"
#if WITH_EDITOR
#include "Engine/PoseWatchRenderData.h"
#include "Engine/PoseWatch.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMeshComponent)

LLM_DEFINE_TAG(SkeletalMesh_TransformData);

#define LOCTEXT_NAMESPACE "SkeletalMeshComponent"

TAutoConsoleVariable<int32> CVarUseParallelAnimationEvaluation(TEXT("a.ParallelAnimEvaluation"), 1, TEXT("If 1, animation evaluation will be run across the task graph system. If 0, evaluation will run purely on the game thread"));
TAutoConsoleVariable<int32> CVarUseParallelAnimUpdate(TEXT("a.ParallelAnimUpdate"), 1, TEXT("If != 0, then we update animation blend tree, native update, asset players and montages (is possible) on worker threads."));
TAutoConsoleVariable<int32> CVarForceUseParallelAnimUpdate(TEXT("a.ForceParallelAnimUpdate"), 0, TEXT("If != 0, then we update animations on worker threads regardless of the setting on the project or anim blueprint."));
TAutoConsoleVariable<int32> CVarUseParallelAnimationInterpolation(TEXT("a.ParallelAnimInterpolation"), 1, TEXT("If 1, animation interpolation will be run across the task graph system. If 0, interpolation will run purely on the game thread"));

static TAutoConsoleVariable<float> CVarStallParallelAnimation(
	TEXT("CriticalPathStall.ParallelAnimation"),
	0.0f,
	TEXT("Sleep for the given time in each parallel animation task. Time is given in ms. This is a debug option used for critical path analysis and forcing a change in the critical path."));

// Deprecated. Please switch to ANIM_SKINNED_ASSET_ISPC_ENABLED_DEFAULT and "a.SkinnedAsset.ISPC", see SkinnedAsset.cpp.
#if !defined(ANIM_SKELETAL_MESH_ISPC_ENABLED_DEFAULT)
#define ANIM_SKELETAL_MESH_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
bool bAnim_SkeletalMesh_ISPC_Enabled = INTEL_ISPC && ANIM_SKELETAL_MESH_ISPC_ENABLED_DEFAULT;
#else
bool bAnim_SkeletalMesh_ISPC_Enabled = ANIM_SKELETAL_MESH_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarAnimSkeletalMeshISPCEnabled(TEXT("a.SkeletalMesh.ISPC"), bAnim_SkeletalMesh_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in animation skeletal mesh components. Deprecated, please switch to a.SkinnedAsset.ISPC"));
#endif

static TAutoConsoleVariable<int32> CVarCacheLocalSpaceBounds(
	TEXT("a.CacheLocalSpaceBounds"),
	1,
	TEXT("If 1 (default) local-space bounds are calculated and cached, otherwise worldspace bounds are built and cached (and inverse transformed to produce local bounds)."));

DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Instance Spawn Time"), STAT_AnimSpawnTime, STATGROUP_Anim, );
DEFINE_STAT(STAT_AnimSpawnTime);
DEFINE_STAT(STAT_PostAnimEvaluation);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Animation);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);

static bool GParallelAnimCompletionTaskHighPriority = false;
static FAutoConsoleVariableRef CVarParallelAnimCompletionTaskHighPriority(
	TEXT("TaskGraph.TaskPriorities.ParallelAnimCompletionTaskHighPriority"),
	GParallelAnimCompletionTaskHighPriority,
	TEXT("Allows parallel anim completion tasks to take priority on the GT so further work (if needed) can be kicked off earlier."),
	ECVF_Default
);

FAutoConsoleTaskPriority CPrio_ParallelAnimationEvaluationTask(
	TEXT("TaskGraph.TaskPriorities.ParallelAnimationEvaluationTask"),
	TEXT("Task and thread priority for FParallelAnimationEvaluationTask"),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FParallelAnimationEvaluationTask
{
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

public:
	FParallelAnimationEvaluationTask(TWeakObjectPtr<USkeletalMeshComponent> InSkeletalMeshComponent)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelAnimationEvaluationTask, STATGROUP_TaskGraphTasks);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_ParallelAnimationEvaluationTask.Get();
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (USkeletalMeshComponent* Comp = SkeletalMeshComponent.Get())
		{
			FScopeCycleCounterUObject ContextScope(Comp);
#if !UE_BUILD_TEST && !UE_BUILD_SHIPPING
			float Stall = CVarStallParallelAnimation.GetValueOnAnyThread();
			if (Stall > 0.0f)
			{
				FPlatformProcess::Sleep(Stall / 1000.0f);
			}
#endif
			if (CurrentThread != ENamedThreads::GameThread)
			{
				GInitRunaway();
			}

			Comp->ParallelAnimationEvaluation();
		}
	}
};

class FParallelAnimationCompletionTask
{
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

public:
	FParallelAnimationCompletionTask(TWeakObjectPtr<USkeletalMeshComponent> InSkeletalMeshComponent)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelAnimationCompletionTask, STATGROUP_TaskGraphTasks);
	}
	static ENamedThreads::Type GetDesiredThread()
	{
		if (GParallelAnimCompletionTaskHighPriority)
		{
			return static_cast<ENamedThreads::Type>(ENamedThreads::GameThread | ENamedThreads::HighTaskPriority);
		}
		return ENamedThreads::GameThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimGameThreadTime);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Animation);

		if (USkeletalMeshComponent* Comp = SkeletalMeshComponent.Get())
		{
			FScopeCycleCounterUObject ComponentScope(Comp);
			FScopeCycleCounterUObject MeshScope(Comp->GetSkeletalMeshAsset());

			if(IsValidRef(Comp->ParallelAnimationEvaluationTask))
			{
				const bool bPerformPostAnimEvaluation = true;
				Comp->CompleteParallelAnimationEvaluation(bPerformPostAnimEvaluation);
			}
		}
	}
};

USkeletalMeshComponent::USkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bWantsInitializeComponent = true;
	GlobalAnimRateScale = 1.0f;
	bNoSkeletonUpdate = false;
	VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
	PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::SimulationUpatesComponentTransform;
	SetGenerateOverlapEvents(false);
	LineCheckBoundsScale = FVector(1.0f, 1.0f, 1.0f);

	EndPhysicsTickFunction.TickGroup = TG_EndPhysics;
	EndPhysicsTickFunction.bCanEverTick = true;
	EndPhysicsTickFunction.bStartWithTickEnabled = true;

	ClothTickFunction.TickGroup = TG_PrePhysics;
	ClothTickFunction.EndTickGroup = TG_PostPhysics;
	ClothTickFunction.bCanEverTick = true;

	bWaitForParallelClothTask = false;
	bNotifySyncComponentToRBPhysics = false;

	ClothMaxDistanceScale = 1.0f;
	bResetAfterTeleport = true;
	TeleportDistanceThreshold = 300.0f;
	TeleportRotationThreshold = 0.0f;	// angles in degree, disabled by default
	ClothBlendWeight = 1.0f;

	ClothTeleportMode = EClothingTeleportMode::None;
	PrevRootBoneMatrix = GetBoneMatrix(0); // save the root bone transform

	// pre-compute cloth teleport thresholds for performance
	ComputeTeleportRotationThresholdInRadians();
	ComputeTeleportDistanceThresholdInRadians();

	bBindClothToLeaderComponent = false;
	bClothingSimulationSuspended = false;

#if WITH_EDITORONLY_DATA
	DefaultPlayRate_DEPRECATED = 1.0f;
	bDefaultPlaying_DEPRECATED = true;
	bOverrideDefaultAnimatingRig = false;
#endif
	bEnablePhysicsOnDedicatedServer = UPhysicsSettings::Get()->bSimulateSkeletalMeshOnDedicatedServer;
	bEnableUpdateRateOptimizations = false;
	RagdollAggregateThreshold = UPhysicsSettings::Get()->RagdollAggregateThreshold;

	bUpdateMeshWhenKinematic = false;

	LastPoseTickFrame = 0u;

	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;

	bTickInEditor = true;

	CachedAnimCurveUidVersion = 0;
	ResetRootBodyIndex();

	ClothingSimulationFactory = UClothingSimulationFactory::GetDefaultClothingSimulationFactoryClass();

	ClothingSimulation = nullptr;
	ClothingSimulationContext = nullptr;
	ClothingInteractor = nullptr;

	bAllowClothActors = true;
	bPostEvaluatingAnimation = false;
	bAllowAnimCurveEvaluation = true;
	bDisablePostProcessBlueprint = false;

	// By default enable overlaps when blending physics - user can disable if they are sure it's unnecessary
	bUpdateOverlapsOnAnimationFinalize = true;

	bPropagateCurvesToFollowers = false;

	bSkipKinematicUpdateWhenInterpolating = false;
	bSkipBoundsUpdateWhenInterpolating = false;

	DeferredKinematicUpdateIndex = INDEX_NONE;
}

USkeletalMesh* USkeletalMeshComponent::GetSkeletalMeshAsset() const
{
	return Cast<USkeletalMesh>(GetSkinnedAsset());
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
		if (Ar.IsSaving())
		{
			if ((NULL != AnimationBlueprint_DEPRECATED) && (NULL == AnimBlueprintGeneratedClass))
			{
				AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(AnimationBlueprint_DEPRECATED->GeneratedClass);
			}
		}
#endif

	Super::Serialize(Ar);

	// to count memory : TODO: REMOVE?
	if (Ar.IsCountingMemory())
	{
		BoneSpaceTransforms.CountBytes(Ar);
		RequiredBones.CountBytes(Ar);
	}

	if (Ar.UEVer() < VER_UE4_REMOVE_SKELETALMESH_COMPONENT_BODYSETUP_SERIALIZATION)
	{
		//we used to serialize bodysetup of skeletal mesh component. We no longer do this, but need to not break existing content
		if (bEnablePerPolyCollision)
		{
			Ar << BodySetup;
		}
	}

	// Since we separated simulation vs blending
	// if simulation is on when loaded, just set blendphysics to be true
	if (BodyInstance.bSimulatePhysics)
	{
		bBlendPhysics = true;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && (Ar.UEVer() < VER_UE4_EDITORONLY_BLUEPRINTS))
	{
		if ((NULL != AnimationBlueprint_DEPRECATED))
		{
			// Migrate the class from the animation blueprint once, and null the value so we never get in again
			AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(AnimationBlueprint_DEPRECATED->GeneratedClass);
			AnimationBlueprint_DEPRECATED = NULL;
		}
	}
#endif

	if (Ar.IsLoading() && (Ar.UEVer() < VER_UE4_NO_ANIM_BP_CLASS_IN_GAMEPLAY_CODE))
	{
		if (nullptr != AnimBlueprintGeneratedClass)
		{
			AnimClass = AnimBlueprintGeneratedClass;
		}
	}

	if (Ar.IsLoading() && AnimBlueprintGeneratedClass)
	{
		AnimBlueprintGeneratedClass = nullptr;
	}

	if (Ar.IsLoading() && (Ar.UEVer() < VER_UE4_AUTO_WELDING))
	{
		BodyInstance.bAutoWeld = false;
	}

	Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);
	if (Ar.IsLoading() && Ar.CustomVer(FAnimPhysObjectVersion::GUID) < FAnimPhysObjectVersion::RenameDisableAnimCurvesToAllowAnimCurveEvaluation)
	{
		bAllowAnimCurveEvaluation = !bDisableAnimCurves_DEPRECATED;
	}

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMeshComponent::PostLoad()
{
	Super::PostLoad();

	// We know for sure that an override was set if this is non-zero.
	if(MinLodModel > 0)
	{
		bOverrideMinLod = true;
	}

#if WITH_EDITORONLY_DATA
	// Update property alias
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SkeletalMeshAsset = GetSkeletalMeshAsset();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void USkeletalMeshComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	UpdateEndPhysicsTickRegisteredState();
	UpdateClothTickRegisteredState();
}

void USkeletalMeshComponent::SetComponentTickEnabled(bool bEnabled)
{
	Super::SetComponentTickEnabled(bEnabled);

	// Unregister the cloth tick function when the component's tick is being disabled (it will be re-enabled at the next component tick update once it runs again)
	if (!bEnabled)
	{
		RegisterClothTick(false);
	}
}

void USkeletalMeshComponent::RegisterEndPhysicsTick(bool bRegister)
{
	if (bRegister != EndPhysicsTickFunction.IsTickFunctionRegistered())
	{
		if (bRegister)
		{
			UWorld* World = GetWorld();
			if (World->EndPhysicsTickFunction.IsTickFunctionRegistered() && SetupActorComponentTickFunction(&EndPhysicsTickFunction))
			{
				EndPhysicsTickFunction.Target = this;
				// Make sure our EndPhysicsTick gets called after physics simulation is finished
				if (World != nullptr)
				{
					EndPhysicsTickFunction.AddPrerequisite(World, World->EndPhysicsTickFunction);
				}
			}
		}
		else
		{
			EndPhysicsTickFunction.UnRegisterTickFunction();
		}
	}
}

void USkeletalMeshComponent::RegisterClothTick(bool bRegister)
{
	if (bRegister != ClothTickFunction.IsTickFunctionRegistered())
	{
		if (bRegister)
		{
			if (SetupActorComponentTickFunction(&ClothTickFunction))
			{
				ClothTickFunction.Target = this;
				ClothTickFunction.AddPrerequisite(this, PrimaryComponentTick);
				ClothTickFunction.AddPrerequisite(this, EndPhysicsTickFunction);	//If this tick function is running it means that we are doing physics blending so we should wait for its results
			}
		}
		else
		{
			ClothTickFunction.UnRegisterTickFunction();
		}
	}
}

bool USkeletalMeshComponent::ShouldRunEndPhysicsTick() const
{
	return	(bEnablePhysicsOnDedicatedServer || !IsNetMode(NM_DedicatedServer)) && // Early out if we are on a dedicated server and not running physics.
			((IsSimulatingPhysics() && RigidBodyIsAwake()) || ShouldBlendPhysicsBones());
}

void USkeletalMeshComponent::UpdateEndPhysicsTickRegisteredState()
{
	RegisterEndPhysicsTick(PrimaryComponentTick.IsTickFunctionRegistered() && ShouldRunEndPhysicsTick());
}

bool USkeletalMeshComponent::ShouldRunClothTick() const
{
	if(bClothingSimulationSuspended)
	{
		return false;
	}

	if(CanSimulateClothing())
	{
		return true;
	}

	return	false;
}

extern TAutoConsoleVariable<int32> CVarEnableClothPhysics;

bool USkeletalMeshComponent::CanSimulateClothing() const
{
	if(!GetSkeletalMeshAsset() || !bAllowClothActors || !CVarEnableClothPhysics.GetValueOnAnyThread())
	{
		return false;
	}

	return GetSkeletalMeshAsset()->HasActiveClothingAssets() && !IsNetMode(NM_DedicatedServer);
}

void USkeletalMeshComponent::UpdateClothTickRegisteredState()
{
	RegisterClothTick(PrimaryComponentTick.IsTickFunctionRegistered() && ShouldRunClothTick());
}

void USkeletalMeshComponent::FinalizePoseEvaluationResult(const USkeletalMesh* InMesh, TArray<FTransform>& OutBoneSpaceTransforms, FVector& OutRootBoneTranslation, FCompactPose& InFinalPose) const
{
	const TArray<FTransform>& RefBonePose = InMesh->GetRefSkeleton().GetRefBonePose();
	if(InFinalPose.IsValid() && InFinalPose.GetNumBones() > 0)
	{
		InFinalPose.NormalizeRotations();

		// Bone index array is in increasing order, so we can fill gaps with reference pose
		const auto FillReferencePose = [&](int32 BeginIndex, int32 EndIndex)
		{
			for (int32 MeshPoseIndex = BeginIndex; MeshPoseIndex < EndIndex; ++MeshPoseIndex)
			{
				OutBoneSpaceTransforms[MeshPoseIndex] = RefBonePose[MeshPoseIndex];
			}
		};

		int32 LastMeshPoseIndex = 0;
		const int32 BoneCount = RefBonePose.Num();
		OutBoneSpaceTransforms.SetNum(BoneCount);
		for (const FCompactPoseBoneIndex BoneIndex : InFinalPose.ForEachBoneIndex())
		{
			const int32 MeshPoseIndex = InFinalPose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex).GetInt();
			FillReferencePose(LastMeshPoseIndex, MeshPoseIndex);
			OutBoneSpaceTransforms[MeshPoseIndex] = InFinalPose[BoneIndex];
			LastMeshPoseIndex = MeshPoseIndex + 1;
		}
		FillReferencePose(LastMeshPoseIndex, BoneCount);
	}
	else
	{
		OutBoneSpaceTransforms = RefBonePose;
	}

	OutRootBoneTranslation = OutBoneSpaceTransforms[0].GetTranslation() - RefBonePose[0].GetTranslation();
}

void USkeletalMeshComponent::FinalizeAttributeEvaluationResults(const FBoneContainer& BoneContainer, const UE::Anim::FHeapAttributeContainer& FinalContainer,
	UE::Anim::FMeshAttributeContainer& OutContainer) const
{
	OutContainer.CopyFrom(FinalContainer, BoneContainer);
}

bool USkeletalMeshComponent::NeedToSpawnAnimScriptInstance() const
{
	USkeletalMesh* MeshAsset = GetSkeletalMeshAsset();
	IAnimClassInterface* AnimClassInterface = IAnimClassInterface::GetFromClass(AnimClass);
	USkeleton* AnimSkeleton = (AnimClassInterface) ? AnimClassInterface->GetTargetSkeleton() : nullptr;
	if(AnimSkeleton == nullptr)
	{
		// Fall back to mesh skeleton (anim BP could have been a template)
		AnimSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	}
	const bool bSkeletonCompatible = (MeshAsset && AnimSkeleton && MeshAsset->GetSkeleton()) ? MeshAsset->GetSkeleton()->IsCompatible(AnimSkeleton) : false;
	const bool bSkelMeshCompatible = (MeshAsset && AnimSkeleton) ? AnimSkeleton->IsCompatibleMesh(MeshAsset, false) : false;
	const bool bAnimSkelValid = !AnimClassInterface || (bSkeletonCompatible && bSkelMeshCompatible);

	if (AnimationMode == EAnimationMode::AnimationBlueprint && AnimClass && bAnimSkelValid)
	{
		// Check for an 'invalid' AnimScriptInstance:
		// - Could be NULL (in the case of 'standard' first-time initialization)
		// - Could have a different class (in the case where the active anim BP has changed)
		// - Could have a different outer (in the case where an actor has been spawned using an existing actor as a template, as the component is shallow copied directly from the template)
		if ( (AnimScriptInstance == nullptr) || (AnimScriptInstance->GetClass() != AnimClass) || AnimScriptInstance->GetOuter() != this )
		{
			return true;
		}
	}

	return false;
}

bool USkeletalMeshComponent::NeedToSpawnPostPhysicsInstance(bool bForceReinit) const
{
	if(GetSkeletalMeshAsset())
	{
		const UClass* MainInstanceClass = *AnimClass;
		const UClass* ClassToUse = *GetSkeletalMeshAsset()->GetPostProcessAnimBlueprint();
		const UClass* CurrentClass = PostProcessAnimInstance ? PostProcessAnimInstance->GetClass() : nullptr;

		// We need to have an instance, and we have the wrong class (different or null)
		if(ClassToUse && (ClassToUse != CurrentClass || bForceReinit ) && MainInstanceClass != ClassToUse)
		{
			return true;
		}
	}

	return false;
}

bool USkeletalMeshComponent::IsAnimBlueprintInstanced() const
{
	return (AnimScriptInstance && AnimScriptInstance->GetClass() == AnimClass);
}

void USkeletalMeshComponent::OnRegister()
{
	UpdateHasValidBodies();	//Make sure this is done before we call into the Super which will trigger OnCreatePhysicsState

	Super::OnRegister();

	// Ensure we have an empty list of linked instances on registration. Ready for the initialization below 
	// to correctly populate that list.
	ResetLinkedAnimInstances();

	// We force an initialization here because we're in one of two cases.
	// 1) First register, no spawned instance, need to initialize
	// 2) We're being re-registered, in which case we've went through
	// OnUnregister and unconditionally uninitialized our anim instances
	// so we need to force initialize them before we begin to tick.
	InitAnim(true);

	if (bRenderStatic || (VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered && !FApp::CanEverRender()))
	{
		SetComponentTickEnabled(false);
	}

	// If no simulation factory is currently set, set it to the default factory
	if (!ClothingSimulationFactory)
	{
		ClothingSimulationFactory = UClothingSimulationFactory::GetDefaultClothingSimulationFactoryClass();
	}

	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	// Look up for the best simulation factory to support each asset
	if (ClothingSimulationFactory && SkelMesh)
	{
		// Check whether all clothing assets are supported by the current factory
		bool bSupportsAllAssets = true;

		UClothingSimulationFactory* const DefaultObject = ClothingSimulationFactory->GetDefaultObject<UClothingSimulationFactory>();
		for (UClothingAssetBase* const ClothingAsset : SkelMesh->GetMeshClothingAssets())
		{
			if (ClothingAsset && !DefaultObject->SupportsAsset(ClothingAsset))
			{
				bSupportsAllAssets = false;

				UE_LOG(LogSkeletalMesh, Display,
					TEXT("OnRegister[%s]: [%s] is currently unable to provide a fully functional simulation for each of this SkeletalMesh's clothing assets."),
					*GetPathNameSafe(SkelMesh),
					*ClothingSimulationFactory->GetName());
				UE_LOG(LogSkeletalMesh, Display,
					TEXT("OnRegister[%s]: The ClothingSimulationFactory property will now be automatically updated to use the most functional simulation that can be found."),
					*GetPathNameSafe(SkelMesh));

				break;
			}
		}

		// Try to find a new clothing factory that matches most asset requirements
		if (!bSupportsAllAssets)
		{
			int MostSupportedNumAssets = 0;

			const TArray<IClothingSimulationFactoryClassProvider*> ClassProviders = IModularFeatures::Get().GetModularFeatureImplementations<IClothingSimulationFactoryClassProvider>(IClothingSimulationFactoryClassProvider::FeatureName);
			for (const IClothingSimulationFactoryClassProvider* const ClassProvider : ClassProviders)
			{
				if (ClassProvider)
				{
					if (const TSubclassOf<UClothingSimulationFactory> NewClothingSimulationFactory = ClassProvider->GetClothingSimulationFactoryClass())
					{
						int NumAssets = 0;
						int SupportedNumAssets = 0;
						UClothingSimulationFactory* const NewDefaultObject = NewClothingSimulationFactory->GetDefaultObject<UClothingSimulationFactory>();
						for (UClothingAssetBase* const ClothingAsset : SkelMesh->GetMeshClothingAssets())
						{
							if (ClothingAsset)
							{
								if (NewDefaultObject->SupportsAsset(ClothingAsset))
								{
									++SupportedNumAssets;
								}
								++NumAssets;
							}
						}

						if (SupportedNumAssets > MostSupportedNumAssets)
						{
							ClothingSimulationFactory = NewClothingSimulationFactory;
							MostSupportedNumAssets = SupportedNumAssets;
							if (SupportedNumAssets == NumAssets)
							{
								bSupportsAllAssets = true;
								break;  // Stop at the first factory that supports all assets
							}
						}
					}
				}
			}

			UE_CLOG(!MostSupportedNumAssets, LogSkeletalMesh, Warning,
				TEXT("OnRegister[%s]: There is no clothing simulation factory available that supports any of this SkeletalMesh's clothing assets."),
				*GetPathNameSafe(SkelMesh));

			UE_CLOG(MostSupportedNumAssets && !bSupportsAllAssets, LogSkeletalMesh, Warning,
				TEXT("OnRegister[%s]: The most suitable clothing simulation factory available only partially supports this SkeletalMesh's clothing assets."),
				*GetPathNameSafe(SkelMesh));
		}
	}

	RecreateClothingActors();
}

void USkeletalMeshComponent::OnUnregister()
{
	const bool bBlockOnTask = true; // wait on evaluation task so we complete any work before this component goes away
	const bool bPerformPostAnimEvaluation = false; // Skip post evaluation, it would be wasted work

	// Wait for any in flight animation evaluation to complete
	HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);

	// Wait for any in flight clothing simulation to complete
	HandleExistingParallelClothSimulation();

	//clothing actors will be re-created in TickClothing
	ReleaseAllClothingResources();

	if (AnimScriptInstance)
	{
		AnimScriptInstance->UninitializeAnimation();
	}

	for(UAnimInstance* LinkedInstance : LinkedInstances)
	{
		LinkedInstance->UninitializeAnimation();
	}
	ResetLinkedAnimInstances();

	if(PostProcessAnimInstance)
	{
		PostProcessAnimInstance->UninitializeAnimation();
	}

	UClothingSimulationFactory* SimFactory = GetClothingSimFactory();
	if(ClothingSimulation && SimFactory)
	{
		ClothingSimulation->DestroyContext(ClothingSimulationContext);
		ClothingSimulation->DestroyActors();
		ClothingSimulation->Shutdown();

		SimFactory->DestroySimulation(ClothingSimulation);
		ClothingSimulation = nullptr;
		ClothingSimulationContext = nullptr;
	}

	if (DeferredKinematicUpdateIndex != INDEX_NONE)
	{
		UWorld* World = GetWorld();
		FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr;

		if (PhysScene != nullptr)
		{
			PhysScene->ClearPreSimKinematicUpdate(this);
		}
	}

	RequiredBones.Reset();

	Super::OnUnregister();
}

void USkeletalMeshComponent::InitAnim(bool bForceReinit)
{
	CSV_SCOPED_TIMING_STAT(Animation, InitAnim);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SkelMeshComp_InitAnim);
	LLM_SCOPE(ELLMTag::Animation);

	// a lot of places just call InitAnim without checking Mesh, so 
	// I'm moving the check here
	if ( GetSkeletalMeshAsset() != nullptr && IsRegistered() )
	{
		//clear cache UID since we don't know if skeleton changed
		CachedAnimCurveUidVersion = 0;

		// we still need this in case users doesn't call tick, but sent to renderer
		MorphTargetWeights.SetNumZeroed(GetSkeletalMeshAsset()->GetMorphTargets().Num());

		// We may be doing parallel evaluation on the current anim instance
		// Calling this here with true will block this init till that thread completes
		// and it is safe to continue
		const bool bBlockOnTask = true; // wait on evaluation task so it is safe to continue with Init
		const bool bPerformPostAnimEvaluation = true; // That will swap buffer back to ComponentTransform, and finish evaluate. This is required - otherwise, we won't have a buffer.
		HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);

		bool bBlueprintMismatch = (AnimClass != nullptr) &&
			(AnimScriptInstance != nullptr) && (AnimScriptInstance->GetClass() != AnimClass);

		const USkeleton* AnimSkeleton = (AnimScriptInstance)? AnimScriptInstance->CurrentSkeleton : nullptr;

		const bool bClearAnimInstance = AnimScriptInstance && !AnimSkeleton;
		const bool bSkeletonMismatch = AnimSkeleton && (AnimScriptInstance->CurrentSkeleton!=GetSkeletalMeshAsset()->GetSkeleton());
		const bool bSkeletonCompatible = AnimSkeleton && GetSkeletalMeshAsset()->GetSkeleton() && !bSkeletonMismatch && GetSkeletalMeshAsset()->GetSkeleton()->IsCompatible(AnimSkeleton);

		LastPoseTickFrame = 0;

		if (bBlueprintMismatch || bSkeletonMismatch || !bSkeletonCompatible || bClearAnimInstance)
		{
			ClearAnimScriptInstance();
		}

		// this has to be called before Initialize Animation because it will required RequiredBones list when InitializeAnimScript
		RecalcRequiredBones(GetPredictedLODLevel());

		// In Editor, animations won't get ticked. So Update once to get accurate representation instead of T-Pose.
		// Also allow this to be an option to support pre-4.19 games that might need it..
		const bool bTickAnimationNow =
			(((GetWorld()->WorldType == EWorldType::Editor) && !bForceRefpose)
			|| UAnimationSettings::Get()->bTickAnimationOnSkeletalMeshInit)
			&& !bUseRefPoseOnInitAnim;

		const bool bInitializedAnimInstance = InitializeAnimScriptInstance(bForceReinit, !bTickAnimationNow);

		// Make sure we have a valid pose.
		// We don't allocate transform data when using LeaderPoseComponent, so we have nothing to render.
		if (!LeaderPoseComponent.IsValid())
		{	
			if (bInitializedAnimInstance || (AnimScriptInstance == nullptr))
			{ 
				if (bTickAnimationNow)
				{
					TickAnimation(0.f, false);
					RefreshBoneTransforms();
				}
				else
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					BoneSpaceTransforms = GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
					//Mini RefreshBoneTransforms (the bit we actually care about)
					GetSkeletalMeshAsset()->FillComponentSpaceTransforms(BoneSpaceTransforms, FillComponentSpaceTransformsRequiredBones, GetEditableComponentSpaceTransforms());
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
					bNeedToFlipSpaceBaseBuffers = true; // Have updated space bases so need to flip
					FlipEditableSpaceBases();
				}

				if (bInitializedAnimInstance)
				{
					// Allow blueprints to respond to the event in editor
					FEditorScriptExecutionGuard ScriptGuard;
					OnAnimInitialized.Broadcast();
				}
			}
		}

		UpdateComponentToWorld();
	}
}

#if WITH_EDITOR
void USkeletalMeshComponent::ApplyEditedComponentSpaceTransforms()
{
	// Flip buffers once to copy the directly-written component space transforms
	bNeedToFlipSpaceBaseBuffers = true;
	bHasValidBoneTransform = false;
	FlipEditableSpaceBases();
	bHasValidBoneTransform = true;

	InvalidateCachedBounds();
	UpdateBounds();
	MarkRenderTransformDirty();
	MarkRenderDynamicDataDirty();

	for (auto& FollowerComponent : GetFollowerPoseComponents())
	{
		if (FollowerComponent.IsValid())
		{
			FollowerComponent->UpdateFollowerComponent();
		}
	}
}
#endif

bool USkeletalMeshComponent::InitializeAnimScriptInstance(bool bForceReinit, bool bInDeferRootNodeInitialization)
{
	bool bInitializedMainInstance = false;
	bool bInitializedPostInstance = false;

	if (IsRegistered())
	{
		USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
		check(SkelMesh);

		if (NeedToSpawnAnimScriptInstance())
		{
			SCOPE_CYCLE_COUNTER(STAT_AnimSpawnTime);
			AnimScriptInstance = NewObject<UAnimInstance>(this, AnimClass);

			if (AnimScriptInstance)
			{
				// If we have any linked instances left we need to clear them out now, we're about to have a new leader instance
				ResetLinkedAnimInstances();

				AnimScriptInstance->InitializeAnimation(bInDeferRootNodeInitialization);
				bInitializedMainInstance = true;
			}
		}
		else 
		{
			bool bShouldSpawnSingleNodeInstance = SkelMesh && SkelMesh->GetSkeleton() && AnimationMode == EAnimationMode::AnimationSingleNode;
			if (bShouldSpawnSingleNodeInstance)
			{
				SCOPE_CYCLE_COUNTER(STAT_AnimSpawnTime);

				UAnimSingleNodeInstance* OldInstance = nullptr;
				if (!bForceReinit)
				{
					OldInstance = Cast<UAnimSingleNodeInstance>(AnimScriptInstance);
				}

				AnimScriptInstance = NewObject<UAnimSingleNodeInstance>(this);

				if (AnimScriptInstance)
				{
					ResetLinkedAnimInstances();
				
					AnimScriptInstance->InitializeAnimation(bInDeferRootNodeInitialization);
					bInitializedMainInstance = true;
				}

				if (OldInstance && AnimScriptInstance)
				{
					// Copy data from old instance unless we force reinitialized
					FSingleAnimationPlayData CachedData;
					CachedData.PopulateFrom(OldInstance);
					CachedData.Initialize(Cast<UAnimSingleNodeInstance>(AnimScriptInstance));
				}
				else
				{
					// otherwise, initialize with AnimationData
					AnimationData.Initialize(Cast<UAnimSingleNodeInstance>(AnimScriptInstance));
				}

				if (AnimScriptInstance)
				{
					AnimScriptInstance->AddToCluster(this);
				}
			}
		}

		// May need to clear out the post physics instance
		UClass* NewMeshInstanceClass = *SkelMesh->GetPostProcessAnimBlueprint();
		if(!NewMeshInstanceClass || NewMeshInstanceClass == *AnimClass)
		{
			PostProcessAnimInstance = nullptr;
		}

		if(NeedToSpawnPostPhysicsInstance(bForceReinit))
		{
			PostProcessAnimInstance = NewObject<UAnimInstance>(this, *SkelMesh->GetPostProcessAnimBlueprint());

			if(PostProcessAnimInstance)
			{
				PostProcessAnimInstance->InitializeAnimation();

				if(FAnimNode_LinkedInputPose* InputNode = PostProcessAnimInstance->GetLinkedInputPoseNode())
				{
					InputNode->CachedInputPose.SetBoneContainer(&PostProcessAnimInstance->GetRequiredBones());
				}

				bInitializedPostInstance = true;
			}
		}
		else if (!SkelMesh->GetPostProcessAnimBlueprint().Get())
		{
			PostProcessAnimInstance = nullptr;
		}

		if (AnimScriptInstance && !bInitializedMainInstance && bForceReinit)
		{
			ResetLinkedAnimInstances();
		
			AnimScriptInstance->InitializeAnimation(bInDeferRootNodeInitialization);
			bInitializedMainInstance = true;
		}

		if(PostProcessAnimInstance && !bInitializedPostInstance && bForceReinit)
		{
			PostProcessAnimInstance->InitializeAnimation();
			bInitializedPostInstance = true;
		}

		// refresh morph targets - this can happen when re-registration happens
		RefreshMorphTargets();
	}
	return bInitializedMainInstance || bInitializedPostInstance;
}

bool USkeletalMeshComponent::IsWindEnabled() const
{
	// Wind is enabled in game worlds
	return GetWorld() && GetWorld()->IsGameWorld();
}

void USkeletalMeshComponent::ClearAnimScriptInstance()
{
	if (AnimScriptInstance)
	{
		// We may be doing parallel evaluation on the current anim instance
		// Calling this here with true will block this init till that thread completes
		// and it is safe to continue
		const bool bBlockOnTask = true; // wait on evaluation task so it is safe to swap the buffers
		const bool bPerformPostAnimEvaluation = true; // Do PostEvaluation so we make sure to swap the buffers back. 
		HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);

		AnimScriptInstance->EndNotifyStates();
	}
	AnimScriptInstance = nullptr;
	ResetLinkedAnimInstances();
	ClearCachedAnimProperties();
}

void USkeletalMeshComponent::ClearCachedAnimProperties()
{
	CachedBoneSpaceTransforms.Empty();
	CachedComponentSpaceTransforms.Empty();
	CachedCurve.Empty();
	CachedAttributes.Empty();
}

void USkeletalMeshComponent::InitializeComponent()
{
	Super::InitializeComponent();

	InitAnim(false);
}

void USkeletalMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	// Trace the 'first frame' markers
	TRACE_SKELETAL_MESH_COMPONENT(this);

	ForEachAnimInstance([](UAnimInstance* InAnimInstance)
	{
		InAnimInstance->NativeBeginPlay();
		InAnimInstance->BlueprintBeginPlay();
	});
}

#if WITH_EDITOR
void USkeletalMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	// Set the skinned asset pointer with the alias pointer (must happen before the call to Super::PostEditChangeProperty)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, SkeletalMeshAsset))
	{
		SetSkinnedAsset(SkeletalMeshAsset);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	Super::PostEditChangeProperty(PropertyChangedEvent);


	if ( PropertyThatChanged != nullptr )
	{
		// if the blueprint has changed, recreate the AnimInstance
		if ( PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( USkeletalMeshComponent, AnimationMode ) )
		{
			if (AnimationMode == EAnimationMode::AnimationBlueprint)
			{
				if (AnimClass == nullptr)
				{
					ClearAnimScriptInstance();
				}
				else
				{
					if (NeedToSpawnAnimScriptInstance())
					{
						SCOPE_CYCLE_COUNTER(STAT_AnimSpawnTime);
						AnimScriptInstance = NewObject<UAnimInstance>(this, AnimClass);
						AnimScriptInstance->InitializeAnimation();
					}
				}
			}
		}

		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMeshComponent, AnimClass))
		{
			InitAnim(false);
		}

		USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if(PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( USkeletalMeshComponent, SkeletalMeshAsset))
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			ValidateAnimation();

			// Check the post physics mesh instance, as the mesh has changed
			if(PostProcessAnimInstance)
			{
				UClass* CurrentClass = PostProcessAnimInstance->GetClass();
				UClass* MeshClass = SkelMesh ? *SkelMesh->GetPostProcessAnimBlueprint() : nullptr;
				if(CurrentClass != MeshClass)
				{
					if(MeshClass)
					{
						PostProcessAnimInstance = NewObject<UAnimInstance>(this, *SkelMesh->GetPostProcessAnimBlueprint());
						PostProcessAnimInstance->InitializeAnimation();
					}
					else
					{
						// No instance needed for the new mesh
						PostProcessAnimInstance = nullptr;
					}
				}
			}

			if(OnSkeletalMeshPropertyChanged.IsBound())
			{
				OnSkeletalMeshPropertyChanged.Broadcast();
			}

			// Skeletal mesh was switched so we should clean up the override materials and dirty the render state to recreate material proxies
			if (OverrideMaterials.Num())
			{
				CleanUpOverrideMaterials();
				MarkRenderStateDirty();
			}
		}

		// when user changes simulate physics, just make sure to update blendphysics together
		// bBlendPhysics isn't the editor exposed property, it should work with simulate physics
		if ( PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( FBodyInstance, bSimulatePhysics ))
		{
			bBlendPhysics = BodyInstance.bSimulatePhysics;
		}

		if ( PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( FSingleAnimationPlayData, AnimToPlay ))
		{
			// make sure the animation skeleton matches the current skeletalmesh
			if (AnimationData.AnimToPlay && SkelMesh && SkelMesh->GetSkeleton() && SkelMesh->GetSkeleton()->IsCompatible(AnimationData.AnimToPlay->GetSkeleton()))
			{
				PlayAnimation(AnimationData.AnimToPlay, false);
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("Invalid animation"));
				AnimationData.AnimToPlay = nullptr;
			}
		}

		if ( PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( FSingleAnimationPlayData, SavedPosition ))
		{
			AnimationData.ValidatePosition();
			SetPosition(AnimationData.SavedPosition, false);
		}

		if ( PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( USkeletalMeshComponent, TeleportDistanceThreshold ) )
		{
			ComputeTeleportDistanceThresholdInRadians();
		}

		if ( PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED( USkeletalMeshComponent, TeleportRotationThreshold ) )
		{
			ComputeTeleportRotationThresholdInRadians();
		}
	}
}

void USkeletalMeshComponent::LoadedFromAnotherClass(const FName& OldClassName)
{
	Super::LoadedFromAnotherClass(OldClassName);

	if(GetLinkerUEVersion() < VER_UE4_REMOVE_SINGLENODEINSTANCE)
	{
		static FName SingleAnimSkeletalComponent_NAME(TEXT("SingleAnimSkeletalComponent"));

		if(OldClassName == SingleAnimSkeletalComponent_NAME)
		{
			SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);

			// support old compatibility code that changed variable name
			if (SequenceToPlay_DEPRECATED!=nullptr && AnimToPlay_DEPRECATED== nullptr)
			{
				AnimToPlay_DEPRECATED = SequenceToPlay_DEPRECATED;
				SequenceToPlay_DEPRECATED = nullptr;
			}

			AnimationData.AnimToPlay = AnimToPlay_DEPRECATED;
			AnimationData.bSavedLooping = bDefaultLooping_DEPRECATED;
			AnimationData.bSavedPlaying = bDefaultPlaying_DEPRECATED;
			AnimationData.SavedPosition = DefaultPosition_DEPRECATED;
			AnimationData.SavedPlayRate = DefaultPlayRate_DEPRECATED;

			MarkPackageDirty();
		}
	}
}

TSoftObjectPtr<UObject> USkeletalMeshComponent::GetDefaultAnimatingRig() const
{
	if (bOverrideDefaultAnimatingRig)
	{
		return DefaultAnimatingRigOverride;
	}
	if (GetSkeletalMeshAsset())
	{
		return GetSkeletalMeshAsset()->GetDefaultAnimatingRig();
	}
	return nullptr;
}

void USkeletalMeshComponent::SetDefaultAnimatingRigOverride(TSoftObjectPtr<UObject> InAnimatingRig) 
{
	DefaultAnimatingRigOverride = InAnimatingRig;
}

TSoftObjectPtr<UObject> USkeletalMeshComponent::GetDefaultAnimatingRigOverride() const
{
	return DefaultAnimatingRigOverride;
}

#endif // WITH_EDITOR




bool USkeletalMeshComponent::ShouldOnlyTickMontages(const float DeltaTime) const
{
	// Ignore DeltaSeconds == 0.f, as that is used when we want to force an update followed by RefreshBoneTransforms.
	// RefreshBoneTransforms will need an updated graph.
	return (VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered)
		&& !bRecentlyRendered 
		&& (DeltaTime > 0.f);
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime, bool bNeedsValidRootMotion)
{
	SCOPED_NAMED_EVENT(USkeletalMeshComponent_TickAnimation, FColor::Yellow);
	SCOPE_CYCLE_COUNTER(STAT_AnimGameThreadTime);
	SCOPE_CYCLE_COUNTER(STAT_AnimTickTime);

	// if curves have to be refreshed before updating animation
	if (!AreRequiredCurvesUpToDate())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_RecalcRequiredCurves);
		RecalcRequiredCurves();
	}

	bool bIsCompiling = false;
#if WITH_EDITOR
	bIsCompiling = GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->IsCompiling();
#endif
	if (GetSkeletalMeshAsset() != nullptr && !bIsCompiling)
	{
		// We're about to UpdateAnimation, this will potentially queue events that we'll need to dispatch.
		bNeedsQueuedAnimEventsDispatched = true;

		// Tick all of our anim instances
		TickAnimInstances(DeltaTime, bNeedsValidRootMotion);

		/**
			If we're called directly for autonomous proxies, TickComponent is not guaranteed to get called.
			So dispatch all queued events here if we're doing MontageOnly ticking.
		*/
		if (ShouldOnlyTickMontages(DeltaTime))
		{
			ConditionallyDispatchQueuedAnimEvents();
		}
	}
}

void USkeletalMeshComponent::SetPredictedLODLevel(int32 InPredictedLODLevel)
{
	int32 OldPredictedLODLevel = GetPredictedLODLevel();
	
	Super::SetPredictedLODLevel(InPredictedLODLevel);

	if(OldPredictedLODLevel != GetPredictedLODLevel())
	{
		bRequiredBonesUpToDate = false;
	}
} 

void USkeletalMeshComponent::TickAnimInstances(float DeltaTime, bool bNeedsValidRootMotion)
{
	// Allow animation instance to do some processing before the linked instances update
	if (AnimScriptInstance != nullptr)
	{
		AnimScriptInstance->PreUpdateLinkedInstances(DeltaTime);
	}

	// We update linked instances first incase we're using either root motion or non-threaded update.
	// This ensures that we go through the pre update process and initialize the proxies correctly.
	for (UAnimInstance* LinkedInstance : LinkedInstances)
	{
		// Sub anim instances are always forced to do a parallel update 
		LinkedInstance->UpdateAnimation(DeltaTime * GlobalAnimRateScale, false, UAnimInstance::EUpdateAnimationFlag::ForceParallelUpdate);
	}

	if (AnimScriptInstance != nullptr)
	{
		// Tick the animation
		AnimScriptInstance->UpdateAnimation(DeltaTime * GlobalAnimRateScale, bNeedsValidRootMotion);
	}

	if(ShouldUpdatePostProcessInstance())
	{
		PostProcessAnimInstance->UpdateAnimation(DeltaTime * GlobalAnimRateScale, false);
	}
}

bool USkeletalMeshComponent::UpdateLODStatus()
{
	if (Super::UpdateLODStatus())
	{
		bRequiredBonesUpToDate = false;
		return true;
	}

	return false;
}

void USkeletalMeshComponent::UpdateVisualizeLODString(FString& DebugString)
{
	Super::UpdateVisualizeLODString(DebugString);

	uint32 NumVertices = 0;
	if (GetSkeletalMeshAsset())
	{
		if (FSkeletalMeshRenderData* RenderData = GetSkeletalMeshAsset()->GetResourceForRendering())
		{
			if (RenderData->LODRenderData.IsValidIndex(GetPredictedLODLevel()))
			{
				NumVertices = RenderData->LODRenderData[GetPredictedLODLevel()].GetNumVertices();
			}
		}
	}

	DebugString = DebugString + FString::Printf(TEXT("\nRequiredBones(%d) NumVerts(%d)"), 
		RequiredBones.Num(), NumVertices);
}

bool USkeletalMeshComponent::ShouldUpdateTransform(bool bLODHasChanged) const
{
#if WITH_EDITOR	
	// If we're in an editor world (Non running, WorldType will be PIE when simulating or in PIE) then we only want transform updates on LOD changes as the
	// animation isn't running so it would just waste CPU time
	if(GetWorld()->WorldType == EWorldType::Editor)
	{
		if( bUpdateAnimationInEditor )
		{
			return true;
		}

		// if leader pose is ticking, follower also has to update it
		if (LeaderPoseComponent.IsValid())
		{
			const USkeletalMeshComponent* Leader = CastChecked<USkeletalMeshComponent>(LeaderPoseComponent.Get());
			if (Leader->GetUpdateAnimationInEditor())
			{
				return true;
			}
		}

		return bLODHasChanged;
	}
#endif

	// If forcing RefPose we can skip updating the skeleton for perf, except if it's using MorphTargets.
	const bool bSkipBecauseOfRefPose = bForceRefpose && bOldForceRefPose && (MorphTargetCurves.Num() == 0) && ((AnimScriptInstance) ? !AnimScriptInstance->HasMorphTargetCurves() : true);

	return (Super::ShouldUpdateTransform(bLODHasChanged) && !bNoSkeletonUpdate && !bSkipBecauseOfRefPose);
}

bool USkeletalMeshComponent::ShouldTickPose() const
{
	if (LeaderPoseComponent.IsValid() && !bFollowerShouldTickPose)
	{
		return false;
	}

	// When we stop root motion we go back to ticking after CharacterMovement. Unfortunately that means that we could tick twice that frame.
	// So only enforce a single tick per frame.
	const bool bAlreadyTickedThisFrame = PoseTickedThisFrame();

#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		if (bUpdateAnimationInEditor)
		{
			return true;
		}
	}
#endif 

	// Autonomous Ticking is allowed to occur multiple times per frame, as we can receive and process multiple networking updates the same frame.
	const bool bShouldTickBasedOnAutonomousCheck = bIsAutonomousTickPose || (!bOnlyAllowAutonomousTickPose && !bAlreadyTickedThisFrame);
	// When playing networked Root Motion Montages, we want these to play on dedicated servers and remote clients for networking and position correction purposes.
	// So we force pose updates in that case to keep root motion and position in sync.
	const bool bShouldTickBasedOnVisibility = ((VisibilityBasedAnimTickOption < EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered) || bRecentlyRendered || IsPlayingNetworkedRootMotionMontage());

	return (bShouldTickBasedOnVisibility && bShouldTickBasedOnAutonomousCheck && IsRegistered() && (AnimScriptInstance || PostProcessAnimInstance) && !bPauseAnims && GetWorld()->AreActorsInitialized() && !bNoSkeletonUpdate);
}

bool USkeletalMeshComponent::ShouldTickAnimation() const
{
	if(bExternalTickRateControlled)
	{
		return bExternalUpdate;
	}
	else
	{
		return (AnimUpdateRateParams != nullptr) && (!ShouldUseUpdateRateOptimizations() || !AnimUpdateRateParams->ShouldSkipUpdate());
	}
}

static FThreadSafeCounter Ticked;
static FThreadSafeCounter NotTicked;

static TAutoConsoleVariable<int32> CVarSpewAnimRateOptimization(
	TEXT("SpewAnimRateOptimization"),
	0,
	TEXT("True to spew overall anim rate optimization tick rates."));

void USkeletalMeshComponent::TickPose(float DeltaTime, bool bNeedsValidRootMotion)
{
	Super::TickPose(DeltaTime, bNeedsValidRootMotion);

	if (ShouldTickAnimation())
	{
		// Don't care about roll over, just care about uniqueness (and 32-bits should give plenty).
		LastPoseTickFrame = static_cast<uint32>(GFrameCounter);

		float DeltaTimeForTick;
		if(bExternalTickRateControlled)
		{
			DeltaTimeForTick = ExternalDeltaTime;
		}
		else if(ShouldUseUpdateRateOptimizations())
		{
			DeltaTimeForTick = DeltaTime + AnimUpdateRateParams->GetTimeAdjustment();
		}
		else
		{
			DeltaTimeForTick = DeltaTime;
		}

		TickAnimation(DeltaTimeForTick, bNeedsValidRootMotion);
		if (CVarSpewAnimRateOptimization.GetValueOnGameThread() > 0 && Ticked.Increment()==500)
		{
			UE_LOG(LogTemp, Display, TEXT("%d Ticked %d NotTicked"), Ticked.GetValue(), NotTicked.GetValue());
			Ticked.Reset();
			NotTicked.Reset();
		}
	}
	else if(!bExternalTickRateControlled)
	{
		if (AnimScriptInstance)
		{
			AnimScriptInstance->OnUROSkipTickAnimation();
		}

		for(UAnimInstance* LinkedInstance : LinkedInstances)
		{
			LinkedInstance->OnUROSkipTickAnimation();
		}

		if(PostProcessAnimInstance)
		{
			PostProcessAnimInstance->OnUROSkipTickAnimation();
		}

		if (CVarSpewAnimRateOptimization.GetValueOnGameThread())
		{
			NotTicked.Increment();
		}
	}
}

void USkeletalMeshComponent::ResetMorphTargetCurves()
{
	ActiveMorphTargets.Reset();

	if (GetSkeletalMeshAsset())
	{
		MorphTargetWeights.SetNum(GetSkeletalMeshAsset()->GetMorphTargets().Num());

		// we need this code to ensure the buffer gets cleared whether or not you have morphtarget curve set
		// the case, where you had morphtargets weight on, and when you clear the weight, you want to make sure 
		// the buffer gets cleared and resized
		if (MorphTargetWeights.Num() > 0)
		{
			FMemory::Memzero(MorphTargetWeights.GetData(), MorphTargetWeights.GetAllocatedSize());
		}
	}
	else
	{
		MorphTargetWeights.Reset();
	}
}

void USkeletalMeshComponent::UpdateMorphTargetOverrideCurves()
{
	if (GetSkeletalMeshAsset())
	{
		if (MorphTargetCurves.Num() > 0)
		{
			FAnimationRuntime::AppendActiveMorphTargets(GetSkeletalMeshAsset(), MorphTargetCurves, ActiveMorphTargets, MorphTargetWeights);
		}
	}
}

static TAutoConsoleVariable<int32> CVarAnimationDelaysEndGroup(
	TEXT("tick.AnimationDelaysEndGroup"),
	1,
	TEXT("If > 0, then skeletal meshes that do not rely on physics simulation will set their animation end tick group to TG_PostPhysics."));
static TAutoConsoleVariable<int32> CVarHiPriSkinnedMeshesTicks(
	TEXT("tick.HiPriSkinnedMeshes"),
	1,
	TEXT("If > 0, then schedule the skinned component ticks in a tick group before other ticks."));


void USkeletalMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Animation);

#if WITH_EDITOR
	//Do not tick skeletal mesh component if the skeletal mesh asset is compiling
	if (GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->IsCompiling())
	{
		return;
	}
#endif

	if (ClothingSimulation)
	{
		ClothingSimulation->UpdateWorldForces(this);
	}

	UpdateEndPhysicsTickRegisteredState();
	UpdateClothTickRegisteredState();

	// If we are suspended, we will not simulate clothing, but as clothing is simulated in local space
	// relative to a root bone we need to extract simulation positions as this bone could be animated.
	if((!CVarEnableClothPhysics.GetValueOnGameThread() || bClothingSimulationSuspended) && ClothingSimulation)
	{
		CSV_SCOPED_TIMING_STAT(Animation, Cloth);

		// First update the simulation context, since the simulation isn't ticking
		// and it is still required to get the correct simulation data and bounds.
		constexpr bool bIsInitialization = false;
		ClothingSimulation->FillContext(this, DeltaTime, ClothingSimulationContext, bIsInitialization);

		ClothingSimulation->GetSimulationData(CurrentSimulationData, this, Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get()));
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	PendingRadialForces.Reset();

	// Update bOldForceRefPose
	bOldForceRefPose = bForceRefpose;

	/** Update the end group and tick priority */
	const bool bDoLateEnd = CVarAnimationDelaysEndGroup.GetValueOnGameThread() > 0;
	const bool bRequiresPhysics = EndPhysicsTickFunction.IsTickFunctionRegistered();
	const ETickingGroup EndTickGroup = bDoLateEnd && !bRequiresPhysics ? TG_PostPhysics : TG_PrePhysics;
	if (ThisTickFunction)
	{
		ThisTickFunction->EndTickGroup = EndTickGroup;

		// Note that if animation is so long that we are blocked in EndPhysics we may want to reduce the priority. However, there is a risk that this function will not go wide early enough.
		// This requires profiling and is very game dependent so cvar for now makes sense
		bool bDoHiPri = CVarHiPriSkinnedMeshesTicks.GetValueOnGameThread() > 0;
		if (ThisTickFunction->bHighPriority != bDoHiPri)
		{
			ThisTickFunction->SetPriorityIncludingPrerequisites(bDoHiPri);
		}
	}

	// If we are waiting for ParallelEval to complete or if we require Physics, 
	// then FinalizeBoneTransform will be called and Anim events will be dispatched there. 
	// We prefer doing it there so these events are triggered once we have a new updated pose.
	// Note that it's possible that FinalizeBoneTransform has already been called here if not using ParallelUpdate.
	// or it's possible that it hasn't been called at all if we're skipping Evaluate due to not being visible.
	// ConditionallyDispatchQueuedAnimEvents will catch that and only Dispatch events if not already done.
	if (!IsRunningParallelEvaluation() && !bRequiresPhysics)
	{
		/////////////////////////////////////////////////////////////////////////////
		// Notify / Event Handling!
		// This can do anything to our component (including destroy it) 
		// Any code added after this point needs to take that into account
		/////////////////////////////////////////////////////////////////////////////

		ConditionallyDispatchQueuedAnimEvents();
	}
}

void USkeletalMeshComponent::ConditionallyDispatchQueuedAnimEvents()
{
	if (bNeedsQueuedAnimEventsDispatched)
	{
		bNeedsQueuedAnimEventsDispatched = false;

		// Copy linked instances here, as anim notifies could potentially modify this array
		for (UAnimInstance* LinkedInstance : TArray<TObjectPtr<UAnimInstance>, TInlineAllocator<8>>(LinkedInstances))
		{
			LinkedInstance->DispatchQueuedAnimEvents();
		}

		if (AnimScriptInstance)
		{
			AnimScriptInstance->DispatchQueuedAnimEvents();
		}

		if (PostProcessAnimInstance)
		{
			PostProcessAnimInstance->DispatchQueuedAnimEvents();
		}
	}
}

/** 
 *	Utility for taking two arrays of bone indices, which must be strictly increasing, and finding the intersection between them.
 *	That is - any item in the output should be present in both A and B. Output is strictly increasing as well.
 */
static void IntersectBoneIndexArrays(TArray<FBoneIndexType>& Output, const TArray<FBoneIndexType>& A, const TArray<FBoneIndexType>& B)
{
	int32 APos = 0;
	int32 BPos = 0;
	while(	APos < A.Num() && BPos < B.Num() )
	{
		// If value at APos is lower, increment APos.
		if( A[APos] < B[BPos] )
		{
			APos++;
		}
		// If value at BPos is lower, increment APos.
		else if( B[BPos] < A[APos] )
		{
			BPos++;
		}
		// If they are the same, put value into output, and increment both.
		else
		{
			Output.Add( A[APos] );
			APos++;
			BPos++;
		}
	}
}

/** Takes sorted array Base and then adds any elements from sorted array Insert which is missing from it, preserving order.
 * this assumes both arrays are sorted and contain unique bone indices. */
static void MergeInBoneIndexArrays(TArray<FBoneIndexType>& BaseArray, const TArray<FBoneIndexType>& InsertArray)
{
	// Then we merge them into the array of required bones.
	int32 BaseBonePos = 0;
	int32 InsertBonePos = 0;

	// Iterate over each of the bones we need.
	while( InsertBonePos < InsertArray.Num() )
	{
		// Find index of physics bone
		FBoneIndexType InsertBoneIndex = InsertArray[InsertBonePos];

		// If at end of BaseArray array - just append.
		if( BaseBonePos == BaseArray.Num() )
		{
			BaseArray.Add(InsertBoneIndex);
			BaseBonePos++;
			InsertBonePos++;
		}
		// If in the middle of BaseArray, merge together.
		else
		{
			// Check that the BaseArray array is strictly increasing, otherwise merge code does not work.
			check( BaseBonePos == 0 || BaseArray[BaseBonePos-1] < BaseArray[BaseBonePos] );

			// Get next required bone index.
			FBoneIndexType BaseBoneIndex = BaseArray[BaseBonePos];

			// We have a bone in BaseArray not required by Insert. Thats ok - skip.
			if( BaseBoneIndex < InsertBoneIndex )
			{
				BaseBonePos++;
			}
			// Bone required by Insert is in 
			else if( BaseBoneIndex == InsertBoneIndex )
			{
				BaseBonePos++;
				InsertBonePos++;
			}
			// Bone required by Insert is missing - insert it now.
			else // BaseBoneIndex > InsertBoneIndex
			{
				BaseArray.InsertUninitialized(BaseBonePos);
				BaseArray[BaseBonePos] = InsertBoneIndex;

				BaseBonePos++;
				InsertBonePos++;
			}
		}
	}
}

// this is optimized version of updating only curves
// if you call RecalcRequiredBones, curve should be refreshed
void USkeletalMeshComponent::RecalcRequiredCurves()
{
	if (!GetSkeletalMeshAsset())
	{
		return;
	}

	if (GetSkeletalMeshAsset()->GetSkeleton())
	{
		CachedCurveUIDList = GetSkeletalMeshAsset()->GetSkeleton()->GetDefaultCurveUIDList();
	}

	const FCurveEvaluationOption CurveEvalOption(bAllowAnimCurveEvaluation, &DisallowedAnimCurves, GetPredictedLODLevel());

	// make sure animation requiredcurve to mark as dirty
	if (AnimScriptInstance)
	{
		AnimScriptInstance->RecalcRequiredCurves(CurveEvalOption);
	}

	for(UAnimInstance* LinkedInstance : LinkedInstances)
	{
		LinkedInstance->RecalcRequiredCurves(CurveEvalOption);
	}

	if(PostProcessAnimInstance)
	{
		PostProcessAnimInstance->RecalcRequiredCurves(CurveEvalOption);
	}

	MarkRequiredCurveUpToDate();
}

void USkeletalMeshComponent::ComputeRequiredBones(TArray<FBoneIndexType>& OutRequiredBones, TArray<FBoneIndexType>& OutFillComponentSpaceTransformsRequiredBones, int32 LODIndex, bool bIgnorePhysicsAsset) const
{
	OutRequiredBones.Reset();
	OutFillComponentSpaceTransformsRequiredBones.Reset();

	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	if (!SkelMesh)
	{
		return;
	}

	FSkeletalMeshRenderData* SkelMeshRenderData = GetSkeletalMeshRenderData();
	if (!SkelMeshRenderData)
	{
		//No Render Data?
		// Jira UE-64409
		UE_LOG(LogAnimation, Warning, TEXT("Skeletal Mesh asset '%s' has no render data"), *SkelMesh->GetName());
		return;
	}

	// Make sure we access a valid LOD
	// @fixme jira UE-30028 Avoid crash when called with partially loaded asset
	if (SkelMeshRenderData->LODRenderData.Num() == 0)
	{
		//No LODS?
		UE_LOG(LogAnimation, Warning, TEXT("Skeletal Mesh asset '%s' has no LODs"), *SkelMesh->GetName());
		return;
	}

	LODIndex = FMath::Clamp(LODIndex, 0, SkelMeshRenderData->LODRenderData.Num() - 1);

	// The list of bones we want is taken from the predicted LOD level.
	FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[LODIndex];
	OutRequiredBones = LODData.RequiredBones;

	// Add virtual bones
	MergeInBoneIndexArrays(OutRequiredBones, SkelMesh->GetRefSkeleton().GetRequiredVirtualBones());

	const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	// If we have a PhysicsAsset, we also need to make sure that all the bones used by it are always updated, as its used
	// by line checks etc. We might also want to kick in the physics, which means having valid bone transforms.
	if (!bIgnorePhysicsAsset && PhysicsAsset)
	{
		TArray<FBoneIndexType> PhysAssetBones;
		PhysAssetBones.Reserve(PhysicsAsset->SkeletalBodySetups.Num());
		for (int32 i = 0; i<PhysicsAsset->SkeletalBodySetups.Num(); i++)
		{
			if (!ensure(PhysicsAsset->SkeletalBodySetups[i]))
			{
				continue;
			}
			int32 PhysBoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(PhysicsAsset->SkeletalBodySetups[i]->BoneName);
			if (PhysBoneIndex != INDEX_NONE)
			{
				PhysAssetBones.Add(PhysBoneIndex);
			}
		}

		// Then sort array of required bones in hierarchy order
		PhysAssetBones.Sort();

		// Make sure all of these are in RequiredBones.
		MergeInBoneIndexArrays(OutRequiredBones, PhysAssetBones);
	}

	// TODO - Make sure that bones with per-poly collision are also always updated.

	// Purge invisible bones and their children
	// this has to be done before mirror table check/physics body checks
	// mirror table/phys body ones has to be calculated
	if (ShouldUpdateBoneVisibility())
	{
		const TArray<uint8>& EditableBoneVisibilityStates = GetEditableBoneVisibilityStates();
		check(EditableBoneVisibilityStates.Num() == GetNumComponentSpaceTransforms());
		
		if (ensureMsgf(EditableBoneVisibilityStates.Num() >= OutRequiredBones.Num(), 
			TEXT("Skeletal Mesh asset '%s' has incorrect BoneVisibilityStates. # of BoneVisibilityStatese (%d), # of OutRequiredBones (%d)"), 
			*SkelMesh->GetName(), EditableBoneVisibilityStates.Num(), OutRequiredBones.Num()))
		{
			int32 VisibleBoneWriteIndex = 0;
			for (int32 i = 0; i < OutRequiredBones.Num(); ++i)
			{
				FBoneIndexType CurBoneIndex = OutRequiredBones[i];

				// Current bone visible?
				if (EditableBoneVisibilityStates[CurBoneIndex] == BVS_Visible || 
					VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
				{
					OutRequiredBones[VisibleBoneWriteIndex++] = CurBoneIndex;
				}
			}

			// Remove any trailing junk in the OutRequiredBones array
			const int32 NumBonesHidden = OutRequiredBones.Num() - VisibleBoneWriteIndex;
			if (NumBonesHidden > 0)
			{
				OutRequiredBones.RemoveAt(VisibleBoneWriteIndex, NumBonesHidden);
			}
		}
	}

	// Add in any bones that may be required when mirroring.
	// JTODO: This is only required if there are mirroring nodes in the tree, but hard to know...
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (SkelMesh->SkelMirrorTable.Num() > 0 &&
		SkelMesh->SkelMirrorTable.Num() == BoneSpaceTransforms.Num())
	{
		TArray<FBoneIndexType> MirroredDesiredBones;
		MirroredDesiredBones.AddUninitialized(RequiredBones.Num());

		const TArray<struct FBoneMirrorInfo>& SkelMirrorTable = SkelMesh->GetSkelMirrorTable();
		// Look up each bone in the mirroring table.
		for (int32 i = 0; i<OutRequiredBones.Num(); i++)
		{
			MirroredDesiredBones[i] = SkelMirrorTable[OutRequiredBones[i]].SourceIndex;
		}

		// Sort to ensure strictly increasing order.
		MirroredDesiredBones.Sort();

		// Make sure all of these are in OutRequiredBones, and 
		MergeInBoneIndexArrays(OutRequiredBones, MirroredDesiredBones);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	TArray<FBoneIndexType> NeededBonesForFillComponentSpaceTransforms;
	{
		TArray<FBoneIndexType> ForceAnimatedSocketBones;
		TArray<USkeletalMeshSocket*> ActiveSocketList = SkelMesh->GetActiveSocketList();
		ForceAnimatedSocketBones.Reserve(ActiveSocketList.Num());
		for (const USkeletalMeshSocket* Socket : ActiveSocketList)
		{
			int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(Socket->BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				if (Socket->bForceAlwaysAnimated)
				{
					ForceAnimatedSocketBones.AddUnique(BoneIndex);
				}
				else
				{
					NeededBonesForFillComponentSpaceTransforms.AddUnique(BoneIndex);
				}
			}
		}

		// Then sort array of required bones in hierarchy order
		ForceAnimatedSocketBones.Sort();

		// Make sure all of these are in OutRequiredBones.
		MergeInBoneIndexArrays(OutRequiredBones, ForceAnimatedSocketBones);
	}

	// Gather any bones referenced by shadow shapes
	if (FSkeletalMeshSceneProxy* SkeletalMeshProxy = (FSkeletalMeshSceneProxy*)SceneProxy)
	{
		const TArray<FBoneIndexType>& ShadowShapeBones = SkeletalMeshProxy->GetSortedShadowBoneIndices();

		if (ShadowShapeBones.Num())
		{
			// Sort in hierarchy order then merge to required bones array
			MergeInBoneIndexArrays(OutRequiredBones, ShadowShapeBones);
		}
	}

	// Ensure that we have a complete hierarchy down to those bones.
	FAnimationRuntime::EnsureParentsPresent(OutRequiredBones, SkelMesh->GetRefSkeleton());

	OutFillComponentSpaceTransformsRequiredBones.Reset(OutRequiredBones.Num() + NeededBonesForFillComponentSpaceTransforms.Num());
	OutFillComponentSpaceTransformsRequiredBones = OutRequiredBones;

	NeededBonesForFillComponentSpaceTransforms.Sort();
	MergeInBoneIndexArrays(OutFillComponentSpaceTransformsRequiredBones, NeededBonesForFillComponentSpaceTransforms);
	FAnimationRuntime::EnsureParentsPresent(OutFillComponentSpaceTransformsRequiredBones, SkelMesh->GetRefSkeleton());
}

void USkeletalMeshComponent::RecalcRequiredBones(int32 LODIndex)
{
	if (!GetSkeletalMeshAsset())
	{
		return;
	}

	ComputeRequiredBones(RequiredBones, FillComponentSpaceTransformsRequiredBones, LODIndex, /*bIgnorePhysicsAsset=*/ false);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	BoneSpaceTransforms = GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// make sure animation requiredBone to mark as dirty
	if (AnimScriptInstance)
	{
		AnimScriptInstance->RecalcRequiredBones();
	}

	for (UAnimInstance* LinkedInstance : LinkedInstances)
	{
		LinkedInstance->RecalcRequiredBones();
	}

	if (PostProcessAnimInstance)
	{
		PostProcessAnimInstance->RecalcRequiredBones();
	}

	// when recalc requiredbones happend
	// this should always happen
	MarkRequiredCurveUpToDate();
	bRequiredBonesUpToDate = true;

	// Invalidate cached bones.
	ClearCachedAnimProperties();
}

void USkeletalMeshComponent::MarkRequiredCurveUpToDate()
{
	if (GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->GetSkeleton())
	{
		CachedAnimCurveUidVersion = GetSkeletalMeshAsset()->GetSkeleton()->GetAnimCurveUidVersion();
	}
}

bool USkeletalMeshComponent::AreRequiredCurvesUpToDate() const
{
	return (!GetSkeletalMeshAsset() || !GetSkeletalMeshAsset()->GetSkeleton() || CachedAnimCurveUidVersion == GetSkeletalMeshAsset()->GetSkeleton()->GetAnimCurveUidVersion());
}

void USkeletalMeshComponent::EvaluateAnimation(const USkeletalMesh* InSkeletalMesh, UAnimInstance* InAnimInstance, FVector& OutRootBoneTranslation, FBlendedHeapCurve& OutCurve, FCompactPose& OutPose, UE::Anim::FHeapAttributeContainer& OutAttributes) const
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(SkeletalComponentAnimEvaluate, !IsInGameThread());

	if( !InSkeletalMesh )
	{
		return;
	}

	// We can only evaluate animation if RequiredBones is properly setup for the right mesh!
	if( InSkeletalMesh->GetSkeleton() && 
		InAnimInstance &&
		InAnimInstance->ParallelCanEvaluate(InSkeletalMesh))
	{
		FParallelEvaluationData EvaluationData = { OutCurve, OutPose, OutAttributes };
		InAnimInstance->ParallelEvaluateAnimation(bForceRefpose, InSkeletalMesh, EvaluationData);
	}
	else
	{
		OutCurve.InitFrom(&CachedCurveUIDList);
	}
}

void USkeletalMeshComponent::UpdateFollowerComponent()
{
	check (LeaderPoseComponent.IsValid());

	ResetMorphTargetCurves();

	if (USkeletalMeshComponent* LeaderSMC = Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get()))
	{
		// first set any animation-driven curves from the leader SMC
		if (LeaderSMC->AnimScriptInstance)
		{
			LeaderSMC->AnimScriptInstance->RefreshCurves(this);
		}

		// we changed order of morphtarget to be overriden by SetMorphTarget from BP
		// so this has to go first
		// now propagate BP-driven curves from the leader SMC...
		if (GetSkeletalMeshAsset())
		{
			check(MorphTargetWeights.Num() == GetSkeletalMeshAsset()->GetMorphTargets().Num());
			if (LeaderSMC->MorphTargetCurves.Num() > 0)
			{
				FAnimationRuntime::AppendActiveMorphTargets(GetSkeletalMeshAsset(), LeaderSMC->MorphTargetCurves, ActiveMorphTargets, MorphTargetWeights);
			}

			// if follower also has it, add it here. 
			if (MorphTargetCurves.Num() > 0)
			{
				FAnimationRuntime::AppendActiveMorphTargets(GetSkeletalMeshAsset(), MorphTargetCurves, ActiveMorphTargets, MorphTargetWeights);
			}
		}

	}
 
	Super::UpdateFollowerComponent();
}

#if WITH_EDITOR

void USkeletalMeshComponent::PerformAnimationEvaluation(const USkeletalMesh* InSkeletalMesh, UAnimInstance* InAnimInstance, TArray<FTransform>& OutSpaceBases, TArray<FTransform>& OutBoneSpaceTransforms, FVector& OutRootBoneTranslation, FBlendedHeapCurve& OutCurve, UE::Anim::FMeshAttributeContainer& OutAttributes)
{
	PerformAnimationProcessing(InSkeletalMesh, InAnimInstance, true, OutSpaceBases, OutBoneSpaceTransforms, OutRootBoneTranslation, OutCurve, OutAttributes);
}

void USkeletalMeshComponent::PerformAnimationEvaluation(const USkeletalMesh* InSkeletalMesh, UAnimInstance* InAnimInstance, TArray<FTransform>& OutSpaceBases, TArray<FTransform>& OutBoneSpaceTransforms, FVector& OutRootBoneTranslation, FBlendedHeapCurve& OutCurve)
{
	UE::Anim::FMeshAttributeContainer Attributes;
	PerformAnimationEvaluation(InSkeletalMesh, InAnimInstance, OutSpaceBases, OutBoneSpaceTransforms, OutRootBoneTranslation, OutCurve, Attributes);
}

#endif

void USkeletalMeshComponent::PerformAnimationProcessing(const USkeletalMesh* InSkeletalMesh, UAnimInstance* InAnimInstance, bool bInDoEvaluation, TArray<FTransform>& OutSpaceBases, TArray<FTransform>& OutBoneSpaceTransforms, FVector& OutRootBoneTranslation, FBlendedHeapCurve& OutCurve, UE::Anim::FMeshAttributeContainer& OutAttributes)
{
	CSV_SCOPED_TIMING_STAT(Animation, WorkerThreadTickTime);
	ANIM_MT_SCOPE_CYCLE_COUNTER(PerformAnimEvaluation, !IsInGameThread());

	// Can't do anything without a SkeletalMesh
	if (!InSkeletalMesh)
	{
		return;
	}

	// update anim instance
	if(InAnimInstance && InAnimInstance->NeedsUpdate())
	{
		InAnimInstance->ParallelUpdateAnimation();
	}
	
	if(ShouldPostUpdatePostProcessInstance())
	{
		// If we don't have an anim instance, we may still have a post physics instance
		PostProcessAnimInstance->ParallelUpdateAnimation();
	}

	// Do nothing more if no bones in skeleton.
	if(bInDoEvaluation && OutSpaceBases.Num() > 0)
	{
		FMemMark Mark(FMemStack::Get());
		FCompactPose EvaluatedPose;

		UE::Anim::FHeapAttributeContainer Attributes;		

		// evaluate pure animations, and fill up BoneSpaceTransforms
		EvaluateAnimation(InSkeletalMesh, InAnimInstance, OutRootBoneTranslation, OutCurve, EvaluatedPose, Attributes);
		EvaluatePostProcessMeshInstance(OutBoneSpaceTransforms, EvaluatedPose, OutCurve, InSkeletalMesh, OutRootBoneTranslation, Attributes);

		// Finalize the transforms from the evaluation
		FinalizePoseEvaluationResult(InSkeletalMesh, OutBoneSpaceTransforms, OutRootBoneTranslation, EvaluatedPose);

		if (EvaluatedPose.IsValid())
		{
			FinalizeAttributeEvaluationResults(EvaluatedPose.GetBoneContainer(), Attributes, OutAttributes);
		}

		// Fill SpaceBases from LocalAtoms
		InSkeletalMesh->FillComponentSpaceTransforms(OutBoneSpaceTransforms, FillComponentSpaceTransformsRequiredBones, OutSpaceBases);
	}
}


void USkeletalMeshComponent::PerformAnimationProcessing(const USkeletalMesh* InSkeletalMesh, UAnimInstance* InAnimInstance, bool bInDoEvaluation, TArray<FTransform>& OutSpaceBases, TArray<FTransform>& OutBoneSpaceTransforms, FVector& OutRootBoneTranslation, FBlendedHeapCurve& OutCurve)
{
	UE::Anim::FMeshAttributeContainer Attributes;	
	PerformAnimationProcessing(InSkeletalMesh, InAnimInstance, bInDoEvaluation, OutSpaceBases, OutBoneSpaceTransforms, OutRootBoneTranslation, OutCurve, Attributes);
}

void USkeletalMeshComponent::EvaluatePostProcessMeshInstance(TArray<FTransform>& OutBoneSpaceTransforms, FCompactPose& InOutPose, FBlendedHeapCurve& OutCurve, const USkeletalMesh* InSkeletalMesh, FVector& OutRootBoneTranslation) const
{
	UE::Anim::FHeapAttributeContainer Attributes;
	EvaluatePostProcessMeshInstance(OutBoneSpaceTransforms, InOutPose, OutCurve, InSkeletalMesh, OutRootBoneTranslation, Attributes);
}

void USkeletalMeshComponent::EvaluatePostProcessMeshInstance(TArray<FTransform>& OutBoneSpaceTransforms, FCompactPose& InOutPose, FBlendedHeapCurve& OutCurve, const USkeletalMesh* InSkeletalMesh, FVector& OutRootBoneTranslation, UE::Anim::FHeapAttributeContainer& OutAttributes) const
{
	if (ShouldEvaluatePostProcessInstance())
	{
		// Push the previous pose to any input nodes required
		if (FAnimNode_LinkedInputPose* InputNode = PostProcessAnimInstance->GetLinkedInputPoseNode())
		{
			if (InOutPose.IsValid())
			{
				InputNode->CachedInputPose.CopyBonesFrom(InOutPose);
				InputNode->CachedInputCurve.CopyFrom(OutCurve);
				InputNode->CachedAttributes.CopyFrom(OutAttributes);
			}
			else
			{
				const FBoneContainer& RequiredBone = PostProcessAnimInstance->GetRequiredBonesOnAnyThread();
				InputNode->CachedInputPose.ResetToRefPose(RequiredBone);
				InputNode->CachedInputCurve.InitFrom(RequiredBone);
			}
		}

		EvaluateAnimation(InSkeletalMesh, PostProcessAnimInstance, OutRootBoneTranslation, OutCurve, InOutPose, OutAttributes);
	}
}

const IClothingSimulation* USkeletalMeshComponent::GetClothingSimulation() const
{
	return ClothingSimulation;
}

IClothingSimulation* USkeletalMeshComponent::GetClothingSimulation()
{
	return ClothingSimulation;
}

const IClothingSimulationContext* USkeletalMeshComponent::GetClothingSimulationContext() const
{
	return ClothingSimulationContext;
}

IClothingSimulationContext* USkeletalMeshComponent::GetClothingSimulationContext()
{
	check(IsInGameThread());
	return ClothingSimulationContext;
}

UClothingSimulationInteractor* USkeletalMeshComponent::GetClothingSimulationInteractor() const
{
	check(IsInGameThread());
	return ClothingInteractor;
}

void USkeletalMeshComponent::CompleteParallelClothSimulation()
{
	if(IsValidRef(ParallelClothTask))
	{
		// No longer need this task, it has completed
		ParallelClothTask.SafeRelease();

		// Write back to the GT cache
		WritebackClothingSimulationData();
	}
}

void USkeletalMeshComponent::UpdateClothSimulationContext(float InDeltaTime)
{
	//Do the teleport cloth test here on the game thread
	CheckClothTeleport();

	bool bMustUpdateClothTransform = bForceCollisionUpdate;

	if (bPendingClothTransformUpdate)	//it's possible we want to update cloth collision based on a pending transform
	{
		bPendingClothTransformUpdate = false;
		if (PendingTeleportType == ETeleportType::TeleportPhysics)	//If the pending transform came from a teleport, make sure to teleport the cloth in this upcoming simulation
		{
			ClothTeleportMode = (ClothTeleportMode == EClothingTeleportMode::TeleportAndReset) ? ClothTeleportMode : EClothingTeleportMode::Teleport;
		}
		else if (PendingTeleportType == ETeleportType::ResetPhysics)
		{
			ClothTeleportMode = EClothingTeleportMode::TeleportAndReset;
		}
		bMustUpdateClothTransform = true;
	}
	if (bMustUpdateClothTransform)
	{
		UpdateClothTransformImp();
	}

	// Fill the context for the next simulation
	if(ClothingSimulation)
	{
		const bool bIsInitialization = false;
		ClothingSimulation->FillContext(this, InDeltaTime, ClothingSimulationContext, bIsInitialization);

		if(ClothingInteractor)
		{
			ClothingInteractor->Sync(ClothingSimulation, ClothingSimulationContext);
		}
	}

	PendingTeleportType = ETeleportType::None;
	ClothTeleportMode = EClothingTeleportMode::None;
}

void USkeletalMeshComponent::HandleExistingParallelClothSimulation()
{
	if (bBindClothToLeaderComponent)
	{
		if (USkeletalMeshComponent* LeaderComp = Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get()))
		{
			LeaderComp->HandleExistingParallelClothSimulation();
		}
	}

	if(IsValidRef(ParallelClothTask))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_EndParallelClothTask);
		CSV_SCOPED_SET_WAIT_STAT(Cloth);

		// There's a simulation in flight
		check(IsInGameThread());
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(ParallelClothTask, ENamedThreads::GameThread);
		CompleteParallelClothSimulation();
	}
}

void USkeletalMeshComponent::WritebackClothingSimulationData()
{
	if(ClothingSimulation)
	{
		CSV_SCOPED_TIMING_STAT(Animation, Cloth);

		USkinnedMeshComponent* OverrideComponent = nullptr;
		if(LeaderPoseComponent.IsValid())
		{
			OverrideComponent = LeaderPoseComponent.Get();

			// Check if our bone map is actually valid, if not there is no clothing data to build
			if(LeaderBoneMap.Num() == 0)
			{
				CurrentSimulationData.Reset();
				return;
			}
		}

		ClothingSimulation->GetSimulationData(CurrentSimulationData, this, OverrideComponent);
	}
}

UClothingSimulationFactory* USkeletalMeshComponent::GetClothingSimFactory() const
{
	UClass* SimFactoryClass = *ClothingSimulationFactory;
	if(SimFactoryClass)
	{
		return SimFactoryClass->GetDefaultObject<UClothingSimulationFactory>();
	}

	// No simulation factory set
	return nullptr;
}

void USkeletalMeshComponent::DoInstancePreEvaluation()
{
	if (AnimScriptInstance)
	{
		AnimScriptInstance->PreEvaluateAnimation();

		for (UAnimInstance* LinkedInstance : LinkedInstances)
		{
			LinkedInstance->PreEvaluateAnimation();
		}
	}

	if (ShouldEvaluatePostProcessInstance())
	{
		PostProcessAnimInstance->PreEvaluateAnimation();
	}
}

void USkeletalMeshComponent::DoInstancePostEvaluation()
{
	if (AnimScriptInstance)
	{
		AnimScriptInstance->PostEvaluateAnimation();

		for (UAnimInstance* LinkedInstance : LinkedInstances)
		{
			LinkedInstance->PostEvaluateAnimation();
		}
	}

	if (PostProcessAnimInstance)
	{
		PostProcessAnimInstance->PostEvaluateAnimation();
	}
}

void USkeletalMeshComponent::RefreshBoneTransforms(FActorComponentTickFunction* TickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimGameThreadTime);
	SCOPE_CYCLE_COUNTER(STAT_RefreshBoneTransforms);

	check(IsInGameThread()); //Only want to call this from the game thread as we set up tasks etc
	
	if (!GetSkeletalMeshAsset() || GetNumComponentSpaceTransforms() == 0)
	{
		return;
	}

	// Recalculate the RequiredBones array, if necessary
	if (!bRequiredBonesUpToDate)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_RecalcRequiredBones);
		RecalcRequiredBones(GetPredictedLODLevel());
	}
	// if curves have to be refreshed
	else if (!AreRequiredCurvesUpToDate())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_RecalcRequiredCurves);
		RecalcRequiredCurves();
	}

	const bool bCachedShouldUseUpdateRateOptimizations = ShouldUseUpdateRateOptimizations() && AnimUpdateRateParams != nullptr;
	const bool bDoEvaluationRateOptimization = (bExternalTickRateControlled && bExternalEvaluationRateLimited) || (bCachedShouldUseUpdateRateOptimizations && AnimUpdateRateParams->DoEvaluationRateOptimizations());

	//Handle update rate optimization setup
	//Dont mark cache as invalid if we aren't performing optimization anyway
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const bool bInvalidCachedBones = bDoEvaluationRateOptimization &&
									 ((BoneSpaceTransforms.Num() != GetSkeletalMeshAsset()->GetRefSkeleton().GetNum())
									 || (BoneSpaceTransforms.Num() != CachedBoneSpaceTransforms.Num())
									 || (GetNumComponentSpaceTransforms() != CachedComponentSpaceTransforms.Num()));

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	TArray<uint16> const* CurrentAnimCurveUIDFinder = (AnimScriptInstance) ? &AnimScriptInstance->GetRequiredBones().GetUIDToArrayLookupTable() : 
		((ShouldEvaluatePostProcessInstance() && PostProcessAnimInstance) ? &PostProcessAnimInstance->GetRequiredBones().GetUIDToArrayLookupTable() : nullptr);
	const bool bAnimInstanceHasCurveUIDList = CurrentAnimCurveUIDFinder != nullptr;

	const int32 CurrentCurveCount = (CurrentAnimCurveUIDFinder) ? FBlendedCurve::GetValidElementCount(CurrentAnimCurveUIDFinder) : 0;

	const bool bInvalidCachedCurve = bDoEvaluationRateOptimization && 
									 bAnimInstanceHasCurveUIDList &&
									(CachedCurve.UIDToArrayIndexLUT != CurrentAnimCurveUIDFinder || CachedCurve.Num() != CurrentCurveCount);


	const bool bInvalidCachedAttributes = bDoEvaluationRateOptimization && CachedAttributes != CustomAttributes;

	const bool bShouldDoEvaluation = !bDoEvaluationRateOptimization || bInvalidCachedBones || bInvalidCachedCurve || (bExternalTickRateControlled && bExternalUpdate) || (bCachedShouldUseUpdateRateOptimizations && !AnimUpdateRateParams->ShouldSkipEvaluation());

	const bool bShouldInterpolateSkippedFrames = (bExternalTickRateControlled && bExternalInterpolate) || (bCachedShouldUseUpdateRateOptimizations && AnimUpdateRateParams->ShouldInterpolateSkippedFrames());

	const bool bShouldDoInterpolation = TickFunction != nullptr && bDoEvaluationRateOptimization && !bInvalidCachedBones && bShouldInterpolateSkippedFrames && bAnimInstanceHasCurveUIDList;

	const bool bShouldDoParallelInterpolation = bShouldDoInterpolation && CVarUseParallelAnimationInterpolation.GetValueOnGameThread() == 1;

	const bool bDoPAE = !!CVarUseParallelAnimationEvaluation.GetValueOnGameThread() && (FApp::ShouldUseThreadingForPerformance() || FForkProcessHelper::SupportsMultithreadingPostFork());

	const bool bMainInstanceValidForParallelWork = AnimScriptInstance == nullptr || AnimScriptInstance->CanRunParallelWork();
	const bool bPostInstanceValidForParallelWork = PostProcessAnimInstance == nullptr || PostProcessAnimInstance->CanRunParallelWork();
	const bool bHasValidInstanceForParallelWork = HasValidAnimationInstance() && bMainInstanceValidForParallelWork && bPostInstanceValidForParallelWork;
	const bool bDoParallelEvaluation = bHasValidInstanceForParallelWork && bDoPAE && (bShouldDoEvaluation || bShouldDoParallelInterpolation) && TickFunction && TickFunction->IsCompletionHandleValid();
	const bool bBlockOnTask = !bDoParallelEvaluation;  // If we aren't trying to do parallel evaluation then we
															// will need to wait on an existing task.

	const bool bPerformPostAnimEvaluation = true;
	if (HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation))
	{
		return;
	}

	AnimEvaluationContext.SkeletalMesh = GetSkeletalMeshAsset();
	AnimEvaluationContext.AnimInstance = AnimScriptInstance;
	AnimEvaluationContext.PostProcessAnimInstance = (ShouldEvaluatePostProcessInstance())? ToRawPtr(PostProcessAnimInstance): nullptr;

	if (CurrentAnimCurveUIDFinder)
	{
		if (AnimCurves.UIDToArrayIndexLUT != CurrentAnimCurveUIDFinder || AnimCurves.Num() != CurrentCurveCount)
		{
			AnimCurves.InitFrom(CurrentAnimCurveUIDFinder);
		}
	}
	else
	{
		AnimCurves.Empty();
	}

	AnimEvaluationContext.bDoEvaluation = bShouldDoEvaluation;
	AnimEvaluationContext.bDoInterpolation = bShouldDoInterpolation;
	AnimEvaluationContext.bDuplicateToCacheBones = bInvalidCachedBones || (bDoEvaluationRateOptimization && AnimEvaluationContext.bDoEvaluation && !AnimEvaluationContext.bDoInterpolation);
	AnimEvaluationContext.bDuplicateToCacheCurve = bInvalidCachedCurve || (bDoEvaluationRateOptimization && AnimEvaluationContext.bDoEvaluation && !AnimEvaluationContext.bDoInterpolation && CurrentAnimCurveUIDFinder != nullptr);

	AnimEvaluationContext.bDuplicateToCachedAttributes = bInvalidCachedAttributes || (bDoEvaluationRateOptimization && AnimEvaluationContext.bDoEvaluation && !AnimEvaluationContext.bDoInterpolation);

	if (!bDoEvaluationRateOptimization)
	{
		//If we aren't optimizing clear the cached local atoms
		CachedBoneSpaceTransforms.Reset();
		CachedComponentSpaceTransforms.Reset();
		CachedCurve.Empty();
		CachedAttributes.Empty();
	}

	if (bShouldDoEvaluation)
	{
		// If we need to eval the graph, and we're not going to update it.
		// make sure it's been ticked at least once!
		{
			bool bShouldTickAnimation = false;		
			if (AnimScriptInstance && !AnimScriptInstance->NeedsUpdate())
			{
				bShouldTickAnimation = !AnimScriptInstance->GetUpdateCounter().HasEverBeenUpdated();
			}

			bShouldTickAnimation = bShouldTickAnimation || (ShouldPostUpdatePostProcessInstance() && !PostProcessAnimInstance->GetUpdateCounter().HasEverBeenUpdated());

			if (bShouldTickAnimation)
			{
				// We bypass TickPose() and call TickAnimation directly, so URO doesn't intercept us.
				TickAnimation(0.f, false);
			}
		}

		// If we're going to evaluate animation, call PreEvaluateAnimation()
		{
			DoInstancePreEvaluation();
		}
	}

	if (bDoParallelEvaluation)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_SetupParallel);

		DispatchParallelEvaluationTasks(TickFunction);
	}
	else
	{
		if (AnimEvaluationContext.bDoEvaluation || AnimEvaluationContext.bDoInterpolation)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_GamethreadEval);

			DoParallelEvaluationTasks_OnGameThread();
		}
		else
		{
			if (!AnimEvaluationContext.bDoInterpolation)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_CopyBones);

				if(CachedBoneSpaceTransforms.Num())
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					BoneSpaceTransforms.Reset();
					BoneSpaceTransforms.Append(CachedBoneSpaceTransforms);
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				}
				if(CachedComponentSpaceTransforms.Num())
				{
					TArray<FTransform>& LocalEditableSpaceBases = GetEditableComponentSpaceTransforms();
					LocalEditableSpaceBases.Reset();
					LocalEditableSpaceBases.Append(CachedComponentSpaceTransforms);
				}
				if (CachedCurve.IsValid())
				{
					AnimCurves.CopyFrom(CachedCurve);
				}

				if (CachedAttributes.ContainsData())
				{
					CustomAttributes.CopyFrom(CachedAttributes);
				}
			}
			if(AnimScriptInstance && AnimScriptInstance->NeedsUpdate())
			{
				AnimScriptInstance->ParallelUpdateAnimation();
			}

			if(ShouldPostUpdatePostProcessInstance())
			{
				PostProcessAnimInstance->ParallelUpdateAnimation();
			}
		}

		PostAnimEvaluation(AnimEvaluationContext);
		AnimEvaluationContext.Clear();
	}

	if (TickFunction == nullptr && ShouldBlendPhysicsBones())
	{
		//Since we aren't doing this through the tick system, and we wont have done it in PostAnimEvaluation, assume we want the buffer flipped now
		FinalizeBoneTransform();
	}
}

void USkeletalMeshComponent::SwapEvaluationContextBuffers()
{
	Exchange(AnimEvaluationContext.ComponentSpaceTransforms, GetEditableComponentSpaceTransforms());
	Exchange(AnimEvaluationContext.CachedComponentSpaceTransforms, CachedComponentSpaceTransforms);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Exchange(AnimEvaluationContext.BoneSpaceTransforms, BoneSpaceTransforms);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Exchange(AnimEvaluationContext.CachedBoneSpaceTransforms, CachedBoneSpaceTransforms);
	Exchange(AnimEvaluationContext.Curve, AnimCurves);
	Exchange(AnimEvaluationContext.CachedCurve, CachedCurve);
	Exchange(AnimEvaluationContext.RootBoneTranslation, RootBoneTranslation);

	Exchange(AnimEvaluationContext.CustomAttributes, CustomAttributes);
	Exchange(AnimEvaluationContext.CachedCustomAttributes, CachedAttributes);
}

void USkeletalMeshComponent::DispatchParallelEvaluationTasks(FActorComponentTickFunction* TickFunction)
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);
	SwapEvaluationContextBuffers();

#if WITH_EDITOR
	// We can only finish compilation on the game-thread, so wait here before spawning eval tasks.
	if (GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->IsCompiling())
	{
		FSkinnedAssetCompilingManager::Get().FinishCompilation({ GetSkeletalMeshAsset() });
	}
#endif

	// start parallel work
	check(!IsValidRef(ParallelAnimationEvaluationTask));
	ParallelAnimationEvaluationTask = TGraphTask<FParallelAnimationEvaluationTask>::CreateTask().ConstructAndDispatchWhenReady(this);

	// set up a task to run on the game thread to accept the results
	FGraphEventArray Prerequistes;
	Prerequistes.Add(ParallelAnimationEvaluationTask);
	FGraphEventRef TickCompletionEvent = TGraphTask<FParallelAnimationCompletionTask>::CreateTask(&Prerequistes).ConstructAndDispatchWhenReady(this);

	if ( TickFunction )
	{
		TickFunction->GetCompletionHandle()->DontCompleteUntil(TickCompletionEvent);
	}
}

void USkeletalMeshComponent::DoParallelEvaluationTasks_OnGameThread()
{
	SwapEvaluationContextBuffers();

	ParallelAnimationEvaluation();

	SwapEvaluationContextBuffers();
}

void USkeletalMeshComponent::DispatchParallelTickPose(FActorComponentTickFunction* TickFunction)
{
	check(TickFunction);
	
	if(GetSkeletalMeshAsset() != nullptr)
	{
		if ((AnimScriptInstance && AnimScriptInstance->NeedsUpdate()) ||
			(PostProcessAnimInstance && PostProcessAnimInstance->NeedsUpdate()))
		{
			if (ShouldTickAnimation())
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_USkeletalMeshComponent_RefreshBoneTransforms_DispatchParallelTickPose);

				// This duplicates *some* of the logic from RefreshBoneTransforms()
				const bool bDoPAE = !!CVarUseParallelAnimationEvaluation.GetValueOnGameThread() && (FApp::ShouldUseThreadingForPerformance() || FForkProcessHelper::SupportsMultithreadingPostFork());

				const bool bDoParallelUpdate = bDoPAE && TickFunction->IsCompletionHandleValid();

				const bool bBlockOnTask = !bDoParallelUpdate;   // If we aren't trying to do parallel update then we
																// will need to wait on an existing task.

				const bool bPerformPostAnimEvaluation = true;
				if (HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation))
				{
					return;
				}

				// Do a mini-setup of the eval context
				AnimEvaluationContext.SkeletalMesh = GetSkeletalMeshAsset();
				AnimEvaluationContext.AnimInstance = AnimScriptInstance;

				// We dont set up the Curve here, as we dont use it in Update()
				AnimCurves.Empty();

				CustomAttributes.Empty();

				// Set us up to NOT perform evaluation
				AnimEvaluationContext.bDoEvaluation = false;
				AnimEvaluationContext.bDoInterpolation = false;
				AnimEvaluationContext.bDuplicateToCacheBones = false;
				AnimEvaluationContext.bDuplicateToCacheCurve = false;
				AnimEvaluationContext.bDuplicateToCachedAttributes = false;

				if(bDoParallelUpdate)
				{
					DispatchParallelEvaluationTasks(TickFunction);
				}
				else
				{
					// we cant update on a worker thread, so perform the work here
					DoParallelEvaluationTasks_OnGameThread();
					PostAnimEvaluation(AnimEvaluationContext);
				}
			}
		}
	}
}

void USkeletalMeshComponent::PostAnimEvaluation(FAnimationEvaluationContext& EvaluationContext)
{
#if DO_CHECK
	checkf(!bPostEvaluatingAnimation, TEXT("PostAnimEvaluation already in progress, recursion detected for SkeletalMeshComponent [%s], AnimInstance [%s]"), *GetPathNameSafe(this), *GetPathNameSafe(EvaluationContext.AnimInstance));

	FGuardValue_Bitfield(bPostEvaluatingAnimation, true);
#endif

	SCOPE_CYCLE_COUNTER(STAT_PostAnimEvaluation);

	if (EvaluationContext.AnimInstance)
	{
		EvaluationContext.AnimInstance->PostUpdateAnimation();
	}

	if (ShouldPostUpdatePostProcessInstance())
	{
		PostProcessAnimInstance->PostUpdateAnimation();
	}

	if (!IsRegistered()) // Notify/Event has caused us to go away so cannot carry on from here
	{
		return;
	}

	if(CVarUseParallelAnimationInterpolation.GetValueOnGameThread() == 0)
	{
		if (EvaluationContext.bDuplicateToCacheCurve)
		{
			ensureAlwaysMsgf(AnimCurves.IsValid(), TEXT("Animation Curve is invalid (%s). TotalCount(%d) "),
				*GetPathNameSafe(GetSkeletalMeshAsset()), AnimCurves.NumValidCurveCount);
			CachedCurve.CopyFrom(AnimCurves);
		}

	
		if (EvaluationContext.bDuplicateToCachedAttributes)
		{
			CachedAttributes.CopyFrom(CustomAttributes);
		}
	
		if (EvaluationContext.bDuplicateToCacheBones)
		{
			CachedComponentSpaceTransforms.Reset();
			CachedComponentSpaceTransforms.Append(GetEditableComponentSpaceTransforms());
			CachedBoneSpaceTransforms.Reset();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			CachedBoneSpaceTransforms.Append(BoneSpaceTransforms);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		if (EvaluationContext.bDoInterpolation)
		{
			SCOPE_CYCLE_COUNTER(STAT_InterpolateSkippedFrames);

			float Alpha;
			if(bEnableUpdateRateOptimizations && AnimUpdateRateParams)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (AnimScriptInstance)
				{
					AnimScriptInstance->OnUROPreInterpolation();
				}

				for(UAnimInstance* LinkedInstance : LinkedInstances)
				{
					LinkedInstance->OnUROPreInterpolation();
				}

				if(PostProcessAnimInstance)
				{
					PostProcessAnimInstance->OnUROPreInterpolation();
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS

				Alpha = AnimUpdateRateParams->GetInterpolationAlpha();
			}
			else
			{
				Alpha = ExternalInterpolationAlpha;
			}

			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FAnimationRuntime::LerpBoneTransforms(BoneSpaceTransforms, CachedBoneSpaceTransforms, Alpha, RequiredBones);
			GetSkeletalMeshAsset()->FillComponentSpaceTransforms(BoneSpaceTransforms, FillComponentSpaceTransformsRequiredBones, GetEditableComponentSpaceTransforms());
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			// interpolate curve
			AnimCurves.LerpTo(CachedCurve, Alpha);

			// Interpolate custom attributes
			UE::Anim::Attributes::InterpolateAttributes(CustomAttributes, CachedAttributes, Alpha);
		}
	}

	// Work below only matters if bone transforms have been updated.
	// i.e. if we're using URO and skipping a frame with no interpolation, 
	// we don't need to do that work.
	if (EvaluationContext.bDoEvaluation || EvaluationContext.bDoInterpolation)
	{
		// clear morphtarget curve sets since we're going to apply new changes
		ResetMorphTargetCurves();

		if(AnimScriptInstance)
		{
#if WITH_EDITOR
			GetEditableAnimationCurves() = AnimCurves;
#endif 
			GetEditableCustomAttributes() = CustomAttributes;

			// curve update happens first
			AnimScriptInstance->UpdateCurvesPostEvaluation();

			// this is same curves, and we don't have to process same for everything. 
			// we just copy curves from main for the case where GetCurveValue works in that instance
			for(UAnimInstance* LinkedInstance : LinkedInstances)
			{
				LinkedInstance->CopyCurveValues(*AnimScriptInstance);
			}
		}

		// now update morphtarget curves that are added via SetMorphTarget
		UpdateMorphTargetOverrideCurves();

		if(PostProcessAnimInstance)
		{
			if (AnimScriptInstance)
			{
				// this is same curves, and we don't have to process same for everything. 
				// we just copy curves from main for the case where GetCurveValue works in that instance
				PostProcessAnimInstance->CopyCurveValues(*AnimScriptInstance);
			}
			else
			{
				// if no main anim instance, we'll have to have post processor to handle it
				PostProcessAnimInstance->UpdateCurvesPostEvaluation();
			}
		}

		// If we have actually evaluated animations, we need to call PostEvaluateAnimation now.
		if (EvaluationContext.bDoEvaluation)
		{
			DoInstancePostEvaluation();
		}

		bNeedToFlipSpaceBaseBuffers = true;

		if (Bodies.Num() > 0 || bEnablePerPolyCollision)
		{
			// update physics data from animated data
			if(bSkipKinematicUpdateWhenInterpolating)
			{
				if(EvaluationContext.bDoEvaluation)
				{
					// push newly evaluated bones to physics
					UpdateKinematicBonesToAnim(EvaluationContext.bDoInterpolation ? CachedComponentSpaceTransforms : GetEditableComponentSpaceTransforms(), ETeleportType::None, true);
					UpdateRBJointMotors();
				}
			}
			else
			{
				UpdateKinematicBonesToAnim(GetEditableComponentSpaceTransforms(), ETeleportType::None, true);
				UpdateRBJointMotors();
			}
		}


#if WITH_EDITOR	
		// If we have no physics to blend or in editor since there is no physics tick group, we are done
		if (!ShouldBlendPhysicsBones() || GetWorld()->WorldType == EWorldType::Editor)
		{
			// Flip buffers, update bounds, attachments etc.
			FinalizeAnimationUpdate();
		}
#else
		if (!ShouldBlendPhysicsBones())
		{
			// Flip buffers, update bounds, attachments etc.
			FinalizeAnimationUpdate();
		}
#endif
	}
	else 
	{
		// Since we're not calling FinalizeBoneTransforms via PostBlendPhysics,
		// make sure we call ConditionallyDispatchQueuedAnimEvents() in case we ticked, but didn't evalutate.

		/////////////////////////////////////////////////////////////////////////////
		// Notify / Event Handling!
		// This can do anything to our component (including destroy it) 
		// Any code added after this point needs to take that into account
		/////////////////////////////////////////////////////////////////////////////

		ConditionallyDispatchQueuedAnimEvents();
	}

	AnimEvaluationContext.Clear();
}

void USkeletalMeshComponent::ApplyAnimationCurvesToComponent(const TMap<FName, float>* InMaterialParameterCurves, const TMap<FName, float>* InAnimationMorphCurves)
{
	const bool bContainsMaterialCurves = InMaterialParameterCurves && InMaterialParameterCurves->Num() > 0;
	if (bContainsMaterialCurves)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnimInstanceProxy_UpdateComponentsMaterialParameters);
		for (auto Iter = InMaterialParameterCurves->CreateConstIterator(); Iter; ++Iter)
		{
			FName ParameterName = Iter.Key();
			float ParameterValue = Iter.Value();
			SetScalarParameterValueOnMaterials(ParameterName, ParameterValue);
		}
	}

	const bool bContainsMorphCurves = InAnimationMorphCurves && InAnimationMorphCurves->Num() > 0;
	if (GetSkeletalMeshAsset() && bContainsMorphCurves)
	{
		// we want to append to existing curves - i.e. BP driven curves 
		FAnimationRuntime::AppendActiveMorphTargets(GetSkeletalMeshAsset(), *InAnimationMorphCurves, ActiveMorphTargets, MorphTargetWeights);
	}

	/** Push through curves to follower components */
	if (bPropagateCurvesToFollowers && bContainsMorphCurves && bContainsMaterialCurves && FollowerPoseComponents.Num() > 0)
	{
		for (TWeakObjectPtr<USkinnedMeshComponent> MeshComponent : FollowerPoseComponents)
		{
			if (USkeletalMeshComponent* SKComponent = Cast<USkeletalMeshComponent>(MeshComponent.Get()))
			{
				SKComponent->ApplyAnimationCurvesToComponent(InMaterialParameterCurves, InAnimationMorphCurves);
			}
		}
	}
}

FBoxSphereBounds USkeletalMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	SCOPE_CYCLE_COUNTER(STAT_CalcSkelMeshBounds);

	// fixme laurent - extend concept of LocalBounds to all SceneComponent
	// as rendered calls CalcBounds*() directly in FScene::UpdatePrimitiveTransform, which is pretty expensive for SkelMeshes.
	// No need to calculated that again, just use cached local bounds.
	if (bCachedWorldSpaceBoundsUpToDate || bCachedLocalBoundsUpToDate)
	{
		FBoxSphereBounds Result;
		if (bCachedLocalBoundsUpToDate)
		{
			Result = CachedWorldOrLocalSpaceBounds.TransformBy(LocalToWorld);
		}
		else
		{
			Result = CachedWorldOrLocalSpaceBounds.TransformBy(CachedWorldToLocalTransform * LocalToWorld.ToMatrixWithScale());
		}

		if (bIncludeComponentLocationIntoBounds)
		{
			const FVector ComponentLocation = GetComponentLocation();
			return Result + FBoxSphereBounds(ComponentLocation, FVector(1.0f), 1.0f);
		}
		else
		{
			return Result;
		}
	}
	// Calculate new bounds
	else
	{
		FVector RootBoneOffset = RootBoneTranslation;

		// if to use LeaderPoseComponent's fixed skel bounds, 
		// send LeaderPoseComponent's Root Bone Translation
		if (LeaderPoseComponent.IsValid())
		{
			const USkinnedMeshComponent* const LeaderPoseComponentInst = LeaderPoseComponent.Get();
			check(LeaderPoseComponentInst);
			if (LeaderPoseComponentInst->GetSkinnedAsset() &&
				LeaderPoseComponentInst->bComponentUseFixedSkelBounds &&
				LeaderPoseComponentInst->IsA((USkeletalMeshComponent::StaticClass())))
			{
				const USkeletalMeshComponent* BaseComponent = CastChecked<USkeletalMeshComponent>(LeaderPoseComponentInst);
				RootBoneOffset = BaseComponent->RootBoneTranslation; // Adjust bounds by root bone translation
			}
		}

		const bool bCacheLocalSpaceBounds = CVarCacheLocalSpaceBounds.GetValueOnGameThread() != 0;
		
		const FTransform CachedBoundsTransform = bCacheLocalSpaceBounds ? FTransform::Identity : LocalToWorld;

		FBoxSphereBounds NewBounds = CalcMeshBound((FVector3f)RootBoneOffset, bHasValidBodies, CachedBoundsTransform);

		if (bIncludeComponentLocationIntoBounds)
		{
			const FVector ComponentLocation = GetComponentLocation();
			const FBoxSphereBounds ComponentLocationBounds(ComponentLocation, FVector(1.0f), 1.0f);
			if (bCacheLocalSpaceBounds)
			{
				NewBounds = NewBounds.TransformBy(LocalToWorld);
				NewBounds = NewBounds + ComponentLocationBounds;
				NewBounds = NewBounds.TransformBy(LocalToWorld.ToInverseMatrixWithScale());
			}
			else
			{
				NewBounds = NewBounds + ComponentLocationBounds;
			}			
		}

		AddClothingBounds(NewBounds, CachedBoundsTransform);

		CachedWorldOrLocalSpaceBounds = NewBounds;
		bCachedLocalBoundsUpToDate = bCacheLocalSpaceBounds;
		bCachedWorldSpaceBoundsUpToDate = !bCacheLocalSpaceBounds;

		if (bCacheLocalSpaceBounds)
		{ 
			CachedWorldToLocalTransform.SetIdentity();
			return NewBounds.TransformBy(LocalToWorld);
		}
		else
		{
			CachedWorldToLocalTransform = LocalToWorld.ToInverseMatrixWithScale();
			return NewBounds;
		}

	}
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkelMesh, bool bReinitPose)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SetSkeletalMesh);
	SCOPE_CYCLE_UOBJECT(NewSkelMesh, InSkelMesh);

	if (InSkelMesh == GetSkeletalMeshAsset())
	{
		// do nothing if the input mesh is the same mesh we're already using.
		return;
	}

	// Update property alias
#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SkeletalMeshAsset = InSkelMesh;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	// We may be doing parallel evaluation on the current anim instance
	// Calling this here with true will block this init till that thread completes
	// and it is safe to continue
	const bool bBlockOnTask = true; // wait on evaluation task so it is safe to continue with Init
	const bool bPerformPostAnimEvaluation = true;
	HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);

	UPhysicsAsset* OldPhysAsset = GetPhysicsAsset();

	{
		FRenderStateRecreator RenderStateRecreator(this);

		// The SetSkeletalMesh base implementation is being phased out of USkinnedMeshComponent, and SetSkinnedAssetAndUpdate replaces it,
		// but the SetSkeletalMesh function will remain to provide continuity and a more convenient USkeletalMesh based API avoiding the uncertainty of the Cast to USkeletalMesh.
		// Also, since USkeletalMeshComponent::SetSkinnedAssetAndUpdate now calls USkeletalMeshComponent::SetSkeletalMesh, and since SetSkinnedAssetAndUpdate is virtual,
		// the Super keyword is necessary to make sure that the correct USkinnedMeshComponent base function is called and prevent a recursive call loop.
		Super::SetSkinnedAssetAndUpdate(InSkelMesh, bReinitPose);
		
#if WITH_EDITOR
		ValidateAnimation();
#endif

		if(IsPhysicsStateCreated())
		{
			if(GetPhysicsAsset() == OldPhysAsset && OldPhysAsset && Bodies.Num() == OldPhysAsset->SkeletalBodySetups.Num())	//Make sure that we actually created all the bodies for the asset (needed for old assets in editor)
			{
				UpdateBoneBodyMapping();
			}
			else
			{
				RecreatePhysicsState();
			}
		}

		UpdateHasValidBodies();
		ClearMorphTargets();

		// Make sure that required bones are invalidated as we have just changed our mesh
		// RecalcRequiredBones will be called by InitAnim below
		bRequiredBonesUpToDate = false;
		
		InitAnim(bReinitPose);

		RecreateClothingActors();
	}

	// Mark cached material parameter names dirty
	MarkCachedMaterialParameterNameIndicesDirty();

	// Update this component streaming data.
	IStreamingManager::Get().NotifyPrimitiveUpdated(this);
}

void USkeletalMeshComponent::SetSkinnedAssetAndUpdate(USkinnedAsset* InSkinnedAsset, bool bReinitPose)
{
	SetSkeletalMesh(Cast<USkeletalMesh>(InSkinnedAsset), bReinitPose);
}

void USkeletalMeshComponent::SetSkeletalMeshWithoutResettingAnimation(USkeletalMesh* InSkelMesh)
{
	SetSkeletalMesh(InSkelMesh,false);
}

bool USkeletalMeshComponent::AllocateTransformData()
{
	LLM_SCOPE_BYNAME("SkeletalMesh/TransformData");

	// Allocate transforms if not present.
	if ( Super::AllocateTransformData() )
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if(BoneSpaceTransforms.Num() != GetSkeletalMeshAsset()->GetRefSkeleton().GetNum() )
		{
			BoneSpaceTransforms = GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		return true;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	BoneSpaceTransforms.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	return false;
}

void USkeletalMeshComponent::DeallocateTransformData()
{
	Super::DeallocateTransformData();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	BoneSpaceTransforms.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMeshComponent::SetForceRefPose(bool bNewForceRefPose)
{
	bForceRefpose = bNewForceRefPose;
	MarkRenderStateDirty();
}

void USkeletalMeshComponent::ToggleDisablePostProcessBlueprint()
{
	SetDisablePostProcessBlueprint(!bDisablePostProcessBlueprint);
}

bool USkeletalMeshComponent::GetDisablePostProcessBlueprint() const
{
	return bDisablePostProcessBlueprint;
}

void USkeletalMeshComponent::SetDisablePostProcessBlueprint(bool bInDisablePostProcess)
{
	// If we're re-enabling - reinitialize the post process instance as it may
	// not have been ticked in some time
	if(!bInDisablePostProcess && bDisablePostProcessBlueprint && PostProcessAnimInstance)
	{
		PostProcessAnimInstance->InitializeAnimation();
	}

	bDisablePostProcessBlueprint = bInDisablePostProcess;
}

void USkeletalMeshComponent::K2_SetAnimInstanceClass(class UClass* NewClass)
{
	SetAnimInstanceClass(NewClass);
}

void USkeletalMeshComponent::SetAnimClass(class UClass* NewClass)
{
	SetAnimInstanceClass(NewClass);
}

class UClass* USkeletalMeshComponent::GetAnimClass()
{
	return AnimClass;
}

void USkeletalMeshComponent::SetAnimInstanceClass(class UClass* NewClass)
{
	if (NewClass != nullptr)
	{
		// set the animation mode
		const bool bWasUsingBlueprintMode = AnimationMode == EAnimationMode::AnimationBlueprint;
		AnimationMode = EAnimationMode::Type::AnimationBlueprint;

		if (NewClass != AnimClass || !bWasUsingBlueprintMode)
		{
			// Only need to initialize if it hasn't already been set or we weren't previously using a blueprint instance
			AnimClass = NewClass;
			ClearAnimScriptInstance();
			InitAnim(true);
		}
	}
	else
	{
		// Need to clear the instance as well as the blueprint.
		// @todo is this it?
		AnimClass = nullptr;
		ClearAnimScriptInstance();
	}
}

UAnimInstance* USkeletalMeshComponent::GetAnimInstance() const
{
	return AnimScriptInstance;
}

UAnimInstance* USkeletalMeshComponent::GetPostProcessInstance() const
{
	return PostProcessAnimInstance;
}

void USkeletalMeshComponent::ResetLinkedAnimInstances()
{
	for(UAnimInstance* LinkedInstance : LinkedInstances)
	{
		if(LinkedInstance && LinkedInstance->bCreatedByLinkedAnimGraph)
		{
			LinkedInstance->EndNotifyStates();
			LinkedInstance->MarkAsGarbage();
			LinkedInstance = nullptr;
		}
	}
	LinkedInstances.Reset();
}

UAnimInstance* USkeletalMeshComponent::GetLinkedAnimGraphInstanceByTag(FName InName) const
{
	if(AnimScriptInstance)
	{
		return AnimScriptInstance->GetLinkedAnimGraphInstanceByTag(InName);
	}
	return nullptr;
}

void USkeletalMeshComponent::GetLinkedAnimGraphInstancesByTag(FName InTag, TArray<UAnimInstance*>& OutLinkedInstances) const
{
	if(AnimScriptInstance)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AnimScriptInstance->GetLinkedAnimGraphInstancesByTag(InTag, OutLinkedInstances);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void USkeletalMeshComponent::LinkAnimGraphByTag(FName InTag, TSubclassOf<UAnimInstance> InClass)
{
	if(AnimScriptInstance)
	{
		AnimScriptInstance->LinkAnimGraphByTag(InTag, InClass);
	}
}

void USkeletalMeshComponent::LinkAnimClassLayers(TSubclassOf<UAnimInstance> InClass)
{
	if(AnimScriptInstance)
	{
		AnimScriptInstance->LinkAnimClassLayers(InClass);
	}
}

void USkeletalMeshComponent::UnlinkAnimClassLayers(TSubclassOf<UAnimInstance> InClass)
{
	if(AnimScriptInstance)
	{
		AnimScriptInstance->UnlinkAnimClassLayers(InClass);
	}
}

UAnimInstance* USkeletalMeshComponent::GetLinkedAnimLayerInstanceByGroup(FName InGroup) const
{
	if(AnimScriptInstance)
	{
		return AnimScriptInstance->GetLinkedAnimLayerInstanceByGroup(InGroup);
	}
	return nullptr;
}

UAnimInstance* USkeletalMeshComponent::GetLinkedAnimLayerInstanceByClass(TSubclassOf<UAnimInstance> InClass) const
{
	if(AnimScriptInstance)
	{
		return AnimScriptInstance->GetLinkedAnimLayerInstanceByClass(InClass);
	}
	return nullptr;
}

void USkeletalMeshComponent::ForEachAnimInstance(TFunctionRef<void(UAnimInstance*)> InFunction)
{
	if(AnimScriptInstance)
	{
		InFunction(AnimScriptInstance);
	}

	// Copy LinkedInstances because the array can be concurrently modified inside this loop 
	for(UAnimInstance* LinkedInstance : TArray<TObjectPtr<UAnimInstance>, TInlineAllocator<8>>(LinkedInstances))
	{
		if(LinkedInstance)
		{
			InFunction(LinkedInstance);
		}
	}

	if(PostProcessAnimInstance)
	{
		InFunction(PostProcessAnimInstance);
	}
}

bool USkeletalMeshComponent::HasValidAnimationInstance() const
{
	return AnimScriptInstance || PostProcessAnimInstance;
}

void USkeletalMeshComponent::ResetAnimInstanceDynamics(ETeleportType InTeleportType)
{
	if(AnimScriptInstance)
	{
		AnimScriptInstance->ResetDynamics(InTeleportType);
	}

	for(UAnimInstance* LinkedInstance : LinkedInstances)
	{
		LinkedInstance->ResetDynamics(InTeleportType);
	}

	if(PostProcessAnimInstance)
	{
		PostProcessAnimInstance->ResetDynamics(InTeleportType);
	}
}

void USkeletalMeshComponent::NotifySkelControlBeyondLimit( USkelControlLookAt* LookAt ) {}

void USkeletalMeshComponent::SkelMeshCompOnParticleSystemFinished( UParticleSystemComponent* PSC )
{
	PSC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	PSC->UnregisterComponent();
}


void USkeletalMeshComponent::HideBone( int32 BoneIndex, EPhysBodyOp PhysBodyOption)
{
	Super::HideBone(BoneIndex, PhysBodyOption);

	if (!GetSkeletalMeshAsset())
	{
		return;
	}

	if (LeaderPoseComponent.IsValid())
	{
		return;
	}

	// if valid bone index
	if (BoneIndex >= 0 && GetNumBones() > BoneIndex)
	{
		bRequiredBonesUpToDate = false;

		if (PhysBodyOption != PBO_None)
		{
			FName HideBoneName = GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BoneIndex);
			if (PhysBodyOption == PBO_Term)
			{
				TermBodiesBelow(HideBoneName);
			}
		}
	}
	else
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("HideBone[%s]: Invalid Body Index (%d) has entered. This component doesn't contain buffer for the given body."), *GetPathNameSafe(GetSkeletalMeshAsset()), BoneIndex);
	}
}

void USkeletalMeshComponent::UnHideBone( int32 BoneIndex )
{
	Super::UnHideBone(BoneIndex);

	if (!GetSkeletalMeshAsset())
	{
		return;
	}

	if (LeaderPoseComponent.IsValid())
	{
		return;
	}

	if (BoneIndex >= 0 && GetNumBones() > BoneIndex)
	{
		bRequiredBonesUpToDate = false;

		//FName HideBoneName = GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BoneIndex);
		// It's okay to turn this on for terminated bodies
		// It won't do any if BodyData isn't found
		// @JTODO
		//SetCollisionBelow(true, HideBoneName);
	}
	else
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("UnHideBone[%s]: Invalid Body Index (%d) has entered. This component doesn't contain buffer for the given body."), *GetPathNameSafe(GetSkeletalMeshAsset()), BoneIndex);
	}
}


bool USkeletalMeshComponent::IsAnySimulatingPhysics() const
{
	for ( int32 BodyIndex=0; BodyIndex<Bodies.Num(); ++BodyIndex )
	{
		if (Bodies[BodyIndex]->IsInstanceSimulatingPhysics())
		{
			return true;
		}
	}

	return false;
}

void USkeletalMeshComponent::SetMorphTarget(FName MorphTargetName, float Value, bool bRemoveZeroWeight)
{
	float *CurveValPtr = MorphTargetCurves.Find(MorphTargetName);
	bool bShouldAddToList = !bRemoveZeroWeight || FPlatformMath::Abs(Value) > ZERO_ANIMWEIGHT_THRESH;
	if ( bShouldAddToList )
	{
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr = Value;
		}
		else
		{
			MorphTargetCurves.Add(MorphTargetName, Value);
		}
	}
	// if less than ZERO_ANIMWEIGHT_THRESH
	// no reason to keep them on the list
	else 
	{
		// remove if found
		MorphTargetCurves.Remove(MorphTargetName);
	}
}

void USkeletalMeshComponent::ClearMorphTargets()
{
	MorphTargetCurves.Empty();
}

float USkeletalMeshComponent::GetMorphTarget( FName MorphTargetName ) const
{
	const float *CurveValPtr = MorphTargetCurves.Find(MorphTargetName);
	
	if(CurveValPtr)
	{
		return *CurveValPtr;
	}
	else
	{
		return 0.0f;
	}
}

FVector USkeletalMeshComponent::GetClosestCollidingRigidBodyLocation(const FVector& TestLocation) const
{
	float BestDistSq = UE_BIG_NUMBER;
	FVector Best = TestLocation;

	UPhysicsAsset* PhysicsAsset = GetPhysicsAsset();
	if( PhysicsAsset )
	{
		for (int32 i=0; i<Bodies.Num(); i++)
		{
			FBodyInstance* BodyInst = Bodies[i];
			if( BodyInst && BodyInst->IsValidBodyInstance() && (BodyInst->GetCollisionEnabled() != ECollisionEnabled::NoCollision) )
			{
				const FVector BodyLocation = BodyInst->GetUnrealWorldTransform().GetTranslation();
				const float DistSq = (BodyLocation - TestLocation).SizeSquared();
				if( DistSq < BestDistSq )
				{
					Best = BodyLocation;
					BestDistSq = DistSq;
				}
			}
		}
	}

	return Best;
}

void USkeletalMeshComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	for (int32 i=0; i < Bodies.Num(); ++i)
	{
		if (Bodies[i] != nullptr && Bodies[i]->IsValidBodyInstance())
		{
			Bodies[i]->GetBodyInstanceResourceSizeEx(CumulativeResourceSize);
		}
	}
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode::Type InAnimationMode)
{
	bool bNeedChange = AnimationMode != InAnimationMode;
	if (bNeedChange)
	{
		AnimationMode = InAnimationMode;
		ClearAnimScriptInstance();
	}

	// when mode is swapped, make sure to reinitialize
	// even if it was same mode, this was due to users who wants to use BP construction script to do this
	// if you use it in the construction script, it gets serialized, but it never instantiate. 
	if(GetSkeletalMeshAsset() != nullptr && (bNeedChange || AnimationMode == EAnimationMode::AnimationBlueprint))
	{
		if (InitializeAnimScriptInstance(true))
		{
			OnAnimInitialized.Broadcast();
		}
	}
}

EAnimationMode::Type USkeletalMeshComponent::GetAnimationMode() const
{
	return AnimationMode;
}

void USkeletalMeshComponent::PlayAnimation(class UAnimationAsset* NewAnimToPlay, bool bLooping)
{
	SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SetAnimation(NewAnimToPlay);
	Play(bLooping);
}

void USkeletalMeshComponent::SetAnimation(UAnimationAsset* NewAnimToPlay)
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetAnimationAsset(NewAnimToPlay, false);
		SingleNodeInstance->SetPlaying(false);
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}
}

void USkeletalMeshComponent::Play(bool bLooping)
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPlaying(true);
		SingleNodeInstance->SetLooping(bLooping);
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}
}

void USkeletalMeshComponent::Stop()
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPlaying(false);
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}
}

bool USkeletalMeshComponent::IsPlaying() const
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		return SingleNodeInstance->IsPlaying();
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}

	return false;
}

void USkeletalMeshComponent::SetPosition(float InPos, bool bFireNotifies)
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPosition(InPos, bFireNotifies);
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}
}

float USkeletalMeshComponent::GetPosition() const
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		return SingleNodeInstance->GetCurrentTime();
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}

	return 0.f;
}

void USkeletalMeshComponent::SetPlayRate(float Rate)
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPlayRate(Rate);
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}
}

float USkeletalMeshComponent::GetPlayRate() const
{
	UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
	if (SingleNodeInstance)
	{
		return SingleNodeInstance->GetPlayRate();
	}
	else if( AnimScriptInstance != nullptr )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Currently in Animation Blueprint mode. Please change AnimationMode to Use Animation Asset"));
	}

	return 0.f;
}

void USkeletalMeshComponent::OverrideAnimationData(UAnimationAsset* InAnimToPlay, bool bIsLooping /*= true*/, bool bIsPlaying /*= true*/, float Position /*= 0.f*/, float PlayRate /*= 1.f*/)
{
	AnimationData.AnimToPlay = InAnimToPlay;
	AnimationData.bSavedLooping = bIsLooping;
	AnimationData.bSavedPlaying = bIsPlaying;
	AnimationData.SavedPosition = Position;
	AnimationData.SavedPlayRate = PlayRate;
	SetAnimationMode(EAnimationMode::AnimationSingleNode);
	TickAnimation(0.f, false);
	RefreshBoneTransforms();
}

class UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
	return Cast<class UAnimSingleNodeInstance>(AnimScriptInstance);
}

bool USkeletalMeshComponent::PoseTickedThisFrame() const 
{ 
	return GFrameCounter == LastPoseTickFrame; 
}

FTransform USkeletalMeshComponent::ConvertLocalRootMotionToWorld(const FTransform& InTransform)
{
	// Make sure component to world is up to date
	ConditionalUpdateComponentToWorld();

#if !(UE_BUILD_SHIPPING)
	if (GetComponentTransform().ContainsNaN())
	{
		logOrEnsureNanError(TEXT("SkeletalMeshComponent: GetComponentTransform() contains NaN!"));
		SetComponentToWorld(FTransform::Identity);
	}
#endif

	//Calculate new actor transform after applying root motion to this component
	const FTransform ActorToWorld = GetOwner()->GetTransform();

	const FTransform ComponentToActor = ActorToWorld.GetRelativeTransform(GetComponentTransform());
	const FTransform NewComponentToWorld = InTransform * GetComponentTransform();
	const FTransform NewActorTransform = ComponentToActor * NewComponentToWorld;

	const FVector DeltaWorldTranslation = NewActorTransform.GetTranslation() - ActorToWorld.GetTranslation();

	const FQuat NewWorldRotation = GetComponentTransform().GetRotation() * InTransform.GetRotation();
	const FQuat DeltaWorldRotation = NewWorldRotation * GetComponentTransform().GetRotation().Inverse();
	
	const FTransform DeltaWorldTransform(DeltaWorldRotation, DeltaWorldTranslation);

	UE_LOG(LogRootMotion, Log,  TEXT("ConvertLocalRootMotionToWorld LocalT: %s, LocalR: %s, WorldT: %s, WorldR: %s."),
		*InTransform.GetTranslation().ToCompactString(), *InTransform.GetRotation().Rotator().ToCompactString(),
		*DeltaWorldTransform.GetTranslation().ToCompactString(), *DeltaWorldTransform.GetRotation().Rotator().ToCompactString());

	return DeltaWorldTransform;
}

FRootMotionMovementParams USkeletalMeshComponent::ConsumeRootMotion()
{
	float InterpAlpha;
	
	if(bExternalTickRateControlled)
	{
		InterpAlpha = ExternalInterpolationAlpha;
	}
	else
	{
		InterpAlpha = ShouldUseUpdateRateOptimizations() ? AnimUpdateRateParams->GetRootMotionInterp() : 1.f;
	}

	return ConsumeRootMotion_Internal(InterpAlpha);
}

#if WITH_EDITOR
FPoseWatchDynamicData::FPoseWatchDynamicData(USkeletalMeshComponent* InComponent)
{
	check(InComponent);
	if (InComponent->GetSkeletalMeshAsset())
	{
		PoseWatches = InComponent->PoseWatches;

		for (FAnimNodePoseWatch& PoseWatch : PoseWatches)
		{
			PoseWatch.CopyPoseWatchData(InComponent->GetSkeletalMeshAsset()->GetRefSkeleton());
		}
	}
}
#endif

FRootMotionMovementParams USkeletalMeshComponent::ConsumeRootMotion_Internal(float InAlpha)
{
	FRootMotionMovementParams RootMotion;
	if(AnimScriptInstance)
	{
		RootMotion.Accumulate(AnimScriptInstance->ConsumeExtractedRootMotion(InAlpha));

		for(UAnimInstance* LinkedInstance : LinkedInstances)
		{
			RootMotion.Accumulate(LinkedInstance->ConsumeExtractedRootMotion(InAlpha));
		}
	}

	if(PostProcessAnimInstance)
	{
		RootMotion.Accumulate(PostProcessAnimInstance->ConsumeExtractedRootMotion(InAlpha));
	}

	return RootMotion;
}

float USkeletalMeshComponent::CalculateMass(FName BoneName)
{
	float Mass = 0.0f;

	if (Bodies.Num())
	{
		for (int32 i = 0; i < Bodies.Num(); ++i)
		{
			UBodySetup* BodySetupPtr = Bodies[i]->GetBodySetup();
			//if bone name is not provided calculate entire mass - otherwise get mass for just the bone
			if (BodySetupPtr && (BoneName == NAME_None || BoneName == BodySetupPtr->BoneName))
			{
				Mass += BodySetupPtr->CalculateMass(this);
			}
		}
	}
	else	//We want to calculate mass before we've initialized body instances - in this case use physics asset setup
	{
		using BodySetupContainerType = decltype(UPhysicsAsset::SkeletalBodySetups);
		BodySetupContainerType* BodySetups = nullptr;
		if (UPhysicsAsset* PhysicsAsset = GetPhysicsAsset())
		{
			BodySetups = &PhysicsAsset->SkeletalBodySetups;
		}

		if (BodySetups)
		{
			for (int32 i = 0; i < BodySetups->Num(); ++i)
			{
				if ((*BodySetups)[i] && (BoneName == NAME_None || BoneName == (*BodySetups)[i]->BoneName))
				{
					Mass += (*BodySetups)[i]->CalculateMass(this);
				}
			}
		}
	}

	return Mass;
}

bool USkeletalMeshComponent::IsShown(const FEngineShowFlags& ShowFlags) const
{
	return ShowFlags.SkeletalMeshes;
}

#if WITH_EDITOR

bool USkeletalMeshComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && MeshObject != nullptr)
	{
		FSkeletalMeshRenderData* SkelMeshRenderData = GetSkeletalMeshRenderData();
		if (SkelMeshRenderData && SkelMeshRenderData->LODRenderData.Num() > 0)
		{
			// Transform verts into world space. Note that this assumes skeletal mesh is in reference pose...
			const FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[0];
			// When editor accesses cooked data and mesh streaming is on, lower LODs are cooked as StreamingBulkData instead of cooking inline,
			// so the vertex buffer may contain valid meta data but no actual vertex data. Check it is valid before accessing it.
			if (LODData.StaticVertexBuffers.PositionVertexBuffer.GetVertexData())
			{
				for (uint32 VertIdx = 0; VertIdx < LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices(); VertIdx++)
				{
					const FVector3f& VertexPos(LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertIdx));
					const FVector Location = GetComponentTransform().TransformPosition((FVector)VertexPos);
					const bool bLocationIntersected = FMath::PointBoxIntersection(Location, InSelBBox);

					// If the selection box doesn't have to encompass the entire component and a skeletal mesh vertex has intersected with
					// the selection box, this component is being touched by the selection box
					if (!bMustEncompassEntireComponent && bLocationIntersected)
					{
						return true;
					}

					// If the selection box has to encompass the entire component and a skeletal mesh vertex didn't intersect with the selection
					// box, this component does not qualify
					else if (bMustEncompassEntireComponent && !bLocationIntersected)
					{
						return false;
					}
				}	

				// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
				// is consider touching
				if (bMustEncompassEntireComponent)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool USkeletalMeshComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && MeshObject != nullptr)
	{
		FSkeletalMeshRenderData* SkelMeshRenderData = GetSkeletalMeshRenderData();
		if (SkelMeshRenderData && SkelMeshRenderData->LODRenderData.Num() > 0)
		{
			// Transform verts into world space. Note that this assumes skeletal mesh is in reference pose...
			const FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[0];
			// When editor accesses cooked data and mesh streaming is on, lower LODs are cooked as StreamingBulkData instead of cooking inline,
			// so the vertex buffer may contain valid meta data but no actual vertex data. Check it is valid before accessing it.
			if (LODData.StaticVertexBuffers.PositionVertexBuffer.GetVertexData())
			{
				for (uint32 VertIdx = 0; VertIdx < LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices(); VertIdx++)
				{
					const FVector3f& VertexPos(LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertIdx));
					const FVector Location = GetComponentTransform().TransformPosition((FVector)VertexPos);
					const bool bLocationIntersected = InFrustum.IntersectSphere(Location, 0.0f);

					// If the selection box doesn't have to encompass the entire component and a skeletal mesh vertex has intersected with
					// the selection box, this component is being touched by the selection box
					if (!bMustEncompassEntireComponent && bLocationIntersected)
					{
						return true;
					}

					// If the selection box has to encompass the entire component and a skeletal mesh vertex didn't intersect with the selection
					// box, this component does not qualify
					else if (bMustEncompassEntireComponent && !bLocationIntersected)
					{
						return false;
					}
				}

				// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
				// is consider touching
				return true;
			}
		}
	}

	return false;
}


void USkeletalMeshComponent::UpdateCollisionProfile()
{
	Super::UpdateCollisionProfile();

	for(int32 i=0; i < Bodies.Num(); ++i)
	{
		if(Bodies[i]->BodySetup.IsValid())
		{
			Bodies[i]->LoadProfileData(false);
		}
	}
}

FDelegateHandle USkeletalMeshComponent::RegisterOnSkeletalMeshPropertyChanged( const FOnSkeletalMeshPropertyChanged& Delegate )
{
	return OnSkeletalMeshPropertyChanged.Add(Delegate);
}

void USkeletalMeshComponent::UnregisterOnSkeletalMeshPropertyChanged( FDelegateHandle Handle )
{
	OnSkeletalMeshPropertyChanged.Remove(Handle);
}

void USkeletalMeshComponent::ValidateAnimation()
{
	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	if (SkelMesh && SkelMesh->GetSkeleton() == nullptr)
	{
		UE_LOG(LogAnimation, Warning, TEXT("SkeletalMesh %s has no skeleton. This needs to fixed before an animation can be set"), *SkelMesh->GetFullName());
		if (AnimationMode == EAnimationMode::AnimationSingleNode)
		{
			AnimationData.AnimToPlay = nullptr;
		}
		else if(AnimationMode == EAnimationMode::AnimationBlueprint)
		{
			AnimClass = nullptr;
		}
		else
		{
			// if custom mode, you still can't use the animation instance
			AnimScriptInstance = nullptr;
		}
		return;
	}

	if(AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		if (AnimationData.AnimToPlay && SkelMesh)
		{
			if (AnimationData.AnimToPlay->GetSkeleton() == nullptr)
			{
				UE_LOG(LogAnimation, Warning, TEXT("Animation %s is incompatible because it has no skeleton, removing animation from actor."), *AnimationData.AnimToPlay->GetName());
				AnimationData.AnimToPlay = nullptr;
			}
			else if (!AnimationData.AnimToPlay->GetSkeleton()->IsCompatible(SkelMesh->GetSkeleton()))
			{
				UE_LOG(LogAnimation, Warning, TEXT("Animation %s is incompatible with the skeletal mesh's skeleton, removing animation from actor."), *AnimationData.AnimToPlay->GetName());
				AnimationData.AnimToPlay = nullptr;
			}
		}
	}
	else if (AnimationMode == EAnimationMode::AnimationBlueprint)
	{
		IAnimClassInterface* AnimClassInterface = IAnimClassInterface::GetFromClass(AnimClass);
		if (AnimClassInterface && SkelMesh)
		{
			if (AnimClassInterface->GetTargetSkeleton() == nullptr)
			{
				UE_LOG(LogAnimation, Warning, TEXT("AnimBP %s is incompatible because it has no target skeleton, removing AnimBP from actor."), *AnimClass->GetName());
				AnimClass = nullptr;
			}
			else if (SkelMesh->GetSkeleton() == nullptr)
			{
				UE_LOG(LogAnimation, Warning, TEXT("AnimBP %s is incompatible because mesh %s has no skeleton, removing AnimBP from actor."), *AnimClass->GetName(), *SkelMesh->GetName());
				AnimClass = nullptr;
			}
			else if (!AnimClassInterface->GetTargetSkeleton()->IsCompatible(SkelMesh->GetSkeleton()))
			{
				UE_LOG(LogAnimation, Warning, TEXT("AnimBP %s is incompatible with skeleton %s, removing AnimBP from actor."), *AnimClass->GetName(), *SkelMesh->GetSkeleton()->GetName());
				AnimClass = nullptr;
			}
		}
	}
}

#endif

bool USkeletalMeshComponent::IsPlayingRootMotion() const
{
	return (IsPlayingRootMotionFromEverything() || IsPlayingNetworkedRootMotionMontage());
}

bool USkeletalMeshComponent::IsPlayingNetworkedRootMotionMontage() const
{
	if (AnimScriptInstance)
	{
		if (AnimScriptInstance->RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly)
		{
			if (const FAnimMontageInstance* MontageInstance = AnimScriptInstance->GetRootMotionMontageInstance())
			{
				return !MontageInstance->IsRootMotionDisabled();
			}
		}
	}

	return false;
}

bool USkeletalMeshComponent::IsPlayingRootMotionFromEverything() const
{
	return AnimScriptInstance && (AnimScriptInstance->RootMotionMode == ERootMotionMode::RootMotionFromEverything);
}

void USkeletalMeshComponent::ResetRootBodyIndex()
{
	RootBodyData.BodyIndex = INDEX_NONE;
	RootBodyData.TransformToRoot = FTransform::Identity;
}

void USkeletalMeshComponent::SetRootBodyIndex(int32 InBodyIndex)
{
	// this is getting called prior to initialization. 
	// @todo : better fix is to initialize it? overkilling it though. 
	if (InBodyIndex != INDEX_NONE)
	{
		RootBodyData.BodyIndex = InBodyIndex;
		RootBodyData.TransformToRoot = FTransform::Identity;

		// Only need to do further work if we have any bodies at all (ie physics state is created)
		if (Bodies.Num() > 0)
		{
			if (Bodies.IsValidIndex(RootBodyData.BodyIndex))
			{
				FBodyInstance* BI = Bodies[RootBodyData.BodyIndex];
				RootBodyData.TransformToRoot = GetComponentToWorld().GetRelativeTransform(BI->GetUnrealWorldTransform());
			}
			else
			{
				ResetRootBodyIndex();
			}
		}
	}
}

void USkeletalMeshComponent::RefreshMorphTargets()
{
	ResetMorphTargetCurves();

	if (GetSkeletalMeshAsset() && AnimScriptInstance)
	{
		// as this can be called from any worker thread (i.e. from CreateRenderState_Concurrent) we cant currently be doing parallel evaluation
		check(!IsRunningParallelEvaluation());
		AnimScriptInstance->RefreshCurves(this);

		for(UAnimInstance* LinkedInstance : LinkedInstances)
		{
			LinkedInstance->RefreshCurves(this);
		}
		
		if(PostProcessAnimInstance)
		{
			PostProcessAnimInstance->RefreshCurves(this);
		}
	}
	else if (USkeletalMeshComponent* LeaderSMC = Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get()))
	{
		if (LeaderSMC->AnimScriptInstance)
		{
			LeaderSMC->AnimScriptInstance->RefreshCurves(this);
		}
	}
	
	UpdateMorphTargetOverrideCurves();
}

void USkeletalMeshComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

#if WITH_EDITOR
	if (GetSkeletalMeshAsset() && GetSkeletalMeshAsset()->IsCompiling())
	{
		return;
	}

	if (SceneProxy)
	{
		UpdatePoseWatches();
		FPoseWatchDynamicData* NewDynamicData = new FPoseWatchDynamicData(this);

		FSkeletalMeshSceneProxy* TargetProxy = (FSkeletalMeshSceneProxy*)SceneProxy;

		ENQUEUE_RENDER_COMMAND(PoseWatchDynamicDataCommand)(
			[TargetProxy, NewDynamicData](FRHICommandListImmediate& RHICommandList)
			{
				if (TargetProxy->PoseWatchDynamicData)
				{
					delete TargetProxy->PoseWatchDynamicData;
				}

				TargetProxy->PoseWatchDynamicData = NewDynamicData;
			}
		);
	} //-V773
#endif
}

void USkeletalMeshComponent::ParallelAnimationEvaluation() 
{
	if (AnimEvaluationContext.bDoInterpolation)
	{
		PerformAnimationProcessing(AnimEvaluationContext.SkeletalMesh, AnimEvaluationContext.AnimInstance, AnimEvaluationContext.bDoEvaluation, AnimEvaluationContext.CachedComponentSpaceTransforms, AnimEvaluationContext.CachedBoneSpaceTransforms, AnimEvaluationContext.RootBoneTranslation, AnimEvaluationContext.CachedCurve, AnimEvaluationContext.CachedCustomAttributes);
	}
	else
	{
		PerformAnimationProcessing(AnimEvaluationContext.SkeletalMesh, AnimEvaluationContext.AnimInstance, AnimEvaluationContext.bDoEvaluation, AnimEvaluationContext.ComponentSpaceTransforms, AnimEvaluationContext.BoneSpaceTransforms, AnimEvaluationContext.RootBoneTranslation, AnimEvaluationContext.Curve, AnimEvaluationContext.CustomAttributes);
	}

	ParallelDuplicateAndInterpolate(AnimEvaluationContext);

	if(AnimEvaluationContext.bDoEvaluation || AnimEvaluationContext.bDoInterpolation)
	{
		if(AnimEvaluationContext.AnimInstance)
		{
			AnimEvaluationContext.AnimInstance->UpdateCurvesToEvaluationContext(AnimEvaluationContext);
		}
		else if(AnimEvaluationContext.PostProcessAnimInstance)
		{
			AnimEvaluationContext.PostProcessAnimInstance->UpdateCurvesToEvaluationContext(AnimEvaluationContext);
		}
	}
}

void USkeletalMeshComponent::ParallelDuplicateAndInterpolate(FAnimationEvaluationContext& InAnimEvaluationContext)
{
	if(CVarUseParallelAnimationInterpolation.GetValueOnAnyThread() != 0)
	{
		if (InAnimEvaluationContext.bDuplicateToCacheCurve)
		{
			ensureAlwaysMsgf(InAnimEvaluationContext.Curve.IsValid(), TEXT("Animation Curve is invalid (%s). TotalCount(%d) "),
				*GetPathNameSafe(GetSkeletalMeshAsset()), InAnimEvaluationContext.Curve.NumValidCurveCount);
			InAnimEvaluationContext.CachedCurve.CopyFrom(InAnimEvaluationContext.Curve);
		}

		if (InAnimEvaluationContext.bDuplicateToCachedAttributes)
		{
			InAnimEvaluationContext.CachedCustomAttributes.CopyFrom(InAnimEvaluationContext.CustomAttributes);
		}

		if (InAnimEvaluationContext.bDuplicateToCacheBones)
		{
			InAnimEvaluationContext.CachedComponentSpaceTransforms.Reset();
			InAnimEvaluationContext.CachedComponentSpaceTransforms.Append(InAnimEvaluationContext.ComponentSpaceTransforms);
			InAnimEvaluationContext.CachedBoneSpaceTransforms.Reset();
			InAnimEvaluationContext.CachedBoneSpaceTransforms.Append(InAnimEvaluationContext.BoneSpaceTransforms);
		}

		if (InAnimEvaluationContext.bDoInterpolation)
		{
			SCOPE_CYCLE_COUNTER(STAT_InterpolateSkippedFrames);

			float Alpha;
			if(bEnableUpdateRateOptimizations && AnimUpdateRateParams)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (AnimScriptInstance)
				{
					AnimScriptInstance->OnUROPreInterpolation();
					AnimScriptInstance->OnUROPreInterpolation_AnyThread(InAnimEvaluationContext);
				}

				for(UAnimInstance* LinkedInstance : LinkedInstances)
				{
					LinkedInstance->OnUROPreInterpolation();
					LinkedInstance->OnUROPreInterpolation_AnyThread(InAnimEvaluationContext);
				}

				if(PostProcessAnimInstance)
				{
					PostProcessAnimInstance->OnUROPreInterpolation();
					PostProcessAnimInstance->OnUROPreInterpolation_AnyThread(InAnimEvaluationContext);
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS

				Alpha = AnimUpdateRateParams->GetInterpolationAlpha();
			}
			else
			{
				Alpha = ExternalInterpolationAlpha;
			}

			FAnimationRuntime::LerpBoneTransforms(InAnimEvaluationContext.BoneSpaceTransforms, InAnimEvaluationContext.CachedBoneSpaceTransforms, Alpha, RequiredBones);
			InAnimEvaluationContext.SkeletalMesh->FillComponentSpaceTransforms(InAnimEvaluationContext.BoneSpaceTransforms, FillComponentSpaceTransformsRequiredBones, InAnimEvaluationContext.ComponentSpaceTransforms);

			// interpolate curve
			InAnimEvaluationContext.Curve.LerpTo(InAnimEvaluationContext.CachedCurve, Alpha);

			UE::Anim::Attributes::InterpolateAttributes(InAnimEvaluationContext.CustomAttributes, InAnimEvaluationContext.CachedCustomAttributes, Alpha);
		}
	}
}

void USkeletalMeshComponent::CompleteParallelAnimationEvaluation(bool bDoPostAnimEvaluation)
{
	SCOPED_NAMED_EVENT(USkeletalMeshComponent_CompleteParallelAnimationEvaluation, FColor::Yellow);
	ParallelAnimationEvaluationTask.SafeRelease(); //We are done with this task now, clean up!

	if (bDoPostAnimEvaluation && (AnimEvaluationContext.AnimInstance == AnimScriptInstance) && (AnimEvaluationContext.SkeletalMesh == GetSkeletalMeshAsset()) && (AnimEvaluationContext.ComponentSpaceTransforms.Num() == GetNumComponentSpaceTransforms()))
	{
		SwapEvaluationContextBuffers();

		PostAnimEvaluation(AnimEvaluationContext);
	}
	
	AnimEvaluationContext.Clear();
}

bool USkeletalMeshComponent::HandleExistingParallelEvaluationTask(bool bBlockOnTask, bool bPerformPostAnimEvaluation)
{
	// We are already processing eval and we are not inside it
	if (IsRunningParallelEvaluation()
#if TASKGRAPH_NEW_FRONTEND 
		&& ParallelAnimationEvaluationTask->IsAwaitable()
#endif
	)
	{
		if (bBlockOnTask)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMeshComponent::BlockOnParallelEvaluationTask);
			check(IsInGameThread()); // Only attempt this from game thread!
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(ParallelAnimationEvaluationTask, ENamedThreads::GameThread);
			CompleteParallelAnimationEvaluation(bPerformPostAnimEvaluation); //Perform completion now
		}
		return true;
	}
	return false;
}

void USkeletalMeshComponent::SuspendClothingSimulation()
{
	bClothingSimulationSuspended = true;
}

void USkeletalMeshComponent::ResumeClothingSimulation()
{
	bClothingSimulationSuspended = false;
	ForceClothNextUpdateTeleport();
}

bool USkeletalMeshComponent::IsClothingSimulationSuspended() const
{
	return bClothingSimulationSuspended;
}

void USkeletalMeshComponent::BindClothToLeaderPoseComponent()
{
	if(USkeletalMeshComponent* LeaderComp = Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get()))
	{
		if(GetSkeletalMeshAsset() != LeaderComp->GetSkeletalMeshAsset())
		{
			// Not the same mesh, can't bind
			return;
		}

		if(ClothingSimulation && LeaderComp->ClothingSimulation)
		{
			bDisableClothSimulation = true;

			// When we extract positions from now we'll just take the Leader components positions
			bBindClothToLeaderComponent = true;
		}
	}
}

void USkeletalMeshComponent::UnbindClothFromLeaderPoseComponent(bool bRestoreSimulationSpace)
{
	USkeletalMeshComponent* LeaderComp = Cast<USkeletalMeshComponent>(LeaderPoseComponent.Get());
	if(LeaderComp && bBindClothToLeaderComponent)
	{
		if(ClothingSimulation)
		{
			bDisableClothSimulation = false;
		}

		bBindClothToLeaderComponent = false;
	}
}

void USkeletalMeshComponent::SetAllowRigidBodyAnimNode(bool bInAllow, bool bReinitAnim)
{
	if(bDisableRigidBodyAnimNode == bInAllow)
	{
		bDisableRigidBodyAnimNode = !bInAllow;

		if(bReinitAnim && bRegistered)
		{
			// need to reinitialize rigid body nodes for new setting to take effect
			if (AnimScriptInstance)
			{
				AnimScriptInstance->InitializeAnimation();
			}
			if (PostProcessAnimInstance)
			{
				PostProcessAnimInstance->InitializeAnimation();
			}
		}
	}
}

void USkeletalMeshComponent::SetAllowClothActors(bool bInAllow)
{
	bAllowClothActors = bInAllow;
}

bool USkeletalMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	UPhysicsAsset* PhysicsAsset = GetPhysicsAsset();
	if (PhysicsAsset && GetComponentTransform().GetScale3D().IsUniform())
	{
		const int32 MaxBodies = PhysicsAsset->SkeletalBodySetups.Num();
		for (int32 Idx = 0; Idx < MaxBodies; Idx++)
		{
			UBodySetup* const BS = PhysicsAsset->SkeletalBodySetups[Idx];
			int32 const BoneIndex = BS ? GetBoneIndex(BS->BoneName) : INDEX_NONE;

			if (BoneIndex != INDEX_NONE)
			{
				FTransform WorldBoneTransform = GetBoneTransform(BoneIndex, GetComponentTransform());
				if (FMath::Abs(WorldBoneTransform.GetDeterminant()) > (float)UE_KINDA_SMALL_NUMBER)
				{
					GeomExport.ExportRigidBodySetup(*BS, WorldBoneTransform);
				}
			}
		}
	}

	// skip fallback export of body setup data
	return false;
}

void USkeletalMeshComponent::FinalizeBoneTransform() 
{
	Super::FinalizeBoneTransform();

	// After pose has been finalized, dispatch AnimNotifyEvents in case they want to use up to date pose.
	// (For example attaching particle systems to up to date sockets).

	/////////////////////////////////////////////////////////////////////////////
	// Notify / Event Handling!
	// This can do anything to our component (including destroy it) 
	// Any code added after this point needs to take that into account
	/////////////////////////////////////////////////////////////////////////////

	ConditionallyDispatchQueuedAnimEvents();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnBoneTransformsFinalized.Broadcast();  // Deprecated in 4.27
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	OnBoneTransformsFinalizedMC.Broadcast();

	TRACE_SKELETAL_MESH_COMPONENT(this);
}

void USkeletalMeshComponent::GetCurrentRefToLocalMatrices(TArray<FMatrix44f>& OutRefToLocals, int32 InLodIdx) const
{
	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	if(SkelMesh)
	{
		FSkeletalMeshRenderData* RenderData = SkelMesh->GetResourceForRendering();
		if (ensureMsgf(RenderData->LODRenderData.IsValidIndex(InLodIdx),
			TEXT("GetCurrentRefToLocalMatrices (SkelMesh :%s) input LODIndex (%d) doesn't match with render data size (%d)."),
			*SkelMesh->GetPathName(), InLodIdx, RenderData->LODRenderData.Num()))
		{
			UpdateRefToLocalMatrices(OutRefToLocals, this, RenderData, InLodIdx, nullptr);
		}
		else
		{
			const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
			OutRefToLocals.AddUninitialized(RefSkeleton.GetNum());
			for (int32 Index = 0; Index < OutRefToLocals.Num(); ++Index)
			{
				OutRefToLocals[Index] = FMatrix44f::Identity;
			}
		}
	}
}

bool USkeletalMeshComponent::ShouldUpdatePostProcessInstance() const
{
	return PostProcessAnimInstance && !bDisablePostProcessBlueprint;
}

bool USkeletalMeshComponent::ShouldPostUpdatePostProcessInstance() const
{
	return PostProcessAnimInstance && PostProcessAnimInstance->NeedsUpdate() && !bDisablePostProcessBlueprint;
}

bool USkeletalMeshComponent::ShouldEvaluatePostProcessInstance() const
{
	return PostProcessAnimInstance && !bDisablePostProcessBlueprint;
}

void USkeletalMeshComponent::SetRefPoseOverride(const TArray<FTransform>& NewRefPoseTransforms)
{
	Super::SetRefPoseOverride(NewRefPoseTransforms);
	bRequiredBonesUpToDate = false;
}

void USkeletalMeshComponent::ClearRefPoseOverride()
{
	Super::ClearRefPoseOverride();
	bRequiredBonesUpToDate = false;
}

FDelegateHandle USkeletalMeshComponent::RegisterOnPhysicsCreatedDelegate(const FOnSkelMeshPhysicsCreated& Delegate)
{
	return OnSkelMeshPhysicsCreated.Add(Delegate);
}

void USkeletalMeshComponent::UnregisterOnPhysicsCreatedDelegate(const FDelegateHandle& DelegateHandle)
{
	OnSkelMeshPhysicsCreated.Remove(DelegateHandle);
}

FDelegateHandle USkeletalMeshComponent::RegisterOnTeleportDelegate(const FOnSkelMeshTeleported& Delegate)
{
	return OnSkelMeshPhysicsTeleported.Add(Delegate);
}

void USkeletalMeshComponent::UnregisterOnTeleportDelegate(const FDelegateHandle& DelegateHandle)
{
	OnSkelMeshPhysicsTeleported.Remove(DelegateHandle);
}

FDelegateHandle USkeletalMeshComponent::RegisterOnBoneTransformsFinalizedDelegate(const FOnBoneTransformsFinalizedMultiCast::FDelegate& Delegate)
{
	return OnBoneTransformsFinalizedMC.Add(Delegate);
}

void USkeletalMeshComponent::UnregisterOnBoneTransformsFinalizedDelegate(const FDelegateHandle& DelegateHandle)
{
	OnBoneTransformsFinalizedMC.Remove(DelegateHandle);
}

bool USkeletalMeshComponent::MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit /*= nullptr*/, EMoveComponentFlags MoveFlags /*= MOVECOMP_NoFlags*/, ETeleportType Teleport /*= ETeleportType::None*/)
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if(World && World->IsGameWorld())
	{
		if (FBodyInstance* BI = GetBodyInstance())
		{
			//If the root body is simulating and we're told to move without teleportation we warn. This is hard to support because of bodies chained together which creates some ambiguity
			if (BI->IsInstanceSimulatingPhysics() && Teleport == ETeleportType::None && (MoveFlags&EMoveComponentFlags::MOVECOMP_SkipPhysicsMove) == 0)
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("MovingSimulatedSkeletalMesh", "Attempting to move a fully simulated skeletal mesh {0}. Please use the Teleport flag"),
					FText::FromString(GetPathNameSafe(this))));
			}
		}
	}
#endif

	bool bSuccess = Super::MoveComponentImpl(Delta, NewRotation, bSweep, OutHit, MoveFlags, Teleport);
	if(bSuccess && Teleport != ETeleportType::None)
	{
		// If a skeletal mesh component recieves a teleport we should reset any other dynamic simulations
		ResetAnimInstanceDynamics(Teleport);

		OnSkelMeshPhysicsTeleported.Broadcast();
	}

	return bSuccess;
}

void USkeletalMeshComponent::AddFollowerPoseComponent(USkinnedMeshComponent* SkinnedMeshComponent)
{
	Super::AddFollowerPoseComponent(SkinnedMeshComponent);

	if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(SkinnedMeshComponent))
	{
		SkeletalMeshComponent->bRequiredBonesUpToDate = false;
	}

	bRequiredBonesUpToDate = false;
}

void USkeletalMeshComponent::RemoveFollowerPoseComponent(USkinnedMeshComponent* SkinnedMeshComponent)
{
	Super::RemoveFollowerPoseComponent(SkinnedMeshComponent);

	if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(SkinnedMeshComponent))
	{
		SkeletalMeshComponent->bRequiredBonesUpToDate = false;
	}

	bRequiredBonesUpToDate = false;
}

void USkeletalMeshComponent::SnapshotPose(FPoseSnapshot& Snapshot)
{
	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	if (ensureAsRuntimeWarning(SkelMesh != nullptr))
	{
		const TArray<FTransform>& ComponentSpaceTMs = GetComponentSpaceTransforms();
		const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
		const TArray<FTransform>& RefPoseSpaceBaseTMs = RefSkeleton.GetRefBonePose();

		Snapshot.SkeletalMeshName = SkelMesh->GetFName();

		const int32 NumSpaceBases = ComponentSpaceTMs.Num();
		Snapshot.LocalTransforms.Reset(NumSpaceBases);
		Snapshot.LocalTransforms.AddUninitialized(NumSpaceBases);
		Snapshot.BoneNames.Reset(NumSpaceBases);
		Snapshot.BoneNames.AddUninitialized(NumSpaceBases);

		//Set root bone which is always evaluated.
		Snapshot.LocalTransforms[0] = ComponentSpaceTMs[0];
		Snapshot.BoneNames[0] = RefSkeleton.GetBoneName(0);

		int32 CurrentRequiredBone = 1;
		for (int32 ComponentSpaceIdx = 1; ComponentSpaceIdx < NumSpaceBases; ++ComponentSpaceIdx)
		{
			Snapshot.BoneNames[ComponentSpaceIdx] = RefSkeleton.GetBoneName(ComponentSpaceIdx);

			const bool bBoneHasEvaluated = FillComponentSpaceTransformsRequiredBones.IsValidIndex(CurrentRequiredBone) && ComponentSpaceIdx == FillComponentSpaceTransformsRequiredBones[CurrentRequiredBone];
			const int32 ParentIndex = RefSkeleton.GetParentIndex(ComponentSpaceIdx);
			ensureMsgf(ParentIndex != INDEX_NONE, TEXT("Getting an invalid parent bone for bone %d, but this should not be possible since this is not the root bone!"), ComponentSpaceIdx);

			const FTransform& ParentTransform = ComponentSpaceTMs[ParentIndex];
			const FTransform& ChildTransform = ComponentSpaceTMs[ComponentSpaceIdx];
			Snapshot.LocalTransforms[ComponentSpaceIdx] = bBoneHasEvaluated ? ChildTransform.GetRelativeTransform(ParentTransform) : RefPoseSpaceBaseTMs[ComponentSpaceIdx];

			if (bBoneHasEvaluated)
			{
				CurrentRequiredBone++;
			}
		}

		Snapshot.bIsValid = true;
	}
	else
	{
		Snapshot.bIsValid = false;
	}
}

void USkeletalMeshComponent::SetUpdateAnimationInEditor(const bool NewUpdateState)
{
	#if WITH_EDITOR
	if (IsRegistered())
	{
		bUpdateAnimationInEditor = NewUpdateState;
	}
	#endif
}

void USkeletalMeshComponent::SetUpdateClothInEditor(const bool NewUpdateState)
{
#if WITH_EDITOR
	if (IsRegistered())
	{
		bUpdateClothInEditor = NewUpdateState; 
	}
#endif
}

float USkeletalMeshComponent::GetTeleportRotationThreshold() const
{
	return TeleportDistanceThreshold;
}

void USkeletalMeshComponent::SetTeleportRotationThreshold(float Threshold)
{
	TeleportRotationThreshold = Threshold;
	ComputeTeleportRotationThresholdInRadians();
}

float USkeletalMeshComponent::GetTeleportDistanceThreshold() const
{
	return TeleportDistanceThreshold;
}

void USkeletalMeshComponent::SetTeleportDistanceThreshold(float Threshold)
{
	TeleportDistanceThreshold = Threshold;
	ComputeTeleportDistanceThresholdInRadians();
}

void USkeletalMeshComponent::ComputeTeleportRotationThresholdInRadians()
{
	ClothTeleportCosineThresholdInRad = FMath::Cos(FMath::DegreesToRadians(TeleportRotationThreshold));
}

void USkeletalMeshComponent::ComputeTeleportDistanceThresholdInRadians()
{
	ClothTeleportDistThresholdSquared = TeleportDistanceThreshold * TeleportDistanceThreshold;
}

void USkeletalMeshComponent::SetDisableAnimCurves(bool bInDisableAnimCurves)
{
	SetAllowAnimCurveEvaluation(!bInDisableAnimCurves);
}

void USkeletalMeshComponent::SetAllowAnimCurveEvaluation(bool bInAllow)
{
	if (bAllowAnimCurveEvaluation != bInAllow)
	{
		bAllowAnimCurveEvaluation = bInAllow;
		// clear cache uid version, so it will update required curves
		CachedAnimCurveUidVersion = 0;
	}
}

void USkeletalMeshComponent::AllowAnimCurveEvaluation(FName NameOfCurve, bool bAllow)
{
	// if allow is same as disallowed curve, which means it mismatches
	if (bAllow == DisallowedAnimCurves.Contains(NameOfCurve))
	{
		if (bAllow)
		{
			DisallowedAnimCurves.Remove(NameOfCurve);
			CachedAnimCurveUidVersion = 0;
		}
		else
		{
			DisallowedAnimCurves.Add(NameOfCurve);
			CachedAnimCurveUidVersion = 0;

		}
	}
}

void USkeletalMeshComponent::ResetAllowedAnimCurveEvaluation()
{
	DisallowedAnimCurves.Reset();
	CachedAnimCurveUidVersion = 0;
}

void USkeletalMeshComponent::SetAllowedAnimCurvesEvaluation(const TArray<FName>& List, bool bAllow)
{
	// Reset already clears the version - CachedAnimCurveUidVersion = 0;
	ResetAllowedAnimCurveEvaluation();
	if (bAllow)
	{
		struct FFilterDisallowedList
		{
			FFilterDisallowedList(const TArray<FName>& InAllowedList) : AllowedList(InAllowedList) {}

			FORCEINLINE bool operator()(const FName& Name) const
			{
				return AllowedList.Contains(Name);
			}
			const TArray<FName>& AllowedList;
		};

		if (GetSkeletalMeshAsset())
		{
			USkeleton* Skeleton = GetSkeletalMeshAsset()->GetSkeleton();
			if (Skeleton)
			{
				const FSmartNameMapping* Mapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
				if (Mapping != nullptr)
				{
					TArray<FName> CurveNames;
					Mapping->FillNameArray(CurveNames);

					DisallowedAnimCurves = CurveNames;
					DisallowedAnimCurves.RemoveAllSwap(FFilterDisallowedList(List));
				}
			}

		}
	}
	else
	{
		DisallowedAnimCurves = List;
	}
}

const TArray<FTransform>& USkeletalMeshComponent::GetCachedComponentSpaceTransforms() const
{
	return CachedComponentSpaceTransforms;
}


bool USkeletalMeshComponent::GetFloatAttribute_Ref(const FName& BoneName, const FName& AttributeName, float& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<float, FFloatAnimationAttribute>(BoneName, AttributeName, OutValue, OutValue, LookupType);
}

bool USkeletalMeshComponent::GetTransformAttribute_Ref(const FName& BoneName, const FName& AttributeName, FTransform& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	bool bResult = FindAttributeChecked<FTransform, FTransformAnimationAttribute>(BoneName, AttributeName, OutValue, OutValue, LookupType);
	return bResult;
}

bool USkeletalMeshComponent::GetIntegerAttribute_Ref(const FName& BoneName, const FName& AttributeName, int32& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<int32, FIntegerAnimationAttribute>(BoneName, AttributeName, OutValue, OutValue, LookupType);
}

bool USkeletalMeshComponent::GetStringAttribute_Ref(const FName& BoneName, const FName& AttributeName, FString& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<FString, FStringAnimationAttribute>(BoneName, AttributeName, OutValue, OutValue, LookupType);
}

bool USkeletalMeshComponent::GetFloatAttribute(const FName& BoneName, const FName& AttributeName, float DefaultValue, float& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<float, FFloatAnimationAttribute>(BoneName, AttributeName, DefaultValue, OutValue, LookupType);
}

bool USkeletalMeshComponent::GetTransformAttribute(const FName& BoneName, const FName& AttributeName, FTransform DefaultValue, FTransform& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<FTransform, FTransformAnimationAttribute>(BoneName, AttributeName, DefaultValue, OutValue, LookupType);
}

bool USkeletalMeshComponent::GetIntegerAttribute(const FName& BoneName, const FName& AttributeName, int32 DefaultValue, int32& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<int32, FIntegerAnimationAttribute>(BoneName, AttributeName, DefaultValue, OutValue, LookupType);
}

bool USkeletalMeshComponent::GetStringAttribute(const FName& BoneName, const FName& AttributeName, FString DefaultValue, FString& OutValue, ECustomBoneAttributeLookup LookupType /*= ECustomBoneAttributeLookup::BoneOnly*/)
{
	return FindAttributeChecked<FString, FStringAnimationAttribute>(BoneName, AttributeName, DefaultValue, OutValue, LookupType);
}

template<typename DataType, typename CustomAttributeType>
bool USkeletalMeshComponent::FindAttributeChecked(const FName& BoneName, const FName& AttributeName, DataType DefaultValue, DataType& OutValue, ECustomBoneAttributeLookup LookupType)
{
	OutValue = DefaultValue;
	bool bFound = false;

	USkeletalMesh* SkelMesh = GetSkeletalMeshAsset();
	if (SkelMesh)
	{
		const UE::Anim::FMeshAttributeContainer& Attributes = GetCustomAttributes();
		const int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(BoneName);

		const CustomAttributeType* AttributePtr = Attributes.Find<CustomAttributeType>(UE::Anim::FAttributeId(AttributeName, FCompactPoseBoneIndex(BoneIndex)));

		if (AttributePtr == nullptr && LookupType != ECustomBoneAttributeLookup::BoneOnly)
		{
			if (LookupType == ECustomBoneAttributeLookup::ImmediateParent)
			{
				const int32 ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					AttributePtr = Attributes.Find<CustomAttributeType>(UE::Anim::FAttributeId(AttributeName, FCompactPoseBoneIndex(ParentIndex)));
				}
			}
			else if (LookupType == ECustomBoneAttributeLookup::ParentHierarchy)
			{
				int32 SearchBoneIndex = BoneIndex;
				int32 ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(SearchBoneIndex);

				while (ParentIndex != INDEX_NONE)
				{
					AttributePtr = Attributes.Find<CustomAttributeType>(UE::Anim::FAttributeId(AttributeName, FCompactPoseBoneIndex(ParentIndex)));
					if (AttributePtr)
					{
						break;
					}

					SearchBoneIndex = ParentIndex;
					ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(SearchBoneIndex);
				}
			}
		}

		if (AttributePtr != nullptr)
		{
			OutValue = AttributePtr->Value;
			bFound = true;
		}
	}

	return bFound;
}


TArray<FTransform> USkeletalMeshComponent::GetBoneSpaceTransforms() 
{
	// We may be doing parallel evaluation on the current anim instance
	// Calling this here with true will block this init till that thread completes
	// and it is safe to continue
	const bool bBlockOnTask = true; // wait on evaluation task so it is safe to swap the buffers
	const bool bPerformPostAnimEvaluation = true; // Do PostEvaluation so we make sure to swap the buffers back. 
	HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return BoneSpaceTransforms;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if WITH_EDITOR
void USkeletalMeshComponent::UpdatePoseWatches()
{
	PoseWatches.Empty();
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (AnimInstance)
	{
		if (AnimInstance->IsBeingDebugged())
		{
			UAnimBlueprintGeneratedClass* AnimBPGenClass = CastChecked<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass());
			if (AnimBPGenClass)
			{
				if (USkeletalMesh* TmpSkeletalMesh = GetSkeletalMeshAsset())
				{
					FAnimBlueprintDebugData& DebugData = AnimBPGenClass->GetAnimBlueprintDebugData();
					for (FAnimNodePoseWatch& PoseWatch : DebugData.AnimNodePoseWatch)
					{
						if (const UPoseWatchPoseElement* PoseWatchPoseElement = PoseWatch.PoseWatchPoseElement)
						{
							if (PoseWatchPoseElement->GetIsEnabled() && PoseWatchPoseElement->GetIsVisible())
							{
								PoseWatches.Add(PoseWatch); // Copy
							}
						}
					}
				}
			}
		}
	}
}
#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE

