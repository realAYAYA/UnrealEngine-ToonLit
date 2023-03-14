// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLResources.h: OpenGL resource RHI definitions.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "HAL/LowLevelMemTracker.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"
#include "Containers/BitArray.h"
#include "Math/IntPoint.h"
#include "Misc/CommandLine.h"
#include "Templates/RefCounting.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "BoundShaderStateCache.h"
#include "RenderResource.h"
#include "OpenGLShaderResources.h"
#include "PsoLruCache.h"

class FOpenGLDynamicRHI;
class FOpenGLLinkedProgram;
class FOpenGLTexture;
typedef TArray<ANSICHAR> FAnsiCharArray;


extern void OnBufferDeletion( GLuint BufferResource );
extern void OnPixelBufferDeletion( GLuint PixelBufferResource );
extern void OnUniformBufferDeletion( GLuint UniformBufferResource, uint32 AllocatedSize, bool bStreamDraw, uint32 Offset, uint8* Pointer );
extern void OnProgramDeletion( GLint ProgramResource );

extern void CachedBindBuffer( GLenum Type, GLuint Buffer );
extern void CachedBindPixelUnpackBuffer( GLenum Type, GLuint Buffer );
extern void CachedBindUniformBuffer( GLuint Buffer );
extern bool IsUniformBufferBound( GLuint Buffer );

namespace OpenGLConsoleVariables
{
	extern int32 bUseMapBuffer;
	extern int32 MaxSubDataSize;
	extern int32 bUseStagingBuffer;
	extern int32 bBindlessTexture;
	extern int32 bUseBufferDiscard;
};

#if PLATFORM_WINDOWS
#define RESTRICT_SUBDATA_SIZE 1
#else
#define RESTRICT_SUBDATA_SIZE 0
#endif

void IncrementBufferMemory(GLenum Type, uint32 NumBytes);
void DecrementBufferMemory(GLenum Type, uint32 NumBytes);

// Extra stats for finer-grained timing
// They shouldn't always be on, as they may impact overall performance
#define OPENGLRHI_DETAILED_STATS 0


#if OPENGLRHI_DETAILED_STATS
	DECLARE_CYCLE_STAT_EXTERN(TEXT("MapBuffer time"),STAT_OpenGLMapBufferTime,STATGROUP_OpenGLRHI, );
	DECLARE_CYCLE_STAT_EXTERN(TEXT("UnmapBuffer time"),STAT_OpenGLUnmapBufferTime,STATGROUP_OpenGLRHI, );
	#define SCOPE_CYCLE_COUNTER_DETAILED(Stat)	SCOPE_CYCLE_COUNTER(Stat)
	#define DETAILED_QUICK_SCOPE_CYCLE_COUNTER(x) QUICK_SCOPE_CYCLE_COUNTER(x)
#else
	#define SCOPE_CYCLE_COUNTER_DETAILED(Stat)
	#define DETAILED_QUICK_SCOPE_CYCLE_COUNTER(x)
#endif

#if UE_BUILD_TEST
#define USE_REAL_RHI_FENCES (0)
#define USE_CHEAP_ASSERTONLY_RHI_FENCES (1)
#define GLAF_CHECK(x) \
if (!(x)) \
{  \
	UE_LOG(LogRHI, Fatal, TEXT("AssertFence Fail on line %s."), TEXT(PREPROCESSOR_TO_STRING(__LINE__))); \
	FPlatformMisc::LocalPrint(TEXT("Failed a check on line:\n")); FPlatformMisc::LocalPrint(TEXT(PREPROCESSOR_TO_STRING(__LINE__))); FPlatformMisc::LocalPrint(TEXT("\n")); *((int*)3) = 13; \
}

#elif DO_CHECK
#define USE_REAL_RHI_FENCES (1)
#define USE_CHEAP_ASSERTONLY_RHI_FENCES (1)
#define GLAF_CHECK(x)  check(x)

//#define GLAF_CHECK(x) if (!(x)) { FPlatformMisc::LocalPrint(TEXT("Failed a check on line:\n")); FPlatformMisc::LocalPrint(TEXT( PREPROCESSOR_TO_STRING(__LINE__))); FPlatformMisc::LocalPrint(TEXT("\n")); *((int*)3) = 13; }

#else
#define USE_REAL_RHI_FENCES (0)
#define USE_CHEAP_ASSERTONLY_RHI_FENCES (0)

#define GLAF_CHECK(x) 

#endif

#define GLDEBUG_LABELS_ENABLED (!UE_BUILD_SHIPPING)

class FOpenGLRHIThreadResourceFence
{
	FGraphEventRef RealRHIFence;

public:

	FORCEINLINE_DEBUGGABLE void Reset()
	{
		if (IsRunningRHIInSeparateThread())
		{
			GLAF_CHECK(IsInRenderingThread());
			GLAF_CHECK(!RealRHIFence.GetReference() || RealRHIFence->IsComplete());
			RealRHIFence = nullptr;
		}
	}
	FORCEINLINE_DEBUGGABLE void SetRHIThreadFence()
	{
		if (IsRunningRHIInSeparateThread())
		{
			GLAF_CHECK(IsInRenderingThread());

			GLAF_CHECK(!RealRHIFence.GetReference() || RealRHIFence->IsComplete());
			if (IsRunningRHIInSeparateThread())
			{
				RealRHIFence = FRHICommandListExecutor::GetImmediateCommandList().RHIThreadFence(false);
			}
		}
	}
	FORCEINLINE_DEBUGGABLE void WriteAssertFence()
	{
		if (IsRunningRHIInSeparateThread())
		{
			GLAF_CHECK((IsInRenderingThread() && !IsRunningRHIInSeparateThread()) || (IsInRHIThread() && IsRunningRHIInSeparateThread()));
		}
	}

	FORCEINLINE_DEBUGGABLE void WaitFenceRenderThreadOnly()
	{
		if (IsRunningRHIInSeparateThread())
		{
			// Do not check if running on RHI thread.
			// all rhi thread operations will be in order, check for RHIT isnt required.
			if (IsInRenderingThread())
			{
				if (RealRHIFence.GetReference() && RealRHIFence->IsComplete())
				{
					RealRHIFence = nullptr;
				}
				else if (RealRHIFence.GetReference())
				{
					UE_LOG(LogRHI, Warning, TEXT("FOpenGLRHIThreadResourceFence waited."));
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FOpenGLRHIThreadResourceFence_Wait);
					FRHICommandListExecutor::WaitOnRHIThreadFence(RealRHIFence);
					RealRHIFence = nullptr;
				}
			}
		}
	}
};

class FOpenGLAssertRHIThreadFence
{
#if USE_REAL_RHI_FENCES
	FGraphEventRef RealRHIFence;
#endif
#if USE_CHEAP_ASSERTONLY_RHI_FENCES
	FThreadSafeCounter AssertFence;
#endif

public:

	FORCEINLINE_DEBUGGABLE void Reset()
	{
		if (IsRunningRHIInSeparateThread())
		{
			check(IsInRenderingThread() || IsInRHIThread());
#if USE_REAL_RHI_FENCES

			GLAF_CHECK(!RealRHIFence.GetReference() || RealRHIFence->IsComplete());
			RealRHIFence = nullptr;
#endif
#if USE_CHEAP_ASSERTONLY_RHI_FENCES
			int32 AFenceVal = AssertFence.GetValue();
			GLAF_CHECK(AFenceVal == 0 || AFenceVal == 2);
			AssertFence.Set(1);
#endif
		}
	}
	FORCEINLINE_DEBUGGABLE void SetRHIThreadFence()
	{
		if (IsRunningRHIInSeparateThread())
		{
			check(IsInRenderingThread() || IsInRHIThread());

#if USE_CHEAP_ASSERTONLY_RHI_FENCES
			int32 AFenceVal = AssertFence.GetValue();
			GLAF_CHECK(AFenceVal == 1 || AFenceVal == 2);
#endif
#if USE_REAL_RHI_FENCES
			GLAF_CHECK(!RealRHIFence.GetReference() || RealRHIFence->IsComplete());
			// Only get the fence if running on RT.
			if (IsRunningRHIInSeparateThread() && IsInRenderingThread())
			{
				RealRHIFence = FRHICommandListExecutor::GetImmediateCommandList().RHIThreadFence(false);
			}
#endif
		}
	}
	FORCEINLINE_DEBUGGABLE void WriteAssertFence()
	{
		if (IsRunningRHIInSeparateThread())
		{
			check((IsInRenderingThread() && !IsRunningRHIInSeparateThread()) || (IsInRHIThread() && IsRunningRHIInSeparateThread()));
#if USE_CHEAP_ASSERTONLY_RHI_FENCES
			int32 NewValue = AssertFence.Increment();
			GLAF_CHECK(NewValue == 2);
#endif
		}
	}

	FORCEINLINE_DEBUGGABLE void WaitFenceRenderThreadOnly()
	{
		if (IsRunningRHIInSeparateThread())
		{
			// Do not check if running on RHI thread.
			// all rhi thread operations will be in order, check for RHIT isnt required.
			if (IsInRenderingThread())
			{
#if USE_CHEAP_ASSERTONLY_RHI_FENCES
				GLAF_CHECK(AssertFence.GetValue() == 0 || AssertFence.GetValue() == 2);
#endif
#if USE_REAL_RHI_FENCES
				GLAF_CHECK(!RealRHIFence.GetReference() || RealRHIFence->IsComplete());
				if (RealRHIFence.GetReference())
				{
					FRHICommandListExecutor::WaitOnRHIThreadFence(RealRHIFence);
					RealRHIFence = nullptr;
				}
#endif
			}
		}
	}
};


//////////////////////////////////////////////////////////////////////////
// Proxy object that fulfils immediate requirements of RHIResource creation whilst allowing deferment of GL resource creation on to the RHI thread.

template<typename TRHIType, typename TOGLResourceType>
class TOpenGLResourceProxy : public TRHIType
{
public:
	TOpenGLResourceProxy(TFunction<TOGLResourceType*(TRHIType*)> CreateFunc)
		: GLResourceObject(nullptr)
	{
		check((bool)CreateFunc);
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
		{
			GLResourceObject = CreateFunc(this);
			GLResourceObject->AddRef();
			bQueuedCreation = false;
		}
		else
		{
			CreationFence.Reset();
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([this, CreateFunc = MoveTemp(CreateFunc)]()
			{
				GLResourceObject = CreateFunc(this);
				GLResourceObject->AddRef();
				CreationFence.WriteAssertFence();
			});
			CreationFence.SetRHIThreadFence();
			bQueuedCreation = true;
		}
	}

	virtual ~TOpenGLResourceProxy()
	{
		// Wait for any queued creation calls.
		WaitIfQueued();

		check(GLResourceObject);

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
		{
			GLResourceObject->Release();
		}
		else
		{
			RunOnGLRenderContextThread([GLResourceObject = GLResourceObject]()
			{
				GLResourceObject->Release();
			});
			GLResourceObject = nullptr;
		}
	}

	TOpenGLResourceProxy(const TOpenGLResourceProxy&) = delete;
	TOpenGLResourceProxy& operator = (const TOpenGLResourceProxy&) = delete;

	TOGLResourceType* GetGLResourceObject()
	{
		CreationFence.WaitFenceRenderThreadOnly();
		return GLResourceObject;
	}

	FORCEINLINE TOGLResourceType* GetGLResourceObject_OnRHIThread()
	{
		check(IsInRHIThread());
		return GLResourceObject;
	}

	typedef TOGLResourceType ContainedGLType;
private:
	void WaitIfQueued()
	{
		if (bQueuedCreation)
		{
			CreationFence.WaitFenceRenderThreadOnly();
		}
	}

	//FOpenGLRHIThreadResourceFence CreationFence;
	FOpenGLAssertRHIThreadFence CreationFence;
	TRefCountPtr<TOGLResourceType> GLResourceObject;
	bool bQueuedCreation;
};


template<typename TRHIType, typename TOGLResourceType>
class TOpenGLShaderProxy : public TOpenGLResourceProxy< TRHIType, TOGLResourceType>
{
public:
	TOpenGLShaderProxy(const FOpenGLCompiledShaderKey& InKey, TFunction<TOGLResourceType* (TRHIType*)> CreateFunc)
		: TOpenGLResourceProxy< TRHIType, TOGLResourceType>(CreateFunc), Key(InKey)
	{
	}
	
	const FOpenGLCompiledShaderKey& GetCompiledShaderKey() const { return Key; }

private:

	FOpenGLCompiledShaderKey Key;
};

typedef TOpenGLShaderProxy<FRHIVertexShader, FOpenGLVertexShader> FOpenGLVertexShaderProxy;
typedef TOpenGLShaderProxy<FRHIPixelShader, FOpenGLPixelShader> FOpenGLPixelShaderProxy;
typedef TOpenGLShaderProxy<FRHIGeometryShader, FOpenGLGeometryShader> FOpenGLGeometryShaderProxy;
typedef TOpenGLShaderProxy<FRHIComputeShader, FOpenGLComputeShader> FOpenGLComputeShaderProxy;


template <typename T>
struct TIsGLProxyObject
{
	enum { Value = false };
};

template<typename TRHIType, typename TOGLResourceType>
struct TIsGLProxyObject<TOpenGLResourceProxy<TRHIType, TOGLResourceType>>
{
	enum { Value = true };
};

template<typename TRHIType, typename TOGLResourceType>
struct TIsGLProxyObject<TOpenGLShaderProxy<TRHIType, TOGLResourceType>>
{
	enum { Value = true };
};

typedef void (*BufferBindFunction)( GLenum Type, GLuint Buffer );

template <typename BaseType, BufferBindFunction BufBind>
class TOpenGLBuffer : public BaseType
{
	void LoadData( uint32 InOffset, uint32 InSize, const void* InData)
	{
		VERIFY_GL_SCOPE();
		const uint8* Data = (const uint8*)InData;
		const uint32 BlockSize = OpenGLConsoleVariables::MaxSubDataSize;

		if (BlockSize > 0)
		{
			while ( InSize > 0)
			{
				const uint32 BufferSize = FMath::Min<uint32>( BlockSize, InSize);

				FOpenGL::BufferSubData( Type, InOffset, BufferSize, Data);

				InOffset += BufferSize;
				InSize -= BufferSize;
				Data += BufferSize;
			}
		}
		else
		{
			FOpenGL::BufferSubData( Type, InOffset, InSize, InData);
		}
	}

	GLenum GetAccess()
	{
		// Previously there was special-case logic to always use GL_STATIC_DRAW for vertex buffers allocated from staging buffer.
		// However it seems to be incorrect as NVidia drivers complain (via debug output callback) about VIDEO->HOST copying for buffers with such hints
		return bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	}
public:

	GLuint Resource;
	GLenum Type;

	TOpenGLBuffer()
		: Resource(0)
		, Type(0)
		, bIsLocked(false)
		, bIsLockReadOnly(false)
		, bStreamDraw(false)
		, bLockBufferWasAllocated(false)
		, LockSize(0)
		, LockOffset(0)
		, LockBuffer(NULL)
		, RealSize(0)
	{ }

	TOpenGLBuffer(FRHICommandListBase* RHICmdList, GLenum InType, uint32 InStride, uint32 InSize, EBufferUsageFlags InUsage,
		const void *InData, bool bStreamedDraw = false, GLuint ResourceToUse = 0, uint32 ResourceSize = 0)
	: BaseType(InStride,InSize,InUsage)
	, Resource(0)
	, Type(InType)
	, bIsLocked(false)
	, bIsLockReadOnly(false)
	, bStreamDraw(bStreamedDraw)
	, bLockBufferWasAllocated(false)
	, LockSize(0)
	, LockOffset(0)
	, LockBuffer(NULL)
	, RealSize(InSize)
	{
		RealSize = ResourceSize ? ResourceSize : InSize;

		if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
		{
			CreateGLBuffer(InData, ResourceToUse, ResourceSize);
		}
		else
		{
			void* BuffData = nullptr;
			if (InData)
			{
				BuffData = RHICmdList->Alloc(RealSize, 16);
				FMemory::Memcpy(BuffData, InData, RealSize);
			}
			TransitionFence.Reset();
			ALLOC_COMMAND_CL(*RHICmdList, FRHICommandGLCommand)([=]() 
			{
				CreateGLBuffer(BuffData, ResourceToUse, ResourceSize); 
				TransitionFence.WriteAssertFence();
			});
			TransitionFence.SetRHIThreadFence();
		}
	}

	void CreateGLBuffer(const void *InData, const GLuint ResourceToUse, const uint32 ResourceSize)
	{
		VERIFY_GL_SCOPE();
		uint32 InSize = BaseType::GetSize();
		RealSize = ResourceSize ? ResourceSize : InSize;
		if( ResourceToUse )
		{
			Resource = ResourceToUse;
			check( Type != GL_UNIFORM_BUFFER || !IsUniformBufferBound(Resource) );
			Bind();
			FOpenGL::BufferSubData(Type, 0, InSize, InData);
		}
		else
		{
			if (BaseType::GLSupportsType())
			{
				FOpenGL::GenBuffers(1, &Resource);
				check( Type != GL_UNIFORM_BUFFER || !IsUniformBufferBound(Resource) );
				Bind();
#if !RESTRICT_SUBDATA_SIZE
				if( InData == NULL || RealSize <= InSize )
				{
					glBufferData(Type, RealSize, InData, GetAccess());
				}
				else
				{
					glBufferData(Type, RealSize, NULL, GetAccess());
					FOpenGL::BufferSubData(Type, 0, InSize, InData);
				}
#else
				glBufferData(Type, RealSize, NULL, GetAccess());
				if ( InData != NULL )
				{
					LoadData( 0, FMath::Min<uint32>(InSize,RealSize), InData);
				}
#endif
				IncrementBufferMemory(Type, RealSize);
			}
			else
			{
				BaseType::CreateType(Resource, InData, InSize);
			}
		}
	}

	virtual ~TOpenGLBuffer()
	{
		// this is a bit of a special case, normally the RT destroys all rhi resources...but this isn't an rhi resource
		TransitionFence.WaitFenceRenderThreadOnly();

		if (Resource != 0)
		{
			auto DeleteGLResources = [Resource=Resource, RealSize= RealSize, bStreamDraw= (bool)bStreamDraw, LockBuffer = LockBuffer, bLockBufferWasAllocated=bLockBufferWasAllocated]()
			{
				VERIFY_GL_SCOPE();
				if (BaseType::OnDelete(Resource, RealSize, bStreamDraw, 0))
				{
					FOpenGL::DeleteBuffers(1, &Resource);
				}
				if (LockBuffer != NULL)
				{
					if (bLockBufferWasAllocated)
					{
						FMemory::Free(LockBuffer);
					}
					else
					{
						UE_LOG(LogRHI,Warning,TEXT("Destroying TOpenGLBuffer without returning memory to the driver; possibly called RHIMapStagingSurface() but didn't call RHIUnmapStagingSurface()? Resource %u"), Resource);
					}
				}
			};

			RunOnGLRenderContextThread(MoveTemp(DeleteGLResources));
			LockBuffer = nullptr;
			DecrementBufferMemory(Type, RealSize);

			ReleaseCachedBuffer();
		}

	}

	void Bind()
	{
		VERIFY_GL_SCOPE();
		BufBind(Type, Resource);
	}

	uint8 *Lock(uint32 InOffset, uint32 InSize, bool bReadOnly, bool bDiscard)
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLMapBufferTime);
		check(InOffset + InSize <= this->GetSize());
		//check( LockBuffer == NULL );	// Only one outstanding lock is allowed at a time!
		check( !bIsLocked );	// Only one outstanding lock is allowed at a time!
		VERIFY_GL_SCOPE();

		Bind();

		bIsLocked = true;
		bIsLockReadOnly = bReadOnly;
		uint8 *Data = NULL;

		// Discard if the input size is the same as the backing store size, regardless of the input argument, as orphaning the backing store will typically be faster.
		bDiscard = (bDiscard || (!bReadOnly && InSize == RealSize)) && FOpenGL::DiscardFrameBufferToResize();

		// Map buffer is faster in some circumstances and slower in others, decide when to use it carefully.
		bool const bUseMapBuffer = BaseType::GLSupportsType() && (bReadOnly || OpenGLConsoleVariables::bUseMapBuffer);

		// If we're able to discard the current data, do so right away
		// If we can then we should orphan the buffer name & reallocate the backing store only once as calls to glBufferData may do so even when the size is the same.
		uint32 DiscardSize = (bDiscard && !bUseMapBuffer && InSize == RealSize && !RESTRICT_SUBDATA_SIZE) ? 0 : RealSize;

		// Don't call BufferData if Bindless is on, as bindless texture buffers make buffers immutable
		if (bDiscard && !OpenGLConsoleVariables::bBindlessTexture && OpenGLConsoleVariables::bUseBufferDiscard)
		{
			if (BaseType::GLSupportsType())
			{
				// @todo Lumin hack:
				// When not hinted with GL_STATIC_DRAW, glBufferData() would introduce long uploading times
				// that would show up in TGD. Without the workaround of hinting glBufferData() with the static buffer usage, 
				// the buffer mapping / unmapping has an unexpected cost(~5 - 10ms) that manifests itself in light grid computation 
				// and vertex buffer mapping for bone matrices. We believe this issue originates from the driver as the OpenGL spec 
				// specifies the following on the usage hint parameter of glBufferData() :
				//
				// > usage is a hint to the GL implementation as to how a buffer object's data store will be accessed. 
				// > This enables the GL implementation to make more intelligent decisions that may significantly impact buffer object performance. 
				// > It does not, however, constrain the actual usage of the data store.
				//
				// As the alternative approach of using uniform buffers for bone matrix uploading (isntead of buffer mapping/unmapping)
				// limits the number of bone matrices to 75 in the current engine architecture and that is not desirable, 
				// we can stick with the STATIC_DRAW hint workaround for glBufferData().
				//
				// We haven't seen the buffer mapping/unmapping issue show up elsewhere in the pipeline in our test scenes. 
				// However, depending on the UnrealEditor features that are used, this issue might pop up elsewhere that we're yet to see.
				// As there are concerns for maximum number of bone matrices, going for the GL_STATIC_DRAW hint should be safer, 
				// given the fact that it won't constrain the actual usage of the data store as per the OpenGL4 spec.
#if 0
				glBufferData(Type, DiscardSize, NULL, GL_STATIC_DRAW);
#else
				glBufferData(Type, DiscardSize, NULL, GetAccess());
#endif			
			}
		}

		if ( bUseMapBuffer)
		{
			FOpenGL::EResourceLockMode LockMode = bReadOnly ? FOpenGL::EResourceLockMode::RLM_ReadOnly : FOpenGL::EResourceLockMode::RLM_WriteOnly;
			Data = static_cast<uint8*>( FOpenGL::MapBufferRange( Type, InOffset, InSize, LockMode ) );
//			checkf(Data != NULL, TEXT("FOpenGL::MapBufferRange Failed, glError %d (0x%x)"), glGetError(), glGetError());

			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = Data;
			bLockBufferWasAllocated = false;
		}
		else
		{
			// Allocate a temp buffer to write into
			LockOffset = InOffset;
			LockSize = InSize;
			if (CachedBuffer && InSize <= CachedBufferSize)
			{
				LockBuffer = CachedBuffer;
				CachedBuffer = nullptr;
				// Keep CachedBufferSize to keep the actual size allocated.
			}
			else
			{
				ReleaseCachedBuffer();
				LockBuffer = FMemory::Malloc( InSize );
				CachedBufferSize = InSize; // Safegard
			}
			Data = static_cast<uint8*>( LockBuffer );
			bLockBufferWasAllocated = true;
		}

		if (Data == nullptr)
		{
			UE_LOG(LogRHI, Fatal, TEXT("Failed to lock buffer: Resource %u, Size %u, Offset %u, bReadOnly %d, bUseMapBuffer %d, glError (0x%x)"), 
				(uint32)Resource, 
				InSize, 
				InOffset, 
				bReadOnly ? 1:0, 
				bUseMapBuffer ? 1:0, 
				glGetError());
		}
		
		return Data;
	}

	uint8 *LockWriteOnlyUnsynchronized(uint32 InOffset, uint32 InSize, bool bDiscard)
	{
		//SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLMapBufferTime);
		check(InOffset + InSize <= this->GetSize());
		//check( LockBuffer == NULL );	// Only one outstanding lock is allowed at a time!
		check( !bIsLocked );	// Only one outstanding lock is allowed at a time!
		VERIFY_GL_SCOPE();

		Bind();

		bIsLocked = true;
		bIsLockReadOnly = false;
		uint8 *Data = NULL;

		// Discard if the input size is the same as the backing store size, regardless of the input argument, as orphaning the backing store will typically be faster.
		bDiscard = (bDiscard || InSize == RealSize) && FOpenGL::DiscardFrameBufferToResize();

		// Map buffer is faster in some circumstances and slower in others, decide when to use it carefully.
		bool const bUseMapBuffer = BaseType::GLSupportsType() && OpenGLConsoleVariables::bUseMapBuffer;

		// If we're able to discard the current data, do so right away
		// If we can then we should orphan the buffer name & reallocate the backing store only once as calls to glBufferData may do so even when the size is the same.
		uint32 DiscardSize = (bDiscard && !bUseMapBuffer && InSize == RealSize && !RESTRICT_SUBDATA_SIZE) ? 0 : RealSize;

		// Don't call BufferData if Bindless is on, as bindless texture buffers make buffers immutable
		if ( bDiscard && !OpenGLConsoleVariables::bBindlessTexture && OpenGLConsoleVariables::bUseBufferDiscard)
		{
			if (BaseType::GLSupportsType())
			{
				glBufferData( Type, DiscardSize, NULL, GetAccess());
			}
		}

		if ( bUseMapBuffer)
		{
			FOpenGL::EResourceLockMode LockMode = bDiscard ? FOpenGL::EResourceLockMode::RLM_WriteOnly : FOpenGL::EResourceLockMode::RLM_WriteOnlyUnsynchronized;
			Data = static_cast<uint8*>( FOpenGL::MapBufferRange( Type, InOffset, InSize, LockMode ) );
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = Data;
			bLockBufferWasAllocated = false;
		}
		else
		{
			// Allocate a temp buffer to write into
			LockOffset = InOffset;
			LockSize = InSize;
			if (CachedBuffer && InSize <= CachedBufferSize)
			{
				LockBuffer = CachedBuffer;
				CachedBuffer = nullptr;
				// Keep CachedBufferSize to keep the actual size allocated.
			}
			else
			{
				ReleaseCachedBuffer();
				LockBuffer = FMemory::Malloc( InSize );
				CachedBufferSize = InSize; // Safegard
			}
			Data = static_cast<uint8*>( LockBuffer );
			bLockBufferWasAllocated = true;
		}

		if (Data == nullptr)
		{
			UE_LOG(LogRHI, Fatal, TEXT("Failed to lock buffer (write only): Resource %u, Size %u, Offset %u, bUseMapBuffer %d, glError (0x%x)"), 
				(uint32)Resource, 
				InSize, 
				InOffset, 
				bUseMapBuffer ? 1:0, 
				glGetError());
		}

		return Data;
	}

	void Unlock()
	{
		//SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLUnmapBufferTime);
		VERIFY_GL_SCOPE();
		if (bIsLocked)
		{
			Bind();

			if (BaseType::GLSupportsType() && (OpenGLConsoleVariables::bUseMapBuffer || bIsLockReadOnly))
			{
				check(!bLockBufferWasAllocated);
				FOpenGL::UnmapBufferRange(Type, LockOffset, LockSize);
				LockBuffer = NULL;
			}
			else
			{
				if (BaseType::GLSupportsType())
				{
#if !RESTRICT_SUBDATA_SIZE
					// Check for the typical, optimized case
					if( LockSize == RealSize )
					{
						if (FOpenGL::DiscardFrameBufferToResize())
						{
							glBufferData(Type, RealSize, LockBuffer, GetAccess());
						}
						else
						{
							FOpenGL::BufferSubData(Type, 0, LockSize, LockBuffer);
						}
						check( LockBuffer != NULL );
					}
					else
					{
						// Only updating a subset of the data
						FOpenGL::BufferSubData(Type, LockOffset, LockSize, LockBuffer);
						check( LockBuffer != NULL );
					}
#else
					LoadData( LockOffset, LockSize, LockBuffer);
					check( LockBuffer != NULL);
#endif
				}
				check(bLockBufferWasAllocated);

				if (EnumHasAnyFlags(this->GetUsage(), BUF_Volatile))
				{
					ReleaseCachedBuffer(); // Safegard

					CachedBuffer = LockBuffer;
					// Possibly > LockSize when reusing cached allocation.
					CachedBufferSize = FMath::Max<GLuint>(CachedBufferSize, LockSize);
				}
				else
				{
					FMemory::Free(LockBuffer);
				}
				LockBuffer = NULL;
				bLockBufferWasAllocated = false;
				LockSize = 0;
			}
			bIsLocked = false;
		}
	}

	void Update(void *InData, uint32 InOffset, uint32 InSize, bool bDiscard)
	{
		check(InOffset + InSize <= this->GetSize());
		VERIFY_GL_SCOPE();
		Bind();
#if !RESTRICT_SUBDATA_SIZE
		FOpenGL::BufferSubData(Type, InOffset, InSize, InData);
#else
		LoadData( InOffset, InSize, InData);
#endif
	}

	bool IsDynamic() const { return EnumHasAnyFlags(this->GetUsage(), BUF_AnyDynamic); }
	bool IsLocked() const { return bIsLocked; }
	bool IsLockReadOnly() const { return bIsLockReadOnly; }
	void* GetLockedBuffer() const { return LockBuffer; }

	void ReleaseCachedBuffer()
	{
		if (CachedBuffer)
		{
			FMemory::Free(CachedBuffer);
			CachedBuffer = nullptr;
			CachedBufferSize = 0;
		}
		// Don't reset CachedBufferSize if !CachedBuffer since it could be the locked buffer allocation size.
	}

	void Swap(TOpenGLBuffer& Other)
	{
		BaseType::Swap(Other);
		::Swap(Resource, Other.Resource);
		::Swap(RealSize, Other.RealSize);
	}

private:

	uint32 bIsLocked : 1;
	uint32 bIsLockReadOnly : 1;
	uint32 bStreamDraw : 1;
	uint32 bLockBufferWasAllocated : 1;

	GLuint LockSize;
	GLuint LockOffset;
	void* LockBuffer;

	// A cached allocation that can be reused. The same allocation can never be in CachedBuffer and LockBuffer at the same time.
	void* CachedBuffer = nullptr;
	// The size of the cached buffer allocation. Can be non zero even though CachedBuffer is  null, to preserve the allocation size.
	GLuint CachedBufferSize = 0;

	uint32 RealSize;	// sometimes (for example, for uniform buffer pool) we allocate more in OpenGL than is requested of us.

	FOpenGLAssertRHIThreadFence TransitionFence;
};

class FOpenGLBasePixelBuffer : public FRefCountedObject
{
public:
	FOpenGLBasePixelBuffer(uint32 InStride,uint32 InSize, EBufferUsageFlags InUsage)
	: Size(InSize)
	, Usage(InUsage)
	{}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnPixelBufferDeletion(Resource);
		return true;
	}
	uint32 GetSize() const { return Size; }
	EBufferUsageFlags GetUsage() const { return Usage; }

	static FORCEINLINE bool GLSupportsType()
	{
		return true;
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

private:
	uint32 Size;
	EBufferUsageFlags Usage;
};

class FOpenGLBaseBuffer : public FRHIBuffer
{
public:
	FOpenGLBaseBuffer()
	{}

	FOpenGLBaseBuffer(uint32 InStride, uint32 InSize, EBufferUsageFlags InUsage): FRHIBuffer(InSize, InUsage, InStride)
	{
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::GraphicsPlatform, InSize, ELLMTracker::Platform, ELLMAllocType::None);
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::Meshes, InSize, ELLMTracker::Default, ELLMAllocType::None);
	}

	~FOpenGLBaseBuffer( void )
	{
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::GraphicsPlatform, -(int64)GetSize(), ELLMTracker::Platform, ELLMAllocType::None);
		LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::Meshes, -(int64)GetSize(), ELLMTracker::Default, ELLMAllocType::None);
    }
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return true;
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

};

struct FOpenGLEUniformBufferData : public FRefCountedObject
{
	FOpenGLEUniformBufferData(uint32 SizeInBytes)
	{
		uint32 SizeInUint32s = (SizeInBytes + 3) / 4;
		Data.Empty(SizeInUint32s);
		Data.AddUninitialized(SizeInUint32s);
		IncrementBufferMemory(GL_UNIFORM_BUFFER,Data.GetAllocatedSize());
	}

	~FOpenGLEUniformBufferData()
	{
		DecrementBufferMemory(GL_UNIFORM_BUFFER,Data.GetAllocatedSize());
	}

	TArray<uint32> Data;
};
typedef TRefCountPtr<FOpenGLEUniformBufferData> FOpenGLEUniformBufferDataRef;

class FOpenGLUniformBuffer : public FRHIUniformBuffer
{
public:
	/** The GL resource for this uniform buffer. */
	GLuint Resource;

	/** The offset of the uniform buffer's contents in the resource. */
	uint32 Offset;

	/** When using a persistently mapped buffer this is a pointer to the CPU accessible data. */
	uint8* PersistentlyMappedBuffer;

	/** Unique ID for state shadowing purposes. */
	uint32 UniqueID;

	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

	/** Emulated uniform data for ES2. */
	FOpenGLEUniformBufferDataRef EmulatedBufferData;

	/** The size of the buffer allocated to hold the uniform buffer contents. May be larger than necessary. */
	uint32 AllocatedSize;

	/** True if the uniform buffer is not used across frames. */
	bool bStreamDraw;

	/** True if the uniform buffer is emulated */
	bool bIsEmulatedUniformBuffer;

	/** Initialization constructor. */
	FOpenGLUniformBuffer(const FRHIUniformBufferLayout* InLayout);

	void SetGLUniformBufferParams(GLuint InResource, uint32 InOffset, uint8* InPersistentlyMappedBuffer, uint32 InAllocatedSize, FOpenGLEUniformBufferDataRef InEmulatedBuffer, bool bInStreamDraw);

	/** Destructor. */
	~FOpenGLUniformBuffer();

	FOpenGLAssertRHIThreadFence AccessFence;
	FOpenGLAssertRHIThreadFence CopyFence;
};

typedef TOpenGLBuffer<FOpenGLBasePixelBuffer, CachedBindPixelUnpackBuffer> FOpenGLPixelBuffer;
typedef TOpenGLBuffer<FOpenGLBaseBuffer, CachedBindBuffer> FOpenGLBuffer;

#define MAX_STREAMED_BUFFERS_IN_ARRAY 2	// must be > 1!
#define MIN_DRAWS_IN_SINGLE_BUFFER 16

template <typename BaseType, uint32 Stride>
class TOpenGLStreamedBufferArray
{
public:

	TOpenGLStreamedBufferArray( void ) {}
	virtual ~TOpenGLStreamedBufferArray( void ) {}

	void Init( uint32 InitialBufferSize )
	{
		for( int32 BufferIndex = 0; BufferIndex < MAX_STREAMED_BUFFERS_IN_ARRAY; ++BufferIndex )
		{
			Buffer[BufferIndex] = new BaseType(Stride, InitialBufferSize, BUF_Volatile, NULL, true);
		}
		CurrentBufferIndex = 0;
		CurrentOffset = 0;
		LastOffset = 0;
		MinNeededBufferSize = InitialBufferSize / MIN_DRAWS_IN_SINGLE_BUFFER;
	}

	void Cleanup( void )
	{
		for( int32 BufferIndex = 0; BufferIndex < MAX_STREAMED_BUFFERS_IN_ARRAY; ++BufferIndex )
		{
			Buffer[BufferIndex].SafeRelease();
		}
	}

	uint8* Lock( uint32 DataSize )
	{
		check(!Buffer[CurrentBufferIndex]->IsLocked());
		DataSize = Align(DataSize, (1<<8));	// to keep the speed up, let's start data for each next draw at 256-byte aligned offset

		// Keep our dynamic buffers at least MIN_DRAWS_IN_SINGLE_BUFFER times bigger than max single request size
		uint32 NeededBufSize = Align( MIN_DRAWS_IN_SINGLE_BUFFER * DataSize, (1 << 20) );	// 1MB increments
		if (NeededBufSize > MinNeededBufferSize)
		{
			MinNeededBufferSize = NeededBufSize;
		}

		// Check if we need to switch buffer, as the current draw data won't fit in current one
		bool bDiscard = false;
		if (Buffer[CurrentBufferIndex]->GetSize() < CurrentOffset + DataSize)
		{
			// We do.
			++CurrentBufferIndex;
			if (CurrentBufferIndex == MAX_STREAMED_BUFFERS_IN_ARRAY)
			{
				CurrentBufferIndex = 0;
			}
			CurrentOffset = 0;

			// Check if we should extend the next buffer, as max request size has changed
			if (MinNeededBufferSize > Buffer[CurrentBufferIndex]->GetSize())
			{
				Buffer[CurrentBufferIndex].SafeRelease();
				Buffer[CurrentBufferIndex] = new BaseType(Stride, MinNeededBufferSize, BUF_Volatile);
			}

			bDiscard = true;
		}

		LastOffset = CurrentOffset;
		CurrentOffset += DataSize;

		return Buffer[CurrentBufferIndex]->LockWriteOnlyUnsynchronized(LastOffset, DataSize, bDiscard);
	}

	void Unlock( void )
	{
		check(Buffer[CurrentBufferIndex]->IsLocked());
		Buffer[CurrentBufferIndex]->Unlock();
	}

	BaseType* GetPendingBuffer( void ) { return Buffer[CurrentBufferIndex]; }
	uint32 GetPendingOffset( void ) { return LastOffset; }

private:
	TRefCountPtr<BaseType> Buffer[MAX_STREAMED_BUFFERS_IN_ARRAY];
	uint32 CurrentBufferIndex;
	uint32 CurrentOffset;
	uint32 LastOffset;
	uint32 MinNeededBufferSize;
};

struct FOpenGLVertexElement
{
	GLenum Type;
	GLuint StreamIndex;
	GLuint Offset;
	GLuint Size;
	GLuint Divisor;
	GLuint HashStride;
	uint8 bNormalized;
	uint8 AttributeIndex;
	uint8 bShouldConvertToFloat;
	uint8 Padding;

	FOpenGLVertexElement()
		: Padding(0)
	{
	}
};

/** Convenience typedef: preallocated array of OpenGL input element descriptions. */
typedef TArray<FOpenGLVertexElement,TFixedAllocator<MaxVertexElementCount> > FOpenGLVertexElements;

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FOpenGLVertexDeclaration : public FRHIVertexDeclaration
{
public:
	/** Elements of the vertex declaration. */
	FOpenGLVertexElements VertexElements;

	uint16 StreamStrides[MaxVertexElementCount];

	/** Initialization constructor. */
	FOpenGLVertexDeclaration(const FOpenGLVertexElements& InElements, const uint16* InStrides)
		: VertexElements(InElements)
	{
		FMemory::Memcpy(StreamStrides, InStrides, sizeof(StreamStrides));
	}
	
	virtual bool GetInitializer(FVertexDeclarationElementList& Init) override final;
};


/**
 * Combined shader state and vertex definition for rendering geometry.
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FOpenGLBoundShaderState : public FRHIBoundShaderState
{
public:

	FCachedBoundShaderStateLink CacheLink;

	uint16 StreamStrides[MaxVertexElementCount];

	FOpenGLLinkedProgram* LinkedProgram;
	TRefCountPtr<FOpenGLVertexDeclaration> VertexDeclaration;
	TRefCountPtr<FOpenGLVertexShaderProxy> VertexShaderProxy;
	TRefCountPtr<FOpenGLPixelShaderProxy> PixelShaderProxy;
	TRefCountPtr<FOpenGLGeometryShaderProxy> GeometryShaderProxy;

	/** Initialization constructor. */
	FOpenGLBoundShaderState(
		FOpenGLLinkedProgram* InLinkedProgram,
		FRHIVertexDeclaration* InVertexDeclarationRHI,
		FRHIVertexShader* InVertexShaderRHI,
		FRHIPixelShader* InPixelShaderRHI,
		FRHIGeometryShader* InGeometryShaderRHI
		);

	const TBitArray<>& GetTextureNeeds(int32& OutMaxTextureStageUsed);
	const TBitArray<>& GetUAVNeeds(int32& OutMaxUAVUnitUsed) const;
	void GetNumUniformBuffers(int32 NumVertexUniformBuffers[SF_NumGraphicsFrequencies]);

	bool NeedsTextureStage(int32 TextureStageIndex);
	int32 MaxTextureStageUsed();
	bool RequiresDriverInstantiation();

	FOpenGLVertexShader* GetVertexShader()
	{
		check(IsValidRef(VertexShaderProxy));
		return VertexShaderProxy->GetGLResourceObject();
	}

	FOpenGLPixelShader* GetPixelShader()
	{
		check(IsValidRef(PixelShaderProxy));
		return PixelShaderProxy->GetGLResourceObject();
	}

	FOpenGLGeometryShader* GetGeometryShader()	{ return GeometryShaderProxy ? GeometryShaderProxy->GetGLResourceObject() : nullptr;}

	virtual ~FOpenGLBoundShaderState();
};

class FTextureEvictionLRU
{
private:
	typedef TPsoLruCache<FOpenGLTexture*, FOpenGLTexture*> FOpenGLTextureLRUContainer;
	FCriticalSection TextureLRULock;

	static FORCEINLINE_DEBUGGABLE FOpenGLTextureLRUContainer& GetLRUContainer()
	{
		const int32 MaxNumLRUs = 10000;
		static FOpenGLTextureLRUContainer TextureLRU(MaxNumLRUs);
		return TextureLRU;
	}

public:

	static FORCEINLINE_DEBUGGABLE FTextureEvictionLRU& Get()
	{
		static FTextureEvictionLRU Lru;
		return Lru;
	}
	uint32 Num() const { return GetLRUContainer().Num(); }

	void Remove(FOpenGLTexture* TextureBase);
	bool Add(FOpenGLTexture* TextureBase);
	void Touch(FOpenGLTexture* TextureBase);
	void TickEviction();
	FOpenGLTexture* GetLeastRecent();
};
class FTextureEvictionParams
{
public:
	FTextureEvictionParams(uint32 NumMips);
	~FTextureEvictionParams();
	TArray<TArray<uint8>> MipImageData;

 	uint32 bHasRestored : 1;	
	FSetElementId LRUNode;
	uint32 FrameLastRendered;

#if GLDEBUG_LABELS_ENABLED
	FAnsiCharArray TextureDebugName;
	void SetDebugLabelName(const FAnsiCharArray& TextureDebugNameIn) { TextureDebugName = TextureDebugNameIn; }
	void SetDebugLabelName(const ANSICHAR * TextureDebugNameIn) { TextureDebugName.Append(TextureDebugNameIn, FCStringAnsi::Strlen(TextureDebugNameIn) + 1); }
	FAnsiCharArray& GetDebugLabelName() { return TextureDebugName; }
#else
	void SetDebugLabelName(FAnsiCharArray TextureDebugNameIn) { checkNoEntry(); }
	FAnsiCharArray& GetDebugLabelName() { checkNoEntry(); static FAnsiCharArray Dummy;  return Dummy; }
#endif

	void SetMipData(uint32 MipIndex, const void* Data, uint32 Bytes);
	void ReleaseMipData(uint32 RetainMips);

	void CloneMipData(const FTextureEvictionParams& Src, uint32 NumMips, int32 SrcOffset, int DstOffset);

	uint32 GetTotalAllocated() const
	{
		uint32 TotalAllocated = 0;
		for (const auto& MipData : MipImageData)
		{
			TotalAllocated += MipData.Num();
		}
		return TotalAllocated;
	}

	bool AreAllMipsPresent() const
	{
		bool bRet = MipImageData.Num() > 0;
		for (const auto& MipData : MipImageData)
		{
			bRet = bRet && MipData.Num() > 0;
		}
		return bRet;
	}
};

class FOpenGLTextureDesc
{
public:
	FOpenGLTextureDesc(FRHITextureDesc const& InDesc);

	GLenum Target = GL_NONE;
	GLenum Attachment = GL_NONE;

	uint32 MemorySize = 0;

	uint8 bCubemap            : 1;
	uint8 bArrayTexture       : 1;
	uint8 bStreamable         : 1;
	uint8 bDepthStencil       : 1;
	uint8 bCanCreateAsEvicted : 1;
	uint8 bIsPowerOfTwo       : 1;
	uint8 bMultisampleRenderbuffer : 1;

private:
	static bool CanDeferTextureCreation();
};

class FOpenGLTextureCreateDesc : public FRHITextureCreateDesc, public FOpenGLTextureDesc
{
public:
	FOpenGLTextureCreateDesc(FRHITextureCreateDesc const& CreateDesc)
		: FRHITextureCreateDesc(CreateDesc)
		, FOpenGLTextureDesc(CreateDesc)
	{
	}
};

class OPENGLDRV_API FOpenGLTexture : public FRHITexture
{
	// Prevent copying
	FOpenGLTexture(FOpenGLTexture const&) = delete;
	FOpenGLTexture& operator = (FOpenGLTexture const&) = delete;

public:
	// Standard constructor.
	explicit FOpenGLTexture(FOpenGLTextureCreateDesc const& CreateDesc);

	// Constructor for external resources (RHICreateTexture2DFromResource etc).
	explicit FOpenGLTexture(FOpenGLTextureCreateDesc const& CreateDesc, GLuint Resource);

	// Constructor for RHICreateAliasedTexture
	enum EAliasConstructorParam { AliasResource };
	explicit FOpenGLTexture(FOpenGLTexture& Texture, const FString& Name, EAliasConstructorParam);
	void AliasResources(FOpenGLTexture& Texture);

	virtual ~FOpenGLTexture();

	GLuint GetResource()
	{
		TryRestoreGLResource();
		return Resource;
	}

	GLuint& GetResourceRef() 
	{ 
		VERIFY_GL_SCOPE();
		TryRestoreGLResource();
		return Resource;
	}

	// GetRawResourceName - A const accessor to the resource name, this could potentially be an evicted resource.
	// It will not trigger the GL resource's creation.
	GLuint GetRawResourceName() const
	{
		return Resource;
	}

	// GetRawResourceNameRef - A const accessor to the resource name, this could potentially be an evicted resource.
	// It will not trigger the GL resource's creation.
	const GLuint& GetRawResourceNameRef() const
	{
		return Resource;
	}

	void SetResource(GLuint InResource)
	{
		VERIFY_GL_SCOPE();
		Resource = InResource;
	}

	bool IsEvicted() const { VERIFY_GL_SCOPE(); return EvictionParamsPtr.IsValid() && !EvictionParamsPtr->bHasRestored; }

	virtual void* GetTextureBaseRHI() override final
	{
		return this;
	}

	/**
	 * Locks one of the texture's mip-maps.
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/**
	* Returns the size of the memory block that is returned from Lock, threadsafe
	*/
	uint32 GetLockSize(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map. */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);

	/** FRHITexture override.  See FRHITexture::GetNativeResource() */
	virtual void* GetNativeResource() const override
	{
		// this must become a full GL resource here, calling the non-const GetResourceRef ensures this.
		return const_cast<void*>(reinterpret_cast<const void*>(&const_cast<FOpenGLTexture*>(this)->GetResourceRef()));
	}

	/**
	 * Accessors to mark whether or not we have allocated storage for each mip/face.
	 * For non-cubemaps FaceIndex should always be zero.
	 */
	bool GetAllocatedStorageForMip(uint32 MipIndex, uint32 FaceIndex) const
	{
		return bAllocatedStorage[MipIndex * (bCubemap ? 6 : 1) + FaceIndex];
	}
	void SetAllocatedStorageForMip(uint32 MipIndex, uint32 FaceIndex)
	{
		bAllocatedStorage[MipIndex * (bCubemap ? 6 : 1) + FaceIndex] = true;
	}

	// Set allocated storage state for all mip/faces
	void SetAllocatedStorage(bool bInAllocatedStorage)
	{
		bAllocatedStorage.Init(bInAllocatedStorage, this->GetNumMips() * (bCubemap ? 6 : 1));
	}

	/**
	 * Clone texture from a source using CopyImageSubData
	 */
	void CloneViaCopyImage(FOpenGLTexture* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset);

	/**
	 * Clone texture from a source going via PBOs
	 */
	void CloneViaPBO(FOpenGLTexture* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset);

	/**
	 * Resolved the specified face for a read Lock, for non-renderable, CPU readable surfaces this eliminates the readback inside Lock itself.
	 */
	void Resolve(uint32 MipIndex, uint32 ArrayIndex);

	// Texture eviction
	void RestoreEvictedGLResource(bool bAttemptToRetainMips);
	bool CanBeEvicted();
	void TryEvictGLResource();

private:
	static void UpdateTextureStats(FOpenGLTexture* Texture, bool bAllocating);

	void TryRestoreGLResource()
	{
		if (EvictionParamsPtr.IsValid() && !EvictionParamsPtr->bHasRestored)
		{
			VERIFY_GL_SCOPE();
			if (!EvictionParamsPtr->bHasRestored)
			{
				RestoreEvictedGLResource(true);
			}
			else
			{
				check(CanBeEvicted());
				FTextureEvictionLRU::Get().Touch(this);
			}
		}
	}

	void DeleteGLResource();

	void Fill2DGLTextureImage(const FOpenGLTextureFormat& GLFormat, const bool bSRGB, uint32 MipIndex, const void* LockedBuffer, uint32 LockedSize, uint32 ArrayIndex);

	uint32 GetEffectiveSizeZ() const
	{
		FRHITextureDesc const& Desc = GetDesc();

		return Desc.IsTexture3D()
			? Desc.Depth
			: Desc.ArraySize;
	}

private:
	/** The OpenGL texture resource. */
	GLuint Resource = GL_NONE;

public:
	/** The OpenGL texture target. */
	GLenum const Target = 0;

	/** The OpenGL attachment point. This should always be GL_COLOR_ATTACHMENT0 in case of color buffer, but the actual texture may be attached on other color attachments. */
	GLenum const Attachment = 0;

	TUniquePtr<FTextureEvictionParams> EvictionParamsPtr;

	// Pointer to current sampler state in this unit
	class FOpenGLSamplerState* SamplerState = nullptr;

private:
	TArray<TRefCountPtr<FOpenGLPixelBuffer>> PixelBuffers;

	/** Bitfields marking whether we have allocated storage for each mip */
	TBitArray<TInlineAllocator<1> > bAllocatedStorage;

public:
	uint32 const MemorySize;

public:
	uint8 const bIsPowerOfTwo       : 1;
	uint8 const bCanCreateAsEvicted : 1;
	uint8 const bStreamable         : 1;
	uint8 const bCubemap            : 1;
	uint8 const bArrayTexture       : 1;
	uint8 const bDepthStencil       : 1;
	uint8 const bAlias              : 1;
	uint8 const bMultisampleRenderbuffer : 1;
};

template <typename T>
struct TIsGLResourceWithFence
{
	enum
	{
		Value = TOr<
		TPointerIsConvertibleFromTo<T, const FOpenGLTexture>
		//		,TIsDerivedFrom<T, FRHITexture>
		>::Value
	};
};

template<typename T>
static typename TEnableIf<!TIsGLResourceWithFence<T>::Value>::Type CheckRHITFence(T* Resource) {}

template<typename T>
static typename TEnableIf<TIsGLResourceWithFence<T>::Value>::Type CheckRHITFence(T* Resource)
{
	Resource->CreationFence.WaitFenceRenderThreadOnly();
}

/** Given a pointer to a RHI texture that was created by the OpenGL RHI, returns a pointer to the FOpenGLTexture it encapsulates. */
inline FOpenGLTexture* GetOpenGLTextureFromRHITexture(FRHITexture* TextureRHI)
{
	if (!TextureRHI)
	{
		return nullptr;
	}
	else
	{
		return static_cast<FOpenGLTexture*>(TextureRHI->GetTextureBaseRHI());
	}
}

class FOpenGLRenderQuery : public FRHIRenderQuery
{
public:

	/** The query resource. */
	GLuint Resource;

	/** Identifier of the OpenGL context the query is a part of. */
	uint64 ResourceContext;

	/** The cached query result. */
	GLuint64 Result;

	FOpenGLAssertRHIThreadFence CreationFence;

	FThreadSafeCounter TotalBegins;
	FThreadSafeCounter TotalResults;

	/** true if the context the query is in was released from another thread */
	bool bResultWasSuccess;

	/** true if the context the query is in was released from another thread */
	bool bInvalidResource;

	// todo: memory optimize
	ERenderQueryType QueryType;

	FOpenGLRenderQuery(ERenderQueryType InQueryType);
	virtual ~FOpenGLRenderQuery();

	void AcquireResource();
	static void ReleaseResource(GLuint Resource, uint64 ResourceContext);
};

class FOpenGLUnorderedAccessView : public FRHIUnorderedAccessView
{

public:
	FOpenGLUnorderedAccessView(FRHIViewableResource* InParentResource):
		FRHIUnorderedAccessView(InParentResource),
		Resource(0),
		BufferResource(0),
		Format(0),
		UnrealFormat(0)
	{

	}

	GLuint	Resource;
	GLuint	BufferResource;
	GLenum	Format;
	uint8	UnrealFormat;

	virtual uint32 GetBufferSize()
	{
		return 0;
	}

	virtual bool IsLayered() const
	{
		return false;
	}

	virtual GLint GetLayer() const
	{
		return 0;
	}

};

class FOpenGLTextureUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:

	FOpenGLTextureUnorderedAccessView(FRHITexture* InTexture);

	FTextureRHIRef TextureRHI; // to keep the texture alive
	bool bLayered;

	virtual bool IsLayered() const override
	{
		return bLayered;
	}
};

class FOpenGLTexBufferUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:

	FOpenGLTexBufferUnorderedAccessView();
	
	FOpenGLTexBufferUnorderedAccessView(FOpenGLDynamicRHI* InOpenGLRHI, FRHIBuffer* InBuffer, uint8 Format);
	
	virtual ~FOpenGLTexBufferUnorderedAccessView();

	FBufferRHIRef BufferRHI; // to keep source buffer alive

	FOpenGLDynamicRHI* OpenGLRHI;

	virtual uint32 GetBufferSize() override;
};

class FOpenGLBufferUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:
	FOpenGLBufferUnorderedAccessView();
	
	FOpenGLBufferUnorderedAccessView(FOpenGLDynamicRHI* InOpenGLRHI, FRHIBuffer* InBuffer);

	virtual ~FOpenGLBufferUnorderedAccessView();

	FBufferRHIRef BufferRHI; // to keep source buffer alive

	FOpenGLDynamicRHI* OpenGLRHI;

	virtual uint32 GetBufferSize() override;
};

class FOpenGLShaderResourceView : public FRHIShaderResourceView
{
public:
	explicit FOpenGLShaderResourceView(const FShaderResourceViewInitializer& Initializer);
	explicit FOpenGLShaderResourceView(FOpenGLTexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo);

	virtual ~FOpenGLShaderResourceView();

	/** OpenGL texture the buffer is bound with */
	GLuint Resource = GL_NONE;
	GLenum Target = GL_TEXTURE_BUFFER;

	/** Needed on GL <= 4.2 to copy stencil data out of combined depth-stencil surfaces. */
	TRefCountPtr<FOpenGLTexture> Texture;
	TRefCountPtr<FOpenGLBuffer> Buffer;

	int32 LimitMip = -1;

private:
	bool OwnsResource = false;
};

void OPENGLDRV_API ReleaseOpenGLFramebuffers(FRHITexture* TextureRHI);

/** A OpenGL event query resource. */
class FOpenGLEventQuery
{
public:
	/** Initialization constructor. */
	FOpenGLEventQuery();
	~FOpenGLEventQuery();

	/** Issues an event for the query to poll. */
	void IssueEvent();

	/** Waits for the event query to finish. */
	void WaitForCompletion();

private:
	UGLsync Sync = {};
};

class FOpenGLViewport : public FRHIViewport
{
public:

	FOpenGLViewport(class FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen,EPixelFormat PreferredPixelFormat);
	~FOpenGLViewport();

	void Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);

	// Accessors.
	FIntPoint GetSizeXY() const { return FIntPoint(SizeX, SizeY); }
	FOpenGLTexture* GetBackBuffer() const { return BackBuffer; }
	bool IsFullscreen( void ) const { return bIsFullscreen; }

	virtual void WaitForFrameEventCompletion() override;
	virtual void IssueFrameEvent() override;

	virtual void* GetNativeWindow(void** AddParam) const override;

	struct FPlatformOpenGLContext* GetGLContext() const { return OpenGLContext; }
	FOpenGLDynamicRHI* GetOpenGLRHI() const { return OpenGLRHI; }

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override
	{
		CustomPresent = InCustomPresent;
	}
	FRHICustomPresent* GetCustomPresent() const { return CustomPresent.GetReference(); }
private:

	friend class FOpenGLDynamicRHI;

	FOpenGLDynamicRHI* OpenGLRHI;
	struct FPlatformOpenGLContext* OpenGLContext;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	bool bIsValid;
	TRefCountPtr<FOpenGLTexture> BackBuffer;
	TUniquePtr<FOpenGLEventQuery> FrameSyncEvent;
	FCustomPresentRHIRef CustomPresent;
};

class FOpenGLGPUFence final : public FRHIGPUFence
{
public:
	FOpenGLGPUFence(FName InName);
	~FOpenGLGPUFence() override;

	void Clear() override;
	bool Poll() const override;
	
	void WriteInternal();
private:
	struct FOpenGLGPUFenceProxy* Proxy;
};

class FOpenGLStagingBuffer final : public FRHIStagingBuffer
{
	friend class FOpenGLDynamicRHI;
public:
	FOpenGLStagingBuffer() : FRHIStagingBuffer()
	{
		Initialize();
	}

	~FOpenGLStagingBuffer() override;

	// Locks the shadow of VertexBuffer for read. This will stall the RHI thread.
	void *Lock(uint32 Offset, uint32 NumBytes) override;

	// Unlocks the shadow. This is an error if it was not locked previously.
	void Unlock() override;

	uint64 GetGPUSizeBytes() const override { return ShadowSize; }
private:
	void Initialize();

	GLuint ShadowBuffer;
	uint32 ShadowSize;
	void* Mapping;
};

template<class T>
struct TOpenGLResourceTraits
{
};
template<>
struct TOpenGLResourceTraits<FRHIGPUFence>
{
	typedef FOpenGLGPUFence TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIStagingBuffer>
{
	typedef FOpenGLStagingBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIVertexDeclaration>
{
	typedef FOpenGLVertexDeclaration TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIVertexShader>
{
	typedef FOpenGLVertexShaderProxy TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIGeometryShader>
{
	typedef FOpenGLGeometryShaderProxy TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIPixelShader>
{
	typedef FOpenGLPixelShaderProxy TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIComputeShader>
{
	typedef FOpenGLComputeShaderProxy TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIBoundShaderState>
{
	typedef FOpenGLBoundShaderState TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIRenderQuery>
{
	typedef FOpenGLRenderQuery TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIUniformBuffer>
{
	typedef FOpenGLUniformBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIBuffer>
{
	typedef FOpenGLBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIShaderResourceView>
{
	typedef FOpenGLShaderResourceView TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIUnorderedAccessView>
{
	typedef FOpenGLUnorderedAccessView TConcreteType;
};

template<>
struct TOpenGLResourceTraits<FRHIViewport>
{
	typedef FOpenGLViewport TConcreteType;
};
