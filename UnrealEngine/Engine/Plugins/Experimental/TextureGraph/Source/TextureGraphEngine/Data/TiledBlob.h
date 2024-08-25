// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blob.h"

class Job;
typedef std::weak_ptr<Job>			JobPtrW;
typedef T_BlobRef<Blob>				BlobRef;
typedef T_Tiles<BlobPtr>			BlobPtrTiles;

//////////////////////////////////////////////////////////////////////////
/// TiledBlob: Represents a BlobObj that is made up of many Tiles. These blobs
/// are always late bound i.e. they will not allocate any buffers 
/// unless they absolutely need to. 
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API TiledBlob : public Blob
{
	friend class Blobber;

protected:
	mutable BlobPtrTiles			Tiles;					/// The Tiles that make up the larger BlobObj
	mutable CHashPtr				HashValue;				/// The HashValue of tiled BlobObj
	mutable BufferDescriptor		Desc;					/// We keep the combined descriptor separately
	bool							bReady = false;			/// Whether the Buffer is already ready
	bool							bTiledTarget = true;	/// Whether we're rendering to this as a tile by tile Target or not
	JobPtrW							JobObj;					/// The job that is potentially generating this tiled BlobObj
	BlobPtr							SingleBlob;			/// Single BlobObj pointer that we keep to prevent going out of Ref

	void							CalcHashNow() const;
	virtual void					ResetBuffer() override;

	virtual void					TouchTiles(uint64 BatchId);
	virtual void					Touch(uint64 BatchId) override;
	virtual void					MakeSingleBlob_Internal();
	TiledBlob&						operator = (const TiledBlob& RHS);
	virtual void					SetHash(CHashPtr Hash) override;

	virtual void					AddLinkedBlob(BlobPtr LinkedBlob) override;
	virtual void					FinaliseFrom(Blob* RHS) override;

public:
	static AsyncBufferResultPtr		TileBuffer(DeviceBufferRef Buffer, BlobPtrTiles& Tiles);
	AsyncBufferResultPtr			CombineTiles(bool bTouch, bool bIsArray, uint64 BatchId = 0);

									TiledBlob(const BufferDescriptor& Desc, const BlobPtrTiles& Tiles);

public:
									TiledBlob(const BufferDescriptor& Desc, size_t NumTilesX, size_t NumTilesY, CHashPtr HashValue);
									TiledBlob(DeviceBufferRef Buffer);
									TiledBlob(BlobRef BlobObj);
	virtual							~TiledBlob() override;

	static std::shared_ptr<TiledBlob> InitFromTiles(const BufferDescriptor& Desc, BlobPtrTiles& Tiles);

	virtual AsyncBufferResultPtr	Bind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;
	virtual AsyncBufferResultPtr	Unbind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;

	virtual bool					IsValid() const override;
	virtual bool					IsNull() const override;
	virtual AscynCHashPtr			CalcHash() override;	/// SLOW: Because calls Raw() underneath. Don't use directly

	virtual CHashPtr				Hash() const override;
	virtual AsyncBufferResultPtr	Flush(const ResourceBindInfo& BindInfo) override;
	virtual AsyncPrepareResult		PrepareForWrite(const ResourceBindInfo& BindInfo) override;
	virtual FString					DisplayName() const override;
	virtual bool					IsTiled() const override { return true; }
	virtual bool					IsPromise() const { return false; }
	virtual bool					CanCalculateHash() const override;
	virtual void					SetTile(int32 X, int32 Y, BlobRef Tile);
	virtual void					SetTiles(const BlobPtrTiles& InTiles);

	virtual FString&				Name() override { return Desc.Name; }
	virtual const FString&			Name() const override { return Desc.Name; }
	virtual const BufferDescriptor& GetDescriptor() const override { return Desc; }

	virtual AsyncBufferResultPtr	MakeSingleBlob();

	virtual AsyncDeviceBufferRef	TransferTo(Device* Target) override;
	virtual DeviceBufferRef			GetBufferRef() const override;

	virtual void					ResolveLateBound(BlobPtr Ref);
	virtual void					ResolveLateBound(const BufferDescriptor& Desc, bool bOverrideExisting = false);
	virtual void					CopyResolveLateBound(BlobPtr RHS);
	virtual void					SetTransient();
	virtual void					FinaliseNow(bool bNoCalcHash, CHashPtr FixedHash) override;

	//////////////////////////////////////////////////////////////////////////
	/// Min/Max related
	//////////////////////////////////////////////////////////////////////////
	virtual void					SetMinMax(BlobPtr MinMax_) override;

	//////////////////////////////////////////////////////////////////////////
	/// LOD/MipMaps related
	//////////////////////////////////////////////////////////////////////////
	virtual void					SetLODLevel(int32 Level_, BlobPtr BlobObj, BlobPtrW LODParent_, BlobPtrW LODSource_, bool AddToBlobber) override;

	//////////////////////////////////////////////////////////////////////////
	/// Ownership related
	//////////////////////////////////////////////////////////////////////////
#if DEBUG_BLOB_REF_KEEPING == 1
	virtual bool					HasBlobAsTile(Blob* BlobObj) const override;
#endif 

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////

	/// Takes a BlobObj and returns a TiledBlob. If the passed BlobObj is already tiled, then
	/// it does nothing. Otherwise it creates a new TiledBlob with 1x1 tile that contains
	/// the passed BlobObj.
	/// Just a simple helper function, used in places that always require a TiledBlob.
	static std::shared_ptr<TiledBlob>	AsTiledBlob(BlobPtr BlobObj);

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE size_t				Rows() const { return Tiles.Rows(); }
	FORCEINLINE size_t				Cols() const { return Tiles.Cols(); }
	FORCEINLINE const BlobPtrTiles&	GetTiles() const { return Tiles; }
	FORCEINLINE uint32				GetWidth() const { return Desc.Width; }
	FORCEINLINE uint32				GetHeight() const { return Desc.Height; }
	FORCEINLINE bool				TiledTarget() const { return bTiledTarget; }
	FORCEINLINE bool&				TiledTarget() { return bTiledTarget; }
	FORCEINLINE bool				IsValidTileIndex(int32 TileX, int32 TileY) const
	{
		return TileX < static_cast<int32>(Tiles.Rows()) && TileY < static_cast<int32>(Tiles.Cols());
	}

	FORCEINLINE BlobRef				GetTile(int32 TileX, int32 TileY) const 
	{
		if (Tiles.Rows() == 1 && Tiles.Cols() == 1)
			return BlobRef(Tiles[0][0], false);

		check(IsValidTileIndex(TileX, TileY));
		return BlobRef(Tiles[TileX][TileY], false);
	}
	FORCEINLINE BlobPtr				GetSingleBlob() const { return SingleBlob; }

	JobPtrW							Job() const { return JobObj; }
	JobPtrW&						Job() { return JobObj; }
};

//////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<TiledBlob>	TiledBlobPtr;
typedef std::unique_ptr<TiledBlob>	TiledBlobUPtr;
typedef std::weak_ptr<TiledBlob>	TiledBlobPtrW;

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
/// This is a promised TiledBlob that will be the result of the job.
/// These are lazy evaluated.
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API TiledBlob_Promise : public TiledBlob
{
protected:
	mutable std::vector<BlobReadyCallback> Callbacks;		/// The callbacks to call when the promised tiled BlobObj is ready
	bool							bMakeSingleBlob = false;/// Make single BlobObj on finalise
	std::weak_ptr<TiledBlob_Promise> CachedBlob;			/// The cached blob that this is derived off

	virtual void					OnFinaliseInternal(BlobReadyCallback Callback) const override;
	TiledBlob_Promise&				operator = (const TiledBlob_Promise& RHS);
	virtual void					NotifyCallbacks();

	virtual void					AddLinkedBlob(BlobPtr LinkedBlob) override;
	virtual void					FinaliseFrom(Blob* RHS) override;

public:
									TiledBlob_Promise(TiledBlobPtr Source);
									TiledBlob_Promise(const BufferDescriptor& Desc, size_t NumTilesX, size_t NumTilesY, CHashPtr HashValue);
									TiledBlob_Promise(DeviceBufferRef Buffer);
	virtual							~TiledBlob_Promise();

	virtual AsyncBufferResultPtr	Bind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;
	virtual AsyncBufferResultPtr	Unbind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo) override;
	virtual AsyncBufferResultPtr	Flush(const ResourceBindInfo& BindInfo) override;
	virtual bool					IsFinalised() const override { return bIsFinalised; }
	virtual void					SetTile(int32 TileX, int32 TileY, BlobRef Tile) override;
	virtual AsyncBufferResultPtr	MakeSingleBlob() override;
	virtual void					CopyResolveLateBound(BlobPtr RHS) override;
	virtual bool					IsPromise() const override { return true; }

	virtual void					FinaliseNow(bool bNoCalcHash, CHashPtr FixedHash) override;
	virtual AsyncBufferResultPtr	Finalise(bool bNoCalcHash, CHashPtr FixedHash) override;
	void							FinaliseFrom(TiledBlobPtr RHS);

	void							ResetForReplay(); /// For debug purpose, reset the state of the tile as a promise to NOT finalised, increment the replayCount

	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE bool				IsSingleBlob() const { return bMakeSingleBlob; }
};

typedef std::shared_ptr<TiledBlob_Promise>	TiledBlob_PromisePtr;
typedef std::weak_ptr<TiledBlob_Promise>	TiledBlob_PromisePtrW;
typedef T_BlobRef<TiledBlob>				TiledBlobRef;
typedef cti::continuable<TiledBlobRef>		AsyncTiledBlobRef;
