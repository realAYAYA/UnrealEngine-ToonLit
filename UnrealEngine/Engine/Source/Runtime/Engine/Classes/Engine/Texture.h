// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "RenderCommandFence.h"
#include "Serialization/EditorBulkData.h"
#include "Engine/TextureDefines.h"
#include "MaterialValueType.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MaterialShared.h"
#include "TextureResource.h"
#include "RenderResource.h"
#endif
#include "Engine/StreamableRenderAsset.h"
#include "PerPlatformProperties.h"
#include "ImageCore.h"
#if WITH_EDITORONLY_DATA
#include "Misc/TVariant.h"
#include "ObjectCacheEventSink.h"
#include "DerivedDataCacheKeyProxy.h"
#endif

#if WITH_EDITOR
#include "Templates/DontCopy.h"
#endif

#include "Texture.generated.h"

namespace FOodleDataCompression {enum class ECompressor : uint8; enum class ECompressionLevel : int8; }

class FTextureReference;
class FTextureResource;
class ITargetPlatform;
class UAssetUserData;
struct FPropertyChangedEvent;

#if WITH_EDITORONLY_DATA
namespace UE::DerivedData { struct FValueId; }
#endif

USTRUCT()
struct FTextureSourceLayerColorInfo
{
	GENERATED_USTRUCT_BODY();

	/** Per channel min value of the colors for all blocks in mip0 in linear space */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	FLinearColor ColorMin = FLinearColor(EForceInit::ForceInitToZero);

	/** Per channel max value of the colors for all blocks in mip0 in linear space */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	FLinearColor ColorMax = FLinearColor(EForceInit::ForceInitToZero);
};

USTRUCT()
struct FTextureSourceBlock
{
	GENERATED_USTRUCT_BODY()

	ENGINE_API FTextureSourceBlock();

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 BlockX;

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 BlockY;

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 SizeX;

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 SizeY;

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 NumSlices;

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 NumMips;
};

/**
 * Texture source data management.
 */
USTRUCT()
struct FTextureSource
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor. */
	ENGINE_API FTextureSource();

	// should match ERawImageFormat::GetBytesPerPixel
	ENGINE_API static int64 GetBytesPerPixel(ETextureSourceFormat Format);
	// should match ERawImageFormat::IsHDR
	FORCEINLINE static bool IsHDR(ETextureSourceFormat Format) { return UE::TextureDefines::IsHDR(Format); }
	
	enum class ELockState : uint8
	{
		None,
		ReadOnly,
		ReadWrite
	};

#if WITH_EDITOR
	// FwdDecl for member structs
	struct FMipData;

	ENGINE_API void InitBlocked(const ETextureSourceFormat* InLayerFormats,
		const FTextureSourceBlock* InBlocks,
		int32 InNumLayers,
		int32 InNumBlocks,
		const uint8** InDataPerBlock);

	ENGINE_API void InitBlocked(const ETextureSourceFormat* InLayerFormats,
		const FTextureSourceBlock* InBlocks,
		int32 InNumLayers,
		int32 InNumBlocks,
		UE::Serialization::FEditorBulkData::FSharedBufferWithID NewData);

	ENGINE_API void InitLayered(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumLayers,
		int32 NewNumMips,
		const ETextureSourceFormat* NewLayerFormat,
		const uint8* NewData = NULL
		);

	ENGINE_API void InitLayered(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumLayers,
		int32 NewNumMips,
		const ETextureSourceFormat* NewLayerFormat,
		UE::Serialization::FEditorBulkData::FSharedBufferWithID NewData
	);

	/**
	 * Initialize the source data with the given size, number of mips, and format.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewNumSlices - The number of slices in the texture source data.
	 * @param NewNumMips - The number of mips in the texture source data.
	 * @param NewFormat - The format in which source data is stored.
	 * @param NewData - [optional] The new source data.
	 */
	ENGINE_API void Init(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumMips,
		ETextureSourceFormat NewFormat,
		const uint8* NewData = NULL
		);
	
	/**
	* Init and copy in texture bits from Image
	* 
	* @param Image -  Image to initialize with
	*
	* FImageView has gamma information too that is lost
	* TextureSource does not store gamma information (it's in the owning Texture)
	* this function does NOT set Texture->SRGB , you must do so!
	*
	* Init() does UseHashAsGuid
	* Init() must be done inside PreEdit/PostEdit on the owning Texture
	*
	*/
	ENGINE_API void Init(const FImageView & Image);

	/**
	 * Initialize the source data with the given size, number of mips, and format.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewNumSlices - The number of slices in the texture source data.
	 * @param NewNumMips - The number of mips in the texture source data.
	 * @param NewFormat - The format in which source data is stored.
	 * @param NewData - The new source data.
	 */
	ENGINE_API void Init(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumMips,
		ETextureSourceFormat NewFormat,
		UE::Serialization::FEditorBulkData::FSharedBufferWithID NewData
	);

	/**
	 * Initializes the source data for a 2D texture with a full mip chain.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewFormat - Format of the texture source data.
	 */
	ENGINE_API void Init2DWithMipChain(
		int32 NewSizeX,
		int32 NewSizeY,
		ETextureSourceFormat NewFormat
		);

	ENGINE_API void InitLayered2DWithMipChain(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumLayers,
		const ETextureSourceFormat* NewFormat
	);

	/**
	 * Initializes the source data for a cubemap with a full mip chain.
	 * @param NewSizeX - Width of each cube map face.
	 * @param NewSizeY - Height of each cube map face.
	 * @param NewFormat - Format of the cube map source data.
	 */
	ENGINE_API void InitCubeWithMipChain(
		int32 NewSizeX,
		int32 NewSizeY,
		ETextureSourceFormat NewFormat
		);

	/**
	 * Initialize the source data with the given size, number of mips, and format.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewNumMips - The number of mips in the texture source data.
	 * @param NewFormat - The format in which source data is stored.
	 * @param NewSourceData -The new source data.
	 * @param NewSourceFormat -The compression format of the new source data.
	 */
	ENGINE_API void InitWithCompressedSourceData(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumMips,
		ETextureSourceFormat NewFormat,
		const TArrayView64<uint8> NewSourceData,
		ETextureSourceCompressionFormat NewSourceFormat
	);

	/**
	 * Initialize the source data with the given size, number of mips, and format.
	 * @param NewSizeX - Width of the texture source data.
	 * @param NewSizeY - Height of the texture source data.
	 * @param NewNumMips - The number of mips in the texture source data.
	 * @param NewFormat - The format in which source data is stored.
	 * @param NewSourceData -The new source data.
	 * @param NewSourceFormat -The compression format of the new source data.
	 */
	ENGINE_API void InitWithCompressedSourceData(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumMips,
		ETextureSourceFormat NewFormat,
		UE::Serialization::FEditorBulkData::FSharedBufferWithID NewSourceData,
		ETextureSourceCompressionFormat NewSourceFormat
	);

	/** Make a copy with a torn-off BulkData that has the same Guid used for DDC as this->BulkData */
	FTextureSource CopyTornOff() const;

	/** PNG Compresses the source art if possible or tells the bulk data to zlib compress when it saves out to disk. */
	ENGINE_API void Compress();

	/** Force the GUID to change even if mip data has not been modified.
	This should be avoided; typically let the data hash be used as guid to uniquely identify the data. */
	ENGINE_API void ForceGenerateGuid();

	/** Use FMipLock to encapsulate a locked mip */

	/** Lock a mip for reading. */
	ENGINE_API const uint8* LockMipReadOnly(int32 BlockIndex, int32 LayerIndex, int32 MipIndex);

	/** Lock a mip for editing.
	Note that Lock/Unlock for edit automatically does UseHashAsGuid. */
	ENGINE_API uint8* LockMip(int32 BlockIndex, int32 LayerIndex, int32 MipIndex);

	/** Unlock a mip. */
	ENGINE_API void UnlockMip(int32 BlockIndex, int32 LayerIndex, int32 MipIndex);

	/** Retrieve a copy of the data for a particular mip.  Prefer GetMipImage. */
	ENGINE_API bool GetMipData(TArray64<uint8>& OutMipData, int32 BlockIndex, int32 LayerIndex, int32 MipIndex, class IImageWrapperModule* ImageWrapperModule = nullptr);
	
	/** Legacy API that defaults to LayerIndex 0.  Prefer GetMipImage. */
	FORCEINLINE bool GetMipData(TArray64<uint8>& OutMipData, int32 MipIndex, class IImageWrapperModule* ImageWrapperModule = nullptr)
	{
		return GetMipData(OutMipData, 0, 0, MipIndex, ImageWrapperModule);
	}

	/** Retrieve a copy of the MipData as an FImage */
	ENGINE_API bool GetMipImage(FImage & OutImage, int32 BlockIndex, int32 LayerIndex, int32 MipIndex);
	
	/** Retrieve a copy of the MipData as an FImage */
	FORCEINLINE bool GetMipImage(FImage & OutImage, int32 MipIndex)
	{
		return GetMipImage(OutImage, 0, 0, MipIndex);
	}

	/** Returns a FMipData structure that wraps around the entire mip chain for read only operations. This is more efficient than calling the above method once per mip. */
	ENGINE_API FMipData GetMipData(class IImageWrapperModule* ImageWrapperModule);

	/** Computes the size of a single mip. */
	ENGINE_API int64 CalcMipSize(int32 BlockIndex, int32 LayerIndex, int32 MipIndex) const;

	/** Computes the number of bytes per-pixel. */
	ENGINE_API int64 GetBytesPerPixel(int32 LayerIndex = 0) const;

	/** Return true if the source XY size is power-of-2.  Does not check Z size for volumes.  */
	UE_DEPRECATED(5.5,"Prefer AreAllBlocksPowerOfTwo, or IsBlockPowerOfTwo if you really only want one block")
	ENGINE_API bool IsPowerOfTwo(int32 BlockIndex = 0) const { return IsBlockPowerOfTwo(BlockIndex); }
	
	/** Return true if the source XY size is power-of-2.  Does not check Z size for volumes.  */
	ENGINE_API bool IsBlockPowerOfTwo(int32 BlockIndex) const;
	ENGINE_API bool AreAllBlocksPowerOfTwo() const;

	/** Returns true if source art is available. */
	ENGINE_API bool IsValid() const;

	/** Access the given block */
	ENGINE_API void GetBlock(int32 Index, FTextureSourceBlock& OutBlock) const;

	/** Logical size of the texture includes all blocks */
	ENGINE_API FIntPoint GetLogicalSize() const;

	/** Size of texture in blocks */
	ENGINE_API FIntPoint GetSizeInBlocks() const;

	/** Returns the unique ID string for this source art. */
	FString GetIdString() const;

	/** Returns the compression format of the source data in string format for use with the UI. */
	FString GetSourceCompressionAsString() const;

	/** Returns the compression format of the source data in enum format. */
	FORCEINLINE ETextureSourceCompressionFormat GetSourceCompression() const { return CompressionFormat; }
	FORCEINLINE bool IsSourceCompressed() const { return GetSourceCompression() != ETextureSourceCompressionFormat::TSCF_None; }

	/** Get GammaSpace for this Source (asks owner) */
	ENGINE_API EGammaSpace GetGammaSpace(int LayerIndex) const;
	
	/** Get TextureClass for this Source (asks owner) */
	ENGINE_API ETextureClass GetTextureClass() const;

	/** Get mipped number of slices (volumes mip but arrays don't) */
	ENGINE_API int GetMippedNumSlices(int NumSlices,int MipIndex) const;

	/** Support for copy/paste */
	void ExportCustomProperties(FOutputDevice& Out, uint32 Indent);
	void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn);

	/** Trivial accessors. These will only give values for Block0 so may not be correct for UDIM/multi-block textures, use GetBlock() for this case. */
	FGuid GetPersistentId() const { return BulkData.GetIdentifier(); }
	/** GetId() returns a hash of the Id member (data hash) and also the attributes of the Source.
	( GetId does not just return Id ) **/
	ENGINE_API FGuid GetId() const;
	FORCEINLINE int64 GetSizeX() const { return SizeX; }
	FORCEINLINE int64 GetSizeY() const { return SizeY; }
	FORCEINLINE int32 GetNumSlices() const { return NumSlices; }
	FORCEINLINE int32 GetNumMips() const { return NumMips; }
	FORCEINLINE int32 GetNumLayers() const { return NumLayers; }
	FORCEINLINE int32 GetNumBlocks() const { return Blocks.Num() + 1; }
	FORCEINLINE ETextureSourceFormat GetFormat(int32 LayerIndex = 0) const { return (LayerIndex == 0) ? Format : LayerFormat[LayerIndex]; }
	
	UE_DEPRECATED(5.1, "Use GetSourceCompression instead")
	FORCEINLINE bool IsPNGCompressed() const { return GetSourceCompression() == ETextureSourceCompressionFormat::TSCF_PNG; }
	
	// Warning: bLongLatCubemap is not correct.  LongLat Cubemaps often have bLongLatCubemap == false
	// bLongLatCubemap is sometimes set to true for cube arrays to disambiguate the case of 6 longlat cubemaps in an array
	ENGINE_API bool IsCubeOrCubeArray() const;
	ENGINE_API bool IsVolume() const;
	ENGINE_API bool IsLongLatCubemap() const;
	// returns volume depth, or 1 if not a volume
	ENGINE_API int GetVolumeSizeZ() const;

	FORCEINLINE int64 GetSizeOnDisk() const { return BulkData.GetPayloadSize(); }
	inline bool HasPayloadData() const { return BulkData.HasPayloadData(); }
	/** Returns true if the texture's bulkdata payload is either already in memory or if the payload is 0 bytes in length. It will return false if the payload needs to load from disk */
	FORCEINLINE bool IsBulkDataLoaded() const { return BulkData.DoesPayloadNeedLoading(); }

	// Apply a visitor to the bulkdata :
	ENGINE_API void OperateOnLoadedBulkData(TFunctionRef<void (const FSharedBuffer& BulkDataBuffer)> Operation);

	UE_DEPRECATED(5.0, "There is no longer a need to call LoadBulkDataWithFileReader, FTextureSource::BulkData can now load the data on demand without it.")
	FORCEINLINE bool LoadBulkDataWithFileReader() { return true; }

	FORCEINLINE void RemoveBulkData() { BulkData.UnloadData(); }
	
	/** Sets the GUID to use, and whether that GUID is actually a hash of some data. */
	ENGINE_API void SetId(const FGuid& InId, bool bInGuidIsHash);

	FORCEINLINE int64 CalcMipSize(int32 MipIndex) const { return CalcMipSize(0, 0, MipIndex); }
	/** Lock a mip for reading. */
	FORCEINLINE const uint8* LockMipReadOnly(int32 MipIndex) { return LockMipReadOnly(0, 0, MipIndex); }
	/** Lock a mip for editing. */
	FORCEINLINE uint8* LockMip(int32 MipIndex) { return LockMip(0, 0, MipIndex); }
	FORCEINLINE void UnlockMip(int32 MipIndex) { UnlockMip(0, 0, MipIndex); }

	inline void SetOwner(UTexture* InOwner) { Owner = InOwner; }

	struct FMipAllocation
	{
		/** Create an empty object */
		FMipAllocation() = default;
		/** Take a read only FSharedBuffer, will allocate a new buffer and copy from this if Read/Write access is requested */
		FMipAllocation(FSharedBuffer SrcData);

		// Do not actually do anything for copy constructor or assignments, this is required for as long as
		// we need to support the old bulkdata code path (although previously storing these allocations as
		// raw pointers would allow it to be assigned, this would most likely cause a mismatch in lock counts,
		// either in FTextureSource or the underlying bulkdata and was never actually safe)
		FMipAllocation(const FMipAllocation&) {}
		FMipAllocation& operator =(const FMipAllocation&) { return *this; }

		// We do allow rvalue assignment
		FMipAllocation(FMipAllocation&&);
		FMipAllocation& operator =(FMipAllocation&&);

		~FMipAllocation() = default;

		/** Release all currently owned data and return the object to the default state */
		void Reset();

		/** Returns true if the object contains no data */
		bool IsNull() const { return ReadOnlyReference.IsNull(); }

		/** Returns the overall size of the data in bytes */
		int64 GetSize() const { return ReadOnlyReference.GetSize(); }

		/** Returns a FSharedBuffer that contains the current texture data but cannot be directly modified */
		const FSharedBuffer& GetDataReadOnly() const { return ReadOnlyReference; }

		UE_DEPRECATED(5.4, "Use GetDataReadWriteView instead")
		uint8* GetDataReadWrite() { return (uint8*)GetDataReadWriteView().GetData(); }

		/** Returns a pointer that contains the current texture data and can be written to */
		FMutableMemoryView GetDataReadWriteView();

		/** Returns the internal FSharedBuffer and relinquish ownership, used to transfer the data to virtualized bulkdata */
		FSharedBuffer Release();

	private:
		void CreateReadWriteBuffer(const void* SrcData, int64 DataLength);

		struct FDeleterFree
		{
			void operator()(uint8* Ptr) const
			{
				if (Ptr)
				{
					FMemory::Free(Ptr);
				}
			}
		};

		FSharedBuffer					ReadOnlyReference;
		TUniquePtr<uint8, FDeleterFree>	ReadWriteBuffer;
	};

	/** Structure that encapsulates the decompressed texture data and can be accessed per mip */
	struct FMipData
	{
		/** Allow the copy constructor by rvalue*/
		FMipData(FMipData&& Other)
			: TextureSource(Other.TextureSource)
		{
			MipData = MoveTemp(Other.MipData);
		}

		~FMipData() = default;

		/** Disallow everything else so we don't get duplicates */
		FMipData() = delete;
		FMipData(const FMipData&) = delete;
		FMipData& operator=(const FMipData&) = delete;
		FMipData& operator=(FMipData&& Other) = delete;

		/** Get a copy of a given texture mip, to be stored in OutMipData */
		ENGINE_API bool GetMipData(TArray64<uint8>& OutMipData, int32 BlockIndex, int32 LayerIndex, int32 MipIndex) const;

		/** 
		 * Access the given texture mip in the form of a read only FSharedBuffer.
		 * This method does not make a copy of the mip but instead returns a view.
		 * The returned buffer will keep a ref count on the original full mip chain
		 * allocation, so will remain valid even after the FMipData object has been 
		 * destroyed.
		 */
		ENGINE_API FSharedBuffer GetMipData(int32 BlockIndex, int32 LayerIndex, int32 MipIndex) const;
		ENGINE_API FSharedBuffer GetMipDataWithInfo(int32 InBlockIndex, int32 InLayerIndex, int32 InMipIndex, FImageInfo& OutMipImageInfo) const;

		ENGINE_API bool IsValid() const { return !MipData.IsNull(); }

	private:
		// We only want to allow FTextureSource to create FMipData objects
		friend struct FTextureSource;

		ENGINE_API FMipData(const FTextureSource& InSource, FSharedBuffer InData);

		const FTextureSource& TextureSource;
		FSharedBuffer MipData;
	};
	
	/**
	* FMipLock to encapsulate a locked mip - acquires the lock in construct and unlocks in destruct.
	* 
	* Be very careful! Locking as ReadOnly will still get you a mutable ImageView! Altering a read only mip
	* has consequences!
	*/
	struct FMipLock
	{
		// constructor locks the mip (can fail, check IsValid())
		ENGINE_API FMipLock(ELockState InLockState,FTextureSource * InTextureSource,int32 InBlockIndex, int32 InLayerIndex, int32 InMipIndex);
		ENGINE_API FMipLock(ELockState InLockState,FTextureSource * InTextureSource,int32 InMipIndex);
		
		// move constructor
		ENGINE_API FMipLock(FMipLock&&);

		// destructor unlocks
		ENGINE_API ~FMipLock();
		
		ELockState LockState;
		FTextureSource * TextureSource;
		int32 BlockIndex;
		int32 LayerIndex;
		int32 MipIndex;
		FImageView Image;

		const void * GetRawData() const
		{
			check( LockState != ELockState::None );
			return Image.RawData;
		}
		void * GetMutableData() const
		{
			check( LockState == ELockState::ReadWrite );
			return Image.RawData;
		}
		int64 GetDataSize() const { return Image.GetImageSizeBytes(); }

		bool IsValid() const
		{
			// two criteria should be the same :
			bool bNotNull = Image.RawData != nullptr;
			bool bNotNone = LockState != ELockState::None;
			check( bNotNull == bNotNone );
			return bNotNull;
		}

	private:
		
		// no copying or default construct
		FMipLock() = delete;
		FMipLock(const FMipLock&) = delete;
		FMipLock& operator=(const FMipLock&) = delete;
		FMipLock& operator=(FMipLock&& Other) = delete;
	};
#endif // WITH_EDITOR

private:
	/** Allow UTexture access to internals. */
	friend class UTexture;
	friend class UTexture2D;
	friend class UTextureCube;
	friend class UVolumeTexture;
	friend class UTexture2DArray;
	friend class UTextureCubeArray;

#if WITH_EDITOR
	/** BulkDataLock protects simultaneous access to BulkData ;
	BulkDataLock protects the BulkData, also LockState, NumLockedMips, LockedMipData.
	(eg. it protects the functions LockMip, GetMipData, Compress, Decompress, etc.)
	It does NOT protect scalar fields (eg. "SizeX").
	It is intended to allow multiple read-like threads to read from the same TextureSource.
	If you need to modify a TextureSource you must first manually flush all other threads that could be using it.
	eg. using BlockOnAnyAsyncBuild. */
	TDontCopy<FCriticalSection> BulkDataLock;
	/** Owner for associating BulkData with a package */
	UTexture* Owner;
	/** TextureClass == Owner->GetTextureClass(); a copy is kept here for torn-off **/
	ETextureClass TornOffTextureClass;
	/** if Owner != null, check Owner->GetGammaSpace , if it is null, use TornOffGammaSpace
	* do not check this directly, use GetGammaSpace. **/
	TArray<EGammaSpace, TInlineAllocator<1>> TornOffGammaSpace;
#endif
	/** The bulk source data. */
	UE::Serialization::FEditorBulkData BulkData;
	
	/** Number of mips that are locked. */
	uint32 NumLockedMips;

	/** The state of any lock being held on the mip data */
	ELockState LockState;

	void CheckTextureIsUnlocked(const TCHAR* DebugMessage);

#if WITH_EDITOR
	/** Pointer to locked mip data, if any. */
	FMipAllocation LockedMipData;

	// Internal implementation for locking the mip data, called by LockMipReadOnly or LockMip.
	FMutableMemoryView LockMipInternal(int32 BlockIndex, int32 LayerIndex, int32 MipIndex, ELockState RequestedLockState);
	
	// As per UpdateChannelLinearMinMax(), except acts on incoming new data rather than locking existing mips.
	// This only works on uncompressed incoming data - otherwise the channel bounds will get updated on save.
	void UpdateChannelMinMaxFromIncomingTextureData(FMemoryView InNewTextureData);
	
	/** Returns the source data fully decompressed */
	// ImageWrapperModule is not used
	FSharedBuffer Decompress(class IImageWrapperModule* ImageWrapperModule = nullptr) const;
	/** Attempt to decompress the source data from a compressed format. All failures will be logged and result in the method returning false */
	FSharedBuffer TryDecompressData() const;

	/** Return true if the source art is not png compressed but could be. */
	bool CanPNGCompress() const;
	/** Removes source data. */
	void RemoveSourceData();
	/** Retrieve the size and offset for a source mip. The size includes all slices. */
	int64 CalcMipOffset(int32 BlockIndex, int32 LayerIndex, int32 MipIndex) const;
	
	int64 CalcTotalSize() const;
	int64 CalcBlockSize(int32 BlockIndex) const;
	int64 CalcLayerSize(int32 BlockIndex, int32 LayerIndex) const;
	int64 CalcBlockSize(const FTextureSourceBlock& Block) const;
	int64 CalcLayerSize(const FTextureSourceBlock& Block, int32 LayerIndex) const;

	void InitLayeredImpl(
		int32 NewSizeX,
		int32 NewSizeY,
		int32 NewNumSlices,
		int32 NewNumLayers,
		int32 NewNumMips,
		const ETextureSourceFormat* NewLayerFormat);

	void InitBlockedImpl(const ETextureSourceFormat* InLayerFormats,
		const FTextureSourceBlock* InBlocks,
		int32 InNumLayers,
		int32 InNumBlocks);

	bool EnsureBlocksAreSorted();

public:
	// Runs FImageCore::ComputeChannelLinearMinMax on all blocks and layers (but only mip0), returns false
	// if the source was unable to be locked and leaves the channel minmax as unknown. Compute just gets
	// the values and leaves the source untouched.
	ENGINE_API bool UpdateChannelLinearMinMax();
	ENGINE_API bool ComputeChannelLinearMinMax(int32 InLayerIndex, FLinearColor& OutMinColor, FLinearColor& OutMaxColor) const;
	ENGINE_API const TArray<FTextureSourceLayerColorInfo>& GetLayerColorInfo() const { return LayerColorInfo; }

	/** Uses a hash as the GUID, useful to prevent creating new GUIDs on load for legacy assets.
	This is automatically done by Init() and Mip Lock/Unlock.  New textures should always have the data hash as Id. */
	ENGINE_API void UseHashAsGuid();

	void ReleaseSourceMemory(); // release the memory from the mips (does almost the same as remove source data except doesn't rebuild the guid)
	FORCEINLINE bool HasHadBulkDataCleared() const { return bHasHadBulkDataCleared; }
private:
	/** Used while cooking to clear out unneeded memory after compression */
	bool bHasHadBulkDataCleared;
#endif

#if WITH_EDITORONLY_DATA
	/** GUID used to track changes to the source data.
	Typically with UseHashAsGuid , this "Id" is the hash of the BulkData.
	Note that GetId() is not == Id. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	FGuid Id;

	/** Position of texture block0, only relevant if source has multiple blocks */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 BaseBlockX;

	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 BaseBlockY;

	/** Width of the texture. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 SizeX;

	/** Height of the texture. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 SizeY;

	/** Depth (volume textures) or faces (cube maps). */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 NumSlices;

	/** Number of mips provided as source data for the texture. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	int32 NumMips;

	/** Number of layers (for multi-layered virtual textures) provided as source data for the texture. */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	int32 NumLayers;

	/** RGBA8 source data is optionally compressed as PNG. 
	Deprecated, use CompressionFormat instead.  To be removed.
	Deprecated uproperties are loaded but not saved. */
	UPROPERTY()
	bool bPNGCompressed_DEPRECATED;

	/**
	 * Source represents a cubemap in long/lat format, will have only 1 slice per cube, rather than 6 slices.
	 * Not needed for non-array cubemaps, since we can just look at NumSlices == 1 or 6
	 * But for cube arrays, no way of determining whether NumSlices=6 means 1 cubemap, or 6 long/lat cubemaps
	 */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	bool bLongLatCubemap;

	/** Compression format that source data is stored as. */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	TEnumAsByte<enum ETextureSourceCompressionFormat> CompressionFormat;

	/** Uses hash instead of guid to identify content to improve DDC cache hit. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	bool bGuidIsHash;

	/** Per layer color info. If this is empty we don't have the data, otherwise count is == NumLayers. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	TArray<FTextureSourceLayerColorInfo> LayerColorInfo;

	/** Format in which the source data is stored. */
	UPROPERTY(VisibleAnywhere, Category=TextureSource)
	TEnumAsByte<enum ETextureSourceFormat> Format;

	/** For multi-layered sources, each layer may have a different format (in this case LayerFormat[0] == Format) . */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	TArray< TEnumAsByte<enum ETextureSourceFormat> > LayerFormat;

	/**
	 * All sources have 1 implicit block defined by BaseBlockXY/SizeXY members.  Textures imported as UDIM may have additional blocks defined here.
	 * These are stored sequentially in the source's bulk data.
	 */
	UPROPERTY(VisibleAnywhere, Category = TextureSource)
	TArray<FTextureSourceBlock> Blocks;

	/**
	 * Offsets of each block (including Block0) in the bulk data.
	 * Blocks are not necessarily stored in order, since block indices are sorted by X/Y location.
	 * For non-UDIM textures, this will always have a single entry equal to 0
	 */
	UPROPERTY()
	TArray<int64> BlockDataOffsets;

#endif // WITH_EDITORONLY_DATA
};

/**
 * Optional extra fields for texture platform data required by some platforms.
 * Data in this struct is only serialized if the struct's value is non-default.
 */
struct FOptTexturePlatformData
{
	/** Arbitrary extra data that the runtime may need. */
	uint32 ExtData;
	/** Number of mips making up the mip tail, which must always be resident */
	uint32 NumMipsInTail;

	FOptTexturePlatformData()
		: ExtData(0)
		, NumMipsInTail(0)
	{}

	inline bool operator == (FOptTexturePlatformData const& RHS) const 
	{
		return ExtData == RHS.ExtData
			&& NumMipsInTail == RHS.NumMipsInTail;
	}

	inline bool operator != (FOptTexturePlatformData const& RHS) const
	{
		return !(*this == RHS);
	}

	friend inline FArchive& operator << (FArchive& Ar, FOptTexturePlatformData& Data)
	{
		Ar << Data.ExtData;
		Ar << Data.NumMipsInTail;
		return Ar;
	}
};

/**
 * Platform-specific data used by the texture resource at runtime.
 */
USTRUCT()
struct FTexturePlatformData
{
	GENERATED_USTRUCT_BODY()

	/** Width of the texture. */
	int32 SizeX;
	/** Height of the texture. */
	int32 SizeY;
	/** Packed bits [b31: CubeMap], [b30: HasOptData], [b29-0: NumSlices]. See bit masks below. */
	uint32 PackedData;
	/** Format in which mip data is stored. */
	EPixelFormat PixelFormat;
	/** Additional data required by some platforms.*/
	FOptTexturePlatformData OptData;
	/** Mip data or VT data. one or the other. */
	TIndirectArray<struct FTexture2DMipMap> Mips;
	struct FVirtualTextureBuiltData* VTData=nullptr;

	/** This is only valid if the texture availability is CPU only, see GetHasCpuCopy() */
	TRefCountPtr<const struct FSharedImage> CPUCopy;

#if WITH_EDITORONLY_DATA

	// When in the editor we have some data that is stored in the derived data
	// that we don't want to save in the runtime cooked data:
	// 1. bSourceMipsAlphaDetected - this is part of the calculation for whether to choose
	//		e.g. BC1 or BC3.
	// 2. Interim hash data for cook diff tags - this is used for diff tracking for hunting down
	//		determinism issues.
	// We only have this data if the textures were rebuilt after adding it (I didn't invalidate the ddc for this),
	// hence bSourceMipsAlphaDetectedValid. The Hashes are zero if the data isn't present. 
	uint64 PreEncodeMipsHash=0; // XxHash64

	/** The key associated with this derived data. */
	TVariant<FString, UE::DerivedData::FCacheKeyProxy> DerivedDataKey;

	// Stores information about how we generated this encoded texture.
	// Mostly relevant to Oodle, however notably does actually tell
	// you _which_ encoder was used.
	struct FTextureEncodeResultMetadata
	{
		// Returned from ITextureFormat
		FName Encoder;

		// This struct is not always filled out, allow us to check for invalid data.
		bool bIsValid = false;

		// If this is false, the remaining fields are invalid (as encode speed governs
		// the various Oodle specific values right now.)
		bool bSupportsEncodeSpeed = false;

		// If true, then the encoding settings were overridden in the texture editor
		// for encoding experimentation, and thus RDOSource and EncodeSpeed should 
		// be ignored.
		bool bWasEditorCustomEncoding = false;

		enum class OodleRDOSource : uint8
		{
			Default,	// We defaulted back to the project settings
			LODGroup,	// We used the LCA off the LOD group to generate a lambda
			Texture,	// We used the LCA off the texture to generate a lambda.
		};

		OodleRDOSource RDOSource = OodleRDOSource::Default;

		// The resulting RDO lambda, 0 means no RDO.
		uint8 OodleRDO = 0;

		// enum ETextureEncodeEffort
		uint8 OodleEncodeEffort = 0;

		// enum ETextureUniversalTiling
		uint8 OodleUniversalTiling = 0;

		// Which encode speed we ended up using. Must be either ETextureEncodeSpeed::Final or ETextureEncodeSpeed::Fast.
		uint8 EncodeSpeed = 0;
	};

	FTextureEncodeResultMetadata ResultMetadata;

	struct FStructuredDerivedDataKey
	{
		FIoHash TilingBuildDefinitionKey; // All zeroes if the derived data didn't have a child build.
		FIoHash BuildDefinitionKey;
		FGuid SourceGuid;
		FGuid CompositeSourceGuid;

		bool operator==(const FStructuredDerivedDataKey& Other) const
		{
			return TilingBuildDefinitionKey == Other.TilingBuildDefinitionKey && BuildDefinitionKey == Other.BuildDefinitionKey && SourceGuid == Other.SourceGuid && CompositeSourceGuid == Other.CompositeSourceGuid;
		}

		bool operator!=(const FStructuredDerivedDataKey& Other) const
		{
			return TilingBuildDefinitionKey != Other.TilingBuildDefinitionKey || BuildDefinitionKey != Other.BuildDefinitionKey || SourceGuid != Other.SourceGuid || CompositeSourceGuid != Other.CompositeSourceGuid;
		}
	};

	/** 
		The keys for both types of fetches. We assume that uniqueness
	*	for that is equivalent to uniqueness if we use both FetchFirst and FetchOrBuild. This
	*	is used as the key in to CookedPlatformData, as well as to determine if we need to recache.
	*	Note that since this is read on the game thread constantly in CachePlatformData, it
	*	must be written to on the game thread to avoid false recaches.
	*/
	TVariant<FString, FStructuredDerivedDataKey> FetchOrBuildDerivedDataKey;
	TVariant<FString, FStructuredDerivedDataKey> FetchFirstDerivedDataKey;

	/** Async cache task if one is outstanding. */
	struct FTextureAsyncCacheDerivedDataTask* AsyncTask;

#endif

	/** Default constructor. */
	ENGINE_API FTexturePlatformData();

	/** Destructor. */
	ENGINE_API ~FTexturePlatformData();

private:
	static constexpr uint32 BitMask_CubeMap    = 1u << 31u;
	static constexpr uint32 BitMask_HasOptData = 1u << 30u;
	static constexpr uint32 BitMask_HasCpuCopy = 1u << 29u;
	static constexpr uint32 BitMask_NumSlices  = BitMask_HasCpuCopy - 1u;

public:
	/** Return whether TryLoadMips() would stall because async loaded mips are not yet available. */
	bool IsReadyForAsyncPostLoad() const;

	/**
	 * Try to load mips from the derived data cache.
	 * @param FirstMipToLoad - The first mip index to load.
	 * @param OutMipData -	Must point to an array of pointers with at least
	 *						Texture.Mips.Num() - FirstMipToLoad + 1 entries. Upon
	 *						return those pointers will contain mip data.
	 * @param DebugContext - A string used for debug tracking and logging. Usually Texture->GetPathName()
	 * @returns true if all requested mips have been loaded.
	 */
	bool TryLoadMips(int32 FirstMipToLoad, void** OutMipData, FStringView DebugContext);
	bool TryLoadMipsWithSizes(int32 FirstMipToLoad, void** OutMipData, int64 * OutMipSize, FStringView DebugContext);

	/** Serialization. */
	void Serialize(FArchive& Ar, class UTexture* Owner);

#if WITH_EDITORONLY_DATA
	FString GetDerivedDataMipKeyString(int32 MipIndex, const FTexture2DMipMap& Mip) const;
	static UE::DerivedData::FValueId MakeMipId(int32 MipIndex);
#endif // WITH_EDITORONLY_DATA

	/** 
	 * Serialization for cooked builds.
	 *
	 * @param Ar Archive to serialize with
	 * @param Owner Owner texture
	 * @param bStreamable set up serialization to stream some mips
	 * @param bSerializeMipData if false, no mip bulk data will be serialized.  This should only be false when an alternate All Mip Data Provider is attached.
	 */
	void SerializeCooked(FArchive& Ar, class UTexture* Owner, bool bStreamable, const bool bSerializeMipData);
	
	inline void SetPackedData(int32 InNumSlices, bool bInHasOptData, bool bInCubeMap, bool bInHasCpuCopy)
	{
		PackedData = (InNumSlices & BitMask_NumSlices) | (bInCubeMap ? BitMask_CubeMap : 0) | (bInHasOptData ? BitMask_HasOptData : 0) | (bInHasCpuCopy ? BitMask_HasCpuCopy : 0);
	}

	inline bool GetHasOptData() const
	{
		return (PackedData & BitMask_HasOptData) == BitMask_HasOptData;
	}

	inline void SetOptData(FOptTexturePlatformData Data)
	{
		// Set the opt data flag to true if the specified data is non-default.
		bool bHasOptData = Data != FOptTexturePlatformData();
		PackedData = (bHasOptData ? BitMask_HasOptData : 0) | (PackedData & (~BitMask_HasOptData));

		OptData = Data;
	}

	inline bool GetHasCpuCopy() const
	{
		return (PackedData & BitMask_HasCpuCopy) == BitMask_HasCpuCopy;
	}

	inline void SetHasCpuCopy(bool bInHasCpuCopy)
	{
		PackedData = (bInHasCpuCopy ? BitMask_HasCpuCopy : 0) | (PackedData & (~BitMask_HasCpuCopy));
	}

	inline bool IsCubemap() const
	{
		return (PackedData & BitMask_CubeMap) == BitMask_CubeMap; 
	}

	inline void SetIsCubemap(bool bCubemap)
	{
		PackedData = (bCubemap ? BitMask_CubeMap : 0) | (PackedData & (~BitMask_CubeMap));
	}
	
	inline int32 GetNumSlices() const 
	{
		return (int32)(PackedData & BitMask_NumSlices);
	}

	inline void SetNumSlices(int32 NumSlices)
	{
		PackedData = (NumSlices & BitMask_NumSlices) | (PackedData & (~BitMask_NumSlices));
	}

	inline int32 GetNumMipsInTail() const
	{
		return OptData.NumMipsInTail;
	}

	inline int32 GetExtData() const
	{
		return OptData.ExtData;
	}

#if WITH_EDITOR
	// Clears the data such that a new Cache() call can load new data in to the structure.
	void Reset();

	bool IsAsyncWorkComplete() const;

	// Compresses the texture using the given compressor and adds the result to the DDC.
	// This might not be synchronous, and might be called from a worker thread!
	//
	// If Compressor is 0, uses the default texture compressor module. Must be nonzero
	// if called from a worker thread.
	//
	// InFlags are ETextureCacheFlags.
	// InSettingsPerLayerFetchFirst can be nullptr - if not, then the caceh will check if
	// the corresponding texture exists in the DDC before trying the FetchOrBuild settings.
	// FetchFirst is ignored if forcerebuild is passed as a flag.
	// InSettingsPerLayerFetchOrBuild is required. If a texture matching the settings exists
	// in the ddc, it is used, otherwise it is built.
	void Cache(
		class UTexture& InTexture,
		const struct FTextureBuildSettings* InSettingsPerLayerFetchFirst,
		const struct FTextureBuildSettings* InSettingsPerLayerFetchOrBuild,
		const FTexturePlatformData::FTextureEncodeResultMetadata* OutResultMetadataPerLayerFetchFirst,
		const FTexturePlatformData::FTextureEncodeResultMetadata* OutResultMetadataPerLayerFetchOrBuild,
		uint32 InFlags,
		class ITextureCompressorModule* Compressor);
	void FinishCache();
	bool TryCancelCache();
	void CancelCache();
	ENGINE_API bool TryInlineMipData(int32 FirstMipToLoad = 0, FStringView DebugContext=FStringView());
	ENGINE_API TFuture<TTuple<uint64, uint64>> LaunchEstimateOnDiskSizeTask(
		FOodleDataCompression::ECompressor InOodleCompressor,
		FOodleDataCompression::ECompressionLevel InOodleCompressionLevel,
		uint32 InCompressionBlockSize,
		FStringView InDebugContext);
	bool AreDerivedMipsAvailable(FStringView Context) const;
	bool AreDerivedVTChunksAvailable(FStringView Context) const;
	UE_DEPRECATED(5.0, "Use AreDerivedMipsAvailable with the context instead.")
	bool AreDerivedMipsAvailable() const;
	UE_DEPRECATED(5.0, "Use AreDerivedVTChunksAvailable with the context instead.")
	bool AreDerivedVTChunksAvailable() const;
#endif

	/** Return the number of mips that are not streamable. */
	int32 GetNumNonStreamingMips(bool bIsStreamingPossible) const;
	/** Return the number of mips that streamable but not optional. */
	int32 GetNumNonOptionalMips() const;
	/** Return true if at least one mip can be loaded either from DDC or disk. */
	bool CanBeLoaded() const;

	// Only because we don't want to expose FVirtualTextureBuiltData
	ENGINE_API int32 GetNumVTMips() const;
	ENGINE_API EPixelFormat GetLayerPixelFormat(uint32 LayerIndex) const;

	/** Return the size of the texture pixel data in bytes, not including headers or alignment. */
	ENGINE_API int64 GetPayloadSize(int32 MipBias) const;

private:

	bool CanUseCookedDataPath() const;
};

/**
 * Collection of values that contribute to pixel format chosen for texture
 */
USTRUCT()
struct FTextureFormatSettings
{
	GENERATED_USTRUCT_BODY()

	FTextureFormatSettings()
		: CompressionSettings(TC_Default)
		, CompressionNoAlpha(false)
		, CompressionForceAlpha(false)
		, CompressionNone(false)
		, CompressionYCoCg(false)
		, SRGB(false)
	{}

	UPROPERTY()
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;

	UPROPERTY()
	uint8 CompressionNoAlpha : 1;

	UPROPERTY()
	uint8 CompressionForceAlpha : 1;

	UPROPERTY()
	uint8 CompressionNone : 1;

	UPROPERTY()
	uint8 CompressionYCoCg : 1;

	UPROPERTY()
	uint8 SRGB : 1;
};


USTRUCT(BlueprintType)
struct FTextureSourceColorSettings
{
	GENERATED_USTRUCT_BODY()

	FTextureSourceColorSettings()
		: EncodingOverride(ETextureSourceEncoding::TSE_None)
		, ColorSpace(ETextureColorSpace::TCS_None)
		, RedChromaticityCoordinate(FVector2D::ZeroVector)
		, GreenChromaticityCoordinate(FVector2D::ZeroVector)
		, BlueChromaticityCoordinate(FVector2D::ZeroVector)
		, WhiteChromaticityCoordinate(FVector2D::ZeroVector)
		, ChromaticAdaptationMethod(ETextureChromaticAdaptationMethod::TCAM_Bradford)
	{}

	/** Source encoding of the texture, exposing more options than just sRGB. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement)
	ETextureSourceEncoding EncodingOverride;

	/** Source color space of the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement)
	ETextureColorSpace ColorSpace;

	/** Red chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpace == ETextureColorSpace::TCS_Custom"))
	FVector2D RedChromaticityCoordinate;

	/** Green chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpace == ETextureColorSpace::TCS_Custom"))
	FVector2D GreenChromaticityCoordinate;

	/** Blue chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpace == ETextureColorSpace::TCS_Custom"))
	FVector2D BlueChromaticityCoordinate;

	/** White chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpace == ETextureColorSpace::TCS_Custom"))
	FVector2D WhiteChromaticityCoordinate;

	/** Chromatic adaption method applied if the source white point differs from the working color space white point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement)
	ETextureChromaticAdaptationMethod ChromaticAdaptationMethod;
};

UCLASS(abstract, MinimalAPI, BlueprintType)
class UTexture : public UStreamableRenderAsset, public IInterface_AssetUserData, public IInterface_AsyncCompilation
{
	GENERATED_UCLASS_BODY()

	/*--------------------------------------------------------------------------
		Editor only properties used to build the runtime texture data.
	--------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
	/* Dynamic textures will have ! Source.IsValid() ;
	Also in UEFN , Textures from the cooked-only texture library.  Always check Source.IsValid before using Source. */
	UPROPERTY()
	FTextureSource Source;
#endif

private:
	/** Unique ID for this material, used for caching during distributed lighting */
	UPROPERTY()
	FGuid LightingGuid;

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;

public:

	/** Static texture brightness adjustment (scales HSV value.)  (Non-destructive; Requires texture source art to be available.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Brightness"))
	float AdjustBrightness;

	/** Static texture curve adjustment (raises HSV value to the specified power.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Brightness Curve"))
	float AdjustBrightnessCurve;

	/** Static texture "vibrance" adjustment (0 - 1) (HSV saturation algorithm adjustment.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Vibrance", ClampMin = "0.0", ClampMax = "1.0"))
	float AdjustVibrance;

	/** Static texture saturation adjustment (scales HSV saturation.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Saturation"))
	float AdjustSaturation;

	/** Static texture RGB curve adjustment (raises linear-space RGB color to the specified power.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "RGBCurve"))
	float AdjustRGBCurve;

	/** Static texture hue adjustment (0 - 360) (offsets HSV hue by value in degrees.)  (Non-destructive; Requires texture source art to be available.)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Hue", ClampMin = "0.0", ClampMax = "360.0"))
	float AdjustHue;

	/** Remaps the alpha to the specified min/max range, defines the new value of 0 (Non-destructive; Requires texture source art to be available.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Min Alpha"))
	float AdjustMinAlpha;

	/** Remaps the alpha to the specified min/max range, defines the new value of 1 (Non-destructive; Requires texture source art to be available.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(DisplayName = "Max Alpha"))
	float AdjustMaxAlpha;

	/** If enabled, the texture's alpha channel will be forced to opaque for any compressed texture output format.  Does not apply if output format is uncompressed RGBA. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression, meta=(DisplayName="Compress Without Alpha"))
	uint32 CompressionNoAlpha:1;
	
	/** If true, force alpha channel in output format when possible, eg. for AutoDXT BC1/BC3 choice **/
	UPROPERTY()
	uint32 CompressionForceAlpha:1;

	/** If true, force the texture to be uncompressed no matter the format. */
	UPROPERTY()
	uint32 CompressionNone:1;
	
	/** If enabled, compress with Final quality during this Editor session. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, SkipSerialization, Category=Compression, meta=(NoResetToDefault), meta=(DisplayName="Editor Show Final Encode"))
	uint32 CompressFinal:1;

	/** If enabled, defer compression of the texture until save or manually compressed in the texture editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, SkipSerialization, Category=Compression, meta=(NoResetToDefault), meta=(DisplayName="Editor Defer Compression"))
	uint32 DeferCompression:1;
	
	/** How aggressively should any relevant lossy compression be applied. For compressors that support EncodeSpeed (i.e. Oodle), this is only
	*	applied if enabled (see Project Settings -> Texture Encoding). Note that this is *in addition* to any
	*	unavoidable loss due to the target format - selecting "No Lossy Compression" will not result in zero distortion for BCn formats.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression, AdvancedDisplay)
	TEnumAsByte<ETextureLossyCompressionAmount> LossyCompressionAmount;
	
	/** Oodle Texture SDK Version to encode with.  Enter 'latest' to update; 'None' preserves legacy encoding to avoid patches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression, AdvancedDisplay, meta=(NoResetToDefault))
	FName OodleTextureSdkVersion;

	/** The maximum resolution for generated textures. A value of 0 means the maximum size for the format on each platform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression, meta=(DisplayName="Maximum Texture Size", ClampMin = "0.0"), AdvancedDisplay)
	int32 MaxTextureSize;

	/** The compression quality for generated ASTC textures (i.e. mobile platform textures). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression, meta = (DisplayName = "ASTC Compression Quality"), AdvancedDisplay)
	TEnumAsByte<enum ETextureCompressionQuality> CompressionQuality;

	/** Change this optional ID to force the texture to be recompressed by changing its cache key. */
	UPROPERTY(EditAnywhere, Category=Compression, meta=(NoResetToDefault), meta=(DisplayName = "Compression Cache ID"), AdvancedDisplay)
	FGuid CompressionCacheId;

	/** Removed. */
	UPROPERTY()
	uint32 bDitherMipMapAlpha_DEPRECATED:1;

	/** Whether mip RGBA should be scaled to preserve the number of pixels with Value >= AlphaCoverageThresholds.  AlphaCoverageThresholds are ignored if this is off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AdvancedDisplay)
	bool bDoScaleMipsForAlphaCoverage = false;
	
	/** Alpha values per channel to compare to when preserving alpha coverage. 0 means disable channel.  Typical good values in 0.5 - 0.9, not 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta=(ClampMin = "0", ClampMax = "1.0", EditCondition="bDoScaleMipsForAlphaCoverage"), AdvancedDisplay)
	FVector4 AlphaCoverageThresholds = FVector4(0,0,0,0);

	/** Use faster mip generation filter, usually the same result but occasionally causes color shift in high contrast areas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName = "Use Fast MipGen Filter"), AdvancedDisplay)
	bool bUseNewMipFilter = false;

	/** When true the texture's border will be preserved during mipmap generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, AdvancedDisplay)
	uint32 bPreserveBorder:1;

	/** When true the texture's green channel will be inverted. This is useful for some normal maps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AdvancedDisplay)
	uint32 bFlipGreenChannel:1;

	/** How to pad the texture to a power of 2 size (if necessary) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta = (DisplayName = "Padding and Resizing"))
	TEnumAsByte<enum ETexturePowerOfTwoSetting::Type> PowerOfTwoMode;

	/** The color used to pad the texture out if it is padded due to PowerOfTwoMode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta = (EditCondition = "(PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToPowerOfTwo || PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo) && !bPadWithBorderColor", EditConditionHides))
	FColor PaddingColor;

	/** If set to true, texture padding will be performed using colors of the border pixels. This can be used to improve quality of the generated mipmaps for padded textures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta = (EditCondition = "PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToPowerOfTwo || PowerOfTwoMode == ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo", EditConditionHides))
	bool bPadWithBorderColor;

	/** Width of the resized texture when using "Resize To Specific Resolution" padding and resizing option. If set to zero, original width will be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta = (EditCondition = "PowerOfTwoMode == ETexturePowerOfTwoSetting::ResizeToSpecificResolution", EditConditionHides))
	int32 ResizeDuringBuildX;

	/** Width of the resized texture when using "Resize To Specific Resolution" padding and resizing option. If set to zero, original height will be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta = (EditCondition = "PowerOfTwoMode == ETexturePowerOfTwoSetting::ResizeToSpecificResolution", EditConditionHides))
	int32 ResizeDuringBuildY;

	/** Whether to chroma key the image, replacing any pixels that match ChromaKeyColor with transparent black */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments)
	bool bChromaKeyTexture;

	/** The threshold that components have to match for the texel to be considered equal to the ChromaKeyColor when chroma keying (<=, set to 0 to require a perfect exact match) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(EditCondition="bChromaKeyTexture", ClampMin="0"))
	float ChromaKeyThreshold;

	/** The color that will be replaced with transparent black if chroma keying is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Adjustments, meta=(EditCondition="bChromaKeyTexture"))
	FColor ChromaKeyColor;

	/** Per asset specific setting to define the mip-map generation properties like sharpening and kernel size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail)
	TEnumAsByte<enum TextureMipGenSettings> MipGenSettings;

	/**
	 * Can be defined to modify the roughness based on the normal map variation (mostly from mip maps).
	 * MaxAlpha comes in handy to define a base roughness if no source alpha was there.
	 * Make sure the normal map has at least as many mips as this texture.
	 */
	UE_DEPRECATED(5.3, "Use GetCompositeTexture() and SetCompositeTexture() instead.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compositing, meta = (AllowPrivateAccess, RequiredAssetDataTags = "IsSourceValid=True"), Setter = SetCompositeTexture, Getter = GetCompositeTexture)
	TObjectPtr<class UTexture> CompositeTexture;

private:
	/** Used to track down when CompositeTexture has been modified for notification purpose.
	*	NOTE: Do not make a TObjectPtr or UPROPERTY as it would defeat the purpose.
	*/
	class UTexture* KnownCompositeTexture = nullptr;

	/** Called when the CompositeTexture property gets overwritten without us knowing about it */
	ENGINE_API void OutdatedKnownCompositeTextureDetected() const;
	ENGINE_API void NotifyIfCompositeTextureChanged();

public:
	void SetCompositeTexture(UTexture* InCompositeTexture)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		CompositeTexture = InCompositeTexture;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		NotifyIfCompositeTextureChanged();
	}

	UTexture* GetCompositeTexture() const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// This should never happen and is a last resort, we should have caught the property overwrite well before we reach this code
		// but this can happen from legacy code that use reflection to set the property without using the new _InContainer functions
		// which will bypass the setter we put in place.
		if (KnownCompositeTexture != CompositeTexture)
		{
			OutdatedKnownCompositeTextureDetected();
		}
		return CompositeTexture;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/* defines how the CompositeTexture is applied, e.g. CTM_RoughnessFromNormalAlpha */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compositing, AdvancedDisplay)
	TEnumAsByte<enum ECompositeTextureMode> CompositeTextureMode;

	/**
	 * default 1, high values result in a stronger effect e.g 1, 2, 4, 8
	 * this is not a slider because the texture update would not be fast enough
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compositing, AdvancedDisplay)
	float CompositePower;

	/**
	 * Array of settings used to control the format of given layer
	 * If this array doesn't include an entry for a given layer, values from UTexture will be used
	 */
	UPROPERTY()
	TArray<FTextureFormatSettings> LayerFormatSettings;

#endif // WITH_EDITORONLY_DATA

	/*--------------------------------------------------------------------------
		Properties needed at runtime below.
	--------------------------------------------------------------------------*/

	/*
	 * Level scope index of this texture. It is used to reduce the amount of lookup to map a texture to its level index.
	 * Useful when building texture streaming data, as well as when filling the texture streamer with precomputed data.
     * It relates to FStreamingTextureBuildInfo::TextureLevelIndex and also the index in ULevel::StreamingTextureGuids. 
	 * Default value of -1, indicates that the texture has an unknown index (not yet processed). At level load time, 
	 * -2 is also used to indicate that the texture has been processed but no entry were found in the level table.
	 * After any of these processes, the LevelIndex is reset to INDEX_NONE. Making it ready for the next level task.
	 */
	UPROPERTY(transient, duplicatetransient, NonTransactional)
	int32 LevelIndex = INDEX_NONE;

	/** A bias to the index of the top mip level to use.  That is, number of mip levels to drop when cooking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, meta=(DisplayName="LOD Bias"), AssetRegistrySearchable)
	int32 LODBias;

	/** Compression settings to use when building the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Compression, AssetRegistrySearchable)
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;

	/** The texture filtering mode to use when sampling this texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureFilter> Filter;

	/** The texture mip load options. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, AssetRegistrySearchable, AdvancedDisplay)
	ETextureMipLoadOptions MipLoadOptions;

	/** If the platform supports it, tile the texture when cooking, or keep it linear and tile it when it's actually submitted to the GPU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression, AdvancedDisplay)
	TEnumAsByte<enum TextureCookPlatformTilingSettings> CookPlatformTilingSettings;

	/** If set to true, then Oodle encoder preserves 0 and 255 (0.0 and 1.0) values exactly in alpha channel for BC3/BC7 and in all channels for BC4/BC5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression, meta = (DisplayName = "Preserve Extremes When Compressing With Oodle"), AdvancedDisplay)
	bool bOodlePreserveExtremes = false;

	/** Texture group this texture belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LevelOfDetail, meta=(DisplayName="Texture Group"), AssetRegistrySearchable)
	TEnumAsByte<enum TextureGroup> LODGroup;

	/** This function is used to control access to the Downscale and DownscaleOptions properties from the Texture Editor UI
	 * in order to make it more clear to the user whether these properties will or will not be used when building the texture.
	 */
	UFUNCTION()
	virtual bool AreDownscalePropertiesEditable() const { return false; }

	/** Downscale source texture, applied only to 2d textures without mips 
	 * < 1.0 - use scale value from texture group
	 * 1.0 - do not scale texture
	 * > 1.0 - scale texure
	 */
	UPROPERTY(EditAnywhere, Category=LevelOfDetail, AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="8.0", EditCondition=AreDownscalePropertiesEditable))
	FPerPlatformFloat Downscale;

	/** Texture downscaling options */
	UPROPERTY(EditAnywhere, Category=LevelOfDetail, AdvancedDisplay, meta=(EditCondition=AreDownscalePropertiesEditable))
	ETextureDownscaleOptions DownscaleOptions;

	/** 
	* Whether the texture will be encoded to a gpu format and uploaded to the graphics card, or kept on the CPU for access by gamecode / blueprint. 
	* For CPU availability, the texture will still upload a tiny black texture as a placeholder. Only applies to 2d textures.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="Availability"), AssetRegistrySearchable)
	ETextureAvailability Availability;
	
	/** Whether Texture and its source are in SRGB Gamma color space.  Can only be used with 8-bit and compressed formats.  This should be unchecked if using alpha channels individually as masks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="sRGB"), AssetRegistrySearchable)
	uint8 SRGB:1;

#if WITH_EDITORONLY_DATA
	/* Normalize colors in Normal Maps after mip generation for better and sharper quality; recommended on if not required to match legacy behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="Normalize after making mips", EditCondition="CompressionSettings==1"), AdvancedDisplay)
	uint8 bNormalizeNormals:1;

	/** A flag for using the simplified legacy gamma space e.g pow(color,1/2.2) for converting from FColor to FLinearColor, if we're doing sRGB. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="sRGB Use Legacy Gamma", EditCondition="SRGB"), AdvancedDisplay)
	uint8 bUseLegacyGamma:1;

	/** Texture color management settings: source encoding and color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, AdvancedDisplay)
	FTextureSourceColorSettings SourceColorSettings;

	/* Store the FUE5MainStreamObjectVersion of the Texture uasset loaded for debugging (== LatestVersion if not loaded) */
	int LoadedMainStreamObjectVersion;

	/** Indicates we're currently importing the object (set in PostEditImport, unset in the subsequent PostEditChange) */
	uint8 bIsImporting : 1;
	
	/** Indicates ImportCustomProperties has been called (set in ImportCustomProperties, unset in the subsequent PostEditChange) */
	uint8 bCustomPropertiesImported : 1;

	// When we are open in an asset editor, we have a pointer to a custom encoding
	// object which can optionally cause us to do something other than Fast/Final encode settings.
	TWeakPtr<class FTextureEditorCustomEncode> TextureEditorCustomEncoding;
#endif // WITH_EDITORONLY_DATA

	/** If true, the RHI texture will be created using TexCreate_NoTiling */
	UPROPERTY()
	uint8 bNoTiling:1;

	/** Is this texture streamed in using VT								*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Texture, AssetRegistrySearchable, AdvancedDisplay)
	uint8 VirtualTextureStreaming : 1;

	/** If true the texture stores YCoCg. Blue channel will be filled with a precision scale during compression. */
	UPROPERTY()
	uint8 CompressionYCoCg : 1;

	/** If true, the RHI texture will be created without TexCreate_OfflineProcessed.
	  * This controls what format the data will be uploaded to RHI.
	  * Offline processed textures may have platform specific tiling applied, and/or have their mip tails pre-combined into a single mip's data.
	  * If NotOffline, then it will expect data to be uploaded in standard per-mip layouts.
	  */
	UPROPERTY(transient)
	uint8 bNotOfflineProcessed : 1;

	/**
	 * Gets the memory size of the texture, in bytes.
	 * This is the size in GPU memory of the built platformdata, accounting for LODBias, etc.
	 * Returns zero for error.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMemorySize"), Category = "Rendering|Texture")
	int64 Blueprint_GetMemorySize() const;
	
	/**
	 * Gets the memory size of the texture source top mip, in bytes, and the size on disk of the asset, which may be compressed.
	 * Uses texture source, not available in runtime games.
	 * Does not cause texture source to be loaded, queries cached values.
	 * Returns zero for error.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, meta = (DisplayName = "GetTextureSourceDiskAndMemorySize"), Category = "Rendering|Texture")
	void Blueprint_GetTextureSourceDiskAndMemorySize(int64 & OutDiskSize,int64 & OutMemorySize) const;

	/**
	 * Scan the texture source pixels to compute the min & max values of the RGBA channels.
	 * Uses texture source, not available in runtime games.
	 * Causes texture source data to be loaded, is computed by scanning pixels when called.
	 * Will set Min=Max=zero and return false on failure
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category = "Rendering|Texture")
	bool ComputeTextureSourceChannelMinMax(FLinearColor & OutColorMin, FLinearColor & OutColorMax) const;

private:
	/** Whether the async resource release process has already been kicked off or not */
	UPROPERTY(transient)
	uint8 bAsyncResourceReleaseHasBeenStarted : 1;

protected:
	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Texture)
	TArray<TObjectPtr<UAssetUserData>> AssetUserData;

public:
#if WITH_EDITOR
	/** Used to record texture streamable state when cooking.
	This a per-platform bool to record whether the PlatformData being cooked made streamable mips.
	Key is TargetPlatform->PlatformName */
	TMap<FString,bool> DidSerializeStreamingMipsForPlatform;
#endif

private:
	/** The texture's resource, can be NULL */
	class FTextureResource*	PrivateResource;
	/** Value updated and returned by the render-thread to allow
	  * fenceless update from the game-thread without causing
	  * potential crash in the render thread.
	  */
	class FTextureResource* PrivateResourceRenderThread;

public:
	ENGINE_API UTexture(FVTableHelper& Helper);
	ENGINE_API virtual ~UTexture();
	
	/** Set texture's resource, can be NULL */
	ENGINE_API void SetResource(FTextureResource* Resource);

	/** Get the texture's resource, can be NULL */
	ENGINE_API FTextureResource* GetResource();

	/** Get the const texture's resource, can be NULL */
	ENGINE_API const FTextureResource* GetResource() const;

	/** Stable RHI texture reference that refers to the current RHI texture. Note this is manually refcounted! */
	FTextureReference& TextureReference;

	/** Release fence to know when resources have been freed on the rendering thread. */
	FRenderCommandFence ReleaseFence;

	/** delegate type for texture save events ( Params: UTexture* TextureToSave ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTextureSaved, class UTexture*);
	/** triggered before a texture is being saved */
	ENGINE_API static FOnTextureSaved PreSaveEvent;

	ENGINE_API virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	ENGINE_API virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;
	ENGINE_API virtual void PostEditImport() override;

	/** Get Texture Class */
	ENGINE_API virtual ETextureClass GetTextureClass() const PURE_VIRTUAL(UTexture::GetTextureClass, return ETextureClass::Invalid; );

	/**
	 * Resets the resource for the texture.
	 */
	ENGINE_API void ReleaseResource();

	/**
	 * Creates a new resource for the texture, and updates any cached references to the resource.
	 */
	ENGINE_API virtual void UpdateResource();

	/**
	 * Implemented by subclasses to create a new resource for the texture.
	 */
	virtual class FTextureResource* CreateResource() PURE_VIRTUAL(UTexture::CreateResource,return NULL;);

	/** Cache the combined LOD bias based on texture LOD group and LOD bias. */
	UE_DEPRECATED(5.2, "UpdateCachedLODBias does nothing, remove call")
	void UpdateCachedLODBias()
	{
		// no longer cached, now does nothing
	}
	
	int32 GetCachedLODBias() const override
	{
		// this is the combined LOD Bias with cinematic bias
		return CalculateLODBias(true);
	}

	/**
	* Calculate the combined LOD bias based on texture LOD group and LOD bias.
	*   CalculateLODBias(true) >= CalculateLODBias(false)
	*   CalculateLODBias(true) == CalculateLODBias(false) + NumCinematicMipLevels , except when a clamp was applied
	* @return	LOD bias
	*/
	ENGINE_API int32 CalculateLODBias(bool bWithCinematicMipBias) const;

	/**
	 * @return The material value type of this texture.
	 */
	virtual EMaterialValueType GetMaterialType() const PURE_VIRTUAL(UTexture::GetMaterialType,return MCT_Texture;);

	/**
	 * Returns if the texture is actually being rendered using virtual texturing right now.
	 * Unlike the 'VirtualTextureStreaming' property which reflects the user's desired state
	 * this reflects the actual current state on the renderer depending on the platform, VT
	 * data being built, project settings, ....
	 */
	virtual bool IsCurrentlyVirtualTextured() const
	{
		return false;
	}

	/** Returns the virtual texture build settings. */
	ENGINE_API virtual void GetVirtualTextureBuildSettings(struct FVirtualTextureBuildSettings& OutSettings) const;

	/**
	 * Textures that use the derived data cache must override this function and
	 * provide a pointer to the linked list of platform data.
	 */
	virtual FTexturePlatformData** GetRunningPlatformData() { return NULL; }
	virtual TMap<FString, FTexturePlatformData*>* GetCookedPlatformData() { return NULL; }

	void CleanupCachedRunningPlatformData();
	
	/**
	 * Get the dimensions of the largest mip of the texture when built for the target platform
	 *   accounting for LODBias and other constraints
	 */
	ENGINE_API void GetBuiltTextureSize( const ITargetPlatform* TargetPlatform , int32 & OutSizeX, int32 & OutSizeY ) const;
	ENGINE_API void GetBuiltTextureSize( const class ITargetPlatformSettings* TargetPlatformSettings, const class ITargetPlatformControls* TargetPlatformControls, int32 & OutSizeX, int32 & OutSizeY ) const;

	/**
	 * Serializes cooked platform data.
	 */
	ENGINE_API void SerializeCookedPlatformData(class FArchive& Ar, const bool bSerializeMipData = true);

	//~ Begin IInterface_AssetUserData Interface
	ENGINE_API virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	ENGINE_API virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

#if WITH_EDITOR
	/**
	 * Caches platform data for the texture.
	 * 
	 * @param bAsyncCache spawn a thread to cache the platform data 
	 * @param bAllowAsyncBuild allow building the DDC file in the thread if missing.
	 * @param bAllowAsyncLoading allow loading source data in the thread if missing (the data won't be reusable for later use though)
	 * @param Compressor optional compressor as the texture compressor can not be retrieved from an async thread.
	 * 
	 * This is called optionally from worker threads via the FAsyncEncode class (LightMaps, ShadowMaps)
	 */
	void CachePlatformData(bool bAsyncCache = false, bool bAllowAsyncBuild = false, bool bAllowAsyncLoading = false, class ITextureCompressorModule* Compressor = nullptr);

	/**
	 * Begins caching platform data in the background for the platform requested
	 */
	ENGINE_API virtual void BeginCacheForCookedPlatformData(  const ITargetPlatform* TargetPlatform ) override;

	/**
	 * Have we finished loading all the cooked platform data for the target platforms requested in BeginCacheForCookedPlatformData
	 * 
	 * @param	TargetPlatform target platform to check for cooked platform data
	 */
	ENGINE_API virtual bool IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) override;

	/**
	 * Clears cached cooked platform data for specific platform
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	ENGINE_API virtual void ClearCachedCookedPlatformData( const ITargetPlatform* TargetPlatform ) override;

	/**
	 * Clear all cached cooked platform data
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	ENGINE_API virtual void ClearAllCachedCookedPlatformData() override;

	/**
	 * Returns true if the current texture is a default placeholder because compilation is still ongoing.
	 */
	ENGINE_API virtual bool IsDefaultTexture() const;

	/**
	 * Begins caching platform data in the background.
	 */
	ENGINE_API void BeginCachePlatformData();

	/**
	 * Returns true if all async caching has completed.
	 */
	ENGINE_API bool IsAsyncCacheComplete() const;

	/**
	 * Blocks on async cache tasks and prepares platform data for use.
	 */
	ENGINE_API void FinishCachePlatformData();

	/**
	 * Forces platform data to be rebuilt.
	 * @param InEncodeSpeedOverride		Optionally force a specific encode speed 
	 * 									using the ETextureEncodeSpeedOverride enum.
	 * 									Type hidden to keep out of Texture.h
	 */
	ENGINE_API void ForceRebuildPlatformData(uint8 InEncodeSpeedOverride=255);

	/**
	 * Get an estimate of the peak amount of memory required to build this texture.
	 */
	UE_DEPRECATED(5.1,"Use GetBuildRequiredMemoryEstimate instead")
	ENGINE_API int64 GetBuildRequiredMemory() const;

	/**
	 * Marks platform data as transient. This optionally removes persistent or cached data associated with the platform.
	 */
	ENGINE_API void MarkPlatformDataTransient();

	/**
	* Return maximum dimension for this texture type (2d/cube/vol) in the current RHI
	* use GetMaximumDimensionOfNonVT for platform-independent max dim
	*/
	ENGINE_API virtual uint32 GetMaximumDimension() const;

	/**
	 * Gets settings used to choose format for the given layer
	 */
	ENGINE_API void GetLayerFormatSettings(int32 LayerIndex, FTextureFormatSettings& OutSettings) const;
	ENGINE_API void SetLayerFormatSettings(int32 LayerIndex, const FTextureFormatSettings& InSettings);

	ENGINE_API void GetDefaultFormatSettings(FTextureFormatSettings& OutSettings) const;
	
	ENGINE_API ETextureEncodeSpeed GetDesiredEncodeSpeed() const;
		
	/** Ensure settings are valid after import or edit; this is called by PostEditChange. */
	ENGINE_API virtual void ValidateSettingsAfterImportOrEdit(bool * pRequiresNotifyMaterials = nullptr);

	/* Change the Oodle Texture Sdk Version used to encode this texture to latest.
	*  You should do this any time the texture is modified, such as on reimport, since the bits are changing anyway.
	*/
	ENGINE_API virtual void UpdateOodleTextureSdkVersionToLatest(bool bDoPrePostEditChangeIfChanging = false);
	
	/** Set new default settings that are desired for textures.
	These cannot be set in the constructor automatically to avoid changing old content.
	When new textures are made, or the texture content changes, call this to update settings.
	ApplyDefaultsForNewlyImportedTextures calls this, you don't need to call both.
	*/
	ENGINE_API virtual void SetModernSettingsForNewOrChangedTexture();

	/** Get TextureFormatName with platform remaps and conditional prefix
	 *   this is the entry point API for getting the final texture format name
	 *  OutFormats will be resized to the number of sub-flavors of the platform (typically just 1)
	 *  OutFormats[i] gets an array of format names, one per layer
	 */
	ENGINE_API void GetPlatformTextureFormatNamesWithPrefix(const class ITargetPlatform* TargetPlatform,TArray< TArray<FName> >& OutFormats) const;
#endif // WITH_EDITOR

	EGammaSpace GetGammaSpace() const
	{
		// note: does not validate that the Format respects gamma (TextureSource::GetGammaSpace does)
		
		#if WITH_EDITORONLY_DATA
		// Pow22 only affects texture *Source* encoding of import
		// PlatformData is always sRGB even if bUseLegacyGamma was on
		if ( SRGB && bUseLegacyGamma )
		{
			return EGammaSpace::Pow22;
		}
		#endif

		return SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;
	}

	/** @return the width of the surface represented by the texture. */
	virtual float GetSurfaceWidth() const PURE_VIRTUAL(UTexture::GetSurfaceWidth,return 0;);

	/** @return the height of the surface represented by the texture. */
	virtual float GetSurfaceHeight() const PURE_VIRTUAL(UTexture::GetSurfaceHeight,return 0;);

	/** @return the depth of the surface represented by the texture. */
	virtual float GetSurfaceDepth() const PURE_VIRTUAL(UTexture::GetSurfaceDepth, return 0;);

	/** @return the array size of the surface represented by the texture. */
	virtual uint32 GetSurfaceArraySize() const PURE_VIRTUAL(UTexture::GetSurfaceArraySize, return 0;);

	virtual TextureAddress GetTextureAddressX() const { return TA_Wrap; }
	virtual TextureAddress GetTextureAddressY() const { return TA_Wrap; }
	virtual TextureAddress GetTextureAddressZ() const { return TA_Wrap; }

	/**
	 * Access the GUID which defines this texture's resources externally through FExternalTextureRegistry
	 */
	virtual FGuid GetExternalTextureGuid() const
	{
		return FGuid();
	}

#if WITH_EDITOR
	//~ Begin AsyncCompilation Interface
	virtual bool IsCompiling() const override { return IsDefaultTexture(); }
	//~ End AsyncCompilation Interface

	// if texture is currently in async build action or queue, block until it is done
	//	note: Modify() and PreEditChange() do this.  You should usually be using PreEdit/PostEdit and not calling this directly.
	ENGINE_API virtual void BlockOnAnyAsyncBuild();

	//~ Begin UObject Interface.
	ENGINE_API virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif // WITH_EDITOR
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void PostLoad() override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	ENGINE_API virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	ENGINE_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	ENGINE_API virtual void BeginDestroy() override;
	ENGINE_API virtual bool IsReadyForFinishDestroy() override;
	ENGINE_API virtual void FinishDestroy() override;
	ENGINE_API virtual void PostCDOContruct() override;
#if WITH_EDITORONLY_DATA
	ENGINE_API static void AppendToClassSchema(FAppendToClassSchemaContext& Context);
	ENGINE_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif
	ENGINE_API virtual bool IsPostLoadThreadSafe() const override;
	//~ End UObject Interface.

	//~ Begin UStreamableRenderAsset Interface
	virtual int32 GetLODGroupForStreaming() const final override { return static_cast<int32>(LODGroup); }
	virtual EStreamableRenderAssetType GetRenderAssetType() const final override { return EStreamableRenderAssetType::Texture; }
	ENGINE_API virtual FIoFilenameHash GetMipIoFilenameHash(const int32 MipIndex) const final override;
	ENGINE_API virtual bool DoesMipDataExist(const int32 MipIndex) const final override;
	ENGINE_API virtual bool HasPendingRenderResourceInitialization() const final override;
	ENGINE_API virtual bool HasPendingLODTransition() const final override;
	ENGINE_API virtual void InvalidateLastRenderTimeForStreaming() final override;
	ENGINE_API virtual float GetLastRenderTimeForStreaming() const final override;
	ENGINE_API virtual bool ShouldMipLevelsBeForcedResident() const final override;
	//~ End UStreamableRenderAsset Interface

	/**
	 * Cancels any pending texture streaming actions if possible.
	 * Returns when no more async loading requests are in flight.
	 */
	ENGINE_API static void CancelPendingTextureStreaming();

	/**
	 *	Gets the average brightness of the texture (in linear space)
	 *
	 *	@param	bIgnoreTrueBlack		If true, then pixels w/ 0,0,0 rgb values do not contribute.
	 *	@param	bUseGrayscale			If true, use gray scale else use the max color component.
	 *
	 *	@return	float					The average brightness of the texture
	 */
	ENGINE_API virtual float GetAverageBrightness(bool bIgnoreTrueBlack, bool bUseGrayscale);
	
	// @todo document
	ENGINE_API static const TCHAR* GetTextureGroupString(TextureGroup InGroup);

	// @todo document
	ENGINE_API static const TCHAR* GetMipGenSettingsString(TextureMipGenSettings InEnum);

	// @param	bTextureGroup	true=TexturGroup, false=Texture otherwise
	ENGINE_API static TextureMipGenSettings GetMipGenSettingsFromString(const TCHAR* InStr, bool bTextureGroup);

	/**
	 * Forces textures to recompute LOD settings and stream as needed.
	 * @returns true if the settings were applied, false if they couldn't be applied immediately.
	 */
	ENGINE_API static bool ForceUpdateTextureStreaming();

	/**
	 * Checks whether this texture has a high dynamic range (HDR) source.
	 *
	 * @return true if the texture has an HDR source, false otherwise.
	 */
	bool HasHDRSource(int32 LayerIndex = 0) const
	{
#if WITH_EDITOR
		return FTextureSource::IsHDR(Source.GetFormat(LayerIndex));
#else
		return false;
#endif // WITH_EDITOR
	}


	/** @return true if the compression type is a normal map compression type */
	bool IsNormalMap() const
	{
		return (CompressionSettings == TC_Normalmap);
	}

	/** @return true if the texture has an uncompressed texture setting */
	bool IsUncompressed() const
	{
		return UE::TextureDefines::IsUncompressed(CompressionSettings);
	}

	/** 
	 * Checks whether this texture should be tiled to a platform-specific format during cook, or whether the bNotOfflineProcessed flag 
	 * should be set to true at runtime because it has not been tiled at cook
	 * 
	 * @param  TargetPlatform	The platform for which the texture is being cooked and texture group info will be extracted from. 
	 *                          If null, this info will be extracted from UDeviceProfileManager::Get().GetActiveProfile(), possibly at runtime
	 * @return true if platform tiling during cook is disabled for this texture 
	 */
	ENGINE_API bool IsCookPlatformTilingDisabled(const ITargetPlatform* TargetPlatform) const;

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 * This is the size in GPU memory of the built platformdata, accounting for LODBias, etc.
	 *
	 * @param	Enum	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes.  Returns 0 for error.
	 */
	virtual uint32 CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const
	{
		return 0;
	}

	/** Returns a unique identifier for this texture. Used by the lighting build and texture streamer. */
	const FGuid& GetLightingGuid() const
	{
		return LightingGuid;
	}

	/** 
	 * Assigns a new GUID to a texture. This will be called whenever a texture is created or changes. 
	 * In game, the GUIDs are only used by the texture streamer to link build data to actual textures,
	 * that means new textures don't actually need GUIDs (see FStreamingTextureLevelContext)
	 */
	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#else
		LightingGuid = FGuid(0, 0, 0, 0);
#endif // WITH_EDITORONLY_DATA
	}

	void SetLightingGuid(const FGuid& Guid)
	{
		LightingGuid = Guid;
	}

	/** Generates a deterministic GUID for the texture based on the full name of the object.
	  * Used to ensure that assets created during cook can be deterministic
	  */
	ENGINE_API void SetDeterministicLightingGuid();

	/**
	 * Retrieves the pixel format enum for enum <-> string conversions.
	 */
	ENGINE_API static class UEnum* GetPixelFormatEnum();

	/** Returns the minimum number of mips that must be resident in memory (cannot be streamed). */
	static FORCEINLINE int32 GetStaticMinTextureResidentMipCount()
	{
		return GMinTextureResidentMipCount;
	}

	/** Sets the minimum number of mips that must be resident in memory (cannot be streamed). */
	static void SetMinTextureResidentMipCount(int32 InMinTextureResidentMipCount);

	/** Do the Texture properties make it a possible candidate for streaming.  Can be called by editor or runtime.
	If true, does not mean it will actually be streamed, but if false will definitely not be streamed. */
	bool IsPossibleToStream() const;

#if WITH_EDITOR
	/** Called by ULevel::MarkNoStreamableTexturesPrimitiveComponents when cooking level.
	* Return false for VT. */
	bool IsCandidateForTextureStreamingOnPlatformDuringCook(const ITargetPlatform* InTargetPlatform) const;

	/** Get the largest allowed dimension of non-VT texture
	* this is not for the current RHI (which may have a lower limit), this is for a Texture in general
	*/
	ENGINE_API static int32 GetMaximumDimensionOfNonVT();

	/*
	 * Downsize the 2D Image with the build settings for the texture until all dimensions are <= TargetSize.
	 * This downsizes using the mip generation system and so will only cut sizes in half. Return false
	 * if a parameter is invalid. Returns true if the output is <= TargetSize, whether or not downsizing occurred.
	 */
	ENGINE_API bool DownsizeImageUsingTextureSettings(const ITargetPlatform* TargetPlatform, FImage& InOutImage, int32 TargetSize, int32 LayerIndex, bool & OutMadeChanges) const;

	/*
	*/
	ENGINE_API void GetTargetPlatformBuildSettings(const ITargetPlatform* TargetPlatform, TArray<TArray<FTextureBuildSettings>>& OutSettingsPerFormatPerLayer) const;

#endif

protected:

	/** The minimum number of mips that must be resident in memory (cannot be streamed). */
	static ENGINE_API int32 GMinTextureResidentMipCount;

#if WITH_EDITOR
	// The Texture compiler might use TryCancelCachePlatformData on shutdown
	friend class FTextureCompilingManager;

	enum class ENotifyMaterialsEffectOnShaders
	{
		Default,
		DoesNotInvalidate
	};
	/** Try to cancel any async tasks on PlatformData. 
	 *  Returns true if there is no more async tasks pending, false otherwise.
	 */
	ENGINE_API bool TryCancelCachePlatformData();

	/** Notify any loaded material instances that the texture has changed. */
	ENGINE_API void NotifyMaterials(const ENotifyMaterialsEffectOnShaders EffectOnShaders = ENotifyMaterialsEffectOnShaders::Default);

#endif //WITH_EDITOR

	void BeginFinalReleaseResource();

	/**
	 * Calculates the render resource initial state, expected to be used in InitResource() for derived classes implementing streaming.
	 *
	 * @param	PlatformData - the asset platform data.
	 * @param	bAllowStreaming - where streaming is allowed, might still be disabled based on asset settings.
	 * @param	MaxMipCount - optional limitation on the max mip count.
	 * @return  The state to be passed to FStreamableTextureResource.
	 */
	FStreamableRenderResourceState GetResourcePostInitState(const FTexturePlatformData* PlatformData, bool bAllowStreaming, int32 MinRequestMipCount = 0, int32 MaxMipCount = 0, bool bSkipCanBeLoaded = false) const;
};
