// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ValidationRHICommon.h: Public Validation RHI definitions.
=============================================================================*/

#pragma once

#include "Experimental/ConcurrentLinearAllocator.h"

#if ENABLE_RHI_VALIDATION
extern RHI_API bool GRHIValidationEnabled;
#else
const bool GRHIValidationEnabled = false;
#endif

#if ENABLE_RHI_VALIDATION

class FRHIUniformBuffer;
class FValidationRHI;
struct FRHITextureCreateDesc;

// Forward declaration of function defined in RHIUtilities.h
static inline bool IsStencilFormat(EPixelFormat Format);

namespace RHIValidation
{
	struct FStaticUniformBuffers
	{
		TArray<FRHIUniformBuffer*> Bindings;
		bool bInSetPipelineStateCall{};

		void Reset();
		void ValidateSetShaderUniformBuffer(FRHIUniformBuffer* UniformBuffer);
	};

	static bool IsValidCopyFormat(EPixelFormat SourceFormat, EPixelFormat DestFormat)
	{
		if (SourceFormat == DestFormat)
		{
			return true;
		}
		// Acceptable conversions follow. Add more as required.
		if (SourceFormat == PF_R32G32_UINT && (DestFormat == PF_DXT1 || DestFormat == PF_BC4))
		{
			return true;
		}
		if (SourceFormat == PF_R32G32B32A32_UINT && (DestFormat == PF_DXT3 || DestFormat == PF_DXT5 || DestFormat == PF_BC5 || DestFormat == PF_BC7))
		{
			return true;
		}
		// No valid conversion found
		return false;
	}

	class  FTracker;
	class  FResource;
	class  FTextureResource;
	struct FOperation;
	struct FSubresourceState;
	struct FSubresourceRange;
	struct FResourceIdentity;

	enum class ELoggingMode
	{
		None,
		Manual,
		Automatic
	};

	enum class EResourcePlane
	{
		// Common plane index. Used for all resources
		Common = 0,

		// Additional plane indices for depth stencil resources
		Stencil = 1,
		Htile = 0, // @todo: do we need to track htile resources?

		// Additional plane indices for color render targets
		Cmask = 0, // @todo: do we need to track cmask resources?
		Fmask = 0, // @todo: do we need to track fmask resources?

		Max = 2
	};

	struct FSubresourceIndex
	{
		static constexpr int32 kWholeResource = -1;

		int32 MipIndex;
		int32 ArraySlice;
		int32 PlaneIndex;

		constexpr FSubresourceIndex()
			: MipIndex(kWholeResource)
			, ArraySlice(kWholeResource)
			, PlaneIndex(kWholeResource)
		{}

		constexpr FSubresourceIndex(int32 InMipIndex, int32 InArraySlice, int32 InPlaneIndex)
			: MipIndex(InMipIndex)
			, ArraySlice(InArraySlice)
			, PlaneIndex(InPlaneIndex)
		{}

		inline bool IsWholeResource() const
		{
			return MipIndex == kWholeResource
				&& ArraySlice == kWholeResource
				&& PlaneIndex == kWholeResource;
		}
	};

	struct FState
	{
		ERHIAccess Access;
		ERHIPipeline Pipelines;

		FState() = default;

		FState(ERHIAccess InAccess, ERHIPipeline InPipelines)
			: Access(InAccess)
			, Pipelines(InPipelines)
		{}

		inline bool operator == (const FState& RHS) const
		{
			return Access == RHS.Access &&
				Pipelines == RHS.Pipelines;
		}

		inline bool operator != (const FState& RHS) const
		{
			return !(*this == RHS);
		}

		inline FString ToString() const
		{
			return FString::Printf(TEXT("Access: %s, Pipelines: %s"),
				*GetRHIAccessName(Access),
				*GetRHIPipelineName(Pipelines));
		}
	};

	struct FSubresourceState
	{
		struct FPipelineState
		{
			FPipelineState()
			{
				Current.Access = ERHIAccess::Unknown;
				Current.Pipelines = ERHIPipeline::Graphics;
				Previous = Current;
			}

			FState Previous;
			FState Current;
			EResourceTransitionFlags Flags = EResourceTransitionFlags::None;

			// True when a BeginTransition has been issued, and false when the transition has been ended.
			bool bTransitioning = false;

			// True when the resource has been used within a Begin/EndUAVOverlap region.
			bool bUsedWithAllUAVsOverlap = false;

			// True if the calling code explicitly enabled overlapping on this UAV.
			bool bExplicitAllowUAVOverlap = false;
			bool bUsedWithExplicitUAVsOverlap = false;

			// Pointer to the previous create/begin transition backtraces if logging is enabled for this resource.
			void* CreateTransitionBacktrace = nullptr;
			void* BeginTransitionBacktrace = nullptr;
		};

		TRHIPipelineArray<FPipelineState> States;

		void BeginTransition   (FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& CurrentStateFromRHI, const FState& TargetState, EResourceTransitionFlags NewFlags, ERHIPipeline Pipeline, void* CreateTrace);
		void EndTransition     (FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& CurrentStateFromRHI, const FState& TargetState, ERHIPipeline Pipeline, void* CreateTrace);
		void Assert            (FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& RequiredState, bool bAllowAllUAVsOverlap);
		void AssertTracked     (FResource* Resource, FSubresourceIndex const& SubresourceIndex, const FState& TrackedState);
		void SpecificUAVOverlap(FResource* Resource, FSubresourceIndex const& SubresourceIndex, ERHIPipeline Pipeline, bool bAllow);
	};

	struct FSubresourceRange
	{
		uint32 MipIndex, NumMips;
		uint32 ArraySlice, NumArraySlices;
		uint32 PlaneIndex, NumPlanes;

		FSubresourceRange() = default;

		FSubresourceRange(uint32 InMipIndex, uint32 InNumMips, uint32 InArraySlice, uint32 InNumArraySlices, uint32 InPlaneIndex, uint32 InNumPlanes)
			: MipIndex(InMipIndex)
			, NumMips(InNumMips)
			, ArraySlice(InArraySlice)
			, NumArraySlices(InNumArraySlices)
			, PlaneIndex(InPlaneIndex)
			, NumPlanes(InNumPlanes)
		{}

		inline bool operator == (FSubresourceRange const& RHS) const
		{
			return MipIndex == RHS.MipIndex
				&& NumMips == RHS.NumMips
				&& ArraySlice == RHS.ArraySlice
				&& NumArraySlices == RHS.NumArraySlices
				&& PlaneIndex == RHS.PlaneIndex
				&& NumPlanes == RHS.NumPlanes;
		}

		inline bool operator != (FSubresourceRange const& RHS) const
		{
			return !(*this == RHS);
		}

		inline bool IsWholeResource(FResource& Resource) const;
	};

	struct FResourceIdentity
	{
		FResource* Resource;
		FSubresourceRange SubresourceRange;

		FResourceIdentity() = default;

		inline bool operator == (FResourceIdentity const& RHS) const
		{
			return Resource == RHS.Resource
				&& SubresourceRange == RHS.SubresourceRange;
		}

		inline bool operator != (FResourceIdentity const& RHS) const
		{
			return !(*this == RHS);
		}
	};

	struct FTransientState
	{
		FTransientState() = default;

		enum class EStatus : uint8
		{
			None,
			Acquired,
			Discarded
		};

		FTransientState(ERHIAccess InitialAccess)
			: bTransient(InitialAccess == ERHIAccess::Discard)
		{}

		void* AcquireBacktrace = nullptr;
		void* DiscardBacktrace = nullptr;

		bool bTransient = false;
		EStatus Status = EStatus::None;

		FORCEINLINE bool IsAcquired() const { return Status == EStatus::Acquired; }
		FORCEINLINE bool IsDiscarded() const { return Status == EStatus::Discarded; }

		void Acquire(FResource* Resource, void* CreateTrace);
		void Discard(FResource* Resource, void* CreateTrace);

		static void AliasingOverlap(FResource* ResourceBefore, FResource* ResourceAfter, void* CreateTrace);
	};

	class FResource
	{
		friend FTracker;
		friend FTextureResource;
		friend FOperation;
		friend FSubresourceState;
		friend FSubresourceRange;
		friend FTransientState;
		friend FValidationRHI;

	protected:
		uint32 NumMips = 0;
		uint32 NumArraySlices = 0;
		uint32 NumPlanes = 0;
		FTransientState TransientState;
		ERHIAccess TrackedAccess = ERHIAccess::Unknown;

	private:
		FString DebugName;

		FSubresourceState WholeResourceState;
		TArray<FSubresourceState> SubresourceStates;

		mutable FThreadSafeCounter NumOpRefs;

		inline void EnumerateSubresources(FSubresourceRange const& SubresourceRange, TFunctionRef<void(FSubresourceState&, FSubresourceIndex const&)> Callback, bool bBeginTransition = false);

	public:
		~FResource()
		{
			checkf(NumOpRefs.GetValue() == 0, TEXT("RHI validation resource '%s' is being deleted, but it is still queued in the replay command stream!"), *DebugName);
		}

		ELoggingMode LoggingMode = ELoggingMode::None;

		RHI_API void SetDebugName(const TCHAR* Name, const TCHAR* Suffix = nullptr);
		inline const TCHAR* GetDebugName() const { return DebugName.Len() ? *DebugName : nullptr; }

		inline bool IsBarrierTrackingInitialized() const { return NumMips > 0 && NumArraySlices > 0; }

		inline void AddOpRef() const
		{
			NumOpRefs.Increment();
		}

		inline void ReleaseOpRef() const
		{
			const int32 RefCount = NumOpRefs.Decrement();
			check(RefCount >= 0);
		}

		inline ERHIAccess GetTrackedAccess() const
		{
			return TrackedAccess;
		}

		inline FSubresourceRange GetWholeResourceRange()
		{
			checkSlow(NumMips > 0 && NumArraySlices > 0 && NumPlanes > 0);

			FSubresourceRange SubresourceRange;
			SubresourceRange.MipIndex = 0;
			SubresourceRange.ArraySlice = 0;
			SubresourceRange.PlaneIndex = 0;
			SubresourceRange.NumMips = NumMips;
			SubresourceRange.NumArraySlices = NumArraySlices;
			SubresourceRange.NumPlanes = NumPlanes;
			return SubresourceRange;
		}

		inline FResourceIdentity GetWholeResourceIdentity()
		{
			FResourceIdentity Identity;
			Identity.Resource = this;
			Identity.SubresourceRange = GetWholeResourceRange();
			return Identity;
		}

	protected:
		inline void InitBarrierTracking(int32 InNumMips, int32 InNumArraySlices, int32 InNumPlanes, ERHIAccess InResourceState, const TCHAR* InDebugName)
		{
			checkSlow(InNumMips > 0 && InNumArraySlices > 0 && InNumPlanes > 0);
			check(InResourceState != ERHIAccess::Unknown);

			NumMips = InNumMips;
			NumArraySlices = InNumArraySlices;
			NumPlanes = InNumPlanes;
			TransientState = FTransientState(InResourceState);
			TrackedAccess = InResourceState;

			for (ERHIPipeline Pipeline : GetRHIPipelines())
			{
				auto& State = WholeResourceState.States[Pipeline];

				State.Current.Access = InResourceState;
				State.Current.Pipelines = Pipeline;
				State.Previous = State.Current;
			}

			if (InDebugName != nullptr)
			{
				SetDebugName(InDebugName);
			}
		}
	};

	inline bool FSubresourceRange::IsWholeResource(FResource& Resource) const
	{
		return MipIndex == 0
			&& ArraySlice == 0
			&& PlaneIndex == 0
			&& NumMips == Resource.NumMips
			&& NumArraySlices == Resource.NumArraySlices
			&& NumPlanes == Resource.NumPlanes;
	}

	class FBufferResource : public FResource
	{
	public:
		inline void InitBarrierTracking(ERHIAccess InResourceState, const TCHAR* InDebugName)
		{
			FResource::InitBarrierTracking(1, 1, 1, InResourceState, InDebugName);
		}
	};

	class FAccelerationStructureResource : public FBufferResource
	{
	public:
	};

	class FTextureResource
	{
	private:
		// Don't use inheritance here. Because FRHITextureReferences exist, we have to
		// call through a virtual to get the real underlying tracker resource from an FRHITexture*.
		FResource PRIVATE_TrackerResource;

	public:
		FTextureResource() = default;
		RHI_API FTextureResource(FRHITextureCreateDesc const& CreateDesc);

		virtual ~FTextureResource() {}

		virtual FResource* GetTrackerResource() { return &PRIVATE_TrackerResource; }

		void InitBarrierTracking(FRHITextureCreateDesc const& CreateDesc);

		inline bool IsBarrierTrackingInitialized() const
		{
			// @todo: clean up const_cast once FRHITextureReference is removed and
			// we don't need to keep a separate PRIVATE_TrackerResource object.
			return const_cast<FTextureResource*>(this)->GetTrackerResource()->IsBarrierTrackingInitialized();
		}

		inline void InitBarrierTracking(int32 InNumMips, int32 InNumArraySlices, EPixelFormat PixelFormat, ETextureCreateFlags Flags, ERHIAccess InResourceState, const TCHAR* InDebugName)
		{
			FResource* Resource = GetTrackerResource();
			if (!Resource)
				return;

			int32 InNumPlanes = 1;

			// @todo: htile tracking
			if (IsStencilFormat(PixelFormat))
			{
				InNumPlanes = 2; // Depth + Stencil
			}
			else
			{
				InNumPlanes = 1; // Depth only
			}

			Resource->InitBarrierTracking(InNumMips, InNumArraySlices, InNumPlanes, InResourceState, InDebugName);
		}

		inline FResourceIdentity GetViewIdentity(uint32 InMipIndex, uint32 InNumMips, uint32 InArraySlice, uint32 InNumArraySlices, uint32 InPlaneIndex, uint32 InNumPlanes)
		{
			FResource* Resource = GetTrackerResource();

			checkSlow((InMipIndex + InNumMips) <= Resource->NumMips);
			checkSlow((InArraySlice + InNumArraySlices) <= Resource->NumArraySlices);
			checkSlow((InPlaneIndex + InNumPlanes) <= Resource->NumPlanes);

			if (InNumMips == 0)
			{
				InNumMips = Resource->NumMips;
			}
			if (InNumArraySlices == 0)
			{
				InNumArraySlices = Resource->NumArraySlices;
			}
			if (InNumPlanes == 0)
			{
				InNumPlanes = Resource->NumPlanes;
			}

			FResourceIdentity Identity;
			Identity.Resource = Resource;
			Identity.SubresourceRange.MipIndex = InMipIndex;
			Identity.SubresourceRange.NumMips = InNumMips;
			Identity.SubresourceRange.ArraySlice = InArraySlice;
			Identity.SubresourceRange.NumArraySlices = InNumArraySlices;
			Identity.SubresourceRange.PlaneIndex = InPlaneIndex;
			Identity.SubresourceRange.NumPlanes = InNumPlanes;
			return Identity;
		}

		inline FResourceIdentity GetTransitionIdentity(const FRHITransitionInfo& Info)
		{
			FResource* Resource = GetTrackerResource();

			FResourceIdentity Identity;
			Identity.Resource = Resource;

			if (Info.IsAllMips())
			{
				Identity.SubresourceRange.MipIndex = 0;
				Identity.SubresourceRange.NumMips = Resource->NumMips;
			}
			else
			{
				check(Info.MipIndex < uint32(Resource->NumMips));
				Identity.SubresourceRange.MipIndex = Info.MipIndex;
				Identity.SubresourceRange.NumMips = 1;
			}

			if (Info.IsAllArraySlices())
			{
				Identity.SubresourceRange.ArraySlice = 0;
				Identity.SubresourceRange.NumArraySlices = Resource->NumArraySlices;
			}
			else
			{
				check(Info.ArraySlice < uint32(Resource->NumArraySlices));
				Identity.SubresourceRange.ArraySlice = Info.ArraySlice;
				Identity.SubresourceRange.NumArraySlices = 1;
			}

			if (Info.IsAllPlaneSlices())
			{
				Identity.SubresourceRange.PlaneIndex = 0;
				Identity.SubresourceRange.NumPlanes = Resource->NumPlanes;
			}
			else
			{
				check(Info.PlaneSlice < uint32(Resource->NumPlanes));
				Identity.SubresourceRange.PlaneIndex = Info.PlaneSlice;
				Identity.SubresourceRange.NumPlanes = 1;
			}

			return Identity;
		}

		inline FResourceIdentity GetWholeResourceIdentity()
		{
			return GetTrackerResource()->GetWholeResourceIdentity();
		}

		inline FResourceIdentity GetWholeResourceIdentitySRV()
		{
			FResourceIdentity Identity = GetWholeResourceIdentity();

			// When binding a whole texture for shader read (SRV), we only use the first plane.
			// Other planes like stencil require a separate view to access for read in the shader.
			Identity.SubresourceRange.NumPlanes = 1;

			return Identity;
		}
	};

	struct FView
	{
		FView()
		{
			ResetViewIdentity();
		}

		void ResetViewIdentity()
		{
			ViewIdentity.Resource = nullptr;
			ViewIdentity.SubresourceRange = FSubresourceRange(0, 0, 0, 0, 0, 0);
		}

		FResourceIdentity ViewIdentity;
	};

	struct FShaderResourceView : public FView
	{};

	struct FUnorderedAccessView : public FView
	{};

	struct FFence
	{
		bool bSignaled = false;
	};

	enum class EReplayStatus
	{
		Normal = 0b00,
		Signaled = 0b01,
		Waiting = 0b10
	};
	ENUM_CLASS_FLAGS(EReplayStatus);

	enum class EOpType
	{
		BeginTransition,
		EndTransition,
		SetTrackedAccess,
		AliasingOverlap,
		AcquireTransient,
		DiscardTransient,
		Assert,
		Rename,
		Signal,
		Wait,
		AllUAVsOverlap,
		SpecificUAVOverlap,
		PushBreadcrumb,
		PopBreadcrumb
	};

	struct FUniformBufferResource
	{
		uint64 AllocatedFrameID = 0;
		bool bContainsNullContents = false;
		EUniformBufferUsage UniformBufferUsage;
		void* AllocatedCallstack;

		void InitLifetimeTracking(uint64 FrameID, const void* Contents, EUniformBufferUsage Usage);
		void UpdateAllocation(uint64 FrameID);
		void ValidateLifeTime();
	};

	using FBreadcrumbStack = TArray<const TCHAR*, FConcurrentLinearArrayAllocator>;

	struct FOperation
	{
		EOpType Type;

		union
		{
			struct
			{
				FResourceIdentity Identity;
				FState PreviousState;
				FState NextState;
				EResourceTransitionFlags Flags;
				void* CreateBacktrace;
			} Data_BeginTransition;

			struct
			{
				FResourceIdentity Identity;
				FState PreviousState;
				FState NextState;
				void* CreateBacktrace;
			} Data_EndTransition;

			struct
			{
				FResource* Resource;
				ERHIAccess Access;
			} Data_SetTrackedAccess;

			struct
			{
				FResource* ResourceBefore;
				FResource* ResourceAfter;
				void* CreateBacktrace;
			} Data_AliasingOverlap;

			struct
			{
				FResource* Resource;
				void* CreateBacktrace;
			} Data_AcquireTransient;

			struct
			{
				FResource* Resource;
				void* CreateBacktrace;
			} Data_DiscardTransient;

			struct
			{
				FResourceIdentity Identity;
				FState RequiredState;
			} Data_Assert;

			struct
			{
				FResource* Resource;
				TCHAR* DebugName;
				const TCHAR* Suffix;
			} Data_Rename;

			struct
			{
				FFence* Fence;
				ERHIPipeline Pipeline;
			} Data_Signal;

			struct
			{
				FFence* Fence;
				ERHIPipeline Pipeline;
			} Data_Wait;

			struct
			{
				bool bAllow;
			} Data_AllUAVsOverlap;

			struct
			{
				FResourceIdentity Identity;
				bool bAllow;
			} Data_SpecificUAVOverlap;

			struct
			{
				TCHAR* Breadcrumb;
			} Data_PushBreadcrumb;
		};

		RHI_API EReplayStatus Replay(ERHIPipeline Pipeline, bool& bAllowAllUAVsOverlap, FBreadcrumbStack& Breadcrumbs) const;

		static inline FOperation BeginTransitionResource(FResourceIdentity Identity, FState PreviousState, FState NextState, EResourceTransitionFlags Flags, void* CreateBacktrace)
		{
			for (ERHIPipeline Pipeline : GetRHIPipelines())
			{
				if (EnumHasAnyFlags(PreviousState.Pipelines, Pipeline))
				{
					Identity.Resource->AddOpRef();
				}
			}

			FOperation Op;
			Op.Type = EOpType::BeginTransition;
			Op.Data_BeginTransition.Identity = Identity;
			Op.Data_BeginTransition.PreviousState = PreviousState;
			Op.Data_BeginTransition.NextState = NextState;
			Op.Data_BeginTransition.Flags = Flags;
			Op.Data_BeginTransition.CreateBacktrace = CreateBacktrace;
			return MoveTemp(Op);
		}

		static inline FOperation EndTransitionResource(FResourceIdentity Identity, FState PreviousState, FState NextState, void* CreateBacktrace)
		{
			for (ERHIPipeline Pipeline : GetRHIPipelines())
			{
				if (EnumHasAnyFlags(NextState.Pipelines, Pipeline))
				{
					Identity.Resource->AddOpRef();
				}
			}

			FOperation Op;
			Op.Type = EOpType::EndTransition;
			Op.Data_EndTransition.Identity = Identity;
			Op.Data_EndTransition.PreviousState = PreviousState;
			Op.Data_EndTransition.NextState = NextState;
			Op.Data_EndTransition.CreateBacktrace = CreateBacktrace;
			return MoveTemp(Op);
		}

		static inline FOperation SetTrackedAccess(FResource* Resource, ERHIAccess Access)
		{
			Resource->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::SetTrackedAccess;
			Op.Data_SetTrackedAccess.Resource = Resource;
			Op.Data_SetTrackedAccess.Access = Access;
			return MoveTemp(Op);
		}

		static inline FOperation AliasingOverlap(FResource* ResourceBefore, FResource* ResourceAfter, void* CreateBacktrace)
		{
			ResourceBefore->AddOpRef();
			ResourceAfter->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::AliasingOverlap;
			Op.Data_AliasingOverlap.ResourceBefore = ResourceBefore;
			Op.Data_AliasingOverlap.ResourceAfter = ResourceAfter;
			Op.Data_AliasingOverlap.CreateBacktrace = CreateBacktrace;
			return Op;
		}

		static inline FOperation AcquireTransientResource(FResource* Resource, void* CreateBacktrace)
		{
			Resource->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::AcquireTransient;
			Op.Data_AcquireTransient.Resource = Resource;
			Op.Data_AcquireTransient.CreateBacktrace = CreateBacktrace;
			return MoveTemp(Op);
		}

		static inline FOperation DiscardTransientResource(FResource* Resource, void* CreateBacktrace)
		{
			Resource->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::DiscardTransient;
			Op.Data_DiscardTransient.Resource = Resource;
			Op.Data_DiscardTransient.CreateBacktrace = CreateBacktrace;
			return MoveTemp(Op);
		}

		static inline FOperation Assert(FResourceIdentity Identity, FState RequiredState)
		{
			Identity.Resource->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::Assert;
			Op.Data_Assert.Identity = Identity;
			Op.Data_Assert.RequiredState = RequiredState;
			return MoveTemp(Op);
		}

		static inline FOperation Rename(FResource* Resource, const TCHAR* NewName, const TCHAR* Suffix = nullptr)
		{
			Resource->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::Rename;
			Op.Data_Rename.Resource = Resource;
			AllocStringCopy(Op.Data_Rename.DebugName, NewName);
			Op.Data_Rename.Suffix = Suffix;
			return MoveTemp(Op);
		}

		static inline FOperation Signal(FFence* Fence, ERHIPipeline Pipeline)
		{
			FOperation Op;
			Op.Type = EOpType::Signal;
			Op.Data_Signal.Fence = Fence;
			Op.Data_Signal.Pipeline = Pipeline;
			return MoveTemp(Op);
		}

		static inline FOperation Wait(FFence* Fence, ERHIPipeline Pipeline)
		{
			FOperation Op;
			Op.Type = EOpType::Wait;
			Op.Data_Wait.Fence = Fence;
			Op.Data_Wait.Pipeline = Pipeline;
			return MoveTemp(Op);
		}

		static inline FOperation AllUAVsOverlap(bool bAllow)
		{
			FOperation Op;
			Op.Type = EOpType::AllUAVsOverlap;
			Op.Data_AllUAVsOverlap.bAllow = bAllow;
			return MoveTemp(Op);
		}

		static inline FOperation SpecificUAVOverlap(FResourceIdentity Identity, bool bAllow)
		{
			Identity.Resource->AddOpRef();

			FOperation Op;
			Op.Type = EOpType::SpecificUAVOverlap;
			Op.Data_SpecificUAVOverlap.Identity = Identity;
			Op.Data_SpecificUAVOverlap.bAllow = bAllow;
			return MoveTemp(Op);
		}

		static inline FOperation PushBreadcrumb(const TCHAR* Breadcrumb)
		{
			FOperation Op;
			Op.Type = EOpType::PushBreadcrumb;
			AllocStringCopy(Op.Data_PushBreadcrumb.Breadcrumb, Breadcrumb);
			return Op;
		}

		static inline FOperation PopBreadcrumb()
		{
			FOperation Op;
			Op.Type = EOpType::PopBreadcrumb;
			return Op;
		}

	private:
		static inline void AllocStringCopy(TCHAR*& OutString, const TCHAR* InString)
		{
			int32 Len = FCString::Strlen(InString);
			OutString = new TCHAR[Len + 1];
			FMemory::Memcpy(OutString, InString, Len * sizeof(TCHAR));
			OutString[Len] = 0;
		}
	};

	struct FOperationsList
	{
		using ListType = TArray<FOperation, FConcurrentLinearArrayAllocator>;

		ListType Operations;
		int32 OperationPos = 0;

		inline EReplayStatus Replay(ERHIPipeline Pipeline, bool& bAllowAllUAVsOverlap, FBreadcrumbStack& Breadcrumbs)
		{
			EReplayStatus Status = EReplayStatus::Normal;
			for (; OperationPos < Operations.Num(); ++OperationPos)
			{
				Status |= Operations[OperationPos].Replay(Pipeline, bAllowAllUAVsOverlap, Breadcrumbs);
				if (EnumHasAllFlags(Status, EReplayStatus::Waiting))
				{
					break;
				}
			}
			return Status;
		}

		inline void Reset()
		{
			Operations.SetNum(0, false);
			OperationPos = 0;
		}

		inline void Append(const FOperationsList& Other)
		{
			Operations.Append(Other.Operations.GetData() + Other.OperationPos, Other.Operations.Num() - Other.OperationPos);
		}

		inline bool Incomplete() const
		{
			return OperationPos < Operations.Num();
		}

		inline FOperation* AddRange(int32 Num)
		{
			int32 Index = Operations.AddUninitialized(Num);
			return &Operations[Index];
		}
	};

	enum class EUAVMode
	{
		Graphics,
		Compute,
		Num
	};

	class FTracker
	{
		struct FUAVTracker
		{
		private:
			TArray<FUnorderedAccessView*> UAVs;

		public:
			FUAVTracker()
			{
				UAVs.Reserve(MaxSimultaneousUAVs);
			}

			inline FUnorderedAccessView*& operator[](int32 Slot)
			{
				if (Slot >= UAVs.Num())
				{
					UAVs.SetNumZeroed(Slot + 1);
				}
				return UAVs[Slot];
			}

			inline void Reset()
			{
				UAVs.SetNum(0, false);
			}

			inline void DrawOrDispatch(FTracker* BarrierTracker, const FState& RequiredState)
			{
				// The barrier tracking expects us to call Assert() only once per unique resource.
				// However, multiple UAVs may be bound, all referencing the same resource.
				// Find the unique resources to ensure we only do the tracking once per resource.
				uint32 NumUniqueIdentities = 0;
				FResourceIdentity UniqueIdentities[MaxSimultaneousUAVs];

				for (int32 UAVIndex = 0; UAVIndex < UAVs.Num(); ++UAVIndex)
				{
					if (UAVs[UAVIndex])
					{
						const FResourceIdentity& Identity = UAVs[UAVIndex]->ViewIdentity;

						// Check if we've already seen this resource.
						bool bFound = false;
						for (uint32 Index = 0; !bFound && Index < NumUniqueIdentities; ++Index)
						{
							bFound = UniqueIdentities[Index] == Identity;
						}

						if (!bFound)
						{
							check(NumUniqueIdentities < UE_ARRAY_COUNT(UniqueIdentities));
							UniqueIdentities[NumUniqueIdentities++] = Identity;

							// Assert unique resources have the required state.
							BarrierTracker->AddOp(FOperation::Assert(Identity, RequiredState));
						}
					}
				}
			}
		};

	public:
		FTracker(ERHIPipeline InPipeline)
			: Pipeline(InPipeline)
		{}

		inline void AddOp(const FOperation& Op);

		inline void AddOps(const FOperationsList::ListType& Ops)
		{
			for (const FOperation& Op : Ops)
			{
				AddOp(Op);
			}
		}
		inline void AddOps(const FOperationsList& List)
		{
			AddOps(List.Operations);
		}

		FOperationsList Finalize()
		{
			return MoveTemp(CurrentList);
		}

		inline void PushBreadcrumb(const TCHAR* Breadcrumb)
		{
			AddOp(FOperation::PushBreadcrumb(Breadcrumb));
		}

		inline void PopBreadcrumb()
		{
			AddOp(FOperation::PopBreadcrumb());
		}

		inline void SetTrackedAccess(FResource* Resource, ERHIAccess Access)
		{
			AddOp(FOperation::SetTrackedAccess(Resource, Access));
		}

		inline void Rename(FResource* Resource, const TCHAR* NewName, const TCHAR* Suffix = nullptr)
		{
			AddOp(FOperation::Rename(Resource, NewName, Suffix));
		}

		inline void Assert(FResourceIdentity Identity, ERHIAccess RequiredAccess)
		{
			AddOp(FOperation::Assert(Identity, FState(RequiredAccess, Pipeline)));
		}

		inline void AssertUAV(FUnorderedAccessView* UAV, EUAVMode Mode, int32 Slot)
		{
			checkSlow(Mode == EUAVMode::Compute || Pipeline == ERHIPipeline::Graphics);
			UAVTrackers[int32(Mode)][Slot] = UAV;
		}

		inline void AssertUAV(FUnorderedAccessView* UAV, ERHIAccess Access, int32 Slot)
		{
			checkSlow(!(Access & ~ERHIAccess::UAVMask));
			AssertUAV(UAV, Access == ERHIAccess::UAVGraphics ? EUAVMode::Graphics : EUAVMode::Compute, Slot);
		}

		inline void TransitionResource(FResourceIdentity Identity, FState PreviousState, FState NextState, EResourceTransitionFlags Flags)
		{
			// This function exists due to the implicit transitions that RHI functions make (e.g. RHICopyToResolveTarget).
			// It should be removed when we eventually remove all implicit transitions from the RHI.
			AddOp(FOperation::BeginTransitionResource(Identity, PreviousState, NextState, Flags, nullptr));
			AddOp(FOperation::EndTransitionResource(Identity, PreviousState, NextState, nullptr));
		}

		inline void AllUAVsOverlap(bool bAllow)
		{
			AddOp(FOperation::AllUAVsOverlap(bAllow));
		}

		inline void SpecificUAVOverlap(FResourceIdentity Identity, bool bAllow)
		{
			AddOp(FOperation::SpecificUAVOverlap(Identity, bAllow));
		}

		inline void Dispatch()
		{
			UAVTrackers[int32(EUAVMode::Compute)].DrawOrDispatch(this, FState(ERHIAccess::UAVCompute, Pipeline));
		}

		inline void Draw()
		{
			checkSlow(Pipeline == ERHIPipeline::Graphics);
			UAVTrackers[int32(EUAVMode::Graphics)].DrawOrDispatch(this, FState(ERHIAccess::UAVGraphics, Pipeline));
		}

		inline void ResetUAVState(EUAVMode Mode)
		{
			UAVTrackers[int32(Mode)].Reset();
		}

		inline void ResetAllUAVState()
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(UAVTrackers); ++Index)
			{
				ResetUAVState(EUAVMode(Index));
			}
		}

		static inline int32 GetOpQueueIndex(ERHIPipeline Pipeline)
		{
			switch (Pipeline)
			{
			default: checkNoEntry(); // fallthrough

			case ERHIPipeline::Graphics:
				return 0;

			case ERHIPipeline::AsyncCompute:
				return 1;
			}
		}

		static void ReplayOpQueue(ERHIPipeline OpQueue, FOperationsList&& InOpsList);

	private:
		const ERHIPipeline Pipeline;
		FOperationsList CurrentList;
		FUAVTracker UAVTrackers[int32(EUAVMode::Num)];

		friend FOperation;
		struct FOpQueueState
		{
			bool bWaiting = false;
			bool bAllowAllUAVsOverlap = false;

			FOperationsList Ops;
			TArray<const TCHAR*, FConcurrentLinearArrayAllocator> Breadcrumbs;

		} static RHI_API OpQueues[int32(ERHIPipeline::Num)];
	};

	extern RHI_API void* CaptureBacktrace();
}

#endif // ENABLE_RHI_VALIDATION
