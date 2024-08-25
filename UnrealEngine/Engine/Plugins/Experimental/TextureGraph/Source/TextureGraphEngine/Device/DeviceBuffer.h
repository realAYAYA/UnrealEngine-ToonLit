// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Data/RawBuffer.h"
#include "Helper/DataUtil.h"
#include "DeviceType.h"
#include "Data/G_Collectible.h"


class Device;
class Device_Mem;
class BlobTransform;
struct ResourceBindInfo;

struct TEXTUREGRAPHENGINE_API BufferResult
{
	std::exception_ptr				ExInner;			/// Original exception that was raised by the action
	int32							ErrorCode = 0;		/// What is the error code
};

//////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<BufferResult>			BufferResultPtr;
typedef cti::continuable<BufferResultPtr>		AsyncBufferResultPtr;
typedef std::function<void(BufferResultPtr)>	BufferResult_Callback;
typedef cti::continuable<int32>					AsyncPrepareResult;

class DeviceBufferRef;

typedef std::vector<DeviceType>					DeviceTransferChain;

class DeviceNativeTask;
typedef std::shared_ptr<DeviceNativeTask>		DeviceNativeTaskPtr;
typedef std::vector<DeviceNativeTaskPtr>		DeviceNativeTaskPtrVec;

//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API DeviceBuffer : public G_Collectible
{
	friend class Device;
	friend class Blob;
	friend class Blobber;

public:
	static const DeviceTransferChain DefaultTransferChain;		/// Default transfer Chain
	static const DeviceTransferChain FXOnlyTransferChain;		/// Only exist on the FX device
	static const DeviceTransferChain PersistentTransferChain;	/// Persistent


protected:
	Device*							OwnerDevice = nullptr;		/// The device that this belongs to
	BufferDescriptor				Desc;						/// The descriptor for this buffer

	mutable CHashPtr				HashValue;					/// The actual RawObj buffer HashValue
	RawBufferPtr					RawData;					/// Raw buffer pointer that we keep cached (If we happen to have it)

	bool							bMarkedForCollection = false; /// Whether this instance is marked for collection or not
	bool							bIsPersistent = false;		/// Whether this is persistent or not
	bool							Chain[static_cast<int32>(DeviceType::Count)] = {false}; /// Device transfer Chain

	virtual CHashPtr				CalcHash();

	/// Only accessible from Device. Should not be used elsewhere
	virtual DeviceBuffer*			CreateFromRaw(RawBufferPtr RawObj) = 0;
	virtual DeviceBuffer*			CreateFromDesc(BufferDescriptor Desc, CHashPtr HashValue) = 0;
	virtual DeviceBuffer*			CopyFrom(DeviceBuffer* RHS);
	virtual DeviceBuffer*			Clone();

	/// DO NOT MAKE THIS METHOD PUBLIC!
	/// Its not meant to be accessible outside
	virtual AsyncBufferResultPtr	UpdateRaw(RawBufferPtr RawObj);

	/// To be called from the Device owner only!
	virtual void					ReleaseNative();

	virtual void					Touch(uint64 BatchId);

	FORCEINLINE void				MarkForCollection() { bMarkedForCollection = true; }

public:
									DeviceBuffer(Device* Dev, BufferDescriptor NewDesc, CHashPtr NewHash);
									DeviceBuffer(Device* Dev, RawBufferPtr RawObj);
									DeviceBuffer(const DeviceBuffer& RHS) = delete;	/// non-copyable
	virtual							~DeviceBuffer() override;

	//////////////////////////////////////////////////////////////////////////
	/// Get the RawObj memory buffer against this device buffer. Note that
	/// it can be very expensive to get the RawObj buffer against a given
	/// optimised device buffer. Use this with caution.
	/// These are generally meant to be either used by device native buffers
	/// (e.g. Tex class) or background idle services. If you're calling these
	/// inplace in the middle of the rendering cycle, you're most probably 
	/// doing something terribly wrong. Please speak to someone before 
	/// attempting
	//////////////////////////////////////////////////////////////////////////
	virtual RawBufferPtr			Raw_Now() = 0;	/// SLOW: Read above
	virtual AsyncRawBufferPtr		Raw();			/// SLOW: Read above [use Blob::Raw instead. This is to be used from Blob only]

protected:
	virtual AsyncBufferResultPtr	TransferFrom(DeviceBufferRef& Source) = 0;

public:

	//////////////////////////////////////////////////////////////////////////
	virtual size_t					MemSize() const = 0;
	virtual size_t					DeviceNative_MemSize() const { return 0; }

	virtual AsyncBufferResultPtr	Bind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) = 0;
	virtual AsyncBufferResultPtr	Unbind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo);

	virtual AsyncBufferResultPtr	Flush(const ResourceBindInfo& BindInfo);
	virtual bool					IsValid() const;
	virtual bool					IsNull() const;

	/// With base implementations
	virtual CHashPtr				Hash(bool Calculate = true);
	virtual bool					IsCompatible(Device* Dev) const;

	virtual RawBufferPtr			Min() { return nullptr; }
	virtual RawBufferPtr			Max() { return nullptr; }

	virtual void					SetHash(CHashPtr NewHash);
	virtual AsyncPrepareResult		PrepareForWrite(const ResourceBindInfo& BindInfo);

	void							SetDeviceTransferChain(const DeviceTransferChain& Chain, bool bPersistent = false);
	DeviceTransferChain				GetDeviceTransferChain(bool* Persistent = nullptr) const;
	virtual Device*					GetDowngradeDevice() const;
	virtual Device*					GetUpgradeDevice() const;
	virtual bool					IsTransient() const;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE Device*				GetOwnerDevice() const { return OwnerDevice; }
	FORCEINLINE const 
		BufferDescriptor&			Descriptor() const { return Desc; };
	FORCEINLINE FString&			GetName() { return Desc.Name; };
	FORCEINLINE bool				HasRaw() const { return RawData != nullptr; }

	FORCEINLINE const bool*			TransferChain() const { return Chain; }
	FORCEINLINE bool				IsPersistent() const { return bIsPersistent; }
};

//typedef std::shared_ptr<DeviceBuffer>	DeviceBufferPtr;

class TEXTUREGRAPHENGINE_API DeviceBufferPtr : public std::shared_ptr<DeviceBuffer>
{
public:
	DeviceBufferPtr() : std::shared_ptr<DeviceBuffer>() {}
	DeviceBufferPtr(DeviceBuffer* buffer);
	DeviceBufferPtr(const DeviceBufferPtr& RHS);
	DeviceBufferPtr(const std::shared_ptr<DeviceBuffer>& RHS);
	~DeviceBufferPtr();
};

typedef std::weak_ptr<DeviceBuffer>		DeviceBufferPtrW;
//typedef std::unique_ptr<DeviceBuffer>	DeviceBufferUPtr;

//typedef DeviceBufferPtr					DeviceBufferRef;
//typedef DeviceBufferPtrW				DeviceBufferRefW;

//////////////////////////////////////////////////////////////////////////
/// DeviceBufferRef
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API DeviceBufferRef
{
public:
	

private:
	friend class Device;
	friend class Blob;
	friend class TiledBlob;

	DeviceBufferPtr					Buffer;				/// The buffer info pointer

	void							Clear();
public:
									DeviceBufferRef() {}
	explicit						DeviceBufferRef(DeviceBufferPtr RHS);
	explicit						DeviceBufferRef(DeviceBuffer* RHS);

									~DeviceBufferRef();

	DeviceBufferRef&				operator = (const DeviceBufferRef& RHS);

	DeviceType						GetDeviceType() const; /// Need complete Device Definition so cannot be inlined here

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	//FORCEINLINE DeviceBufferRef		operator = (DeviceBuffer* RHS) { _buffer = DeviceBufferPtr(RHS); }
	FORCEINLINE bool				operator == (const DeviceBufferRef& RHS) const { return Buffer == RHS.Buffer; }
	FORCEINLINE bool				operator == (const DeviceBuffer* RHS) const { return Buffer.get() == RHS; }
	FORCEINLINE bool				operator != (const DeviceBufferRef& RHS) const { return Buffer != RHS.Buffer; }
	FORCEINLINE						operator bool () const { return IsValid(); }
	FORCEINLINE						operator DeviceBufferPtr() const { return Buffer; }
	FORCEINLINE DeviceBuffer*		operator -> () const { return Buffer.get(); }
	FORCEINLINE bool				IsValid() const { return Buffer != nullptr && Buffer->IsValid() ? true : false; }
	FORCEINLINE DeviceBuffer*		Get() const { return Buffer.get(); }
	FORCEINLINE DeviceBufferPtr		GetPtr() const { return Buffer; }

	/// STL compatibility
	FORCEINLINE DeviceBuffer*		get() const { return Buffer.get(); }	/// Compaibility with std::shared_ptr
	FORCEINLINE void				reset() { Clear(); }				/// Compaibility with std::shared_ptr
};

typedef cti::continuable<DeviceBufferRef>		AsyncDeviceBufferRef;
typedef std::function<void(DeviceBufferRef)>	DeviceBufferRef_Callback;
