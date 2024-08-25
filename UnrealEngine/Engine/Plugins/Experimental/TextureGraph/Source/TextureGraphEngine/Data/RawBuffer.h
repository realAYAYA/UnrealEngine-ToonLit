// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Helper/DataUtil.h"
THIRD_PARTY_INCLUDES_START
#include "continuable/continuable.hpp"
THIRD_PARTY_INCLUDES_END

#include <set>
#include <PixelFormat.h>
#include <Engine/Texture.h>

enum class BufferFormat 
{
	Byte,
	Half,
	Float,
	Short,
	Int,

	Count,

	Auto = -1,						/// Automatically deduce
	Custom = -2,
	
	LateBound = -3,					/// The Format is unknown at the time and will 
									/// be known after some async operation has finished
									/// Please don't change this value
};

enum class BufferType
{
	Generic,
	Image,
	Mesh,
	MeshDetail
};

//////////////////////////////////////////////////////////////////////////
/// BufferDescriptor Metadata Defs
/// Some standardised metadata defs that are used very often. This is not
/// meant to be a comprehensive repository. Other systems can make 
/// assumptions and have their own names without having them defined over here.
//////////////////////////////////////////////////////////////////////////
struct RawBufferMetadataDefs
{
	//////////////////////////////////////////////////////////////////////////
	/// FX related metadata
	//////////////////////////////////////////////////////////////////////////
	static const FString			G_FX_UAV;

	//////////////////////////////////////////////////////////////////////////
	/// CPU related metadata
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	/// Semantic metadata
	//////////////////////////////////////////////////////////////////////////
	static const FString			G_LAYER_MASK;
};

//////////////////////////////////////////////////////////////////////////
struct TEXTUREGRAPHENGINE_API BufferDescriptor
{
	typedef std::set<FString>		BufferMetadata;

	FString							Name;					/// The name of the buffer
	uint32							Width = 0;				/// Width of the buffer in terms of number of points
	uint32							Height = 0;				/// Height of the buffer in terms of number of points
	uint32							ItemsPerPoint = 0;		/// How many items of Type BufferFormat per point
	BufferFormat					Format = BufferFormat::Auto;		/// What is the Type of each item in the buffer
	BufferType						Type = BufferType::Image;			/// Buffer Type. This is mostly used for debugging
	FLinearColor					DefaultValue = FLinearColor::Black; /// The default value of the blob values

	bool							bIsSRGB = false;
	bool							bIsTransient = false;	/// Whether this is a transient buffer
	bool							bMipMaps = false;		/// Enable mip maps for this buffer

	BufferMetadata					Metadata;				/// The metadata associated with this buffer descriptor

	HashType						HashValue() const;
	HashType						FormatHashValue() const;
	static const char*				FormatToString(BufferFormat InFormat);

									BufferDescriptor() {}
									BufferDescriptor(uint32 InWidth, uint32 InHeight, uint32 InItemsPerPoint, 
										BufferFormat InFormat = BufferFormat::Float, FLinearColor InDefaultValue = FLinearColor::Black, BufferType InType = BufferType::Image, bool bInMipMaps = false, bool bInSRGB = false);

	static EPixelFormat				BufferPixelFormat(BufferFormat InFormat, uint32 InItemsPerPoint);
	static BufferFormat				BufferFormatFromPixelFormat(EPixelFormat PixelFormat);
	static size_t					BufferFormatSize(BufferFormat InFormat);
	static ETextureSourceFormat		TextureSourceFormat(BufferFormat InFormat, uint32 InItemsPerPoint);
	static BufferDescriptor			Combine(const BufferDescriptor& Desc1, const BufferDescriptor& Desc2);
	static BufferDescriptor			CombineWithPreference(const BufferDescriptor* BaseDesc, const BufferDescriptor* OverrideDesc, const BufferDescriptor* RefDesc);

	ETextureSourceFormat			TextureSourceFormat() const;
	bool							IsValid() const;
	bool							IsFinal() const;

	FORCEINLINE size_t				FormatSize() const { return BufferDescriptor::BufferFormatSize(Format); }
	FORCEINLINE size_t				Pitch() const { return Width * FormatSize() * ItemsPerPoint; }
	FORCEINLINE size_t				Size() const { return Pitch() * Height; }
	FORCEINLINE EPixelFormat		PixelFormat() const { return BufferDescriptor::BufferPixelFormat(Format, ItemsPerPoint); }

	FORCEINLINE bool				HasMetadata(const FString& meta) const { return Metadata.find(meta) != Metadata.end(); }
	FORCEINLINE bool				HasMetadata(FString&& meta) const { return Metadata.find(meta) != Metadata.end(); }
	FORCEINLINE bool				IsLateBound() const { return Format == BufferFormat::LateBound; }
	FORCEINLINE bool				IsAuto() const { return Format == BufferFormat::Auto; }
	FORCEINLINE bool				IsAutoWidth() const { return Width <= 0; }
	FORCEINLINE bool				IsAutoHeight() const { return Height <= 0; }
	FORCEINLINE bool				IsAutoSize() const { return IsAutoWidth() || IsAutoHeight(); }

	void							AddMetadata(const FString& meta) { Metadata.insert(meta); };
	void							AddMetadata(FString&& meta) { Metadata.insert(meta); };

	bool							operator == (const BufferDescriptor& rhs) const;
	bool							operator != (const BufferDescriptor& RHS) const;
	
	FORCEINLINE void				AllowUAV() { AddMetadata(RawBufferMetadataDefs::G_FX_UAV); }
	FORCEINLINE bool				RequiresUAV() const { return HasMetadata(RawBufferMetadataDefs::G_FX_UAV); }
};

typedef std::unique_ptr<BufferDescriptor>	BufferDescriptorUPtr;
typedef std::shared_ptr<BufferDescriptor>	BufferDescriptorPtr;

class RawBuffer;
typedef std::shared_ptr<RawBuffer>			RawBufferPtr;
typedef T_Tiles<std::shared_ptr<RawBuffer>> RawBufferPtrTiles;
typedef cti::continuable<RawBufferPtr>		AsyncRawBufferPtr;
typedef cti::continuable<RawBuffer*>		AsyncRawBufferP;

enum class RawBufferCompressionType
{
	None,							/// No compression
	Auto,							/// Automatically choose a compression Format
	PNG,							/// PNG compression
	ZLib,							/// ZLib compression
	GZip,							/// GZip compression
	LZ4,							/// LZ4 compression
};

class TEXTUREGRAPHENGINE_API RawBuffer
{
public:
	static const uint64				GMinCompress;				/// Must have at least this much data to consider compression. 
																/// Otherwise, its not worth it

private:
	/// Must be fixed size (ideally every member should be 64-bit aligned)
	struct FileHeader
	{
		uint64						Version = sizeof(FileHeader);
		uint64						CompressionType;
		uint64						CompressedLength = 0;

		/// For backwards compatibility Anything that 
	};

	static const RawBufferCompressionType GDefaultCompression;

	friend class					Device_Mem;

	//////////////////////////////////////////////////////////////////////////
	/// UnCompressed data
	//////////////////////////////////////////////////////////////////////////
	const uint8*					Data = nullptr;				/// The actual raw buffer
	uint64							Length = 0;					/// The length of the data
	BufferDescriptor				Desc;						/// The buffer descriptor
	mutable CHashPtr				HashValue;						/// The hash for this buffer
	bool							bIsMemoryAutoManaged = false; // There can be instances where the owner of _data isnt RawBuffer, this check ensures those cases

	//////////////////////////////////////////////////////////////////////////
	/// Compressed data
	//////////////////////////////////////////////////////////////////////////
	const uint8*					CompressedData = nullptr;	/// The compressed data
	uint64							CompressedLength = 0;		/// Length of the compressed buffer
	RawBufferCompressionType		CompressionType = RawBufferCompressionType::None; /// What is the Type of compression used

	//////////////////////////////////////////////////////////////////////////
	/// Disk data (compressed)
	////////////////////////////////////////////////////////////////////////// 
	FString							FileName;		/// The path where the file data is kept

	RawBufferCompressionType		ChooseBestCompressionFormat() const;

	void							FreeUncompressed();
	void							FreeCompressed();
	void							FreeDisk();

	bool							UncompressPNG();
	bool							UncompressGeneric(FName InName);

	bool							CompressPNG();
	bool							CompressGeneric(FName InName, RawBufferCompressionType InType);

	AsyncRawBufferP					ReadFromFile(bool bDoUncompress = false);

public:
									RawBuffer(const uint8* InData, size_t InLength, const BufferDescriptor& InDesc, CHashPtr InHashValue = nullptr, bool bInIsMemoryAutoManaged = false);
	explicit						RawBuffer(const BufferDescriptor& InDesc);
	explicit						RawBuffer(const RawBufferPtrTiles& InTiles);
									~RawBuffer();

	AsyncRawBufferP					Compress(RawBufferCompressionType InCompressionType = RawBufferCompressionType::Auto, bool bFreeMemory = true);
	AsyncRawBufferP					Uncompress(bool bFreeMemory = true);

	AsyncRawBufferP					WriteToFile(const FString& InFileName, bool bFreeMemory = true);

	AsyncRawBufferP					LoadRawBuffer(bool bInDoUncompress, bool bFreeMemory = true);

	void							GetAsLinearColor(TArray<FLinearColor>& Pixels);
	FLinearColor					GetAsLinearColor(int PixelIndex);

	bool							IsPadded() const;
	size_t							GetUnpaddedSize();
	void							CopyUnpaddedBytes(uint8* DestData);
	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE const uint8*		GetData() const { return Data; }
	FORCEINLINE size_t				GetLength() const { return Length; }
	FORCEINLINE CHashPtr			Hash() const { return HashValue; }
	FORCEINLINE const FString&		GetName() const { return Desc.Name; }
	FORCEINLINE const BufferDescriptor& GetDescriptor() const { return Desc; }
	FORCEINLINE uint32				Width() const { return Desc.Width; }
	FORCEINLINE uint32				Height() const { return Desc.Height; }
	FORCEINLINE bool				IsTransient() const { return Desc.bIsTransient; }
	FORCEINLINE bool				IsCompressed() const { return CompressedData != nullptr; }
	FORCEINLINE bool				HasData() const { check(IsInGameThread()); return Data != nullptr; }
};

