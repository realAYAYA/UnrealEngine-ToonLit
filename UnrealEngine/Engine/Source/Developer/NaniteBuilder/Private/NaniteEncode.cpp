// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteEncode.h"

#include "Rendering/NaniteResources.h"
#include "Hash/CityHash.h"
#include "Math/UnrealMath.h"
#include "Cluster.h"
#include "ClusterDAG.h"
#include "Async/ParallelFor.h"
#include "Misc/Compression.h"
#include "Containers/StaticBitArray.h"

#define CONSTRAINED_CLUSTER_CACHE_SIZE				32
#define MAX_DEPENDENCY_CHAIN_FOR_RELATIVE_ENCODING	6		// Reset dependency chain by forcing direct encoding every time a page has this many levels of dependent relative encodings.
															// This prevents long chains of dependent dispatches during decode.
															// As this affects only a small fraction of pages, the compression impact is negligible.

#define FLT_INT_MIN						(-2147483648.0f)	// Smallest float >= INT_MIN
#define FLT_INT_MAX						2147483520.0f		// Largest float <= INT_MAX

namespace Nanite
{

struct FClusterGroupPart					// Whole group or a part of a group that has been split.
{
	TArray<uint32>	Clusters;				// Can be reordered during page allocation, so we need to store a list here.
	FBounds3f		Bounds;
	uint32			PageIndex;
	uint32			GroupIndex;				// Index of group this is a part of.
	uint32			HierarchyNodeIndex;
	uint32			HierarchyChildIndex;
	uint32			PageClusterOffset;
};

struct FPageSections
{
	uint32 Cluster				= 0;
	uint32 MaterialTable		= 0;
	uint32 VertReuseBatchInfo	= 0;
	uint32 DecodeInfo			= 0;
	uint32 Index				= 0;
	uint32 Position				= 0;
	uint32 Attribute			= 0;

	uint32 GetMaterialTableSize() const			{ return Align(MaterialTable, 16); }
	uint32 GetVertReuseBatchInfoSize() const	{ return Align(VertReuseBatchInfo, 16); }
	uint32 GetDecodeInfoSize() const			{ return Align(DecodeInfo, 16); }

	uint32 GetClusterOffset() const				{ return NANITE_GPU_PAGE_HEADER_SIZE; }
	uint32 GetMaterialTableOffset() const		{ return GetClusterOffset() + Cluster; }
	uint32 GetVertReuseBatchInfoOffset() const	{ return GetMaterialTableOffset() + GetMaterialTableSize(); }
	uint32 GetDecodeInfoOffset() const			{ return GetVertReuseBatchInfoOffset() + GetVertReuseBatchInfoSize(); }
	uint32 GetIndexOffset() const				{ return GetDecodeInfoOffset() + GetDecodeInfoSize(); }
	uint32 GetPositionOffset() const			{ return GetIndexOffset() + Index; }
	uint32 GetAttributeOffset() const			{ return GetPositionOffset() + Position; }
	uint32 GetTotal() const						{ return GetAttributeOffset() + Attribute; }

	FPageSections GetOffsets() const
	{
		return FPageSections{ GetClusterOffset(), GetMaterialTableOffset(), GetVertReuseBatchInfoOffset(), GetDecodeInfoOffset(), GetIndexOffset(), GetPositionOffset(), GetAttributeOffset() };
	}

	void operator+=(const FPageSections& Other)
	{
		Cluster				+=	Other.Cluster;
		MaterialTable		+=	Other.MaterialTable;
		VertReuseBatchInfo	+=	Other.VertReuseBatchInfo;
		DecodeInfo			+=	Other.DecodeInfo;
		Index				+=	Other.Index;
		Position			+=	Other.Position;
		Attribute			+=	Other.Attribute;
	}
};

struct FPageGPUHeader
{
	uint32 NumClusters = 0;
	uint32 Pad[3] = { 0 };
};

struct FPageDiskHeader
{
	uint32 NumClusters;
	uint32 NumRawFloat4s;
	uint32 NumVertexRefs;
	uint32 DecodeInfoOffset;
	uint32 StripBitmaskOffset;
	uint32 VertexRefBitmaskOffset;
};

struct FClusterDiskHeader
{
	uint32 IndexDataOffset;
	uint32 PageClusterMapOffset;
	uint32 VertexRefDataOffset;
	uint32 LowBytesOffset;
	uint32 MidBytesOffset;
	uint32 HighBytesOffset;
	uint32 NumVertexRefs;
	uint32 NumPrevRefVerticesBeforeDwords;
	uint32 NumPrevNewVerticesBeforeDwords;
};

struct FPage
{
	uint32	PartsStartIndex = 0;
	uint32	PartsNum = 0;
	uint32	NumClusters = 0;
	uint32	MaxHierarchyDepth = 0;
	bool	bRelativeEncoding = false;

	FPageSections	GpuSizes;
};

struct FUVRange
{
	FUintVector2 Min				= FUintVector2::ZeroValue;
	FUintVector2 NumBits			= FUintVector2::ZeroValue;
};

struct FPackedUVRange
{
	FUintVector2 Data;
};

static void PackUVRange(FPackedUVRange& PackedUVRange, const FUVRange& UVRange)
{
	check(UVRange.NumBits.X <= NANITE_UV_FLOAT_MAX_BITS			&& UVRange.NumBits.Y <= NANITE_UV_FLOAT_MAX_BITS);
	check(UVRange.Min.X     <  (1u << NANITE_UV_FLOAT_MAX_BITS) && UVRange.Min.Y     <  (1u << NANITE_UV_FLOAT_MAX_BITS));
	
	PackedUVRange.Data.X = (UVRange.Min.X << 5) | UVRange.NumBits.X;
	PackedUVRange.Data.Y = (UVRange.Min.Y << 5) | UVRange.NumBits.Y;
}

struct FEncodingInfo
{
	uint32		BitsPerIndex = 0;
	uint32		BitsPerAttribute = 0;

	uint32		NormalPrecision = 0;
	uint32		TangentPrecision = 0;
	
	uint32		ColorMode = 0;
	FIntVector4 ColorMin = FIntVector4(0, 0, 0, 0);
	FIntVector4 ColorBits = FIntVector4(0, 0, 0, 0);

	FPageSections GpuSizes;

	FUVRange	UVRanges[NANITE_MAX_UVS];
};

// Wasteful to store size for every vert but easier this way.
struct FVariableVertex
{
	const float*	Data;
	uint32			SizeInBytes;

	bool operator==( FVariableVertex Other ) const
	{
		return 0 == FMemory::Memcmp( Data, Other.Data, SizeInBytes );
	}
};

FORCEINLINE uint32 GetTypeHash( FVariableVertex Vert )
{
	return CityHash32( (const char*)Vert.Data, Vert.SizeInBytes );
}

template<uint32 BitLength>
class TFixedBitVector
{
	enum { QWordLength = (BitLength + 63) / 64 };
public:
	uint64 Data[QWordLength];

	void Clear()
	{
		FMemory::Memzero(Data);
	}

	void SetBit(uint32 Index)
	{
		check(Index < BitLength);
		Data[Index >> 6] |= 1ull << (Index & 63);
	}

	uint32 GetBit(uint32 Index)
	{
		check(Index < BitLength);
		return uint32(Data[Index >> 6] >> (Index & 63)) & 1u;
	}

	uint32 CountBits()
	{
		uint32 Result = 0;
		for (uint32 i = 0; i < QWordLength; i++)
		{
			Result += FGenericPlatformMath::CountBits(Data[i]);
		}
		return Result;
	}

	TFixedBitVector<BitLength> operator|(const TFixedBitVector<BitLength>& Other) const
	{
		TFixedBitVector<BitLength> Result;
		for (uint32 i = 0; i < QWordLength; i++)
		{
			Result.Data[i] = Data[i] | Other.Data[i];
		}
		return Result;
	}
};

// Naive bit writer for cooking purposes
class FBitWriter
{
public:
	FBitWriter(TArray<uint8>& Buffer) :
		Buffer(Buffer),
		PendingBits(0ull),
		NumPendingBits(0)
	{
	}

	void PutBits(uint32 Bits, uint32 NumBits)
	{
		check((uint64)Bits < (1ull << NumBits));
		PendingBits |= (uint64)Bits << NumPendingBits;
		NumPendingBits += NumBits;

		while (NumPendingBits >= 8)
		{
			Buffer.Add((uint8)PendingBits);
			PendingBits >>= 8;
			NumPendingBits -= 8;
		}
	}

	void Flush(uint32 Alignment=1)
	{
		if (NumPendingBits > 0)
			Buffer.Add((uint8)PendingBits);
		while (Buffer.Num() % Alignment != 0)
			Buffer.Add(0);
		PendingBits = 0;
		NumPendingBits = 0;
	}

private:
	TArray<uint8>& 	Buffer;
	uint64 			PendingBits;
	int32 			NumPendingBits;
};

static uint32 EncodeZigZag(int32 X)
{
	return uint32((X << 1) ^ (X >> 31));
}

static int32 DecodeZigZag(uint32 X)
{
	return int32(X >> 1) ^ -int32(X & 1);
}

static void RemoveRootPagesFromRange(uint32& StartPage, uint32& NumPages, const uint32 NumResourceRootPages)
{
	if (StartPage < NumResourceRootPages)
	{
		NumPages = (uint32)FMath::Max((int32)NumPages - (int32)(NumResourceRootPages - StartPage), 0); 
		StartPage = NumResourceRootPages;
	}

	if(NumPages == 0)
	{
		StartPage = 0;
	}
}

static void RemovePageFromRange(uint32& StartPage, uint32& NumPages, const uint32 PageIndex)
{
	if (NumPages > 0)
	{
		if (StartPage == PageIndex)
		{
			StartPage++;
			NumPages--;
		}
		else if (StartPage + NumPages - 1 == PageIndex)
		{
			NumPages--;
		}
	}

	if (NumPages == 0)
	{
		StartPage = 0;
	}
}

FORCEINLINE static FVector2f OctahedronEncode(FVector3f N)
{
	FVector3f AbsN = N.GetAbs();
	N /= (AbsN.X + AbsN.Y + AbsN.Z);

	if (N.Z < 0.0)
	{
		AbsN = N.GetAbs();
		N.X = (N.X >= 0.0f) ? (1.0f - AbsN.Y) : (AbsN.Y - 1.0f);
		N.Y = (N.Y >= 0.0f) ? (1.0f - AbsN.X) : (AbsN.X - 1.0f);
	}
	
	return FVector2f(N.X, N.Y);
}

FORCEINLINE static void OctahedronEncode(FVector3f N, int32& X, int32& Y, int32 QuantizationBits)
{
	const int32 QuantizationMaxValue = (1 << QuantizationBits) - 1;
	const float Scale = 0.5f * (float)QuantizationMaxValue;
	const float Bias = 0.5f * (float)QuantizationMaxValue + 0.5f;

	FVector2f Coord = OctahedronEncode(N);

	X = FMath::Clamp(int32(Coord.X * Scale + Bias), 0, QuantizationMaxValue);
	Y = FMath::Clamp(int32(Coord.Y * Scale + Bias), 0, QuantizationMaxValue);
}

FORCEINLINE static FVector3f OctahedronDecode(int32 X, int32 Y, int32 QuantizationBits)
{
	const int32 QuantizationMaxValue = (1 << QuantizationBits) - 1;
	float fx = (float)X * (2.0f / (float)QuantizationMaxValue) - 1.0f;
	float fy = (float)Y * (2.0f / (float)QuantizationMaxValue) - 1.0f;
	float fz = 1.0f - FMath::Abs(fx) - FMath::Abs(fy);
	float t = FMath::Clamp(-fz, 0.0f, 1.0f);
	fx += (fx >= 0.0f ? -t : t);
	fy += (fy >= 0.0f ? -t : t);

	return FVector3f(fx, fy, fz).GetUnsafeNormal();
}

FORCEINLINE static void OctahedronEncodePreciseSIMD( FVector3f N, int32& X, int32& Y, int32 QuantizationBits )
{
	const int32 QuantizationMaxValue = ( 1 << QuantizationBits ) - 1;
	FVector2f ScalarCoord = OctahedronEncode( N );

	const VectorRegister4f Scale = VectorSetFloat1( 0.5f * (float)QuantizationMaxValue );
	const VectorRegister4f RcpScale = VectorSetFloat1( 2.0f / (float)QuantizationMaxValue );
	VectorRegister4Int IntCoord = VectorFloatToInt( VectorMultiplyAdd( MakeVectorRegister( ScalarCoord.X, ScalarCoord.Y, ScalarCoord.X, ScalarCoord.Y ), Scale, Scale ) );	// x0, y0, x1, y1
	IntCoord = VectorIntAdd( IntCoord, MakeVectorRegisterInt( 0, 0, 1, 1 ) );
	VectorRegister4f Coord = VectorMultiplyAdd( VectorIntToFloat( IntCoord ), RcpScale, GlobalVectorConstants::FloatMinusOne );	// Coord = Coord * 2.0f / QuantizationMaxValue - 1.0f

	VectorRegister4f Nx = VectorSwizzle( Coord, 0, 2, 0, 2 );
	VectorRegister4f Ny = VectorSwizzle( Coord, 1, 1, 3, 3 );
	VectorRegister4f Nz = VectorSubtract( VectorSubtract( VectorOneFloat(), VectorAbs( Nx ) ), VectorAbs( Ny ) );			// Nz = 1.0f - abs(Nx) - abs(Ny)

	VectorRegister4f T = VectorMin( Nz, VectorZeroFloat() );	// T = min(Nz, 0.0f)
	
	VectorRegister4f NxSign = VectorBitwiseAnd( Nx, GlobalVectorConstants::SignBit() );
	VectorRegister4f NySign = VectorBitwiseAnd( Ny, GlobalVectorConstants::SignBit() );

	Nx = VectorAdd(Nx, VectorBitwiseXor( T, NxSign ) );	// Nx += T ^ NxSign
	Ny = VectorAdd(Ny, VectorBitwiseXor( T, NySign ) );	// Ny += T ^ NySign
	
	VectorRegister4f Dots = VectorMultiplyAdd(Nx, VectorSetFloat1(N.X), VectorMultiplyAdd(Ny, VectorSetFloat1(N.Y), VectorMultiply(Nz, VectorSetFloat1(N.Z))));
	VectorRegister4f Lengths = VectorSqrt(VectorMultiplyAdd(Nx, Nx, VectorMultiplyAdd(Ny, Ny, VectorMultiply(Nz, Nz))));
	Dots = VectorDivide(Dots, Lengths);

	VectorRegister4f Mask = MakeVectorRegister( 0xFFFFFFFCu, 0xFFFFFFFCu, 0xFFFFFFFCu, 0xFFFFFFFCu );
	VectorRegister4f LaneIndices = MakeVectorRegister( 0u, 1u, 2u, 3u );
	Dots = VectorBitwiseOr( VectorBitwiseAnd( Dots, Mask ), LaneIndices );
	
	// Calculate max component
	VectorRegister4f MaxDot = VectorMax( Dots, VectorSwizzle( Dots, 2, 3, 0, 1 ) );
	MaxDot = VectorMax( MaxDot, VectorSwizzle( MaxDot, 1, 2, 3, 0 ) );

	float fIndex = VectorGetComponent( MaxDot, 0 );
	uint32 Index = *(uint32*)&fIndex;
	
	uint32 IntCoordValues[ 4 ];
	VectorIntStore( IntCoord, IntCoordValues );
	X = FMath::Clamp((int32)(IntCoordValues[0] + ( Index & 1 )), 0, QuantizationMaxValue);
	Y = FMath::Clamp((int32)(IntCoordValues[1] + ( ( Index >> 1 ) & 1 )), 0, QuantizationMaxValue);
}

FORCEINLINE static void OctahedronEncodePrecise(FVector3f N, int32& X, int32& Y, int32 QuantizationBits)
{
	const int32 QuantizationMaxValue = (1 << QuantizationBits) - 1;
	FVector2f Coord = OctahedronEncode(N);

	const float Scale = 0.5f * (float)QuantizationMaxValue;
	const float Bias = 0.5f * (float)QuantizationMaxValue;
	int32 NX = FMath::Clamp(int32(Coord.X * Scale + Bias), 0, QuantizationMaxValue);
	int32 NY = FMath::Clamp(int32(Coord.Y * Scale + Bias), 0, QuantizationMaxValue);

	float MinError = 1.0f;
	int32 BestNX = 0;
	int32 BestNY = 0;
	for (int32 OffsetY = 0; OffsetY < 2; OffsetY++)
	{
		for (int32 OffsetX = 0; OffsetX < 2; OffsetX++)
		{
			int32 TX = NX + OffsetX;
			int32 TY = NY + OffsetY;
			if (TX <= QuantizationMaxValue && TY <= QuantizationMaxValue)
			{
				FVector3f RN = OctahedronDecode(TX, TY, QuantizationBits);
				float Error = FMath::Abs(1.0f - (RN | N));
				if (Error < MinError)
				{
					MinError = Error;
					BestNX = TX;
					BestNY = TY;
				}
			}
		}
	}

	X = BestNX;
	Y = BestNY;
}

FORCEINLINE static uint32 PackNormal(FVector3f Normal, uint32 QuantizationBits)
{
	int32 X, Y;
	OctahedronEncodePreciseSIMD(Normal, X, Y, QuantizationBits);

#if 0
	// Test against non-SIMD version
	int32 X2, Y2;
	OctahedronEncodePrecise(Normal, X2, Y2, QuantizationBits);
	FVector3f N0 = OctahedronDecode( X, Y, QuantizationBits );
	FVector3f N1 = OctahedronDecode( X2, Y2, QuantizationBits );
	float dt0 = Normal | N0;
	float dt1 = Normal | N1;
	check( dt0 >= dt1*0.99999f );
#endif
	
	return (Y << QuantizationBits) | X;
}

FORCEINLINE static FVector3f UnpackNormal(uint32 PackedNormal, uint32 QuantizationBits)
{
	const uint32 QuantizationMaxValue = (1u << QuantizationBits) - 1u;
	const uint32 UX = PackedNormal & QuantizationMaxValue;
	const uint32 UY = PackedNormal >> QuantizationBits;
	float X = float(UX) * (2.0f / float(QuantizationMaxValue)) - 1.0f;
	float Y = float(UY) * (2.0f / float(QuantizationMaxValue)) - 1.0f;
	const float Z = 1.0f - FMath::Abs(X) - FMath::Abs(Y);
	const float T = FMath::Clamp(-Z, 0.0f, 1.0f);
	X += (X >= 0.0f) ? -T : T;
	Y += (Y >= 0.0f) ? -T : T;

	return FVector3f(X, Y, Z).GetUnsafeNormal();
}

static bool PackTangent(uint32& QuantizedTangentAngle, FVector3f TangentX, FVector3f TangentZ, uint32 NumTangentBits)
{
	FVector3f LocalTangentX = TangentX;
	FVector3f LocalTangentZ = TangentZ;
	
	// Conditionally swap X and Z, if abs(Z)>abs(X).
	// After this, we know the largest component is in X or Y and at least one of them is going to be non-zero.
	checkSlow(TangentZ.IsNormalized());
	const bool bSwapXZ = (FMath::Abs(LocalTangentZ.Z) > FMath::Abs(LocalTangentZ.X));
	if (bSwapXZ)
	{
		Swap(LocalTangentZ.X, LocalTangentZ.Z);
		Swap(LocalTangentX.X, LocalTangentX.Z);
	}

	FVector3f LocalTangentRefX = FVector3f(-LocalTangentZ.Y, LocalTangentZ.X, 0.0f).GetSafeNormal();
	FVector3f LocalTangentRefY = (LocalTangentZ ^ LocalTangentRefX);

	const float X = LocalTangentX | LocalTangentRefX;
	const float Y = LocalTangentX | LocalTangentRefY;
	const float LenSq = X * X + Y * Y;

	if (LenSq >= 0.0001f)
	{
		float Angle = FMath::Atan2(Y, X);
		if (Angle < PI) Angle += 2.0f * PI;

		const float UnitAngle = Angle / (2.0f * PI);

		int IntAngle = FMath::FloorToInt(UnitAngle * float(1 << NumTangentBits) + 0.5f);
		QuantizedTangentAngle = uint32(IntAngle & ((1 << NumTangentBits) - 1));
		return true;
	}

	return false;
}

static FVector3f UnpackTangent(uint32& QuantizedTangentAngle, FVector3f TangentZ, uint32 NumTangentBits)
{
	FVector3f LocalTangentZ = TangentZ;
	
	const bool bSwapXZ = (FMath::Abs(TangentZ.Z) > FMath::Abs(TangentZ.X));
	if (bSwapXZ)
	{
		Swap(LocalTangentZ.X, LocalTangentZ.Z);
	}

	const FVector3f LocalTangentRefX = FVector3f(-LocalTangentZ.Y, LocalTangentZ.X, 0.0f).GetSafeNormal();
	const FVector3f LocalTangentRefY = (LocalTangentZ ^ LocalTangentRefX);

	const float UnpackedAngle = float(QuantizedTangentAngle) / float(1 << NumTangentBits) * 2.0f * PI;
	FVector3f UnpackedTangentX = (LocalTangentRefX * FMath::Cos(UnpackedAngle) + LocalTangentRefY * FMath::Sin(UnpackedAngle)).GetUnsafeNormal();

	if (bSwapXZ)
	{
		Swap(UnpackedTangentX.X, UnpackedTangentX.Z);
	}

	return UnpackedTangentX;
}

static uint32 PackMaterialTableRange(uint32 TriStart, uint32 TriLength, uint32 MaterialIndex)
{
	uint32 Packed = 0x00000000;
	// uint32 TriStart      :  8; // max 128 triangles
	// uint32 TriLength     :  8; // max 128 triangles
	// uint32 MaterialIndex :  6; // max  64 materials
	// uint32 Padding       : 10;
	check(TriStart <= 128);
	check(TriLength <= 128);
	check(MaterialIndex < 64);
	Packed |= TriStart;
	Packed |= TriLength << 8;
	Packed |= MaterialIndex << 16;
	return Packed;
}

static uint32 PackMaterialFastPath(uint32 Material0Length, uint32 Material0Index, uint32 Material1Length, uint32 Material1Index, uint32 Material2Index)
{
	uint32 Packed = 0x00000000;
	// Material Packed Range - Fast Path (32 bits)
	// uint Material0Index  : 6;   // max  64 materials (0:Material0Length)
	// uint Material1Index  : 6;   // max  64 materials (Material0Length:Material1Length)
	// uint Material2Index  : 6;   // max  64 materials (remainder)
	// uint Material0Length : 7;   // max 128 triangles (num minus one)
	// uint Material1Length : 7;   // max  64 triangles (materials are sorted, so at most 128/2)
	check(Material0Index  <  64);
	check(Material1Index  <  64);
	check(Material2Index  <  64);
	check(Material0Length >= 1);
	check(Material0Length <= 128);
	check(Material1Length <= 64);
	check(Material1Length <= Material0Length);
	Packed |= Material0Index;
	Packed |= Material1Index << 6;
	Packed |= Material2Index << 12;
	Packed |= (Material0Length - 1u) << 18;
	Packed |= Material1Length << 25;
	return Packed;
}

static uint32 PackMaterialSlowPath(uint32 MaterialTableOffset, uint32 MaterialTableLength)
{
	// Material Packed Range - Slow Path (32 bits)
	// uint BufferIndex     : 19; // 2^19 max value (tons, it's per prim)
	// uint BufferLength	: 6;  // max 64 materials, so also at most 64 ranges (num minus one)
	// uint Padding			: 7;  // always 127 for slow path. corresponds to Material1Length=127 in fast path
	check(MaterialTableOffset < 524288); // 2^19 - 1
	check(MaterialTableLength > 0); // clusters with 0 materials use fast path
	check(MaterialTableLength <= 64);
	uint32 Packed = MaterialTableOffset;
	Packed |= (MaterialTableLength - 1u) << 19;
	Packed |= (0xFE000000u);
	return Packed;
}

static uint32 CalcMaterialTableSize( const Nanite::FCluster& InCluster )
{
	uint32 NumMaterials = InCluster.MaterialRanges.Num();
	return NumMaterials > 3 ? NumMaterials : 0;
}

static uint32 CalcVertReuseBatchInfoSize(const TArrayView<const FMaterialRange>& MaterialRanges)
{
	constexpr int32 NumBatchCountBits = 4;
	constexpr int32 NumTriCountBits = 5;
	constexpr int32 WorstCaseFullBatchTriCount = 10;

	int32 TotalNumBatches = 0;
	int32 NumBitsNeeded = 0;

	for (const FMaterialRange& MaterialRange : MaterialRanges)
	{
		const int32 NumBatches = MaterialRange.BatchTriCounts.Num();
		check(NumBatches > 0 && NumBatches < (1 << NumBatchCountBits));
		TotalNumBatches += NumBatches;
		NumBitsNeeded += NumBatchCountBits + NumBatches * NumTriCountBits;
	}
	NumBitsNeeded += FMath::Max(NumBatchCountBits * (3 - MaterialRanges.Num()), 0);
	check(TotalNumBatches < FMath::DivideAndRoundUp(NANITE_MAX_CLUSTER_TRIANGLES, WorstCaseFullBatchTriCount) + MaterialRanges.Num() - 1);

	return FMath::DivideAndRoundUp(NumBitsNeeded, 32);
}

static void PackVertReuseBatchInfo(const TArrayView<const FMaterialRange>& MaterialRanges, TArray<uint32>& OutVertReuseBatchInfo)
{
	constexpr int32 NumBatchCountBits = 4;
	constexpr int32 NumTriCountBits = 5;

	auto AppendBits = [](uint32*& DwordPtr, uint32& BitOffset, uint32 Bits, uint32 NumBits)
	{
		uint32 BitsConsumed = FMath::Min(NumBits, 32u - BitOffset);
		SetBits(*DwordPtr, (Bits & ((1 << BitsConsumed) - 1)), BitsConsumed, BitOffset);
		BitOffset += BitsConsumed;
		if (BitOffset >= 32u)
		{
			check(BitOffset == 32u);
			++DwordPtr;
			BitOffset -= 32u;
		}
		if (BitsConsumed < NumBits)
		{
			Bits >>= BitsConsumed;
			BitsConsumed = NumBits - BitsConsumed;
			SetBits(*DwordPtr, Bits, BitsConsumed, BitOffset);
			BitOffset += BitsConsumed;
			check(BitOffset < 32u);
		}
	};

	const uint32 NumDwordsNeeded = CalcVertReuseBatchInfoSize(MaterialRanges);
	OutVertReuseBatchInfo.Empty(NumDwordsNeeded);
	OutVertReuseBatchInfo.AddZeroed(NumDwordsNeeded);

	uint32* NumArrayDwordPtr = &OutVertReuseBatchInfo[0];
	uint32 NumArrayBitOffset = 0;
	const uint32 NumArrayBits = FMath::Max(MaterialRanges.Num(), 3) * NumBatchCountBits;
	uint32* TriCountDwordPtr = &OutVertReuseBatchInfo[NumArrayBits >> 5];
	uint32 TriCountBitOffset = NumArrayBits & 0x1f;

	for (const FMaterialRange& MaterialRange : MaterialRanges)
	{
		const uint32 NumBatches = MaterialRange.BatchTriCounts.Num();
		check(NumBatches > 0);
		AppendBits(NumArrayDwordPtr, NumArrayBitOffset, NumBatches, NumBatchCountBits);

		for (int32 BatchIndex = 0; BatchIndex < MaterialRange.BatchTriCounts.Num(); ++BatchIndex)
		{
			const uint32 BatchTriCount = MaterialRange.BatchTriCounts[BatchIndex];
			check(BatchTriCount > 0 && BatchTriCount - 1 < (1 << NumTriCountBits));
			AppendBits(TriCountDwordPtr, TriCountBitOffset, BatchTriCount - 1, NumTriCountBits);
		}
	}
}

static uint32 PackMaterialInfo(const Nanite::FCluster& InCluster, TArray<uint32>& OutMaterialTable, TArray<uint32>& OutVertReuseBatchInfo, uint32 MaterialTableStartOffset)
{
	// Encode material ranges
	uint32 NumMaterialTriangles = 0;
	for (int32 RangeIndex = 0; RangeIndex < InCluster.MaterialRanges.Num(); ++RangeIndex)
	{
		check(InCluster.MaterialRanges[RangeIndex].RangeLength <= 128);
		check(InCluster.MaterialRanges[RangeIndex].RangeLength > 0);
		check(InCluster.MaterialRanges[RangeIndex].MaterialIndex < NANITE_MAX_CLUSTER_MATERIALS);
		NumMaterialTriangles += InCluster.MaterialRanges[RangeIndex].RangeLength;
	}

	// All triangles accounted for in material ranges?
	check(NumMaterialTriangles == InCluster.NumTris);

	uint32 PackedMaterialInfo = 0x00000000;

	// The fast inline path can encode up to 3 materials
	if (InCluster.MaterialRanges.Num() <= 3)
	{
		uint32 Material0Length = 0;
		uint32 Material0Index = 0;
		uint32 Material1Length = 0;
		uint32 Material1Index = 0;
		uint32 Material2Index = 0;

		if (InCluster.MaterialRanges.Num() > 0)
		{
			const FMaterialRange& Material0 = InCluster.MaterialRanges[0];
			check(Material0.RangeStart == 0);
			Material0Length = Material0.RangeLength;
			Material0Index = Material0.MaterialIndex;
		}

		if (InCluster.MaterialRanges.Num() > 1)
		{
			const FMaterialRange& Material1 = InCluster.MaterialRanges[1];
			check(Material1.RangeStart == InCluster.MaterialRanges[0].RangeLength);
			Material1Length = Material1.RangeLength;
			Material1Index = Material1.MaterialIndex;
		}

		if (InCluster.MaterialRanges.Num() > 2)
		{
			const FMaterialRange& Material2 = InCluster.MaterialRanges[2];
			check(Material2.RangeStart == Material0Length + Material1Length);
			check(Material2.RangeLength == InCluster.NumTris - Material0Length - Material1Length);
			Material2Index = Material2.MaterialIndex;
		}

		PackedMaterialInfo = PackMaterialFastPath(Material0Length, Material0Index, Material1Length, Material1Index, Material2Index);
	}
	// Slow global table search path
	else
	{
		uint32 MaterialTableOffset = OutMaterialTable.Num() + MaterialTableStartOffset;
		uint32 MaterialTableLength = InCluster.MaterialRanges.Num();
		check(MaterialTableLength > 0);

		for (int32 RangeIndex = 0; RangeIndex < InCluster.MaterialRanges.Num(); ++RangeIndex)
		{
			const FMaterialRange& Material = InCluster.MaterialRanges[RangeIndex];
			OutMaterialTable.Add(PackMaterialTableRange(Material.RangeStart, Material.RangeLength, Material.MaterialIndex));
		}

		PackedMaterialInfo = PackMaterialSlowPath(MaterialTableOffset, MaterialTableLength);
	}

	PackVertReuseBatchInfo(MakeArrayView(InCluster.MaterialRanges), OutVertReuseBatchInfo);

	return PackedMaterialInfo;
}

static void PackCluster(Nanite::FPackedCluster& OutCluster, const Nanite::FCluster& InCluster, const FEncodingInfo& EncodingInfo, bool bHasTangents, uint32 NumTexCoords)
{
	FMemory::Memzero(OutCluster);

	// 0
	OutCluster.SetNumVerts(InCluster.NumVerts);
	OutCluster.SetPositionOffset(0);
	OutCluster.SetNumTris(InCluster.NumTris);
	OutCluster.SetIndexOffset(0);
	OutCluster.ColorMin = EncodingInfo.ColorMin.X | (EncodingInfo.ColorMin.Y << 8) | (EncodingInfo.ColorMin.Z << 16) | (EncodingInfo.ColorMin.W << 24);
	OutCluster.SetColorBitsR(EncodingInfo.ColorBits.X);
	OutCluster.SetColorBitsG(EncodingInfo.ColorBits.Y);
	OutCluster.SetColorBitsB(EncodingInfo.ColorBits.Z);
	OutCluster.SetColorBitsA(EncodingInfo.ColorBits.W);
	OutCluster.SetGroupIndex(InCluster.GroupIndex);

	// 1
	OutCluster.PosStart = InCluster.QuantizedPosStart;
	OutCluster.SetBitsPerIndex(EncodingInfo.BitsPerIndex);
	OutCluster.SetPosPrecision(InCluster.QuantizedPosPrecision);
	OutCluster.SetPosBitsX(InCluster.QuantizedPosBits.X);
	OutCluster.SetPosBitsY(InCluster.QuantizedPosBits.Y);
	OutCluster.SetPosBitsZ(InCluster.QuantizedPosBits.Z);

	// 2
	OutCluster.LODBounds				= FVector4f(InCluster.LODBounds.Center.X, InCluster.LODBounds.Center.Y, InCluster.LODBounds.Center.Z, InCluster.LODBounds.W);

	// 3
	OutCluster.BoxBoundsCenter			= (InCluster.Bounds.Min + InCluster.Bounds.Max) * 0.5f;
	OutCluster.LODErrorAndEdgeLength	= FFloat16(InCluster.LODError).Encoded | (FFloat16(InCluster.EdgeLength).Encoded << 16);

	// 4
	OutCluster.BoxBoundsExtent			= (InCluster.Bounds.Max - InCluster.Bounds.Min) * 0.5f;
	OutCluster.Flags					= NANITE_CLUSTER_FLAG_STREAMING_LEAF | NANITE_CLUSTER_FLAG_ROOT_LEAF;
	
	// 5
	check(NumTexCoords <= NANITE_MAX_UVS);
	static_assert(NANITE_MAX_UVS <= 4, "UV_Prev encoding only supports up to 4 channels");

	uint32 UVBitOffsets = 0;
	uint32 BitOffset = 0;
	for (uint32 i = 0; i < NumTexCoords; i++)
	{
		check(BitOffset < 256);
		UVBitOffsets |= BitOffset << (i * 8);
		const FUVRange& UVRange = EncodingInfo.UVRanges[i];
		BitOffset += UVRange.NumBits.X + UVRange.NumBits.Y;
	}

	OutCluster.SetBitsPerAttribute(EncodingInfo.BitsPerAttribute);
	OutCluster.SetNormalPrecision(EncodingInfo.NormalPrecision);
	OutCluster.SetTangentPrecision(EncodingInfo.TangentPrecision);
	OutCluster.SetHasTangents(bHasTangents);
	OutCluster.SetNumUVs(NumTexCoords);
	OutCluster.SetColorMode(EncodingInfo.ColorMode);
	OutCluster.UVBitOffsets			= UVBitOffsets;
	OutCluster.PackedMaterialInfo	= 0;	// Filled out by WritePages
}

struct FHierarchyNode
{
	FSphere3f		LODBounds[NANITE_MAX_BVH_NODE_FANOUT];
	FBounds3f		Bounds[NANITE_MAX_BVH_NODE_FANOUT];
	float			MinLODErrors[NANITE_MAX_BVH_NODE_FANOUT];
	float			MaxParentLODErrors[NANITE_MAX_BVH_NODE_FANOUT];
	uint32			ChildrenStartIndex[NANITE_MAX_BVH_NODE_FANOUT];
	uint32			NumChildren[NANITE_MAX_BVH_NODE_FANOUT];
	uint32			ClusterGroupPartIndex[NANITE_MAX_BVH_NODE_FANOUT];
};

static void PackHierarchyNode(Nanite::FPackedHierarchyNode& OutNode, const FHierarchyNode& InNode, const TArray<FClusterGroup>& Groups, const TArray<FClusterGroupPart>& GroupParts, const uint32 NumResourceRootPages)
{
	static_assert(NANITE_MAX_RESOURCE_PAGES_BITS + NANITE_MAX_CLUSTERS_PER_GROUP_BITS + NANITE_MAX_GROUP_PARTS_BITS <= 32, "");
	for (uint32 i = 0; i < NANITE_MAX_BVH_NODE_FANOUT; i++)
	{
		OutNode.LODBounds[i] = FVector4f(InNode.LODBounds[i].Center, InNode.LODBounds[i].W);

		const FBounds3f& Bounds = InNode.Bounds[i];
		OutNode.Misc0[i].BoxBoundsCenter = Bounds.GetCenter();
		OutNode.Misc1[i].BoxBoundsExtent = Bounds.GetExtent();

		check(InNode.NumChildren[i] <= NANITE_MAX_CLUSTERS_PER_GROUP);
		OutNode.Misc0[i].MinLODError_MaxParentLODError	= FFloat16( InNode.MinLODErrors[i] ).Encoded | ( FFloat16( InNode.MaxParentLODErrors[i] ).Encoded << 16 );
		OutNode.Misc1[i].ChildStartReference			= InNode.ChildrenStartIndex[i];

		uint32 ResourcePageIndex_NumPages_GroupPartSize = 0;
		if( InNode.NumChildren[ i ] > 0 )
		{
			if( InNode.ClusterGroupPartIndex[ i ] != MAX_uint32 )
			{
				// Leaf node
				const FClusterGroup& Group = Groups[GroupParts[InNode.ClusterGroupPartIndex[i]].GroupIndex];
				uint32 GroupPartSize = InNode.NumChildren[ i ];

				// If group spans multiple pages, request all of them, except the root pages
				uint32 PageIndexStart = Group.PageIndexStart;
				uint32 PageIndexNum = Group.PageIndexNum;
				RemoveRootPagesFromRange(PageIndexStart, PageIndexNum, NumResourceRootPages);
				ResourcePageIndex_NumPages_GroupPartSize = (PageIndexStart << (NANITE_MAX_CLUSTERS_PER_GROUP_BITS + NANITE_MAX_GROUP_PARTS_BITS)) | (PageIndexNum << NANITE_MAX_CLUSTERS_PER_GROUP_BITS) | GroupPartSize;
			}
			else
			{
				// Hierarchy node. No resource page or group size.
				ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFFu;
			}
		}
		OutNode.Misc2[ i ].ResourcePageIndex_NumPages_GroupPartSize = ResourcePageIndex_NumPages_GroupPartSize;
	}
}

static int32 CalculateQuantizedPositionsUniformGrid(TArray< FCluster >& Clusters, const FBounds3f& MeshBounds, const FMeshNaniteSettings& Settings)
{
	// Simple global quantization for EA
	const int32 MaxPositionQuantizedValue	= (1 << NANITE_MAX_POSITION_QUANTIZATION_BITS) - 1;

	{
		// Make sure the worst case bounding box fits with the position encoding settings. Ideally this would be a compile-time check.
		const float MaxValue = FMath::RoundToFloat(NANITE_MAX_COORDINATE_VALUE * FMath::Exp2((float)NANITE_MIN_POSITION_PRECISION));
		checkf(MaxValue <= FLT_INT_MAX && int64(MaxValue) - int64(-MaxValue) <= MaxPositionQuantizedValue, TEXT("Largest cluster bounds doesn't fit in position bits"));
	}
	
	int32 PositionPrecision = Settings.PositionPrecision;
	if (PositionPrecision == MIN_int32)
	{
		// Auto: Guess required precision from bounds at leaf level
		const float MaxSize = MeshBounds.GetExtent().GetMax();

		// Heuristic: We want higher resolution if the mesh is denser.
		// Use geometric average of cluster size as a proxy for density.
		// Alternative interpretation: Bit precision is average of what is needed by the clusters.
		// For roughly uniformly sized clusters this gives results very similar to the old quantization code.
		double TotalLogSize = 0.0;
		int32 TotalNum = 0;
		for (const FCluster& Cluster : Clusters)
		{
			if (Cluster.MipLevel == 0)
			{
				float ExtentSize = Cluster.Bounds.GetExtent().Size();
				if (ExtentSize > 0.0)
				{
					TotalLogSize += FMath::Log2(ExtentSize);
					TotalNum++;
				}
			}
		}
		double AvgLogSize = TotalNum > 0 ? TotalLogSize / TotalNum : 0.0;
		PositionPrecision = 7 - (int32)FMath::RoundToInt(AvgLogSize);

		// Clamp precision. The user now needs to explicitly opt-in to the lowest precision settings.
		// These settings are likely to cause issues and contribute little to disk size savings (~0.4% on test project),
		// so we shouldn't pick them automatically.
		// Example: A very low resolution road or building frame that needs little precision to look right in isolation,
		// but still requires fairly high precision in a scene because smaller meshes are placed on it or in it.
		const int32 AUTO_MIN_PRECISION = 4;	// 1/16cm
		PositionPrecision = FMath::Max(PositionPrecision, AUTO_MIN_PRECISION);
	}

	PositionPrecision = FMath::Clamp(PositionPrecision, NANITE_MIN_POSITION_PRECISION, NANITE_MAX_POSITION_PRECISION);

	float QuantizationScale = FMath::Exp2((float)PositionPrecision);

	// Make sure all clusters are encodable. A large enough cluster could hit the 21bpc limit. If it happens scale back until it fits.
	for (const FCluster& Cluster : Clusters)
	{
		const FBounds3f& Bounds = Cluster.Bounds;
		
		int32 Iterations = 0;
		while (true)
		{
			float MinX = FMath::RoundToFloat(Bounds.Min.X * QuantizationScale);
			float MinY = FMath::RoundToFloat(Bounds.Min.Y * QuantizationScale);
			float MinZ = FMath::RoundToFloat(Bounds.Min.Z * QuantizationScale);

			float MaxX = FMath::RoundToFloat(Bounds.Max.X * QuantizationScale);
			float MaxY = FMath::RoundToFloat(Bounds.Max.Y * QuantizationScale);
			float MaxZ = FMath::RoundToFloat(Bounds.Max.Z * QuantizationScale);

			if (MinX >= FLT_INT_MIN && MinY >= FLT_INT_MIN && MinZ >= FLT_INT_MIN &&
				MaxX <= FLT_INT_MAX && MaxY <= FLT_INT_MAX && MaxZ <= FLT_INT_MAX &&
				((int64)MaxX - (int64)MinX) <= MaxPositionQuantizedValue && ((int64)MaxY - (int64)MinY) <= MaxPositionQuantizedValue && ((int64)MaxZ - (int64)MinZ) <= MaxPositionQuantizedValue)
			{
				break;
			}
			
			QuantizationScale *= 0.5f;
			PositionPrecision--;
			check(PositionPrecision >= NANITE_MIN_POSITION_PRECISION);
			check(++Iterations < 100);	// Endless loop?
		}
	}

	const float RcpQuantizationScale = 1.0f / QuantizationScale;

	ParallelFor( TEXT("NaniteEncode.QuantizeClusterPositions.PF"), Clusters.Num(), 256, [&](uint32 ClusterIndex)
	{
		FCluster& Cluster = Clusters[ClusterIndex];
		
		const uint32 NumClusterVerts = Cluster.NumVerts;
		Cluster.QuantizedPositions.SetNumUninitialized(NumClusterVerts);

		// Quantize positions
		FIntVector IntClusterMax = { MIN_int32,	MIN_int32, MIN_int32 };
		FIntVector IntClusterMin = { MAX_int32,	MAX_int32, MAX_int32 };

		for (uint32 i = 0; i < NumClusterVerts; i++)
		{
			const FVector3f Position = Cluster.GetPosition(i);

			FIntVector& IntPosition = Cluster.QuantizedPositions[i];
			float PosX = FMath::RoundToFloat(Position.X * QuantizationScale);
			float PosY = FMath::RoundToFloat(Position.Y * QuantizationScale);
			float PosZ = FMath::RoundToFloat(Position.Z * QuantizationScale);

			IntPosition = FIntVector((int32)PosX, (int32)PosY, (int32)PosZ);

			IntClusterMax.X = FMath::Max(IntClusterMax.X, IntPosition.X);
			IntClusterMax.Y = FMath::Max(IntClusterMax.Y, IntPosition.Y);
			IntClusterMax.Z = FMath::Max(IntClusterMax.Z, IntPosition.Z);
			IntClusterMin.X = FMath::Min(IntClusterMin.X, IntPosition.X);
			IntClusterMin.Y = FMath::Min(IntClusterMin.Y, IntPosition.Y);
			IntClusterMin.Z = FMath::Min(IntClusterMin.Z, IntPosition.Z);
		}

		// Store in minimum number of bits
		const uint32 NumBitsX = FMath::CeilLogTwo(IntClusterMax.X - IntClusterMin.X + 1);
		const uint32 NumBitsY = FMath::CeilLogTwo(IntClusterMax.Y - IntClusterMin.Y + 1);
		const uint32 NumBitsZ = FMath::CeilLogTwo(IntClusterMax.Z - IntClusterMin.Z + 1);
		check(NumBitsX <= NANITE_MAX_POSITION_QUANTIZATION_BITS);
		check(NumBitsY <= NANITE_MAX_POSITION_QUANTIZATION_BITS);
		check(NumBitsZ <= NANITE_MAX_POSITION_QUANTIZATION_BITS);

		for (uint32 i = 0; i < NumClusterVerts; i++)
		{
			FIntVector& IntPosition = Cluster.QuantizedPositions[i];

			// Update float position with quantized data
			Cluster.GetPosition(i) = FVector3f((float)IntPosition.X * RcpQuantizationScale, (float)IntPosition.Y * RcpQuantizationScale, (float)IntPosition.Z * RcpQuantizationScale);
			
			IntPosition.X -= IntClusterMin.X;
			IntPosition.Y -= IntClusterMin.Y;
			IntPosition.Z -= IntClusterMin.Z;
			check(IntPosition.X >= 0 && IntPosition.X < (1 << NumBitsX));
			check(IntPosition.Y >= 0 && IntPosition.Y < (1 << NumBitsY));
			check(IntPosition.Z >= 0 && IntPosition.Z < (1 << NumBitsZ));
		}


		// Update bounds
		Cluster.Bounds.Min = FVector3f((float)IntClusterMin.X * RcpQuantizationScale, (float)IntClusterMin.Y * RcpQuantizationScale, (float)IntClusterMin.Z * RcpQuantizationScale);
		Cluster.Bounds.Max = FVector3f((float)IntClusterMax.X * RcpQuantizationScale, (float)IntClusterMax.Y * RcpQuantizationScale, (float)IntClusterMax.Z * RcpQuantizationScale);

		Cluster.QuantizedPosBits = FIntVector(NumBitsX, NumBitsY, NumBitsZ);
		Cluster.QuantizedPosStart = IntClusterMin;
		Cluster.QuantizedPosPrecision = PositionPrecision;

	} );
	return PositionPrecision;
}

static float DecodeUVFloat(uint32 EncodedValue, uint32 NumMantissaBits)
{
	const uint32 ExponentAndMantissaMask	= (1u << (NANITE_UV_FLOAT_NUM_EXPONENT_BITS + NumMantissaBits)) - 1u;
	const bool bNeg							= (EncodedValue <= ExponentAndMantissaMask);
	const uint32 ExponentAndMantissa		= (bNeg ? ~EncodedValue : EncodedValue) & ExponentAndMantissaMask;

	const uint32 FloatBits	= 0x3F000000u + (ExponentAndMantissa << (23 - NumMantissaBits));
	float Result			= (float&)FloatBits;
	Result					= FMath::Min(Result * 2.0f - 1.0f, Result);		// Stretch denormals from [0.5,1.0] to [0.0,1.0]

	return bNeg ? -Result : Result;
}

static void VerifyUVFloatEncoding(float Value, uint32 EncodedValue, uint32 NumMantissaBits)
{
	check(FMath::IsFinite(Value));	// NaN and Inf should have been handled already

	const uint32 NumValues = 1u << (1 + NumMantissaBits + NANITE_UV_FLOAT_NUM_EXPONENT_BITS);
	
	const float DecodedValue = DecodeUVFloat(EncodedValue, NumMantissaBits);
	const float Error = FMath::Abs(DecodedValue - Value);

	// Verify that none of the neighbor code points are closer to the original float value.
	if (EncodedValue > 0u)
	{
		const float PrevValue = DecodeUVFloat(EncodedValue - 1u, NumMantissaBits);
		check(FMath::Abs(PrevValue - Value) >= Error);
	}

	if (EncodedValue + 1u < NumValues)
	{
		const float NextValue = DecodeUVFloat(EncodedValue + 1u, NumMantissaBits);
		check(FMath::Abs(NextValue - Value) >= Error);
	}
}

static uint32 EncodeUVFloat(float Value, uint32 NumMantissaBits)
{
	// Encode UV floats as a custom float type where [0,1] is denormal, so it gets uniform precision.
	// As UVs are encoded in clusters as ranges of encoded values, a few modifications to the usual
	// float encoding are made to preserve the original float order when the encoded values are interpreted as uints:
	// 1. Positive values use 1 as sign bit.
	// 2. Negative values use 0 as sign bit and have their exponent and mantissa bits inverted.

	checkSlow(FMath::IsFinite(Value));

	const uint32 SignBitPosition = NANITE_UV_FLOAT_NUM_EXPONENT_BITS + NumMantissaBits;
	const uint32 FloatUInt = (uint32&)Value;
	const uint32 Exponent = (FloatUInt >> 23) & 0xFFu;
	const uint32 Mantissa = FloatUInt & 0x7FFFFFu;
	const uint32 AbsFloatUInt = FloatUInt & 0x7FFFFFFFu;

	uint32 Result;
	if (AbsFloatUInt < 0x3F800000u)
	{
		// Denormal encoding
		// Note: Mantissa can overflow into first non-denormal value (1.0f),
		// but that is desirable to get correct round-to-nearest behavior.
		const float AbsFloat = (float&)AbsFloatUInt;
		Result = uint32(double(AbsFloat * float(1u << NumMantissaBits)) + 0.5);	// Cast to double to make sure +0.5 is lossless
	}
	else
	{
		// Normal encoding
		// Extract exponent and mantissa bits from 32-bit float-
		const uint32 Shift = (23 - NumMantissaBits);
		const uint32 Tmp = (AbsFloatUInt - 0x3F000000u) + (1u << (Shift - 1));	// Bias to round to nearest
		Result = FMath::Min(Tmp >> Shift, (1u << SignBitPosition) - 1u);		// Clamp to largest UV float value
	}

	// Produce a mask that for positive values only flips the sign bit
	// and for negative values only flips the exponent and mantissa bits.
	const uint32 SignMask = (1u << SignBitPosition) - (FloatUInt >> 31u);
	Result ^= SignMask;

#if DO_GUARD_SLOW
	VerifyUVFloatEncoding(Value, Result, NumMantissaBits);
#endif
	return Result;
}

static void CalculateEncodingInfo(FEncodingInfo& Info, const Nanite::FCluster& Cluster, int32 NormalPrecision, int32 TangentPrecision, bool bHasTangents, bool bHasColors, uint32 NumTexCoords)
{
	const uint32 NumClusterVerts = Cluster.NumVerts;
	const uint32 NumClusterTris = Cluster.NumTris;

	FMemory::Memzero(Info);

	// Write triangles indices. Indices are stored in a dense packed bitstream using ceil(log2(NumClusterVerices)) bits per index. The shaders implement unaligned bitstream reads to support this.
	const uint32 BitsPerIndex = NumClusterVerts > 1 ? (FGenericPlatformMath::FloorLog2(NumClusterVerts - 1) + 1) : 0;
	const uint32 BitsPerTriangle = BitsPerIndex + 2 * 5;	// Base index + two 5-bit offsets
	Info.BitsPerIndex = BitsPerIndex;

	FPageSections& GpuSizes = Info.GpuSizes;
	GpuSizes.Cluster = sizeof(FPackedCluster);
	GpuSizes.MaterialTable = CalcMaterialTableSize(Cluster) * sizeof(uint32);
	GpuSizes.VertReuseBatchInfo = Cluster.MaterialRanges.Num() > 3 ? CalcVertReuseBatchInfoSize(Cluster.MaterialRanges) * sizeof(uint32) : 0;
	GpuSizes.DecodeInfo = NumTexCoords * sizeof(FPackedUVRange);
	GpuSizes.Index = (NumClusterTris * BitsPerTriangle + 31) / 32 * 4;

#if NANITE_USE_UNCOMPRESSED_VERTEX_DATA
	const uint32 AttribBytesPerVertex = (3 * sizeof(float) + (bHasTangents ? (4 * sizeof(float)) : 0) + sizeof(uint32) + NumTexCoords * 2 * sizeof(float));

	Info.BitsPerAttribute = AttribBytesPerVertex * 8;
	Info.ColorMin = FIntVector4(0, 0, 0, 0);
	Info.ColorBits = FIntVector4(8, 8, 8, 8);
	Info.ColorMode = NANITE_VERTEX_COLOR_MODE_VARIABLE;
	Info.NormalPrecision = 0;
	Info.TangentPrecision = 0;

	GpuSizes.Position = NumClusterVerts * 3 * sizeof(float);
	GpuSizes.Attribute = NumClusterVerts * AttribBytesPerVertex;
#else
	Info.BitsPerAttribute = 2 * NormalPrecision;

	if (bHasTangents)
	{
		Info.BitsPerAttribute += 1 + TangentPrecision;
	}

	check(NumClusterVerts > 0);
	const bool bIsLeaf = (Cluster.GeneratingGroupIndex == MAX_uint32);

	// Normals
	Info.NormalPrecision = NormalPrecision;
	Info.TangentPrecision = TangentPrecision;

	// Vertex colors
	Info.ColorMode = NANITE_VERTEX_COLOR_MODE_CONSTANT;
	Info.ColorMin = FIntVector4(255, 255, 255, 255);
	if (bHasColors)
	{
		FIntVector4 ColorMin = FIntVector4( 255, 255, 255, 255);
		FIntVector4 ColorMax = FIntVector4( 0, 0, 0, 0);
		for (uint32 i = 0; i < NumClusterVerts; i++)
		{
			FColor Color = Cluster.GetColor(i).ToFColor(false);
			ColorMin.X = FMath::Min(ColorMin.X, (int32)Color.R);
			ColorMin.Y = FMath::Min(ColorMin.Y, (int32)Color.G);
			ColorMin.Z = FMath::Min(ColorMin.Z, (int32)Color.B);
			ColorMin.W = FMath::Min(ColorMin.W, (int32)Color.A);
			ColorMax.X = FMath::Max(ColorMax.X, (int32)Color.R);
			ColorMax.Y = FMath::Max(ColorMax.Y, (int32)Color.G);
			ColorMax.Z = FMath::Max(ColorMax.Z, (int32)Color.B);
			ColorMax.W = FMath::Max(ColorMax.W, (int32)Color.A);
		}

		const FIntVector4 ColorDelta = ColorMax - ColorMin;
		const int32 R_Bits = FMath::CeilLogTwo(ColorDelta.X + 1);
		const int32 G_Bits = FMath::CeilLogTwo(ColorDelta.Y + 1);
		const int32 B_Bits = FMath::CeilLogTwo(ColorDelta.Z + 1);
		const int32 A_Bits = FMath::CeilLogTwo(ColorDelta.W + 1);
		
		uint32 NumColorBits = R_Bits + G_Bits + B_Bits + A_Bits;
		Info.BitsPerAttribute += NumColorBits;
		Info.ColorMin = ColorMin;
		Info.ColorBits = FIntVector4(R_Bits, G_Bits, B_Bits, A_Bits);
		if (NumColorBits > 0)
		{
			Info.ColorMode = NANITE_VERTEX_COLOR_MODE_VARIABLE;
		}
	}

	const int NumMantissaBits = NANITE_UV_FLOAT_NUM_MANTISSA_BITS;	//TODO: make this a build setting
	for( uint32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
	{
		FUVRange& UVRange = Info.UVRanges[UVIndex];

		FUintVector2 UVMin = FUintVector2(0xFFFFFFFFu, 0xFFFFFFFFu);
		FUintVector2 UVMax = FUintVector2(0u, 0u);
		
		for (uint32 i = 0; i < NumClusterVerts; i++)
		{
			const FVector2f& UV = Cluster.GetUVs(i)[UVIndex];

			const uint32 EncodedU = EncodeUVFloat(UV.X, NumMantissaBits);
			const uint32 EncodedV = EncodeUVFloat(UV.Y, NumMantissaBits);

			UVMin.X = FMath::Min(UVMin.X, EncodedU);
			UVMin.Y = FMath::Min(UVMin.Y, EncodedV);
			UVMax.X = FMath::Max(UVMax.X, EncodedU);
			UVMax.Y = FMath::Max(UVMax.Y, EncodedV);
		}

		const FUintVector2 UVDelta = UVMax - UVMin;

		UVRange.Min				= UVMin;
		UVRange.NumBits.X		= FMath::CeilLogTwo(UVDelta.X + 1u);
		UVRange.NumBits.Y		= FMath::CeilLogTwo(UVDelta.Y + 1u);

		Info.BitsPerAttribute	+= UVRange.NumBits.X + UVRange.NumBits.Y;
	}

	const uint32 PositionBitsPerVertex = Cluster.QuantizedPosBits.X + Cluster.QuantizedPosBits.Y + Cluster.QuantizedPosBits.Z;
	GpuSizes.Position = (NumClusterVerts * PositionBitsPerVertex + 31) / 32 * 4;
	GpuSizes.Attribute = (NumClusterVerts * Info.BitsPerAttribute + 31) / 32 * 4;
#endif
}

static void CalculateEncodingInfos(TArray<FEncodingInfo>& EncodingInfos, const TArray<Nanite::FCluster>& Clusters, int32 NormalPrecision, int32 TangentPrecision, bool bHasTangents, bool bHasColors, uint32 NumTexCoords)
{
	uint32 NumClusters = Clusters.Num();
	EncodingInfos.SetNumUninitialized(NumClusters);

	for (uint32 i = 0; i < NumClusters; i++)
	{
		CalculateEncodingInfo(EncodingInfos[i], Clusters[i], NormalPrecision, TangentPrecision, bHasTangents, bHasColors, NumTexCoords);
	}
}

struct FVertexMapEntry
{
	uint32 LocalClusterIndex;
	uint32 VertexIndex;
};

static int32 ShortestWrap(int32 Value, uint32 NumBits)
{
	if (NumBits == 0)
	{
		check(Value == 0);
		return 0;
	}
	const int32 Shift = 32 - NumBits;
	const int32 NumValues = (1 << NumBits);
	const int32 MinValue = -(NumValues >> 1);
	const int32 MaxValue = (NumValues >> 1) - 1;

	Value = (Value << Shift) >> Shift;
	check(Value >= MinValue && Value <= MaxValue);
	return Value;
}
static void EncodeGeometryData(	const uint32 LocalClusterIndex, const FCluster& Cluster, const FEncodingInfo& EncodingInfo, uint32 NumTexCoords,
								TArray<uint32>& StripBitmask, TArray<uint8>& IndexData,
								TArray<uint32>& PageClusterMapData,
								TArray<uint32>& VertexRefBitmask, TArray<uint16>& VertexRefData,
								TArray<uint8>& LowByteStream, TArray<uint8>& MidByteStream, TArray<uint8>& HighByteStream,
								const TArrayView<uint32> PageDependencies, const TArray<TMap<FVariableVertex, FVertexMapEntry>>& PageVertexMaps,
								TMap<FVariableVertex, uint32>& UniqueVertices, uint32& NumCodedVertices, bool bHasTangents)
{
	const uint32 NumClusterVerts = Cluster.NumVerts;
	const uint32 NumClusterTris = Cluster.NumTris;

	VertexRefBitmask.AddZeroed(NANITE_MAX_CLUSTER_VERTICES / 32);
#if NANITE_USE_UNCOMPRESSED_VERTEX_DATA
	// Disable vertex references in uncompressed mode
	NumCodedVertices = NumClusterVerts;
#else
	// Find vertices from same page we can reference instead of storing duplicates
	struct FVertexRef
	{
		uint32 PageIndex;
		uint32 LocalClusterIndex;
		uint32 VertexIndex;
	};

	TArray<FVertexRef> VertexRefs;
	TArray<uint32> UniqueToVertexIndex;
	for (uint32 VertexIndex = 0; VertexIndex < NumClusterVerts; VertexIndex++)
	{
		FVariableVertex Vertex;
		Vertex.Data = &Cluster.Verts[ VertexIndex * Cluster.GetVertSize() ];
		Vertex.SizeInBytes = Cluster.GetVertSize() * sizeof(float);

		FVertexRef VertexRef = {};
		bool bFound = false;

		// Look for vertex in parents
		for (int32 SrcPageIndexIndex = 0; SrcPageIndexIndex < PageDependencies.Num(); SrcPageIndexIndex++)
		{
			uint32 SrcPageIndex = PageDependencies[SrcPageIndexIndex];
			const FVertexMapEntry* EntryPtr = PageVertexMaps[SrcPageIndex].Find(Vertex);
			if (EntryPtr)
			{
				VertexRef = FVertexRef{ (uint32)SrcPageIndexIndex + 1, EntryPtr->LocalClusterIndex, EntryPtr->VertexIndex };
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			// Look for vertex in current page
			uint32* VertexPtr = UniqueVertices.Find(Vertex);
			if (VertexPtr)
			{
				VertexRef = FVertexRef{ 0, (*VertexPtr >> NANITE_MAX_CLUSTER_VERTICES_BITS), *VertexPtr & NANITE_MAX_CLUSTER_VERTICES_MASK };
				bFound = true;
			}
		}

		if(bFound)
		{
			VertexRefs.Add(VertexRef);
			const uint32 BitIndex = (LocalClusterIndex << NANITE_MAX_CLUSTER_VERTICES_BITS) + VertexIndex;
			VertexRefBitmask[BitIndex >> 5] |= 1u << (BitIndex & 31);
		}
		else
		{
			uint32 Val = (LocalClusterIndex << NANITE_MAX_CLUSTER_VERTICES_BITS) | (uint32)VertexIndex;
			UniqueVertices.Add(Vertex, Val);
			UniqueToVertexIndex.Add(VertexIndex);
		}
	}
	NumCodedVertices = UniqueToVertexIndex.Num();

	struct FClusterRef
	{
		uint32 PageIndex;
		uint32 ClusterIndex;

		bool operator==(const FClusterRef& Other) const { return PageIndex == Other.PageIndex && ClusterIndex == Other.ClusterIndex; }
		bool operator<(const FClusterRef& Other) const { return (PageIndex != Other.PageIndex) ? (PageIndex < Other.PageIndex) : (ClusterIndex == Other.ClusterIndex); }
	};

	// Make list of unique Page-Cluster pairs
	TArray<FClusterRef> ClusterRefs;
	for (const FVertexRef& Ref : VertexRefs)
		ClusterRefs.AddUnique(FClusterRef{ Ref.PageIndex, Ref.LocalClusterIndex });
	
	ClusterRefs.Sort();

	for (const FClusterRef& Ref : ClusterRefs)
	{
		PageClusterMapData.Add((Ref.PageIndex << NANITE_MAX_CLUSTERS_PER_PAGE_BITS) | Ref.ClusterIndex);
	}

	// Write vertex refs using Page-Cluster index + vertex index
	uint32 PrevVertexIndex = 0;
	for (const FVertexRef& Ref : VertexRefs)
	{
		uint32 PageClusterIndex = ClusterRefs.Find(FClusterRef{ Ref.PageIndex, Ref.LocalClusterIndex });
		check(PageClusterIndex < 256);
		const uint32 VertexIndexDelta = (Ref.VertexIndex - PrevVertexIndex) & 0xFF;
		VertexRefData.Add(uint16((PageClusterIndex << NANITE_MAX_CLUSTER_VERTICES_BITS) | EncodeZigZag(ShortestWrap(VertexIndexDelta, 8))));
		PrevVertexIndex = Ref.VertexIndex;
	}
#endif

	const uint32 BitsPerIndex = EncodingInfo.BitsPerIndex;
	
	// Write triangle indices
#if NANITE_USE_STRIP_INDICES
	for (uint32 i = 0; i < NANITE_MAX_CLUSTER_TRIANGLES / 32; i++)
	{
		StripBitmask.Add(Cluster.StripDesc.Bitmasks[i][0]);
		StripBitmask.Add(Cluster.StripDesc.Bitmasks[i][1]);
		StripBitmask.Add(Cluster.StripDesc.Bitmasks[i][2]);
	}
	IndexData.Append(Cluster.StripIndexData);
#else
	for (uint32 i = 0; i < NumClusterTris * 3; i++)
	{
		uint32 Index = Cluster.Indexes[i];
		IndexData.Add(Cluster.Indexes[i]);
	}
#endif

	check(NumClusterVerts > 0);

#if NANITE_USE_UNCOMPRESSED_VERTEX_DATA
	FBitWriter BitWriter_Position(LowByteStream);
	for (uint32 VertexIndex = 0; VertexIndex < NumClusterVerts; VertexIndex++)
	{
		const FVector3f& Position = Cluster.GetPosition(VertexIndex);
		BitWriter_Position.PutBits(*(uint32*)&Position.X, 32);
		BitWriter_Position.PutBits(*(uint32*)&Position.Y, 32);
		BitWriter_Position.PutBits(*(uint32*)&Position.Z, 32);
	}
	BitWriter_Position.Flush(sizeof(uint32));

	FBitWriter BitWriter_Attribute(MidByteStream);
	for (uint32 VertexIndex = 0; VertexIndex < NumClusterVerts; VertexIndex++)
	{
		// Normal
		const FVector3f& Normal = Cluster.GetNormal(VertexIndex);
		BitWriter_Attribute.PutBits(*(uint32*)&Normal.X, 32);
		BitWriter_Attribute.PutBits(*(uint32*)&Normal.Y, 32);
		BitWriter_Attribute.PutBits(*(uint32*)&Normal.Z, 32);

		if(bHasTangents)
		{
			const FVector3f TangentX = Cluster.GetTangentX(VertexIndex);
			BitWriter_Attribute.PutBits(*(uint32*)&TangentX.X, 32);
			BitWriter_Attribute.PutBits(*(uint32*)&TangentX.Y, 32);
			BitWriter_Attribute.PutBits(*(uint32*)&TangentX.Z, 32);

			const float TangentYSign = Cluster.GetTangentYSign(VertexIndex) < 0.0f ? -1.0f : 1.0f;
			BitWriter_Attribute.PutBits(*(uint32*)&TangentYSign, 32);
		}

		// Color
		uint32 ColorDW = Cluster.Settings.bHasColors ? Cluster.GetColor(VertexIndex).ToFColor(false).DWColor() : 0xFFFFFFFFu;
		BitWriter_Attribute.PutBits(ColorDW, 32);

		// UVs
		const FVector2f* UVs = Cluster.GetUVs(VertexIndex);
		for (uint32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++)
		{
			const FVector2f& UV = UVs[TexCoordIndex];
			BitWriter_Attribute.PutBits(*(uint32*)&UV.X, 32);
			BitWriter_Attribute.PutBits(*(uint32*)&UV.Y, 32);
		}
	}
	BitWriter_Attribute.Flush(sizeof(uint32));
#else

	// Generate quantized texture coordinates
	TArray<FIntVector2, TFixedAllocator<NANITE_MAX_CLUSTER_VERTICES*NANITE_MAX_UVS>> PackedUVs;
	PackedUVs.AddUninitialized( NumClusterVerts * NumTexCoords );

	const uint32 NumMantissaBits = NANITE_UV_FLOAT_NUM_MANTISSA_BITS;
	for( uint32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
	{
		const FUVRange& UVRange = EncodingInfo.UVRanges[UVIndex];
		const uint32 NumTexCoordValuesU = 1u << UVRange.NumBits.X;
		const uint32 NumTexCoordValuesV = 1u << UVRange.NumBits.Y;

		for(uint32 Index : UniqueToVertexIndex)
		{
			const FVector2f& UV = Cluster.GetUVs(Index)[UVIndex];

			uint32 EncodedU = EncodeUVFloat(UV.X, NumMantissaBits);
			uint32 EncodedV = EncodeUVFloat(UV.Y, NumMantissaBits);

			check(EncodedU >= UVRange.Min.X);
			check(EncodedV >= UVRange.Min.Y);
			EncodedU -= UVRange.Min.X;
			EncodedV -= UVRange.Min.Y;
			
			check(EncodedU >= 0 && EncodedU < NumTexCoordValuesU);
			check(EncodedV >= 0 && EncodedV < NumTexCoordValuesV);
			PackedUVs[NumClusterVerts * UVIndex + Index].X = (int32)EncodedU;
			PackedUVs[NumClusterVerts * UVIndex + Index].Y = (int32)EncodedV;
		}
	}

	auto WriteZigZagDelta = [&LowByteStream, &MidByteStream, &HighByteStream](const int32 Delta, const uint32 NumBytes) {
		const uint32 Value = EncodeZigZag(Delta);
		checkSlow(DecodeZigZag(Value) == Delta);
		
		checkSlow(NumBytes <= 3);
		checkSlow(Value < (1u << (NumBytes*8)));

		if (NumBytes >= 3)
		{
			HighByteStream.Add((Value >> 16) & 0xFFu);
		}

		if (NumBytes >= 2)
		{
			MidByteStream.Add((Value >> 8) & 0xFFu);
		}

		if (NumBytes >= 1)
		{
			LowByteStream.Add(Value & 0xFFu);
		}
	};

	const uint32 NumUniqueToVertices = UniqueToVertexIndex.Num();

	const uint32 BytesPerPositionComponent = (FMath::Max3(Cluster.QuantizedPosBits.X, Cluster.QuantizedPosBits.Y, Cluster.QuantizedPosBits.Z) + 7) / 8;
	const uint32 BytesPerNormalComponent = (EncodingInfo.NormalPrecision + 7) / 8;
	const uint32 BytesPerTangentComponent = (EncodingInfo.TangentPrecision + 1 + 7) / 8;

	FIntVector PrevPosition = FIntVector((1 << Cluster.QuantizedPosBits.X) >> 1, (1 << Cluster.QuantizedPosBits.Y) >> 1, (1 << Cluster.QuantizedPosBits.Z) >> 1);

	// Position
	for (uint32 LocalVertexIndex = 0; LocalVertexIndex < NumUniqueToVertices; LocalVertexIndex++)
	{
		const uint32 VertexIndex = UniqueToVertexIndex[LocalVertexIndex];

		const FIntVector& Position = Cluster.QuantizedPositions[VertexIndex];
		FIntVector PositionDelta = Position - PrevPosition;

		PositionDelta.X = ShortestWrap(PositionDelta.X, Cluster.QuantizedPosBits.X);
		PositionDelta.Y = ShortestWrap(PositionDelta.Y, Cluster.QuantizedPosBits.Y);
		PositionDelta.Z = ShortestWrap(PositionDelta.Z, Cluster.QuantizedPosBits.Z);

		WriteZigZagDelta(PositionDelta.X, BytesPerPositionComponent);
		WriteZigZagDelta(PositionDelta.Y, BytesPerPositionComponent);
		WriteZigZagDelta(PositionDelta.Z, BytesPerPositionComponent);
		PrevPosition = Position;
	}

	FIntPoint PrevNormal = FIntPoint::ZeroValue;

	uint32 PackedNormals[NANITE_MAX_CLUSTER_VERTICES];

	// Normal
	for (uint32 LocalVertexIndex = 0; LocalVertexIndex < NumUniqueToVertices; LocalVertexIndex++)
	{
		const uint32 VertexIndex = UniqueToVertexIndex[LocalVertexIndex];

		const uint32 PackedNormal = PackNormal(Cluster.GetNormal(VertexIndex), EncodingInfo.NormalPrecision);
		const FIntPoint Normal = FIntPoint(PackedNormal & ((1u << EncodingInfo.NormalPrecision) - 1u), PackedNormal >> EncodingInfo.NormalPrecision);
		PackedNormals[LocalVertexIndex] = PackedNormal;
			
		FIntPoint NormalDelta = Normal - PrevNormal;
		NormalDelta.X = ShortestWrap(NormalDelta.X, EncodingInfo.NormalPrecision);
		NormalDelta.Y = ShortestWrap(NormalDelta.Y, EncodingInfo.NormalPrecision);
		PrevNormal = Normal;

		WriteZigZagDelta(NormalDelta.X, BytesPerNormalComponent);
		WriteZigZagDelta(NormalDelta.Y, BytesPerNormalComponent);
	}


	// Tangent
	if (bHasTangents)
	{
		uint32 PrevTangentBits = 0u;
		for (uint32 LocalVertexIndex = 0; LocalVertexIndex < NumUniqueToVertices; LocalVertexIndex++)
		{
			const uint32 VertexIndex = UniqueToVertexIndex[LocalVertexIndex];
			const uint32 PackedTangentZ = PackedNormals[LocalVertexIndex];

			FVector3f TangentX = Cluster.GetTangentX(VertexIndex);
			const FVector3f UnpackedTangentZ = UnpackNormal(PackedTangentZ, EncodingInfo.NormalPrecision);
			checkSlow(UnpackedTangentZ.IsNormalized());

			uint32 TangentBits = PrevTangentBits;	// HACK: If tangent space has collapsed, just repeat the tangent used by the previous vertex
			if(TangentX.SquaredLength() > 1e-8f)
			{
				TangentX = TangentX.GetUnsafeNormal();
				
				const bool bTangentYSign = Cluster.GetTangentYSign(VertexIndex) < 0.0f;
				uint32 QuantizedTangentAngle;
				if (PackTangent(QuantizedTangentAngle, TangentX, UnpackedTangentZ, EncodingInfo.TangentPrecision))
				{
					TangentBits = (bTangentYSign ? (1 << EncodingInfo.TangentPrecision) : 0) | QuantizedTangentAngle;
				}
			}
			
			const uint32 TangentDelta = ShortestWrap(TangentBits - PrevTangentBits, EncodingInfo.TangentPrecision + 1);
			WriteZigZagDelta(TangentDelta, BytesPerTangentComponent);
				
			PrevTangentBits = TangentBits;
		}
	}

	// Color
	if (EncodingInfo.ColorMode == NANITE_VERTEX_COLOR_MODE_VARIABLE)
	{
		FIntVector4 PrevColor = FIntVector4(0);
		for (uint32 LocalVertexIndex = 0; LocalVertexIndex < NumUniqueToVertices; LocalVertexIndex++)
		{
			const uint32 VertexIndex = UniqueToVertexIndex[LocalVertexIndex];
			const FColor Color = Cluster.GetColor(VertexIndex).ToFColor(false);
			const FIntVector4 ColorValue = FIntVector4(Color.R, Color.G, Color.B, Color.A) - EncodingInfo.ColorMin;
			FIntVector4 ColorDelta = ColorValue - PrevColor;

			ColorDelta.X = ShortestWrap(ColorDelta.X, EncodingInfo.ColorBits.X);
			ColorDelta.Y = ShortestWrap(ColorDelta.Y, EncodingInfo.ColorBits.Y);
			ColorDelta.Z = ShortestWrap(ColorDelta.Z, EncodingInfo.ColorBits.Z);
			ColorDelta.W = ShortestWrap(ColorDelta.W, EncodingInfo.ColorBits.W);

			WriteZigZagDelta(ColorDelta.X, 1);
			WriteZigZagDelta(ColorDelta.Y, 1);
			WriteZigZagDelta(ColorDelta.Z, 1);
			WriteZigZagDelta(ColorDelta.W, 1);

			PrevColor = ColorValue;
		}
	}
		
	// UV
	for (uint32 TexCoordIndex = 0; TexCoordIndex < NumTexCoords; TexCoordIndex++)
	{
		const int32 NumTexCoordBitsU = EncodingInfo.UVRanges[TexCoordIndex].NumBits.X;
		const int32 NumTexCoordBitsV = EncodingInfo.UVRanges[TexCoordIndex].NumBits.Y;
		const uint32 BytesPerTexCoordComponent = (FMath::Max(NumTexCoordBitsU, NumTexCoordBitsV) + 7) / 8;
			
		FIntVector2 PrevUV = FIntVector2::ZeroValue;
		for (uint32 LocalVertexIndex = 0; LocalVertexIndex < NumUniqueToVertices; LocalVertexIndex++)
		{
			const uint32 VertexIndex = UniqueToVertexIndex[LocalVertexIndex];
			const FIntVector2 UV = PackedUVs[NumClusterVerts * TexCoordIndex + VertexIndex];

			FIntVector2 UVDelta = UV - PrevUV;
			UVDelta.X = ShortestWrap(UVDelta.X, NumTexCoordBitsU);
			UVDelta.Y = ShortestWrap(UVDelta.Y, NumTexCoordBitsV);
			WriteZigZagDelta(UVDelta.X, BytesPerTexCoordComponent);
			WriteZigZagDelta(UVDelta.Y, BytesPerTexCoordComponent);
			PrevUV = UV;
		}
	}


#endif
}

// Generate a permutation of cluster groups that is sorted first by mip level and then by Morton order x, y and z.
// Sorting by mip level first ensure that there can be no cyclic dependencies between formed pages.
static TArray<uint32> CalculateClusterGroupPermutation( const TArray< FClusterGroup >& ClusterGroups )
{
	struct FClusterGroupSortEntry {
		int32	MipLevel;
		uint32	MortonXYZ;
		uint32	OldIndex;
	};

	uint32 NumClusterGroups = ClusterGroups.Num();
	TArray< FClusterGroupSortEntry > ClusterGroupSortEntries;
	ClusterGroupSortEntries.SetNumUninitialized( NumClusterGroups );

	FVector3f MinCenter = FVector3f( FLT_MAX, FLT_MAX, FLT_MAX );
	FVector3f MaxCenter = FVector3f( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	for( const FClusterGroup& ClusterGroup : ClusterGroups )
	{
		const FVector3f& Center = ClusterGroup.LODBounds.Center;
		MinCenter = FVector3f::Min( MinCenter, Center );
		MaxCenter = FVector3f::Max( MaxCenter, Center );
	}

	const float Scale = 1023.0f / (MaxCenter - MinCenter).GetMax();
	for( uint32 i = 0; i < NumClusterGroups; i++ )
	{
		const FClusterGroup& ClusterGroup = ClusterGroups[ i ];
		FClusterGroupSortEntry& SortEntry = ClusterGroupSortEntries[ i ];
		const FVector3f& Center = ClusterGroup.LODBounds.Center;
		const FVector3f ScaledCenter = ( Center - MinCenter ) * Scale + 0.5f;
		uint32 X = FMath::Clamp( (int32)ScaledCenter.X, 0, 1023 );
		uint32 Y = FMath::Clamp( (int32)ScaledCenter.Y, 0, 1023 );
		uint32 Z = FMath::Clamp( (int32)ScaledCenter.Z, 0, 1023 );

		SortEntry.MipLevel = ClusterGroup.MipLevel;
		SortEntry.MortonXYZ = ( FMath::MortonCode3(Z) << 2 ) | ( FMath::MortonCode3(Y) << 1 ) | FMath::MortonCode3(X);
		if ((ClusterGroup.MipLevel & 1) != 0)
		{
			SortEntry.MortonXYZ ^= 0xFFFFFFFFu;	// Alternate order so end of one level is near the beginning of the next
		}
		SortEntry.OldIndex = i;
	}

	ClusterGroupSortEntries.Sort( []( const FClusterGroupSortEntry& A, const FClusterGroupSortEntry& B ) {
		if( A.MipLevel != B.MipLevel )
			return A.MipLevel > B.MipLevel;
		return A.MortonXYZ < B.MortonXYZ;
	} );

	TArray<uint32> Permutation;
	Permutation.SetNumUninitialized( NumClusterGroups );
	for( uint32 i = 0; i < NumClusterGroups; i++ )
		Permutation[ i ] = ClusterGroupSortEntries[ i ].OldIndex;
	return Permutation;
}

static void SortGroupClusters(TArray<FClusterGroup>& ClusterGroups, const TArray<FCluster>& Clusters)
{
	for (FClusterGroup& Group : ClusterGroups)
	{
		FVector3f SortDirection = FVector3f(1.0f, 1.0f, 1.0f);
		Group.Children.Sort([&Clusters, SortDirection](uint32 ClusterIndexA, uint32 ClusterIndexB) {
			const FCluster& ClusterA = Clusters[ClusterIndexA];
			const FCluster& ClusterB = Clusters[ClusterIndexB];
			float DotA = FVector3f::DotProduct(ClusterA.SphereBounds.Center, SortDirection);
			float DotB = FVector3f::DotProduct(ClusterB.SphereBounds.Center, SortDirection);
			return DotA < DotB;
		});
	}
}

/*
	Build streaming pages
	Page layout:
		Fixup Chunk (Only loaded to CPU memory)
		FPackedCluster
		MaterialRangeTable
		GeometryData
*/

static void AssignClustersToPages(
	TArray< FClusterGroup >& ClusterGroups,
	TArray< FCluster >& Clusters,
	const TArray< FEncodingInfo >& EncodingInfos,
	TArray<FPage>& Pages,
	TArray<FClusterGroupPart>& Parts,
	const uint32 MaxRootPages
	)
{
	check(Pages.Num() == 0);
	check(Parts.Num() == 0);

	const uint32 NumClusterGroups = ClusterGroups.Num();
	Pages.AddDefaulted();

	SortGroupClusters(ClusterGroups, Clusters);
	TArray<uint32> ClusterGroupPermutation = CalculateClusterGroupPermutation(ClusterGroups);

	for (uint32 i = 0; i < NumClusterGroups; i++)
	{
		// Pick best next group			// TODO
		uint32 GroupIndex = ClusterGroupPermutation[i];
		FClusterGroup& Group = ClusterGroups[GroupIndex];
		if( Group.bTrimmed )
			continue;

		uint32 GroupStartPage = MAX_uint32;
	
		for (uint32 ClusterIndex : Group.Children)
		{
			// Pick best next cluster		// TODO
			FCluster& Cluster = Clusters[ClusterIndex];
			const FEncodingInfo& EncodingInfo = EncodingInfos[ClusterIndex];

			// Add to page
			FPage* Page = &Pages.Top();
			bool bRootPage =  (Pages.Num() - 1u) < MaxRootPages;
			if (Page->GpuSizes.GetTotal() + EncodingInfo.GpuSizes.GetTotal() > (bRootPage ? NANITE_ROOT_PAGE_GPU_SIZE : NANITE_STREAMING_PAGE_GPU_SIZE) ||
				Page->NumClusters + 1 > (bRootPage ? NANITE_ROOT_PAGE_MAX_CLUSTERS : NANITE_STREAMING_PAGE_MAX_CLUSTERS))
			{
				// Page is full. Need to start a new one
				Pages.AddDefaulted();
				Page = &Pages.Top();
			}
			
			// Start a new part?
			if (Page->PartsNum == 0 || Parts[Page->PartsStartIndex + Page->PartsNum - 1].GroupIndex != GroupIndex)
			{
				if (Page->PartsNum == 0)
				{
					Page->PartsStartIndex = Parts.Num();
				}
				Page->PartsNum++;

				FClusterGroupPart& Part = Parts.AddDefaulted_GetRef();
				Part.GroupIndex = GroupIndex;
			}

			// Add cluster to page
			uint32 PageIndex = Pages.Num() - 1;
			uint32 PartIndex = Parts.Num() - 1;

			FClusterGroupPart& Part = Parts.Last();
			if (Part.Clusters.Num() == 0)
			{
				Part.PageClusterOffset = Page->NumClusters;
				Part.PageIndex = PageIndex;
			}
			Part.Clusters.Add(ClusterIndex);
			check(Part.Clusters.Num() <= NANITE_MAX_CLUSTERS_PER_GROUP);

			Cluster.GroupPartIndex = PartIndex;
			
			if (GroupStartPage == MAX_uint32)
			{
				GroupStartPage = PageIndex;
			}
			
			Page->GpuSizes += EncodingInfo.GpuSizes;
			Page->NumClusters++;
		}

		Group.PageIndexStart = GroupStartPage;
		Group.PageIndexNum = Pages.Num() - GroupStartPage;
		check(Group.PageIndexNum >= 1);
		check(Group.PageIndexNum <= NANITE_MAX_GROUP_PARTS_MASK);
	}

	// Recalculate bounds for group parts
	for (FClusterGroupPart& Part : Parts)
	{
		check(Part.Clusters.Num() <= NANITE_MAX_CLUSTERS_PER_GROUP);
		check(Part.PageIndex < (uint32)Pages.Num());

		FBounds3f Bounds;
		for (uint32 ClusterIndex : Part.Clusters)
		{
			Bounds += Clusters[ClusterIndex].Bounds;
		}
		Part.Bounds = Bounds;
	}
}

class FPageWriter
{
	TArray<uint8>& Bytes;
public:	
	FPageWriter(TArray<uint8>& InBytes) :
		Bytes(InBytes)
	{
	}

	template<typename T>
	T* Append_Ptr(uint32 Num)
	{
		const uint32 SizeBefore = (uint32)Bytes.Num();
		Bytes.AddZeroed(Num * sizeof(T));
		return (T*)(Bytes.GetData() + SizeBefore);
	}
	
	template<typename T>
	uint32 Append_Offset(uint32 Num)
	{
		const uint32 SizeBefore = (uint32)Bytes.Num();
		Bytes.AddZeroed(Num * sizeof(T));
		return SizeBefore;
	}

	uint32 Offset() const
	{
		return (uint32)Bytes.Num();
	}

	void AlignRelativeToOffset(uint32 StartOffset, uint32 Alignment)
	{
		check(Offset() >= StartOffset);
		const uint32 Remainder = (Offset() - StartOffset) % Alignment;
		if (Remainder != 0)
		{
			Bytes.AddZeroed(Alignment - Remainder);
		}
	}

	void Align(uint32 Alignment)
	{
		AlignRelativeToOffset(0u, Alignment);	
	}
};

static uint32 MarkRelativeEncodingPagesRecursive(TArray<FPage>& Pages, TArray<uint32>& PageDependentsDepth, const TArray<TArray<uint32>>& PageDependents, uint32 PageIndex)
{
	if (PageDependentsDepth[PageIndex] != MAX_uint32)
	{
		return PageDependentsDepth[PageIndex];
	}

	uint32 Depth = 0;
	for (const uint32 DependentPageIndex : PageDependents[PageIndex])
	{
		const uint32 DependentDepth = MarkRelativeEncodingPagesRecursive(Pages, PageDependentsDepth, PageDependents, DependentPageIndex);
		Depth = FMath::Max(Depth, DependentDepth + 1u);
	}

	FPage& Page = Pages[PageIndex];
	Page.bRelativeEncoding = true;

	if (Depth >= MAX_DEPENDENCY_CHAIN_FOR_RELATIVE_ENCODING)
	{
		// Using relative encoding for this page would make the dependency chain too long. Use direct coding instead and reset depth.
		Page.bRelativeEncoding = false;
		Depth = 0;
	}
	
	PageDependentsDepth[PageIndex] = Depth;
	return Depth;
}

static uint32 MarkRelativeEncodingPages(const FResources& Resources, TArray<FPage>& Pages, const TArray<FClusterGroup>& Groups, const TArray<FClusterGroupPart>& Parts)
{
	const uint32 NumPages = Resources.PageStreamingStates.Num();

	// Build list of dependents for each page
	TArray<TArray<uint32>> PageDependents;
	PageDependents.SetNum(NumPages);

	// Memorize how many levels of dependency a given page has
	TArray<uint32> PageDependentsDepth;
	PageDependentsDepth.Init(MAX_uint32, NumPages);

	TBitArray<> PageHasOnlyRootDependencies(false, NumPages);

	for (uint32 PageIndex = 0; PageIndex < NumPages; PageIndex++)
	{
		const FPageStreamingState& PageStreamingState = Resources.PageStreamingStates[PageIndex];

		bool bHasRootDependency = false;
		bool bHasStreamingDependency = false;
		for (uint32 i = 0; i < PageStreamingState.DependenciesNum; i++)
		{
			const uint32 DependencyPageIndex = Resources.PageDependencies[PageStreamingState.DependenciesStart + i];
			if (Resources.IsRootPage(DependencyPageIndex))
			{
				bHasRootDependency = true;
			}
			else
			{
				PageDependents[DependencyPageIndex].AddUnique(PageIndex);
				bHasStreamingDependency = true;
			}
		}

		PageHasOnlyRootDependencies[PageIndex] = (bHasRootDependency && !bHasStreamingDependency);
	}

	uint32 NumRelativeEncodingPages = 0;
	for (uint32 PageIndex = 0; PageIndex < NumPages; PageIndex++)
	{
		FPage& Page = Pages[PageIndex];

		MarkRelativeEncodingPagesRecursive(Pages, PageDependentsDepth, PageDependents, PageIndex);
		
		if (Resources.IsRootPage(PageIndex))
		{
			// Root pages never use relative encoding
			Page.bRelativeEncoding = false;
		}
		else if (PageHasOnlyRootDependencies[PageIndex])
		{
			// Root pages are always resident, so dependencies on them shouldn't count towards dependency chain limit.
			// If a page only has root dependencies, always code it as relative.
			Page.bRelativeEncoding = true;
		}

		if (Page.bRelativeEncoding)
		{
			NumRelativeEncodingPages++;
		}
	}

	return NumRelativeEncodingPages;
}

static TArray<TMap<FVariableVertex, FVertexMapEntry>> BuildVertexMaps(const TArray<FPage>& Pages, const TArray<FCluster>& Clusters, const TArray<FClusterGroupPart>& Parts)
{
	TArray<TMap<FVariableVertex, FVertexMapEntry>> VertexMaps;
	VertexMaps.SetNum(Pages.Num());

	ParallelFor( TEXT("NaniteEncode.BuildVertexMaps.PF"), Pages.Num(), 1, [&VertexMaps, &Pages, &Clusters, &Parts](int32 PageIndex)
	{
		const FPage& Page = Pages[PageIndex];
		for (uint32 i = 0; i < Page.PartsNum; i++)
		{
			const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + i];
			for (uint32 j = 0; j < (uint32)Part.Clusters.Num(); j++)
			{
				const uint32 ClusterIndex = Part.Clusters[j];
				const uint32 LocalClusterIndex = Part.PageClusterOffset + j;
				const FCluster& Cluster = Clusters[ClusterIndex];

				for (uint32 VertexIndex = 0; VertexIndex < Cluster.NumVerts; VertexIndex++)
				{
					FVariableVertex Vertex;
					Vertex.Data = &Cluster.Verts[VertexIndex * Cluster.GetVertSize()];
					Vertex.SizeInBytes = Cluster.GetVertSize() * sizeof(float);
					FVertexMapEntry Entry;
					Entry.LocalClusterIndex = LocalClusterIndex;
					Entry.VertexIndex = VertexIndex;
					VertexMaps[PageIndex].Add(Vertex, Entry);
				}
			}
		}
	});
	return VertexMaps;
}

static void WritePages(	FResources& Resources,
						TArray<FPage>& Pages,
						const TArray<FClusterGroup>& Groups,
						const TArray<FClusterGroupPart>& Parts,
						TArray<FCluster>& Clusters,
						const TArray<FEncodingInfo>& EncodingInfos,
						const bool bHasTangents,
						const uint32 NumTexCoords,
						uint32* OutTotalGPUSize)
{
	check(Resources.PageStreamingStates.Num() == 0);

	TArray< uint8 > StreamableBulkData;
	
	const uint32 NumPages = Pages.Num();
	Resources.PageStreamingStates.SetNum(NumPages);

	uint32 NumReferencedClusters = 0;
	TArray<FFixupChunk> FixupChunks;
	FixupChunks.SetNum(NumPages);
	for (uint32 PageIndex = 0; PageIndex < NumPages; PageIndex++)
	{
		const FPage& Page = Pages[PageIndex];
		NumReferencedClusters += Page.NumClusters;

		FFixupChunk& FixupChunk = FixupChunks[PageIndex];
		FixupChunk.Header.Magic = NANITE_FIXUP_MAGIC;	
		FixupChunk.Header.NumClusters = uint16(Page.NumClusters);

		uint32 NumHierarchyFixups = 0;
		for (uint32 i = 0; i < Page.PartsNum; i++)
		{
			const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + i];
			NumHierarchyFixups += Groups[Part.GroupIndex].PageIndexNum;
		}

		FixupChunk.Header.NumHierachyFixups = uint16(NumHierarchyFixups);	// NumHierarchyFixups must be set before writing cluster fixups
	}

	check(NumReferencedClusters <= (uint32)Clusters.Num());	// There can be unused clusters when trim is used
	Resources.NumClusters = NumReferencedClusters;

	// Add external fixups to pages
	for (const FClusterGroupPart& Part : Parts)
	{
		check(Part.PageIndex < NumPages);

		const FClusterGroup& Group = Groups[Part.GroupIndex];
		check(!Group.bTrimmed);
		for (uint32 ClusterPositionInPart = 0; ClusterPositionInPart < (uint32)Part.Clusters.Num(); ClusterPositionInPart++)
		{
			const FCluster& Cluster = Clusters[Part.Clusters[ClusterPositionInPart]];
			if (Cluster.GeneratingGroupIndex != MAX_uint32)
			{
				const FClusterGroup& GeneratingGroup = Groups[Cluster.GeneratingGroupIndex];
				check(!GeneratingGroup.bTrimmed);
				check(GeneratingGroup.PageIndexNum >= 1);
				
				uint32 PageDependencyStart = GeneratingGroup.PageIndexStart;
				uint32 PageDependencyNum = GeneratingGroup.PageIndexNum;
				RemoveRootPagesFromRange(PageDependencyStart, PageDependencyNum, Resources.NumRootPages);
				RemovePageFromRange(PageDependencyStart, PageDependencyNum, Part.PageIndex);
				
				if (PageDependencyNum == 0)
					continue;	// Dependencies already met by current page and/or root pages
					
				const FClusterFixup ClusterFixup = FClusterFixup(Part.PageIndex, Part.PageClusterOffset + ClusterPositionInPart, PageDependencyStart, PageDependencyNum);
				for (uint32 i = 0; i < GeneratingGroup.PageIndexNum; i++)
				{
					//TODO: Implement some sort of FFixupPart to not redundantly store PageIndexStart/PageIndexNum?
					FFixupChunk& FixupChunk = FixupChunks[GeneratingGroup.PageIndexStart + i];
					FixupChunk.GetClusterFixup(FixupChunk.Header.NumClusterFixups++) = ClusterFixup;
				}
			}
		}
	}

	// Generate page dependencies
	for (uint32 PageIndex = 0; PageIndex < NumPages; PageIndex++)
	{
		const FFixupChunk& FixupChunk = FixupChunks[PageIndex];
		FPageStreamingState& PageStreamingState = Resources.PageStreamingStates[PageIndex];
		PageStreamingState.DependenciesStart = Resources.PageDependencies.Num();
		PageStreamingState.MaxHierarchyDepth = uint8(Pages[PageIndex].MaxHierarchyDepth);

		for (uint32 i = 0; i < FixupChunk.Header.NumClusterFixups; i++)
		{
			uint32 FixupPageIndex = FixupChunk.GetClusterFixup(i).GetPageIndex();
			check(FixupPageIndex < NumPages);
			if (FixupPageIndex == PageIndex)	// Never emit dependencies to ourselves
				continue;

			// Only add if not already in the set.
			// O(n^2), but number of dependencies should be tiny in practice.
			bool bFound = false;
			for (uint32 j = PageStreamingState.DependenciesStart; j < (uint32)Resources.PageDependencies.Num(); j++)
			{
				if (Resources.PageDependencies[j] == FixupPageIndex)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
				continue;

			Resources.PageDependencies.Add(FixupPageIndex);
		}
		PageStreamingState.DependenciesNum = uint16(Resources.PageDependencies.Num() - PageStreamingState.DependenciesStart);
	}

	auto PageVertexMaps = BuildVertexMaps(Pages, Clusters, Parts);

	const uint32 NumRelativeEncodingPages = MarkRelativeEncodingPages(Resources, Pages, Groups, Parts);
	
	// Process pages
	TArray< TArray<uint8> > PageResults;
	PageResults.SetNum(NumPages);

	ParallelFor(TEXT("NaniteEncode.BuildPages.PF"), NumPages, 1, [&Resources, &Pages, &Groups, &Parts, &Clusters, &EncodingInfos, &FixupChunks, &PageVertexMaps, &PageResults, bHasTangents, NumTexCoords](int32 PageIndex)
	{
		const FPage& Page = Pages[PageIndex];
		FFixupChunk& FixupChunk = FixupChunks[PageIndex];

		Resources.PageStreamingStates[PageIndex].Flags = Page.bRelativeEncoding ? NANITE_PAGE_FLAG_RELATIVE_ENCODING : 0;

		// Add hierarchy fixups
		{
			// Parts include the hierarchy fixups for all the other parts of the same group.
			uint32 NumHierarchyFixups = 0;
			for (uint32 i = 0; i < Page.PartsNum; i++)
			{
				const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + i];
				const FClusterGroup& Group = Groups[Part.GroupIndex];
				const uint32 HierarchyRootOffset = Resources.HierarchyRootOffsets[Group.MeshIndex];

				uint32 PageDependencyStart = Group.PageIndexStart;
				uint32 PageDependencyNum = Group.PageIndexNum;
				RemoveRootPagesFromRange(PageDependencyStart, PageDependencyNum, Resources.NumRootPages);

				// Add fixups to all parts of the group
				for (uint32 j = 0; j < Group.PageIndexNum; j++)
				{
					const FPage& Page2 = Pages[Group.PageIndexStart + j];
					for (uint32 k = 0; k < Page2.PartsNum; k++)
					{
						const FClusterGroupPart& Part2 = Parts[Page2.PartsStartIndex + k];
						if (Part2.GroupIndex == Part.GroupIndex)
						{
							const uint32 GlobalHierarchyNodeIndex = HierarchyRootOffset + Part2.HierarchyNodeIndex;
							FixupChunk.GetHierarchyFixup(NumHierarchyFixups++) = FHierarchyFixup(Part2.PageIndex, GlobalHierarchyNodeIndex, Part2.HierarchyChildIndex, Part2.PageClusterOffset, PageDependencyStart, PageDependencyNum);
							break;
						}
					}
				}
			}
			check(NumHierarchyFixups == FixupChunk.Header.NumHierachyFixups);
		}

		// Pack clusters and generate material range data
		TArray<uint32>				CombinedStripBitmaskData;
		TArray<uint32>				CombinedPageClusterPairData;
		TArray<uint32>				CombinedVertexRefBitmaskData;
		TArray<uint16>				CombinedVertexRefData;
		TArray<uint8>				CombinedIndexData;
		TArray<uint8>				CombinedAttributeData;
		TArray<uint32>				MaterialRangeData;
		TArray<uint32>				VertReuseBatchInfo;
		TArray<uint16>				CodedVerticesPerCluster;
		TArray<uint32>				NumPageClusterPairsPerCluster;
		TArray<FPackedCluster>		PackedClusters;

		TArray<uint8>				LowByteStream;
		TArray<uint8>				MidByteStream;
		TArray<uint8>				HighByteStream;

		struct FByteStreamCounters
		{
			uint32 Low = 0;
			uint32 Mid = 0;
			uint32 High = 0;
		};

		TArray<FByteStreamCounters> ByteStreamCounters;
		ByteStreamCounters.SetNumUninitialized(Page.NumClusters);

		PackedClusters.SetNumUninitialized(Page.NumClusters);
		CodedVerticesPerCluster.SetNumUninitialized(Page.NumClusters);
		NumPageClusterPairsPerCluster.SetNumUninitialized(Page.NumClusters);
		
		const uint32 NumPackedClusterDwords = Page.NumClusters * sizeof(FPackedCluster) / sizeof(uint32);
		const uint32 MaterialTableStartOffsetInDwords = (NANITE_GPU_PAGE_HEADER_SIZE / 4) + NumPackedClusterDwords;

		FPageSections GpuSectionOffsets = Page.GpuSizes.GetOffsets();
		TMap<FVariableVertex, uint32> UniqueVertices;

		for (uint32 i = 0; i < Page.PartsNum; i++)
		{
			const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + i];
			for (uint32 j = 0; j < (uint32)Part.Clusters.Num(); j++)
			{
				const uint32 ClusterIndex = Part.Clusters[j];
				const FCluster& Cluster = Clusters[ClusterIndex];
				const FEncodingInfo& EncodingInfo = EncodingInfos[ClusterIndex];

				const uint32 LocalClusterIndex = Part.PageClusterOffset + j;
				FPackedCluster& PackedCluster = PackedClusters[LocalClusterIndex];
				PackCluster(PackedCluster, Cluster, EncodingInfos[ClusterIndex], bHasTangents, NumTexCoords);

				TArray<uint32> LocalVertReuseBatchInfo;
				PackedCluster.PackedMaterialInfo = PackMaterialInfo(Cluster, MaterialRangeData, LocalVertReuseBatchInfo, MaterialTableStartOffsetInDwords);
				check((GpuSectionOffsets.Index & 3) == 0);
				check((GpuSectionOffsets.Position & 3) == 0);
				check((GpuSectionOffsets.Attribute & 3) == 0);
				PackedCluster.SetIndexOffset(GpuSectionOffsets.Index);
				PackedCluster.SetPositionOffset(GpuSectionOffsets.Position);
				PackedCluster.SetAttributeOffset(GpuSectionOffsets.Attribute);
				PackedCluster.SetDecodeInfoOffset(GpuSectionOffsets.DecodeInfo);

				PackedCluster.SetVertResourceBatchInfo(LocalVertReuseBatchInfo, GpuSectionOffsets.VertReuseBatchInfo, Cluster.MaterialRanges.Num());
				if (Cluster.MaterialRanges.Num() > 3)
				{
					VertReuseBatchInfo.Append(MoveTemp(LocalVertReuseBatchInfo));
				}
				
				GpuSectionOffsets += EncodingInfo.GpuSizes;

				const uint32 PrevLow = LowByteStream.Num();
				const uint32 PrevMid = MidByteStream.Num();
				const uint32 PrevHigh = HighByteStream.Num();

				const FPageStreamingState& PageStreamingState = Resources.PageStreamingStates[PageIndex];
				const uint32 DependenciesNum = (PageStreamingState.Flags & NANITE_PAGE_FLAG_RELATIVE_ENCODING) ? PageStreamingState.DependenciesNum : 0u;
				const TArrayView<uint32> PageDependencies = TArrayView<uint32>(Resources.PageDependencies.GetData() + PageStreamingState.DependenciesStart, DependenciesNum);
				const uint32 PrevPageClusterPairs = CombinedPageClusterPairData.Num();
				uint32 NumCodedVertices = 0;
				EncodeGeometryData(	LocalClusterIndex, Cluster, EncodingInfo, NumTexCoords, 
									CombinedStripBitmaskData, CombinedIndexData,
									CombinedPageClusterPairData, CombinedVertexRefBitmaskData, CombinedVertexRefData,
									LowByteStream, MidByteStream, HighByteStream,
									PageDependencies, PageVertexMaps,
									UniqueVertices, NumCodedVertices, bHasTangents);

				ByteStreamCounters[LocalClusterIndex].Low	= LowByteStream.Num() - PrevLow;
				ByteStreamCounters[LocalClusterIndex].Mid	= MidByteStream.Num() - PrevMid;
				ByteStreamCounters[LocalClusterIndex].High	= HighByteStream.Num() - PrevHigh;

				NumPageClusterPairsPerCluster[LocalClusterIndex] = CombinedPageClusterPairData.Num() - PrevPageClusterPairs;
				CodedVerticesPerCluster[LocalClusterIndex] = uint16(NumCodedVertices);
			}
		}
		check(GpuSectionOffsets.Cluster							== Page.GpuSizes.GetMaterialTableOffset());
		check(Align(GpuSectionOffsets.MaterialTable, 16)		== Page.GpuSizes.GetVertReuseBatchInfoOffset());
		check(Align(GpuSectionOffsets.VertReuseBatchInfo, 16)	== Page.GpuSizes.GetDecodeInfoOffset());
		check(Align(GpuSectionOffsets.DecodeInfo, 16)			== Page.GpuSizes.GetIndexOffset());
		check(GpuSectionOffsets.Index							== Page.GpuSizes.GetPositionOffset());
		check(GpuSectionOffsets.Position						== Page.GpuSizes.GetAttributeOffset());
		check(GpuSectionOffsets.Attribute						== Page.GpuSizes.GetTotal());

		// Dword align index data
		CombinedIndexData.SetNumZeroed((CombinedIndexData.Num() + 3) & -4);

		// Perform page-internal fix up directly on PackedClusters
		for (uint32 LocalPartIndex = 0; LocalPartIndex < Page.PartsNum; LocalPartIndex++)
		{
			const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + LocalPartIndex];
			const FClusterGroup& Group = Groups[Part.GroupIndex];
			
			bool bRootGroup = false;
			{
				uint32 PageDependencyStart = Group.PageIndexStart;
				uint32 PageDependencyNum = Group.PageIndexNum;
				RemoveRootPagesFromRange(PageDependencyStart, PageDependencyNum, Resources.NumRootPages);
				bRootGroup = (PageDependencyNum == 0);
			}
			
			for (uint32 ClusterPositionInPart = 0; ClusterPositionInPart < (uint32)Part.Clusters.Num(); ClusterPositionInPart++)
			{
				const FCluster& Cluster = Clusters[Part.Clusters[ClusterPositionInPart]];
				uint32& ClusterFlags = PackedClusters[Part.PageClusterOffset + ClusterPositionInPart].Flags;

				if (bRootGroup)
				{
					ClusterFlags |= NANITE_CLUSTER_FLAG_ROOT_GROUP;
				}

				if (Cluster.GeneratingGroupIndex != MAX_uint32)
				{
					const FClusterGroup& GeneratingGroup = Groups[Cluster.GeneratingGroupIndex];
					uint32 PageDependencyStart = GeneratingGroup.PageIndexStart;
					uint32 PageDependencyNum = GeneratingGroup.PageIndexNum;
					RemoveRootPagesFromRange(PageDependencyStart, PageDependencyNum, Resources.NumRootPages);
					if (PageDependencyNum == 0)
					{
						// Dependencies met by root pages
						ClusterFlags &= ~NANITE_CLUSTER_FLAG_ROOT_LEAF;
					}

					RemovePageFromRange(PageDependencyStart, PageDependencyNum, PageIndex);
					
					if (PageDependencyNum == 0)
					{
						// Dependencies met by current page and/or root pages
						ClusterFlags &= ~NANITE_CLUSTER_FLAG_STREAMING_LEAF;
					}
				}
				else
				{
					ClusterFlags |= NANITE_CLUSTER_FLAG_FULL_LEAF;
				}
			}
		}

		// Begin page
		TArray<uint8>& PageResult = PageResults[PageIndex];
		PageResult.Reset(NANITE_ESTIMATED_MAX_PAGE_DISK_SIZE);

		FPageWriter PageWriter(PageResult);

		// Disk header
		const uint32 PageDiskHeaderOffset = PageWriter.Append_Offset<FPageDiskHeader>(1);

		// 16-byte align material range data to make it easy to copy during GPU transcoding
		MaterialRangeData.SetNum(Align(MaterialRangeData.Num(), 4));
		VertReuseBatchInfo.SetNum(Align(VertReuseBatchInfo.Num(), 4));

		static_assert(sizeof(FPageGPUHeader) % 16 == 0, "sizeof(FGPUPageHeader) must be a multiple of 16");
		static_assert(sizeof(FPackedCluster) % 16 == 0, "sizeof(FPackedCluster) must be a multiple of 16");
		
		// Cluster headers
		const uint32 ClusterDiskHeadersOffset = PageWriter.Append_Offset<FClusterDiskHeader>(Page.NumClusters);
		TArray<FClusterDiskHeader> ClusterDiskHeaders;
		ClusterDiskHeaders.SetNum(Page.NumClusters);

		const uint32 RawFloat4StartOffset = PageWriter.Offset();
		{
			// GPU page header
			FPageGPUHeader& GPUPageHeader = *PageWriter.Append_Ptr<FPageGPUHeader>(1);
			GPUPageHeader = FPageGPUHeader{};
			GPUPageHeader.NumClusters = Page.NumClusters;
		}

		// Write clusters in SOA layout
		{
			const uint32 NumClusterFloat4Propeties = sizeof(FPackedCluster) / 16;
			uint8* Dst = PageWriter.Append_Ptr<uint8>(NumClusterFloat4Propeties * 16 * PackedClusters.Num());
			for (uint32 float4Index = 0; float4Index < NumClusterFloat4Propeties; float4Index++)
			{
				for (const FPackedCluster& PackedCluster : PackedClusters)
				{
					FMemory::Memcpy(Dst, (uint8*)&PackedCluster + float4Index * 16, 16);
					Dst += 16;
				}
			}
		}
		
		{
			// Material table
			uint32 MaterialTableSize = MaterialRangeData.Num() * MaterialRangeData.GetTypeSize();
			uint8* MaterialTable = PageWriter.Append_Ptr<uint8>(MaterialTableSize);
			FMemory::Memcpy(MaterialTable, MaterialRangeData.GetData(), MaterialTableSize);
			check(MaterialTableSize == Page.GpuSizes.GetMaterialTableSize());
		}

		{
			// Vert reuse batch info
			const uint32 VertReuseBatchInfoSize = VertReuseBatchInfo.Num() * VertReuseBatchInfo.GetTypeSize();
			uint8* VertReuseBatchInfoData = PageWriter.Append_Ptr<uint8>(VertReuseBatchInfoSize);
			FMemory::Memcpy(VertReuseBatchInfoData, VertReuseBatchInfo.GetData(), VertReuseBatchInfoSize);
			check(VertReuseBatchInfoSize == Page.GpuSizes.GetVertReuseBatchInfoSize());
		}

		// Decode information
		const uint32 DecodeInfoOffset = PageWriter.Offset();
		for (uint32 i = 0; i < Page.PartsNum; i++)
		{
			const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + i];
			FPackedUVRange* DecodeInfo = PageWriter.Append_Ptr<FPackedUVRange>(Part.Clusters.Num() * NumTexCoords);
			for (uint32 j = 0; j < (uint32)Part.Clusters.Num(); j++)
			{
				const uint32 ClusterIndex = Part.Clusters[j];
				for (uint32 k = 0; k < NumTexCoords; k++)
				{
					PackUVRange(DecodeInfo[k], EncodingInfos[ClusterIndex].UVRanges[k]);
				}
				DecodeInfo += NumTexCoords;
			}
		}
		PageWriter.AlignRelativeToOffset(DecodeInfoOffset, 16u);

		const uint32 RawFloat4EndOffset = PageWriter.Offset();
		
		uint32 StripBitmaskOffset = 0u;
		// Index data
		{
			const uint32 StartOffset = PageWriter.Offset();
			uint32 NextOffset = StartOffset;
#if NANITE_USE_STRIP_INDICES
			for (uint32 i = 0; i < Page.PartsNum; i++)
			{
				const FClusterGroupPart& Part = Parts[Page.PartsStartIndex + i];
				for (uint32 j = 0; j < (uint32)Part.Clusters.Num(); j++)
				{
					const uint32 LocalClusterIndex = Part.PageClusterOffset + j;
					const uint32 ClusterIndex = Part.Clusters[j];
					const FCluster& Cluster = Clusters[ClusterIndex];

					FClusterDiskHeader& ClusterDiskHeader = ClusterDiskHeaders[LocalClusterIndex];
					ClusterDiskHeader.IndexDataOffset = NextOffset;
					ClusterDiskHeader.NumPrevNewVerticesBeforeDwords = Cluster.StripDesc.NumPrevNewVerticesBeforeDwords;
					ClusterDiskHeader.NumPrevRefVerticesBeforeDwords = Cluster.StripDesc.NumPrevRefVerticesBeforeDwords;
					
					NextOffset += Cluster.StripIndexData.Num();
				}
			}

			const uint32 Size = NextOffset - StartOffset;
			uint8* IndexDataPtr = PageWriter.Append_Ptr<uint8>(Size);
			FMemory::Memcpy(IndexDataPtr, CombinedIndexData.GetData(), Size);
			PageWriter.Align(sizeof(uint32));

			StripBitmaskOffset = PageWriter.Offset();

			{
				uint32 StripBitmaskDataSize = CombinedStripBitmaskData.Num() * CombinedStripBitmaskData.GetTypeSize();
				uint8* StripBitmaskData = PageWriter.Append_Ptr<uint8>(StripBitmaskDataSize);
				FMemory::Memcpy(StripBitmaskData, CombinedStripBitmaskData.GetData(), StripBitmaskDataSize);
			}
			
#else
			for (uint32 i = 0; i < Page.NumClusters; i++)
			{
				ClusterDiskHeaders[i].IndexDataOffset = NextOffset;
				NextOffset += PackedClusters[i].GetNumTris() * 3;
			}
			PageWriter.Align(sizeof(uint32));

			const uint32 Size = NextOffset - StartOffset;
			check(Size == CombinedIndexData.Num() * CombinedIndexData.GetTypeSize());
			uint8* IndexDataPtr = PageWriter.Append_Ptr<uint8>(Size);
			FMemory::Memcpy(IndexDataPtr, CombinedIndexData.GetData(), CombinedIndexData.Num() * CombinedIndexData.GetTypeSize());
#endif
		}

		// Write PageCluster Map
		{
			const uint32 StartOffset = PageWriter.Offset();
			uint32 NextOffset = StartOffset;
			for (uint32 i = 0; i < Page.NumClusters; i++)
			{
				ClusterDiskHeaders[i].PageClusterMapOffset = NextOffset;
				NextOffset += NumPageClusterPairsPerCluster[i] * sizeof(uint32);
			}
			const uint32 Size = NextOffset - StartOffset;
			check(Size == CombinedPageClusterPairData.Num() * CombinedPageClusterPairData.GetTypeSize());
			check(Size % 4 == 0);
			uint32* PageClusterMapPtr = PageWriter.Append_Ptr<uint32>(Size / 4);
			FMemory::Memcpy(PageClusterMapPtr, CombinedPageClusterPairData.GetData(), CombinedPageClusterPairData.Num() * CombinedPageClusterPairData.GetTypeSize());
		}

		// Write Vertex Reference Bitmask
		const uint32 VertexRefBitmaskOffset = PageWriter.Offset();
		{
			const uint32 VertexRefBitmaskSize = Page.NumClusters * (NANITE_MAX_CLUSTER_VERTICES / 8);
			uint8* VertexRefBitmask = PageWriter.Append_Ptr<uint8>(VertexRefBitmaskSize);
			FMemory::Memcpy(VertexRefBitmask, CombinedVertexRefBitmaskData.GetData(), VertexRefBitmaskSize);
			check(CombinedVertexRefBitmaskData.Num() * CombinedVertexRefBitmaskData.GetTypeSize() == VertexRefBitmaskSize);
		}

		// Write Vertex References
		{
			const uint32 StartOffset = PageWriter.Offset();
			uint32 NextOffset = StartOffset;
			for (uint32 i = 0; i < Page.NumClusters; i++)
			{
				const uint32 NumVertexRefs = PackedClusters[i].GetNumVerts() - CodedVerticesPerCluster[i];
				ClusterDiskHeaders[i].VertexRefDataOffset	= NextOffset;
				ClusterDiskHeaders[i].NumVertexRefs			= NumVertexRefs;
				NextOffset += NumVertexRefs;
			}
			const uint32 Size = NextOffset - StartOffset;
			uint8* VertexRefs = PageWriter.Append_Ptr<uint8>(Size * 2); // * 2 to also allocate space for the high bytes that follow
			PageWriter.Align(sizeof(uint32));

			// Split low and high bytes for better compression
			for (int32 i = 0; i < CombinedVertexRefData.Num(); i++)
			{
				VertexRefs[i] = CombinedVertexRefData[i] >> 8;
				VertexRefs[i + CombinedVertexRefData.Num()] = CombinedVertexRefData[i] & 0xFF;
			}
		}
		
		// Write low/mid/high byte streams
		{
			const uint32 StartOffset = PageWriter.Offset();
			uint32 NextLowOffset = StartOffset;
			uint32 NextMidOffset = NextLowOffset + LowByteStream.Num();
			uint32 NextHighOffset = NextMidOffset + MidByteStream.Num();
			for (uint32 i = 0; i < Page.NumClusters; i++)
			{
				ClusterDiskHeaders[i].LowBytesOffset = NextLowOffset;
				ClusterDiskHeaders[i].MidBytesOffset = NextMidOffset;
				ClusterDiskHeaders[i].HighBytesOffset = NextHighOffset;
				NextLowOffset += ByteStreamCounters[i].Low;
				NextMidOffset += ByteStreamCounters[i].Mid;
				NextHighOffset += ByteStreamCounters[i].High;
			}

			const uint32 Size = NextHighOffset - StartOffset;
			check(Size == LowByteStream.Num() + MidByteStream.Num() + HighByteStream.Num());

			uint8* Ptr = PageWriter.Append_Ptr<uint8>(Size);
			FMemory::Memcpy(Ptr, LowByteStream.GetData(), LowByteStream.Num());
			Ptr += LowByteStream.Num();
			FMemory::Memcpy(Ptr, MidByteStream.GetData(), MidByteStream.Num());
			Ptr += MidByteStream.Num();
			FMemory::Memcpy(Ptr, HighByteStream.GetData(), HighByteStream.Num());
		}

		const uint32 NumRawFloat4Bytes = RawFloat4EndOffset - RawFloat4StartOffset;
		check((NumRawFloat4Bytes & 15u) == 0u);

		// Write page header
		{
			FPageDiskHeader PageDiskHeader;
			PageDiskHeader.NumClusters = Page.NumClusters;
			PageDiskHeader.NumRawFloat4s = NumRawFloat4Bytes / 16u;
			PageDiskHeader.NumVertexRefs = CombinedVertexRefData.Num();
			PageDiskHeader.DecodeInfoOffset = DecodeInfoOffset;
			PageDiskHeader.StripBitmaskOffset = StripBitmaskOffset;
			PageDiskHeader.VertexRefBitmaskOffset = VertexRefBitmaskOffset;
			FMemory::Memcpy(PageResult.GetData() + PageDiskHeaderOffset, &PageDiskHeader, sizeof(PageDiskHeader));
		}

		// Write cluster headers
		FMemory::Memcpy(PageResult.GetData() + ClusterDiskHeadersOffset, ClusterDiskHeaders.GetData(), ClusterDiskHeaders.Num()* ClusterDiskHeaders.GetTypeSize());

		PageWriter.Align(sizeof(uint32));
#if 0
		FILE* File = nullptr;
		char Filename[128];
		sprintf(Filename, "f:\\test\\newnew\\%d.dat", PageIndex);
		fopen_s(&File, Filename, "wb");
		fwrite(PageResult.GetData(), PageResult.Num(), 1, File);
		fclose(File);
#endif
	});

	// Write pages
	uint32 NumRootPages = 0;
	uint32 TotalRootGPUSize = 0;
	uint32 TotalRootDiskSize = 0;
	uint32 NumStreamingPages = 0;
	uint32 TotalStreamingGPUSize = 0;
	uint32 TotalStreamingDiskSize = 0;
	
	uint32 TotalFixupSize = 0;
	for (uint32 PageIndex = 0; PageIndex < NumPages; PageIndex++)
	{
		const FPage& Page = Pages[PageIndex];
		const bool bRootPage = Resources.IsRootPage(PageIndex);
		FFixupChunk& FixupChunk = FixupChunks[PageIndex];
		TArray<uint8>& BulkData = bRootPage ? Resources.RootData : StreamableBulkData;

		FPageStreamingState& PageStreamingState = Resources.PageStreamingStates[PageIndex];
		PageStreamingState.BulkOffset = BulkData.Num();

		// Write fixup chunk
		uint32 FixupChunkSize = FixupChunk.GetSize();
		check(FixupChunk.Header.NumHierachyFixups <= NANITE_MAX_CLUSTERS_PER_PAGE);
		check(FixupChunk.Header.NumClusterFixups <= NANITE_MAX_CLUSTERS_PER_PAGE);
		BulkData.Append((uint8*)&FixupChunk, FixupChunkSize);
		TotalFixupSize += FixupChunkSize;

		// Copy page to BulkData
		TArray<uint8>& PageData = PageResults[PageIndex];
		BulkData.Append(PageData.GetData(), PageData.Num());
		
		if (bRootPage)
		{
			TotalRootGPUSize += Page.GpuSizes.GetTotal();
			TotalRootDiskSize += PageData.Num();
			NumRootPages++;
		}
		else
		{
			TotalStreamingGPUSize += Page.GpuSizes.GetTotal();
			TotalStreamingDiskSize += PageData.Num();
			NumStreamingPages++;
		}

		PageStreamingState.BulkSize = BulkData.Num() - PageStreamingState.BulkOffset;
		PageStreamingState.PageSize = PageData.Num();
	}

	const uint32 TotalPageGPUSize = TotalRootGPUSize + TotalStreamingGPUSize;
	const uint32 TotalPageDiskSize = TotalRootDiskSize + TotalStreamingDiskSize;
	UE_LOG(LogStaticMesh, Log, TEXT("WritePages:"), NumPages);
	UE_LOG(LogStaticMesh, Log, TEXT("  Root: GPU size: %d bytes. %d Pages. %.3f bytes per page (%.3f%% utilization)."), TotalRootGPUSize, NumRootPages, (float)TotalRootGPUSize / (float)NumRootPages, (float)TotalRootGPUSize / (float(NumRootPages * NANITE_ROOT_PAGE_GPU_SIZE)) * 100.0f);
	if(NumStreamingPages > 0)
	{
		UE_LOG(LogStaticMesh, Log, TEXT("  Streaming: GPU size: %d bytes. %d Pages (%d with relative encoding). %.3f bytes per page (%.3f%% utilization)."), TotalStreamingGPUSize, NumStreamingPages, NumRelativeEncodingPages, (float)TotalStreamingGPUSize / float(NumStreamingPages), (float)TotalStreamingGPUSize / (float(NumStreamingPages * NANITE_STREAMING_PAGE_GPU_SIZE)) * 100.0f);
	}
	else
	{
		UE_LOG(LogStaticMesh, Log, TEXT("  Streaming: 0 bytes."));
	}
	UE_LOG(LogStaticMesh, Log, TEXT("  Page data disk size: %d bytes. Fixup data size: %d bytes."), TotalPageDiskSize, TotalFixupSize);
	UE_LOG(LogStaticMesh, Log, TEXT("  Total GPU size: %d bytes, Total disk size: %d bytes."), TotalPageGPUSize, TotalPageDiskSize + TotalFixupSize);

	// Store PageData
	Resources.StreamablePages.Lock(LOCK_READ_WRITE);
	uint8* Ptr = (uint8*)Resources.StreamablePages.Realloc(StreamableBulkData.Num());
	FMemory::Memcpy(Ptr, StreamableBulkData.GetData(), StreamableBulkData.Num());
	Resources.StreamablePages.Unlock();
	Resources.StreamablePages.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);

	if(OutTotalGPUSize)
	{
		*OutTotalGPUSize = TotalRootGPUSize + TotalStreamingGPUSize;
	}
}

struct FIntermediateNode
{
	uint32				PartIndex	= MAX_uint32;
	uint32				MipLevel	= MAX_int32;
	bool				bLeaf		= false;
	
	FBounds3f			Bound;
	TArray< uint32 >	Children;
};

static uint32 BuildHierarchyRecursive(TArray<FPage>& Pages, TArray<Nanite::FHierarchyNode>& HierarchyNodes, const TArray<FIntermediateNode>& Nodes, const TArray<Nanite::FClusterGroup>& Groups, TArray<Nanite::FClusterGroupPart>& Parts, uint32 CurrentNodeIndex, uint32 Depth)
{
	const FIntermediateNode& INode = Nodes[ CurrentNodeIndex ];
	check( INode.PartIndex == MAX_uint32 );
	check( !INode.bLeaf );

	uint32 HNodeIndex = HierarchyNodes.Num();
	HierarchyNodes.AddZeroed();

	uint32 NumChildren = INode.Children.Num();
	check(NumChildren <= NANITE_MAX_BVH_NODE_FANOUT);
	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++ )
	{
		uint32 ChildNodeIndex = INode.Children[ ChildIndex ];
		const FIntermediateNode& ChildNode = Nodes[ ChildNodeIndex ];
		if( ChildNode.bLeaf )
		{
			// Cluster Group
			check(ChildNode.bLeaf);
			FClusterGroupPart& Part = Parts[ChildNode.PartIndex];
			const FClusterGroup& Group = Groups[Part.GroupIndex];

			FHierarchyNode& HNode = HierarchyNodes[HNodeIndex];
			HNode.Bounds[ChildIndex] = Part.Bounds;
			HNode.LODBounds[ChildIndex] = Group.LODBounds;
			HNode.MinLODErrors[ChildIndex] = Group.MinLODError;
			HNode.MaxParentLODErrors[ChildIndex] = Group.MaxParentLODError;
			HNode.ChildrenStartIndex[ChildIndex] = 0xFFFFFFFFu;
			HNode.NumChildren[ChildIndex] = Part.Clusters.Num();
			HNode.ClusterGroupPartIndex[ChildIndex] = ChildNode.PartIndex;

			check(HNode.NumChildren[ChildIndex] <= NANITE_MAX_CLUSTERS_PER_GROUP);
			Part.HierarchyNodeIndex = HNodeIndex;
			Part.HierarchyChildIndex = ChildIndex;

			Pages[Part.PageIndex].MaxHierarchyDepth = FMath::Max(Pages[Part.PageIndex].MaxHierarchyDepth, Depth);
			check(Pages[Part.PageIndex].MaxHierarchyDepth <= NANITE_MAX_CLUSTER_HIERARCHY_DEPTH);
		}
		else
		{

			// Hierarchy node
			uint32 ChildHierarchyNodeIndex = BuildHierarchyRecursive(Pages, HierarchyNodes, Nodes, Groups, Parts, ChildNodeIndex, Depth + 1);

			const Nanite::FHierarchyNode& ChildHNode = HierarchyNodes[ChildHierarchyNodeIndex];

			FBounds3f Bounds;
			TArray< FSphere3f, TInlineAllocator<NANITE_MAX_BVH_NODE_FANOUT> > LODBoundSpheres;
			float MinLODError = MAX_flt;
			float MaxParentLODError = 0.0f;
			for (uint32 GrandChildIndex = 0; GrandChildIndex < NANITE_MAX_BVH_NODE_FANOUT && ChildHNode.NumChildren[GrandChildIndex] != 0; GrandChildIndex++)
			{
				Bounds += ChildHNode.Bounds[GrandChildIndex];
				LODBoundSpheres.Add(ChildHNode.LODBounds[GrandChildIndex]);
				MinLODError = FMath::Min(MinLODError, ChildHNode.MinLODErrors[GrandChildIndex]);
				MaxParentLODError = FMath::Max(MaxParentLODError, ChildHNode.MaxParentLODErrors[GrandChildIndex]);
			}

			FSphere3f LODBounds = FSphere3f(LODBoundSpheres.GetData(), LODBoundSpheres.Num());

			Nanite::FHierarchyNode& HNode = HierarchyNodes[HNodeIndex];
			HNode.Bounds[ChildIndex] = Bounds;
			HNode.LODBounds[ChildIndex] = LODBounds;
			HNode.MinLODErrors[ChildIndex] = MinLODError;
			HNode.MaxParentLODErrors[ChildIndex] = MaxParentLODError;
			HNode.ChildrenStartIndex[ChildIndex] = ChildHierarchyNodeIndex;
			HNode.NumChildren[ChildIndex] = NANITE_MAX_CLUSTERS_PER_GROUP;
			HNode.ClusterGroupPartIndex[ChildIndex] = MAX_uint32;
		}
	}

	return HNodeIndex;
}

#define BVH_BUILD_WRITE_GRAPHVIZ	0

#if BVH_BUILD_WRITE_GRAPHVIZ
static void WriteDotGraph(const TArray<FIntermediateNode>& Nodes)
{
	FGenericPlatformMisc::LowLevelOutputDebugString(TEXT("digraph {\n"));
	
	const uint32 NumNodes = Nodes.Num();
	for (uint32 NodeIndex = 0; NodeIndex < NumNodes; NodeIndex++)
	{
		const FIntermediateNode& Node = Nodes[NodeIndex];
		if (!Node.bLeaf)
		{
			uint32 NumLeaves = 0;
			for (uint32 ChildIndex : Node.Children)
			{
				if(Nodes[ChildIndex].bLeaf)
				{
					NumLeaves++;
				}
				else
				{
					FGenericPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tn%d -> n%d;\n"), NodeIndex, ChildIndex);
				}
			}
			FGenericPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tn%d [label=\"%d, %d\"];\n"), NodeIndex, Node.Children.Num(), NumLeaves);
		}
	}
	FGenericPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
}
#endif

static float BVH_Cost(const TArray<FIntermediateNode>& Nodes, TArrayView<uint32> NodeIndices)
{
	FBounds3f Bound;
	for (uint32 NodeIndex : NodeIndices)
	{
		Bound += Nodes[NodeIndex].Bound;
	}
	return Bound.GetSurfaceArea();
}

static void BVH_SortNodes(const TArray<FIntermediateNode>& Nodes, TArrayView<uint32> NodeIndices, const TArray<uint32>& ChildSizes)
{
	// Perform NANITE_MAX_BVH_NODE_FANOUT_BITS binary splits
	for (uint32 Level = 0; Level < NANITE_MAX_BVH_NODE_FANOUT_BITS; Level++)
	{
		const uint32 NumBuckets = 1 << Level;
		const uint32 NumChildrenPerBucket = NANITE_MAX_BVH_NODE_FANOUT >> Level;
		const uint32 NumChildrenPerBucketHalf = NumChildrenPerBucket >> 1;

		uint32 BucketStartIndex = 0;
		for (uint32 BucketIndex = 0; BucketIndex < NumBuckets; BucketIndex++)
		{
			const uint32 FirstChild = NumChildrenPerBucket * BucketIndex;
			
			uint32 Sizes[2] = {};
			for (uint32 i = 0; i < NumChildrenPerBucketHalf; i++)
			{
				Sizes[0] += ChildSizes[FirstChild + i];
				Sizes[1] += ChildSizes[FirstChild + i + NumChildrenPerBucketHalf];
			}
			TArrayView<uint32> NodeIndices01 = NodeIndices.Slice(BucketStartIndex, Sizes[0] + Sizes[1]);
			TArrayView<uint32> NodeIndices0 = NodeIndices.Slice(BucketStartIndex, Sizes[0]);
			TArrayView<uint32> NodeIndices1 = NodeIndices.Slice(BucketStartIndex + Sizes[0], Sizes[1]);

			BucketStartIndex += Sizes[0] + Sizes[1];

			auto SortByAxis = [&](uint32 AxisIndex)
			{
				if (AxisIndex == 0)
					NodeIndices01.Sort([&Nodes](uint32 A, uint32 B) { return Nodes[A].Bound.GetCenter().X < Nodes[B].Bound.GetCenter().X; });
				else if (AxisIndex == 1)
					NodeIndices01.Sort([&Nodes](uint32 A, uint32 B) { return Nodes[A].Bound.GetCenter().Y < Nodes[B].Bound.GetCenter().Y; });
				else if (AxisIndex == 2)
					NodeIndices01.Sort([&Nodes](uint32 A, uint32 B) { return Nodes[A].Bound.GetCenter().Z < Nodes[B].Bound.GetCenter().Z; });
				else
					check(false);
			};

			float BestCost = MAX_flt;
			uint32 BestAxisIndex = 0;

			// Try sorting along different axes and pick the best one
			const uint32 NumAxes = 3;
			for (uint32 AxisIndex = 0; AxisIndex < NumAxes; AxisIndex++)
			{
				SortByAxis(AxisIndex);

				float Cost = BVH_Cost(Nodes, NodeIndices0) + BVH_Cost(Nodes, NodeIndices1);
				if (Cost < BestCost)
				{
					BestCost = Cost;
					BestAxisIndex = AxisIndex;
				}
			}

			// Resort if we the best one wasn't the last one
			if (BestAxisIndex != NumAxes - 1)
			{
				SortByAxis(BestAxisIndex);
			}
		}
	}
}

// Build hierarchy using a top-down splitting approach.
// WIP:	So far it just focuses on minimizing worst-case tree depth/latency.
//		It does this by building a complete tree with at most one partially filled level.
//		At most one node is partially filled.
//TODO:	Experiment with sweeping, even if it results in more total nodes and/or makes some paths slightly longer.
static uint32 BuildHierarchyTopDown(TArray<FIntermediateNode>& Nodes, TArrayView<uint32> NodeIndices, bool bSort)
{
	const uint32 N = NodeIndices.Num();
	if (N == 1)
	{
		return NodeIndices[0];
	} 
	
	const uint32 NewRootIndex = Nodes.Num();
	Nodes.AddDefaulted_GetRef();

	if (N <= NANITE_MAX_BVH_NODE_FANOUT)
	{
		Nodes[NewRootIndex].Children = NodeIndices;
		return NewRootIndex;
	}

	// Where does the last (incomplete) level start
	uint32 TopSize = NANITE_MAX_BVH_NODE_FANOUT;
	while (TopSize * NANITE_MAX_BVH_NODE_FANOUT <= N)
	{
		TopSize *= NANITE_MAX_BVH_NODE_FANOUT;
	}
	
	const uint32 LargeChildSize = TopSize;
	const uint32 SmallChildSize = TopSize / NANITE_MAX_BVH_NODE_FANOUT;
	const uint32 MaxExcessPerChild = LargeChildSize - SmallChildSize;

	TArray<uint32> ChildSizes;
	ChildSizes.SetNum(NANITE_MAX_BVH_NODE_FANOUT);
	
	uint32 Excess = N - TopSize;
	for (int32 i = NANITE_MAX_BVH_NODE_FANOUT-1; i >= 0; i--)
	{
		const uint32 ChildExcess = FMath::Min(Excess, MaxExcessPerChild);
		ChildSizes[i] = SmallChildSize + ChildExcess;
		Excess -= ChildExcess;
	}
	check(Excess == 0);

	if (bSort)
	{
		BVH_SortNodes(Nodes, NodeIndices, ChildSizes);
	}
	
	uint32 Offset = 0;
	for (uint32 i = 0; i < NANITE_MAX_BVH_NODE_FANOUT; i++)
	{
		uint32 ChildSize = ChildSizes[i];
		uint32 NodeIndex = BuildHierarchyTopDown(Nodes, NodeIndices.Slice(Offset, ChildSize), bSort);	// Needs to be separated from next statement with sequence point to order access to Nodes array.
		Nodes[NewRootIndex].Children.Add(NodeIndex);
		Offset += ChildSize;
	}

	return NewRootIndex;
}

static void BuildHierarchies(FResources& Resources, TArray<FPage>& Pages, const TArray<FClusterGroup>& Groups, TArray<FClusterGroupPart>& Parts, uint32 NumMeshes)
{
	TArray<TArray<uint32>> PartsByMesh;
	PartsByMesh.SetNum(NumMeshes);

	// Assign group parts to the meshes they belong to
	const uint32 NumTotalParts = Parts.Num();
	for (uint32 PartIndex = 0; PartIndex < NumTotalParts; PartIndex++)
	{
		FClusterGroupPart& Part = Parts[PartIndex];
		PartsByMesh[Groups[Part.GroupIndex].MeshIndex].Add(PartIndex);
	}

	for (uint32 MeshIndex = 0; MeshIndex < NumMeshes; MeshIndex++)
	{
		const TArray<uint32>& PartIndices = PartsByMesh[MeshIndex];
		const uint32 NumParts = PartIndices.Num();
		
		int32 MaxMipLevel = 0;
		for (uint32 i = 0; i < NumParts; i++)
		{
			MaxMipLevel = FMath::Max(MaxMipLevel, Groups[Parts[PartIndices[i]].GroupIndex].MipLevel);
		}

		TArray< FIntermediateNode >	Nodes;
		Nodes.SetNum(NumParts);

		// Build leaf nodes for each LOD level of the mesh
		TArray<TArray<uint32>> NodesByMip;
		NodesByMip.SetNum(MaxMipLevel + 1);
		for (uint32 i = 0; i < NumParts; i++)
		{
			const uint32 PartIndex = PartIndices[i];
			const FClusterGroupPart& Part = Parts[PartIndex];
			const FClusterGroup& Group = Groups[Part.GroupIndex];

			const int32 MipLevel = Group.MipLevel;
			FIntermediateNode& Node = Nodes[i];
			Node.Bound = Part.Bounds;
			Node.PartIndex = PartIndex;
			Node.MipLevel = Group.MipLevel;
			Node.bLeaf = true;
			NodesByMip[Group.MipLevel].Add(i);
		}


		uint32 RootIndex = 0;
		if (Nodes.Num() == 0)
		{
			// Completely empty mesh. This can happen for submeshes of existing geometry collections. 
			// The caller expects the submesh to have a valid hierarchy offset, so we provide an empty node with no children.
			Nodes.AddDefaulted();
		}
		else if (Nodes.Num() == 1)
		{
			// Just a single leaf.
			// Needs to be special-cased as root should always be an inner node.
			FIntermediateNode& Node = Nodes.AddDefaulted_GetRef();
			Node.Children.Add(0);
			Node.Bound = Nodes[0].Bound;
			RootIndex = 1;
		}
		else
		{
			// Build hierarchy:
			// Nanite meshes contain cluster data for many levels of detail. Clusters from different levels
			// of detail can vary wildly in size, which can already be challenge for building a good hierarchy. 
			// Apart from the visibility bounds, the hierarchy also tracks conservative LOD error metrics for the child nodes.
			// The runtime traversal descends into children as long as they are visible and the conservative LOD error is not
			// more detailed than what we are looking for. We have to be very careful when mixing clusters from different LODs
			// as less detailed clusters can easily end up bloating both bounds and error metrics.

			// We have experimented with a bunch of mixed LOD approached, but currently, it seems, building separate hierarchies
			// for each LOD level and then building a hierarchy of those hierarchies gives the best and most predictable results.

			// TODO: The roots of these hierarchies all share the same visibility and LOD bounds, or at least close enough that we could
			//       make a shared conservative bound without losing much. This makes a lot of the work around the root node fairly
			//       redundant. Perhaps we should consider evaluating a shared root during instance cull instead and enable/disable
			//       the per-level hierarchies based on 1D range tests for LOD error.

			TArray<uint32> LevelRoots;
			for (int32 MipLevel = 0; MipLevel <= MaxMipLevel; MipLevel++)
			{
				if (NodesByMip[MipLevel].Num() > 0)
				{
					// Build a hierarchy for the mip level
					uint32 NodeIndex = BuildHierarchyTopDown(Nodes, NodesByMip[MipLevel], true);

					if (Nodes[NodeIndex].bLeaf || Nodes[NodeIndex].Children.Num() == NANITE_MAX_BVH_NODE_FANOUT)
					{
						// Leaf or filled node. Just add it.
						LevelRoots.Add(NodeIndex);
					}
					else
					{
						// Incomplete node. Discard the code and add the children as roots instead.
						LevelRoots.Append(Nodes[NodeIndex].Children);
					}
				}
			}
			// Build top hierarchy. A hierarchy of MIP hierarchies.
			RootIndex = BuildHierarchyTopDown(Nodes, LevelRoots, false);
		}

		check(Nodes.Num() > 0);

#if BVH_BUILD_WRITE_GRAPHVIZ
		WriteDotGraph(Nodes);
#endif

		TArray< FHierarchyNode > HierarchyNodes;
		BuildHierarchyRecursive(Pages, HierarchyNodes, Nodes, Groups, Parts, RootIndex, 0);

		// Convert hierarchy to packed format
		const uint32 NumHierarchyNodes = HierarchyNodes.Num();
		const uint32 PackedBaseIndex = Resources.HierarchyNodes.Num();
		Resources.HierarchyRootOffsets.Add(PackedBaseIndex);
		Resources.HierarchyNodes.AddDefaulted(NumHierarchyNodes);
		for (uint32 i = 0; i < NumHierarchyNodes; i++)
		{
			PackHierarchyNode(Resources.HierarchyNodes[PackedBaseIndex + i], HierarchyNodes[i], Groups, Parts, Resources.NumRootPages);
		}
	}
}

void BuildMaterialRanges(
	const TArray<uint32>& TriangleIndices,
	const TArray<int32>& MaterialIndices,
	TArray<FMaterialTriangle, TInlineAllocator<128>>& MaterialTris,
	TArray<FMaterialRange, TInlineAllocator<4>>& MaterialRanges)
{
	check(MaterialTris.Num() == 0);
	check(MaterialRanges.Num() == 0);
	check(MaterialIndices.Num() * 3 == TriangleIndices.Num());

	const uint32 TriangleCount = MaterialIndices.Num();

	TArray<uint32, TInlineAllocator<64>> MaterialCounts;
	MaterialCounts.AddZeroed(64);

	// Tally up number tris per material index
	for (uint32 i = 0; i < TriangleCount; i++)
	{
		const uint32 MaterialIndex = MaterialIndices[i];
		++MaterialCounts[MaterialIndex];
	}

	for (uint32 i = 0; i < TriangleCount; i++)
	{
		FMaterialTriangle MaterialTri;
		MaterialTri.Index0 = TriangleIndices[(i * 3) + 0];
		MaterialTri.Index1 = TriangleIndices[(i * 3) + 1];
		MaterialTri.Index2 = TriangleIndices[(i * 3) + 2];
		MaterialTri.MaterialIndex = MaterialIndices[i];
		MaterialTri.RangeCount = MaterialCounts[MaterialTri.MaterialIndex];
		check(MaterialTri.RangeCount > 0);
		MaterialTris.Add(MaterialTri);
	}

	// Sort by triangle range count descending, and material index ascending.
	// This groups the material ranges from largest to smallest, which is
	// more efficient for evaluating the sequences on the GPU, and also makes
	// the minus one encoding work (the first range must have more than 1 tri).
	MaterialTris.Sort(
		[](const FMaterialTriangle& A, const FMaterialTriangle& B)
		{
			if (A.RangeCount != B.RangeCount)
			{
				return (A.RangeCount > B.RangeCount);
			}

			return (A.MaterialIndex < B.MaterialIndex);
		} );

	FMaterialRange CurrentRange;
	CurrentRange.RangeStart = 0;
	CurrentRange.RangeLength = 0;
	CurrentRange.MaterialIndex = MaterialTris.Num() > 0 ? MaterialTris[0].MaterialIndex : 0;

	for (int32 TriIndex = 0; TriIndex < MaterialTris.Num(); ++TriIndex)
	{
		const FMaterialTriangle& Triangle = MaterialTris[TriIndex];

		// Material changed, so add current range and reset
		if (CurrentRange.RangeLength > 0 && Triangle.MaterialIndex != CurrentRange.MaterialIndex)
		{
			MaterialRanges.Add(CurrentRange);

			CurrentRange.RangeStart = TriIndex;
			CurrentRange.RangeLength = 1;
			CurrentRange.MaterialIndex = Triangle.MaterialIndex;
		}
		else
		{
			++CurrentRange.RangeLength;
		}
	}

	// Add last triangle to range
	if (CurrentRange.RangeLength > 0)
	{
		MaterialRanges.Add(CurrentRange);
	}

	check(MaterialTris.Num() == TriangleCount);
}

static void BuildMaterialRanges(FCluster& Cluster)
{
	check(Cluster.MaterialRanges.Num() == 0);
	check(Cluster.NumTris <= NANITE_MAX_CLUSTER_TRIANGLES);
	check(Cluster.NumTris * 3 == Cluster.Indexes.Num());

	TArray<FMaterialTriangle, TInlineAllocator<128>> MaterialTris;
	
	BuildMaterialRanges(
		Cluster.Indexes,
		Cluster.MaterialIndexes,
		MaterialTris,
		Cluster.MaterialRanges);

	// Write indices back to clusters
	for (uint32 Triangle = 0; Triangle < Cluster.NumTris; ++Triangle)
	{
		Cluster.Indexes[Triangle * 3 + 0] = MaterialTris[Triangle].Index0;
		Cluster.Indexes[Triangle * 3 + 1] = MaterialTris[Triangle].Index1;
		Cluster.Indexes[Triangle * 3 + 2] = MaterialTris[Triangle].Index2;
		Cluster.MaterialIndexes[Triangle] = MaterialTris[Triangle].MaterialIndex;
	}
}

// Sort cluster triangles into material ranges. Add Material ranges to clusters.
static void BuildMaterialRanges( TArray<FCluster>& Clusters )
{
	//const uint32 NumClusters = Clusters.Num();
	//for( uint32 ClusterIndex = 0; ClusterIndex < NumClusters; ClusterIndex++ )
	ParallelFor(TEXT("NaniteEncode.BuildMaterialRanges.PF"), Clusters.Num(), 256,
		[&]( uint32 ClusterIndex )
		{
			BuildMaterialRanges( Clusters[ ClusterIndex ] );
		} );
}

// Prints material range stats. This has to happen separate from BuildMaterialRanges as materials might be recalculated because of cluster splitting.
static void PrintMaterialRangeStats( TArray<FCluster>& Clusters )
{
	TFixedBitVector<NANITE_MAX_CLUSTER_MATERIALS> UsedMaterialIndices;
	UsedMaterialIndices.Clear();

	uint32 NumClusterMaterials[ 4 ] = { 0, 0, 0, 0 }; // 1, 2, 3, >= 4

	const uint32 NumClusters = Clusters.Num();
	for( uint32 ClusterIndex = 0; ClusterIndex < NumClusters; ClusterIndex++ )
	{
		FCluster& Cluster = Clusters[ ClusterIndex ];

		// TODO: Valid assumption? All null materials should have been assigned default material at this point.
		check( Cluster.MaterialRanges.Num() > 0 );
		NumClusterMaterials[ FMath::Min( Cluster.MaterialRanges.Num() - 1, 3 ) ]++;

		for( const FMaterialRange& MaterialRange : Cluster.MaterialRanges )
		{
			UsedMaterialIndices.SetBit( MaterialRange.MaterialIndex );
		}
	}

	UE_LOG( LogStaticMesh, Log, TEXT( "Material Stats - Unique Materials: %d, Fast Path Clusters: %d, Slow Path Clusters: %d, 1 Material: %d, 2 Materials: %d, 3 Materials: %d, At Least 4 Materials: %d" ),
		UsedMaterialIndices.CountBits(), Clusters.Num() - NumClusterMaterials[ 3 ], NumClusterMaterials[ 3 ], NumClusterMaterials[ 0 ], NumClusterMaterials[ 1 ], NumClusterMaterials[ 2 ], NumClusterMaterials[ 3 ] );

#if 0
	for( uint32 MaterialIndex = 0; MaterialIndex < MAX_CLUSTER_MATERIALS; ++MaterialIndex )
	{
		if( UsedMaterialIndices.GetBit( MaterialIndex ) > 0 )
		{
			UE_LOG( LogStaticMesh, Log, TEXT( "  Material Index: %d" ), MaterialIndex );
		}
	}
#endif
}

#if DO_CHECK
static void VerifyClusterConstaints( const FCluster& Cluster )
{
	check( Cluster.NumTris * 3 == Cluster.Indexes.Num() );
	check( Cluster.NumVerts <= 256 );

	const uint32 NumTriangles = Cluster.NumTris;

	uint32 MaxVertexIndex = 0;
	for( uint32 i = 0; i < NumTriangles; i++ )
	{
		uint32 Index0 = Cluster.Indexes[ i * 3 + 0 ];
		uint32 Index1 = Cluster.Indexes[ i * 3 + 1 ];
		uint32 Index2 = Cluster.Indexes[ i * 3 + 2 ];
		MaxVertexIndex = FMath::Max( MaxVertexIndex, FMath::Max3( Index0, Index1, Index2 ) );
		check( MaxVertexIndex - Index0 < CONSTRAINED_CLUSTER_CACHE_SIZE );
		check( MaxVertexIndex - Index1 < CONSTRAINED_CLUSTER_CACHE_SIZE );
		check( MaxVertexIndex - Index2 < CONSTRAINED_CLUSTER_CACHE_SIZE );
	}
}
#endif

// Weights for individual cache entries based on simulated annealing optimization on DemoLevel.
static int16 CacheWeightTable[ CONSTRAINED_CLUSTER_CACHE_SIZE ] = {
	 577,	 616,	 641,  512,		 614,  635,  478,  651,
	  65,	 213,	 719,  490,		 213,  726,  863,  745,
	 172,	 939,	 805,  885,		 958, 1208, 1319, 1318,
	1475,	1779,	2342,  159,		2307, 1998, 1211,  932
};

// Constrain cluster to only use vertex references that are within a fixed sized trailing window from the current highest encountered vertex index.
// Triangles are reordered based on a FIFO-style cache optimization to minimize the number of vertices that need to be duplicated.
static void ConstrainClusterFIFO( FCluster& Cluster )
{
	uint32 NumOldTriangles = Cluster.NumTris;
	uint32 NumOldVertices = Cluster.NumVerts;

	const uint32 MAX_CLUSTER_TRIANGLES_IN_DWORDS = (NANITE_MAX_CLUSTER_TRIANGLES + 31 ) / 32;

	uint32 VertexToTriangleMasks[NANITE_MAX_CLUSTER_TRIANGLES * 3][MAX_CLUSTER_TRIANGLES_IN_DWORDS] = {};

	// Generate vertex to triangle masks
	for( uint32 i = 0; i < NumOldTriangles; i++ )
	{
		uint32 i0 = Cluster.Indexes[ i * 3 + 0 ];
		uint32 i1 = Cluster.Indexes[ i * 3 + 1 ];
		uint32 i2 = Cluster.Indexes[ i * 3 + 2 ];
		check( i0 != i1 && i1 != i2 && i2 != i0 ); // Degenerate input triangle!

		VertexToTriangleMasks[ i0 ][ i >> 5 ] |= 1 << ( i & 31 );
		VertexToTriangleMasks[ i1 ][ i >> 5 ] |= 1 << ( i & 31 );
		VertexToTriangleMasks[ i2 ][ i >> 5 ] |= 1 << ( i & 31 );
	}

	uint32 TrianglesEnabled[ MAX_CLUSTER_TRIANGLES_IN_DWORDS ] = {};	// Enabled triangles are in the current material range and have not yet been visited.
	uint32 TrianglesTouched[ MAX_CLUSTER_TRIANGLES_IN_DWORDS ] = {};	// Touched triangles have had at least one of their vertices visited.

	uint16 OptimizedIndices[NANITE_MAX_CLUSTER_TRIANGLES * 3 ];

	uint32 NumNewVertices = 0;
	uint32 NumNewTriangles = 0;
	uint16 OldToNewVertex[NANITE_MAX_CLUSTER_TRIANGLES * 3];
	uint16 NewToOldVertex[NANITE_MAX_CLUSTER_TRIANGLES * 3] = {};	// Initialize to make static analysis happy
	FMemory::Memset( OldToNewVertex, -1, sizeof( OldToNewVertex ) );

	auto ScoreVertex = [ &OldToNewVertex, &NumNewVertices ] ( uint32 OldVertex )
	{
		uint16 NewIndex = OldToNewVertex[ OldVertex ];

		int32 CacheScore = 0;
		if( NewIndex != 0xFFFF )
		{
			uint32 CachePosition = ( NumNewVertices - 1 ) - NewIndex;
			if( CachePosition < CONSTRAINED_CLUSTER_CACHE_SIZE )
				CacheScore = CacheWeightTable[ CachePosition ];
		}

		return CacheScore;
	};

	uint32 RangeStart = 0;
	for( FMaterialRange& MaterialRange : Cluster.MaterialRanges )
	{
		check( RangeStart == MaterialRange.RangeStart );
		uint32 RangeLength = MaterialRange.RangeLength;

		// Enable triangles from current range
		for( uint32 i = 0; i < MAX_CLUSTER_TRIANGLES_IN_DWORDS; i++ )
		{
			int32 RangeStartRelativeToDword = (int32)RangeStart - (int32)i * 32;
			int32 BitStart = FMath::Max( RangeStartRelativeToDword, 0 );
			int32 BitEnd = FMath::Max( RangeStartRelativeToDword + (int32)RangeLength, 0 );
			uint32 StartMask = BitStart < 32 ? ( ( 1u << BitStart ) - 1u ) : 0xFFFFFFFFu;
			uint32 EndMask = BitEnd < 32 ? ( ( 1u << BitEnd ) - 1u ) : 0xFFFFFFFFu;
			TrianglesEnabled[ i ] |= StartMask ^ EndMask;
		}

		while( true )
		{
			uint32 NextTriangleIndex = 0xFFFF;
			int32 NextTriangleScore = 0;

			// Pick highest scoring available triangle
			for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MAX_CLUSTER_TRIANGLES_IN_DWORDS; TriangleDwordIndex++ )
			{
				uint32 CandidateMask = TrianglesTouched[ TriangleDwordIndex ] & TrianglesEnabled[ TriangleDwordIndex ];
				while( CandidateMask )
				{
					uint32 TriangleDwordOffset = FMath::CountTrailingZeros( CandidateMask );
					CandidateMask &= CandidateMask - 1;

					int32 TriangleIndex = ( TriangleDwordIndex << 5 ) + TriangleDwordOffset;

					int32 TriangleScore = 0;
					TriangleScore += ScoreVertex( Cluster.Indexes[ TriangleIndex * 3 + 0 ] );
					TriangleScore += ScoreVertex( Cluster.Indexes[ TriangleIndex * 3 + 1 ] );
					TriangleScore += ScoreVertex( Cluster.Indexes[ TriangleIndex * 3 + 2 ] );

					if( TriangleScore > NextTriangleScore )
					{
						NextTriangleIndex = TriangleIndex;
						NextTriangleScore = TriangleScore;
					}
				}
			}

			if( NextTriangleIndex == 0xFFFF )
			{
				// If we didn't find a triangle. It might be because it is part of a separate component. Look for an unvisited triangle to restart from.
				for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MAX_CLUSTER_TRIANGLES_IN_DWORDS; TriangleDwordIndex++ )
				{
					uint32 EnableMask = TrianglesEnabled[ TriangleDwordIndex ];
					if( EnableMask )
					{
						NextTriangleIndex = ( TriangleDwordIndex << 5 ) + FMath::CountTrailingZeros( EnableMask );
						break;
					}
				}

				if( NextTriangleIndex == 0xFFFF )
					break;
			}

			uint32 OldIndex0 = Cluster.Indexes[ NextTriangleIndex * 3 + 0 ];
			uint32 OldIndex1 = Cluster.Indexes[ NextTriangleIndex * 3 + 1 ];
			uint32 OldIndex2 = Cluster.Indexes[ NextTriangleIndex * 3 + 2 ];

			// Mark incident triangles
			for( uint32 i = 0; i < MAX_CLUSTER_TRIANGLES_IN_DWORDS; i++ )
			{
				TrianglesTouched[ i ] |= VertexToTriangleMasks[ OldIndex0 ][ i ] | VertexToTriangleMasks[ OldIndex1 ][ i ] | VertexToTriangleMasks[ OldIndex2 ][ i ];
			}

			uint16& NewIndex0 = OldToNewVertex[OldIndex0];
			uint16& NewIndex1 = OldToNewVertex[OldIndex1];
			uint16& NewIndex2 = OldToNewVertex[OldIndex2];

			// Generate new indices such that they are all within a trailing window of CONSTRAINED_CLUSTER_CACHE_SIZE of NumNewVertices.
			// This can require multiple iterations as new/duplicate vertices can push other vertices outside the window.			
			uint32 TestNumNewVertices = NumNewVertices;
			TestNumNewVertices += (NewIndex0 == 0xFFFF) + (NewIndex1 == 0xFFFF) + (NewIndex2 == 0xFFFF);

			while(true)
			{
				if (NewIndex0 != 0xFFFF && TestNumNewVertices - NewIndex0 >= CONSTRAINED_CLUSTER_CACHE_SIZE)
				{
					NewIndex0 = 0xFFFF;
					TestNumNewVertices++;
					continue;
				}

				if (NewIndex1 != 0xFFFF && TestNumNewVertices - NewIndex1 >= CONSTRAINED_CLUSTER_CACHE_SIZE)
				{
					NewIndex1 = 0xFFFF;
					TestNumNewVertices++;
					continue;
				}

				if (NewIndex2 != 0xFFFF && TestNumNewVertices - NewIndex2 >= CONSTRAINED_CLUSTER_CACHE_SIZE)
				{
					NewIndex2 = 0xFFFF;
					TestNumNewVertices++;
					continue;
				}
				break;
			}

			if (NewIndex0 == 0xFFFF) { NewIndex0 = uint16(NumNewVertices++); }
			if (NewIndex1 == 0xFFFF) { NewIndex1 = uint16(NumNewVertices++); }
			if (NewIndex2 == 0xFFFF) { NewIndex2 = uint16(NumNewVertices++); }
			NewToOldVertex[NewIndex0] = uint16(OldIndex0);
			NewToOldVertex[NewIndex1] = uint16(OldIndex1);
			NewToOldVertex[NewIndex2] = uint16(OldIndex2);

			// Output triangle
			OptimizedIndices[ NumNewTriangles * 3 + 0 ] = NewIndex0;
			OptimizedIndices[ NumNewTriangles * 3 + 1 ] = NewIndex1;
			OptimizedIndices[ NumNewTriangles * 3 + 2 ] = NewIndex2;
			NumNewTriangles++;

			// Disable selected triangle
			TrianglesEnabled[ NextTriangleIndex >> 5 ] &= ~( 1 << ( NextTriangleIndex & 31 ) );
		}
		RangeStart += RangeLength;
	}

	check( NumNewTriangles == NumOldTriangles );

	// Write back new triangle order
	for( uint32 i = 0; i < NumNewTriangles * 3; i++ )
	{
		Cluster.Indexes[ i ] = OptimizedIndices[ i ];
	}

	// Write back new vertex order including possibly duplicates
	TArray< float > OldVertices;
	Swap( OldVertices, Cluster.Verts );

	uint32 VertStride = Cluster.GetVertSize();
	Cluster.Verts.AddUninitialized( NumNewVertices * VertStride );
	for( uint32 i = 0; i < NumNewVertices; i++ )
	{
		FMemory::Memcpy( &Cluster.GetPosition(i), &OldVertices[ NewToOldVertex[ i ] * VertStride ], VertStride * sizeof( float ) );
	}
	Cluster.NumVerts = NumNewVertices;
}


static FORCEINLINE uint32 SetCorner( uint32 Triangle, uint32 LocalCorner )
{
	return ( Triangle << 2 ) | LocalCorner;
}

static FORCEINLINE uint32 CornerToTriangle( uint32 Corner )
{
	return Corner >> 2;
}

static FORCEINLINE uint32 NextCorner( uint32 Corner )
{
	if( ( Corner & 3 ) == 2 )
		Corner &= ~3;
	else
		Corner++;
	return Corner;
}

static FORCEINLINE uint32 PrevCorner( uint32 Corner )
{
	if( ( Corner & 3 ) == 0 )
		Corner |= 2;
	else
		Corner--;
	return Corner;
}

static FORCEINLINE uint32 CornerToIndex( uint32 Corner )
{
	return ( Corner >> 2 ) * 3 + ( Corner & 3 );
}

struct FStripifyWeights
{
	int32 Weights[ 2 ][ 2 ][ 2 ][ 2 ][ CONSTRAINED_CLUSTER_CACHE_SIZE ];
};

static const FStripifyWeights DefaultStripifyWeights = {
	{
		{
			{
				{
					// IsStart=0, HasOpposite=0, HasLeft=0, HasRight=0
					{  142,  124,  131,  184,  138,  149,  148,  127,  154,  148,  152,  133,  133,  132,  170,  141,  109,  148,  138,  117,  126,  112,  144,  126,  116,  139,  122,  141,  122,  133,  134,  137 },
					// IsStart=0, HasOpposite=0, HasLeft=0, HasRight=1
					{  128,  144,  134,  122,  130,  133,  129,  122,  128,  107,  127,  126,   89,  135,   88,  130,   94,  134,  103,  118,  128,   96,   90,  139,   89,  139,  113,  100,  119,  131,  113,  121 },
				},
				{
					// IsStart=0, HasOpposite=0, HasLeft=1, HasRight=0
					{  128,  144,  134,  129,  110,  142,  111,  140,  116,  139,   98,  110,  125,  143,  122,  109,  127,  154,  113,  119,  126,  131,  123,  127,   93,  118,  101,   93,  131,  139,  130,  139 },
					// IsStart=0, HasOpposite=0, HasLeft=1, HasRight=1
					{  120,  128,  137,  105,  113,  121,  120,  120,  112,  117,  124,  129,  129,   98,  137,  133,  122,  159,  141,  104,  129,  119,   98,  111,  110,  115,  114,  125,  115,  140,  109,  137 },
				}
			},
			{
				{
					// IsStart=0, HasOpposite=1, HasLeft=0, HasRight=0
					{  128,  137,  154,  169,  140,  162,  156,  157,  164,  144,  171,  145,  148,  146,  124,  138,  144,  158,  140,  137,  141,  145,  140,  148,  110,  160,  128,  129,  144,  155,  125,  123 },
					// IsStart=0, HasOpposite=1, HasLeft=0, HasRight=1
					{  124,  115,  136,  131,  145,  143,  159,  144,  158,  165,  128,  191,  135,  173,  147,  137,  128,  163,  164,  151,  162,  178,  161,  143,  168,  166,  122,  160,  170,  175,  132,  109 },
				},
				{
					// IsStart=0, HasOpposite=1, HasLeft=1, HasRight=0
					{  134,  112,  132,  123,  126,  138,  148,  138,  145,  136,  146,  133,  141,  165,  139,  145,  119,  167,  135,  120,  146,  120,  117,  136,  102,  156,  128,  120,  132,  143,   91,  136 },
					// IsStart=0, HasOpposite=1, HasLeft=1, HasRight=1
					{  140,   95,  118,  117,  127,  102,  119,  119,  134,  107,  135,  128,  109,  133,  120,  122,  132,  150,  152,  119,  128,  137,  119,  128,  131,  165,  156,  143,  135,  134,  135,  154 },
				}
			}
		},
		{
			{
				{
					// IsStart=1, HasOpposite=0, HasLeft=0, HasRight=0
					{  139,  132,  139,  133,  130,  134,  135,  131,  133,  139,  141,  139,  132,  136,  139,  150,  140,  137,  143,  157,  149,  157,  168,  155,  159,  181,  176,  185,  219,  167,  133,  143 },
					// IsStart=1, HasOpposite=0, HasLeft=0, HasRight=1
					{  125,  127,  126,  131,  128,  114,  130,  126,  129,  131,  125,  127,  131,  126,  137,  129,  140,   99,  142,   99,  149,  121,  155,  118,  131,  156,  168,  144,  175,  155,  112,  129 },
				},
				{
					// IsStart=1, HasOpposite=0, HasLeft=1, HasRight=0
					{  129,  129,  128,  128,  128,  129,  128,  129,  130,  127,  131,  130,  131,  130,  134,  133,  136,  134,  134,  138,  144,  139,  137,  154,  147,  141,  175,  214,  140,  140,  130,  122 },
					// IsStart=1, HasOpposite=0, HasLeft=1, HasRight=1
					{  128,  128,  124,  123,  125,  107,  127,  128,  125,  128,  128,  128,  128,  128,  128,  130,  107,  124,  136,  119,  139,  127,  132,  140,  125,  150,  133,  150,  138,  130,  127,  127 },
				}
			},
			{
				{
					// IsStart=1, HasOpposite=1, HasLeft=0, HasRight=0
					{  104,  125,  126,  129,  126,  122,  128,  126,  126,  127,  125,  122,  130,  126,  130,  131,  130,  132,  118,  101,  119,  121,  143,  114,  122,  145,  132,  144,  116,  142,  114,  127 },
					// IsStart=1, HasOpposite=1, HasLeft=0, HasRight=1
					{  128,  124,   93,  126,  108,  128,  127,  122,  128,  126,  128,  123,   92,  125,   98,   99,  127,  131,  126,  128,  121,  133,  113,  121,  122,  137,  145,  138,  137,  109,  129,  100 },
				},
				{
					// IsStart=1, HasOpposite=1, HasLeft=1, HasRight=0
					{  119,  128,  122,  128,  127,  123,  126,  128,  126,  122,  120,  127,  128,  122,  130,  121,  138,  122,  136,  130,  133,  124,  139,  134,  138,  118,  139,  145,  132,  122,  124,   86 },
					// IsStart=1, HasOpposite=1, HasLeft=1, HasRight=1
					{  116,  124,  119,  126,  118,  113,  114,  125,  128,  111,  129,  122,  129,  129,  135,  130,  138,  132,  115,  138,  114,  119,  122,  136,  138,  128,  141,  119,  139,  119,  130,  128 },
				}
			}
		}
	}
};

static uint32 countbits( uint32 x )
{
	return FMath::CountBits( x );
}

static uint32 firstbithigh( uint32 x )
{
	return FMath::FloorLog2( x );
}

static int32 BitFieldExtractI32( int32 Data, int32 NumBits, int32 StartBit )
{
	return ( Data << ( 32 - StartBit - NumBits ) ) >> ( 32 - NumBits );
}

static uint32 BitFieldExtractU32( uint32 Data, int32 NumBits, int32 StartBit )
{
	return ( Data << ( 32 - StartBit - NumBits ) ) >> ( 32 - NumBits );
}

static uint32 ReadUnalignedDword( const uint8* SrcPtr, int32 BitOffset )	// Note: Only guarantees 25 valid bits
{
	if( BitOffset < 0 )
	{
		// Workaround for reading slightly out of bounds
		check( BitOffset > -8 );
		return *(const uint32*)( SrcPtr ) << ( 8 - ( BitOffset & 7 ) );
	}
	else
	{
		const uint32* DwordPtr = (const uint32*)( SrcPtr + ( BitOffset >> 3 ) );
		return *DwordPtr >> ( BitOffset & 7 );
	}
}

static void UnpackTriangleIndices( const FStripDesc& StripDesc, const uint8* StripIndexData, uint32 TriIndex, uint32* OutIndices )
{
	const uint32 DwordIndex = TriIndex >> 5;
	const uint32 BitIndex = TriIndex & 31u;

	//Bitmask.x: bIsStart, Bitmask.y: bIsRight, Bitmask.z: bIsNewVertex
	const uint32 SMask = StripDesc.Bitmasks[ DwordIndex ][ 0 ];
	const uint32 LMask = StripDesc.Bitmasks[ DwordIndex ][ 1 ];
	const uint32 WMask = StripDesc.Bitmasks[ DwordIndex ][ 2 ];
	const uint32 SLMask = SMask & LMask;
	
	//const uint HeadRefVertexMask = ( SMask & LMask & WMask ) | ( ~SMask & WMask );
	const uint32 HeadRefVertexMask = ( SLMask | ~SMask ) & WMask;	// 1 if head of triangle is ref. S case with 3 refs or L/R case with 1 ref.

	const uint32 PrevBitsMask = ( 1u << BitIndex ) - 1u;
	const uint32 NumPrevRefVerticesBeforeDword = DwordIndex ? BitFieldExtractU32(StripDesc.NumPrevRefVerticesBeforeDwords, 10u, DwordIndex * 10u - 10u) : 0u;
	const uint32 NumPrevNewVerticesBeforeDword = DwordIndex ? BitFieldExtractU32(StripDesc.NumPrevNewVerticesBeforeDwords, 10u, DwordIndex * 10u - 10u) : 0u;

	int32 CurrentDwordNumPrevRefVertices = ( countbits( SLMask & PrevBitsMask ) << 1 ) + countbits( WMask & PrevBitsMask );
	int32 CurrentDwordNumPrevNewVertices = ( countbits( SMask & PrevBitsMask ) << 1 ) + BitIndex - CurrentDwordNumPrevRefVertices;

	int32 NumPrevRefVertices	= NumPrevRefVerticesBeforeDword + CurrentDwordNumPrevRefVertices;
	int32 NumPrevNewVertices	= NumPrevNewVerticesBeforeDword + CurrentDwordNumPrevNewVertices;

	const int32 IsStart	= BitFieldExtractI32( SMask, 1, BitIndex);		// -1: true, 0: false
	const int32 IsLeft	= BitFieldExtractI32( LMask, 1, BitIndex );		// -1: true, 0: false
	const int32 IsRef	= BitFieldExtractI32( WMask, 1, BitIndex );		// -1: true, 0: false

	const uint32 BaseVertex = NumPrevNewVertices - 1u;

	uint32 IndexData = ReadUnalignedDword( StripIndexData, ( NumPrevRefVertices + ~IsStart ) * 5 );	// -1 if not Start

	if( IsStart )
	{
		const int32 MinusNumRefVertices = ( IsLeft << 1 ) + IsRef;
		uint32 NextVertex = NumPrevNewVertices;

		if( MinusNumRefVertices <= -1 ) { OutIndices[ 0 ] = BaseVertex - ( IndexData & 31u ); IndexData >>= 5; } else { OutIndices[ 0 ] = NextVertex++; }
		if( MinusNumRefVertices <= -2 ) { OutIndices[ 1 ] = BaseVertex - ( IndexData & 31u ); IndexData >>= 5; } else { OutIndices[ 1 ] = NextVertex++; }
		if( MinusNumRefVertices <= -3 ) { OutIndices[ 2 ] = BaseVertex - ( IndexData & 31u );				   } else { OutIndices[ 2 ] = NextVertex++; }
	}
	else
	{
		// Handle two first vertices
		const uint32 PrevBitIndex = BitIndex - 1u;
		const int32 IsPrevStart = BitFieldExtractI32( SMask, 1, PrevBitIndex);
		const int32 IsPrevHeadRef = BitFieldExtractI32( HeadRefVertexMask, 1, PrevBitIndex );
		//const int NumPrevNewVerticesInTriangle = IsPrevStart ? ( 3u - ( bfe_u32( /*SLMask*/ LMask, PrevBitIndex, 1 ) << 1 ) - bfe_u32( /*SMask &*/ WMask, PrevBitIndex, 1 ) ) : /*1u - IsPrevRefVertex*/ 0u;
		const int32 NumPrevNewVerticesInTriangle = IsPrevStart & ( 3u - ( (BitFieldExtractU32( /*SLMask*/ LMask, 1, PrevBitIndex) << 1 ) | BitFieldExtractU32( /*SMask &*/ WMask, 1, PrevBitIndex) ) );
		
		//OutIndices[ 1 ] = IsPrevRefVertex ? ( BaseVertex - ( IndexData & 31u ) + NumPrevNewVerticesInTriangle ) : BaseVertex;	// BaseVertex = ( NumPrevNewVertices - 1 );
		OutIndices[ 1 ] = BaseVertex + ( IsPrevHeadRef & ( NumPrevNewVerticesInTriangle - ( IndexData & 31u ) ) );
		//OutIndices[ 2 ] = IsRefVertex ? ( BaseVertex - bfe_u32( IndexData, 5, 5 ) ) : NumPrevNewVertices;
		OutIndices[ 2 ] = NumPrevNewVertices + ( IsRef & ( -1 - BitFieldExtractU32( IndexData, 5, 5 ) ) );

		// We have to search for the third vertex. 
		// Left triangles search for previous Right/Start. Right triangles search for previous Left/Start.
		const uint32 SearchMask = SMask | ( LMask ^ IsLeft );				// SMask | ( IsRight ? LMask : RMask );
		const uint32 FoundBitIndex = firstbithigh( SearchMask & PrevBitsMask );
		const int32 IsFoundCaseS = BitFieldExtractI32( SMask, 1, FoundBitIndex );		// -1: true, 0: false

		const uint32 FoundPrevBitsMask = ( 1u << FoundBitIndex ) - 1u;
		int32 FoundCurrentDwordNumPrevRefVertices = ( countbits( SLMask & FoundPrevBitsMask ) << 1 ) + countbits( WMask & FoundPrevBitsMask );
		int32 FoundCurrentDwordNumPrevNewVertices = ( countbits( SMask & FoundPrevBitsMask ) << 1 ) + FoundBitIndex - FoundCurrentDwordNumPrevRefVertices;

		int32 FoundNumPrevNewVertices = NumPrevNewVerticesBeforeDword + FoundCurrentDwordNumPrevNewVertices;
		int32 FoundNumPrevRefVertices = NumPrevRefVerticesBeforeDword + FoundCurrentDwordNumPrevRefVertices;

		const uint32 FoundNumRefVertices = (BitFieldExtractU32( LMask, 1, FoundBitIndex ) << 1 ) + BitFieldExtractU32( WMask, 1, FoundBitIndex );
		const uint32 IsBeforeFoundRefVertex = BitFieldExtractU32( HeadRefVertexMask, 1, FoundBitIndex - 1 );

		// ReadOffset: Where is the vertex relative to triangle we searched for?
		const int32 ReadOffset = IsFoundCaseS ? IsLeft : 1;
		const uint32 FoundIndexData = ReadUnalignedDword( StripIndexData, ( FoundNumPrevRefVertices - ReadOffset ) * 5 );
		const uint32 FoundIndex = ( FoundNumPrevNewVertices - 1u ) - BitFieldExtractU32( FoundIndexData, 5, 0 );

		bool bCondition = IsFoundCaseS ? ( (int32)FoundNumRefVertices >= 1 - IsLeft ) : (IsBeforeFoundRefVertex != 0u);
		int32 FoundNewVertex = FoundNumPrevNewVertices + ( IsFoundCaseS ? ( IsLeft & ( FoundNumRefVertices == 0 ) ) : -1 );
		OutIndices[ 0 ] = bCondition ? FoundIndex : FoundNewVertex;
		
		// Would it be better to code New verts instead of Ref verts?
		// HeadRefVertexMask would just be WMask?
		
		// TODO: could we do better with non-generalized strips?

		/*
		if( IsFoundCaseS )
		{
			if( IsRight )
			{
				OutIndices[ 0 ] = ( FoundNumRefVertices >= 1 ) ? FoundIndex : FoundNumPrevNewVertices;
				// OutIndices[ 0 ] = ( FoundNumRefVertices >= 1 ) ? ( FoundBaseVertex - Cluster.StripIndices[ FoundNumPrevRefVertices ] ) : FoundNumPrevNewVertices;
			}
			else
			{
				OutIndices[ 0 ] = ( FoundNumRefVertices >= 2 ) ? FoundIndex : ( FoundNumPrevNewVertices + ( FoundNumRefVertices == 0 ? 1 : 0 ) );
				// OutIndices[ 0 ] = ( FoundNumRefVertices >= 2 ) ? ( FoundBaseVertex - Cluster.StripIndices[ FoundNumPrevRefVertices + 1 ] ) : ( FoundNumPrevNewVertices + ( FoundNumRefVertices == 0 ? 1 : 0 ) );
			}
		}
		else
		{
			OutIndices[ 0 ] = IsBeforeFoundRefVertex ? FoundIndex : ( FoundNumPrevNewVertices - 1 );
			// OutIndices[ 0 ] = IsBeforeFoundRefVertex ? ( FoundBaseVertex - Cluster.StripIndices[ FoundNumPrevRefVertices - 1 ] ) : ( FoundNumPrevNewVertices - 1 );
		}
		*/

		if( IsLeft )
		{
			// swap
			std::swap( OutIndices[ 1 ], OutIndices[ 2 ] );
		}
		check(OutIndices[0] != OutIndices[1] && OutIndices[0] != OutIndices[2] && OutIndices[1] != OutIndices[2]);
	}
}

// Class to simultaneously constrain and stripify a cluster
class FStripifier
{
	static const uint32 MAX_CLUSTER_TRIANGLES_IN_DWORDS = (NANITE_MAX_CLUSTER_TRIANGLES + 31 ) / 32;
	static const uint32 INVALID_INDEX = 0xFFFFu;
	static const uint32 INVALID_CORNER = 0xFFFFu;
	static const uint32 INVALID_NODE = 0xFFFFu;
	static const uint32 INVALID_NODE_MEMSET = 0xFFu;

	uint32 VertexToTriangleMasks[NANITE_MAX_CLUSTER_TRIANGLES * 3 ][ MAX_CLUSTER_TRIANGLES_IN_DWORDS ];
	uint16 OppositeCorner[NANITE_MAX_CLUSTER_TRIANGLES * 3 ];
	float TrianglePriorities[NANITE_MAX_CLUSTER_TRIANGLES ];

	class FContext
	{
	public:
		bool TriangleEnabled( uint32 TriangleIndex ) const
		{
			return ( TrianglesEnabled[ TriangleIndex >> 5 ] & ( 1u << ( TriangleIndex & 31u ) ) ) != 0u;
		}

		uint16 OldToNewVertex[NANITE_MAX_CLUSTER_TRIANGLES * 3 ];
		uint16 NewToOldVertex[NANITE_MAX_CLUSTER_TRIANGLES * 3 ];

		uint32 TrianglesEnabled[ MAX_CLUSTER_TRIANGLES_IN_DWORDS ];	// Enabled triangles are in the current material range and have not yet been visited.
		uint32 TrianglesTouched[ MAX_CLUSTER_TRIANGLES_IN_DWORDS ];	// Touched triangles have had at least one of their vertices visited.

		uint32 StripBitmasks[ 4 ][ 3 ];	// [4][Reset, IsLeft, IsRef]

		uint32 NumTriangles;
		uint32 NumVertices;
	};

	void BuildTables( const FCluster& Cluster )
	{
		struct FEdgeNode
		{
			uint16 Corner;	// (Triangle << 2) | LocalCorner
			uint16 NextNode;
		};

		FEdgeNode EdgeNodes[NANITE_MAX_CLUSTER_INDICES ];
		uint16 EdgeNodeHeads[NANITE_MAX_CLUSTER_INDICES * NANITE_MAX_CLUSTER_INDICES ];	// Linked list per edge to support more than 2 triangles per edge.
		FMemory::Memset( EdgeNodeHeads, INVALID_NODE_MEMSET );

		FMemory::Memset( VertexToTriangleMasks, 0 );

		uint32 NumTriangles = Cluster.NumTris;
		uint32 NumVertices = Cluster.NumVerts;

		// Add triangles to edge lists and update valence
		for( uint32 i = 0; i < NumTriangles; i++ )
		{
			uint32 i0 = Cluster.Indexes[ i * 3 + 0 ];
			uint32 i1 = Cluster.Indexes[ i * 3 + 1 ];
			uint32 i2 = Cluster.Indexes[ i * 3 + 2 ];
			check( i0 != i1 && i1 != i2 && i2 != i0 );
			check( i0 < NumVertices && i1 < NumVertices && i2 < NumVertices );

			VertexToTriangleMasks[ i0 ][ i >> 5 ] |= 1 << ( i & 31 );
			VertexToTriangleMasks[ i1 ][ i >> 5 ] |= 1 << ( i & 31 );
			VertexToTriangleMasks[ i2 ][ i >> 5 ] |= 1 << ( i & 31 );

			FVector3f ScaledCenter = Cluster.GetPosition( i0 ) + Cluster.GetPosition( i1 ) + Cluster.GetPosition( i2 );
			TrianglePriorities[ i ] = ScaledCenter.X;	//TODO: Find a good direction to sort by instead of just picking x?

			FEdgeNode& Node0 = EdgeNodes[ i * 3 + 0 ];
			Node0.Corner = (uint16)SetCorner( i, 0 );
			Node0.NextNode = EdgeNodeHeads[ i1 * NANITE_MAX_CLUSTER_INDICES + i2 ];
			EdgeNodeHeads[ i1 * NANITE_MAX_CLUSTER_INDICES + i2 ] = uint16(i * 3 + 0);

			FEdgeNode& Node1 = EdgeNodes[ i * 3 + 1 ];
			Node1.Corner = (uint16)SetCorner( i, 1 );
			Node1.NextNode = EdgeNodeHeads[ i2 * NANITE_MAX_CLUSTER_INDICES + i0 ];
			EdgeNodeHeads[ i2 * NANITE_MAX_CLUSTER_INDICES + i0 ] = uint16(i * 3 + 1);

			FEdgeNode& Node2 = EdgeNodes[ i * 3 + 2 ];
			Node2.Corner = (uint16)SetCorner( i, 2 );
			Node2.NextNode = EdgeNodeHeads[ i0 * NANITE_MAX_CLUSTER_INDICES + i1 ];
			EdgeNodeHeads[ i0 * NANITE_MAX_CLUSTER_INDICES + i1 ] = uint16(i * 3 + 2);
		}

		// Gather adjacency from edge lists	
		for( uint32 i = 0; i < NumTriangles; i++ )
		{
			uint32 i0 = Cluster.Indexes[ i * 3 + 0 ];
			uint32 i1 = Cluster.Indexes[ i * 3 + 1 ];
			uint32 i2 = Cluster.Indexes[ i * 3 + 2 ];

			uint16& Node0 = EdgeNodeHeads[ i2 * NANITE_MAX_CLUSTER_INDICES + i1 ];
			uint16& Node1 = EdgeNodeHeads[ i0 * NANITE_MAX_CLUSTER_INDICES + i2 ];
			uint16& Node2 = EdgeNodeHeads[ i1 * NANITE_MAX_CLUSTER_INDICES + i0 ];
			if( Node0 != INVALID_NODE ) { OppositeCorner[ i * 3 + 0 ] = EdgeNodes[ Node0 ].Corner; Node0 = EdgeNodes[ Node0 ].NextNode; }
			else { OppositeCorner[ i * 3 + 0 ] = INVALID_CORNER; }
			if( Node1 != INVALID_NODE ) { OppositeCorner[ i * 3 + 1 ] = EdgeNodes[ Node1 ].Corner; Node1 = EdgeNodes[ Node1 ].NextNode; }
			else { OppositeCorner[ i * 3 + 1 ] = INVALID_CORNER; }
			if( Node2 != INVALID_NODE ) { OppositeCorner[ i * 3 + 2 ] = EdgeNodes[ Node2 ].Corner; Node2 = EdgeNodes[ Node2 ].NextNode; }
			else { OppositeCorner[ i * 3 + 2 ] = INVALID_CORNER; }
		}

		// Generate vertex to triangle masks
		for( uint32 i = 0; i < NumTriangles; i++ )
		{
			uint32 i0 = Cluster.Indexes[ i * 3 + 0 ];
			uint32 i1 = Cluster.Indexes[ i * 3 + 1 ];
			uint32 i2 = Cluster.Indexes[ i * 3 + 2 ];
			check( i0 != i1 && i1 != i2 && i2 != i0 );

			VertexToTriangleMasks[ i0 ][ i >> 5 ] |= 1 << ( i & 31 );
			VertexToTriangleMasks[ i1 ][ i >> 5 ] |= 1 << ( i & 31 );
			VertexToTriangleMasks[ i2 ][ i >> 5 ] |= 1 << ( i & 31 );
		}
	}

public:
	void ConstrainAndStripifyCluster( FCluster& Cluster )
	{
		const FStripifyWeights& Weights = DefaultStripifyWeights;
		uint32 NumOldTriangles = Cluster.NumTris;
		uint32 NumOldVertices = Cluster.NumVerts;

		BuildTables( Cluster );

		uint32 NumStrips = 0;

		FContext Context = {};
		FMemory::Memset( Context.OldToNewVertex, -1 );

		auto NewScoreVertex = [ &Weights ] ( const FContext& Context, uint32 OldVertex, bool bStart, bool bHasOpposite, bool bHasLeft, bool bHasRight )
		{
			uint16 NewIndex = Context.OldToNewVertex[ OldVertex ];

			int32 CacheScore = 0;
			if( NewIndex != INVALID_INDEX )
			{
				uint32 CachePosition = ( Context.NumVertices - 1 ) - NewIndex;
				if( CachePosition < CONSTRAINED_CLUSTER_CACHE_SIZE )
					CacheScore = Weights.Weights[ bStart ][ bHasOpposite ][ bHasLeft ][ bHasRight ][ CachePosition ];
			}

			return CacheScore;
		};

		auto NewScoreTriangle = [ &Cluster, &NewScoreVertex ] ( const FContext& Context, uint32 TriangleIndex, bool bStart, bool bHasOpposite, bool bHasLeft, bool bHasRight )
		{
			const uint32 OldIndex0 = Cluster.Indexes[ TriangleIndex * 3 + 0 ];
			const uint32 OldIndex1 = Cluster.Indexes[ TriangleIndex * 3 + 1 ];
			const uint32 OldIndex2 = Cluster.Indexes[ TriangleIndex * 3 + 2 ];

			return	NewScoreVertex( Context, OldIndex0, bStart, bHasOpposite, bHasLeft, bHasRight ) +
					NewScoreVertex( Context, OldIndex1, bStart, bHasOpposite, bHasLeft, bHasRight ) +
					NewScoreVertex( Context, OldIndex2, bStart, bHasOpposite, bHasLeft, bHasRight );
		};

		auto VisitTriangle = [ this, &Cluster ] ( FContext& Context, uint32 TriangleCorner, bool bStart, bool bRight)
		{
			const uint32 OldIndex0 = Cluster.Indexes[ CornerToIndex( NextCorner( TriangleCorner ) ) ];
			const uint32 OldIndex1 = Cluster.Indexes[ CornerToIndex( PrevCorner( TriangleCorner ) ) ];
			const uint32 OldIndex2 = Cluster.Indexes[ CornerToIndex( TriangleCorner ) ];

			// Mark incident triangles
			for( uint32 i = 0; i < MAX_CLUSTER_TRIANGLES_IN_DWORDS; i++ )
			{
				Context.TrianglesTouched[ i ] |= VertexToTriangleMasks[ OldIndex0 ][ i ] | VertexToTriangleMasks[ OldIndex1 ][ i ] | VertexToTriangleMasks[ OldIndex2 ][ i ];
			}

			uint16& NewIndex0 = Context.OldToNewVertex[ OldIndex0 ];
			uint16& NewIndex1 = Context.OldToNewVertex[ OldIndex1 ];
			uint16& NewIndex2 = Context.OldToNewVertex[ OldIndex2 ];

			uint32 OrgIndex0 = NewIndex0;
			uint32 OrgIndex1 = NewIndex1;
			uint32 OrgIndex2 = NewIndex2;

			uint32 NextVertexIndex = Context.NumVertices + ( NewIndex0 == INVALID_INDEX ) + ( NewIndex1 == INVALID_INDEX ) + ( NewIndex2 == INVALID_INDEX );
			while(true)
			{
				if( NewIndex0 != INVALID_INDEX && NextVertexIndex - NewIndex0 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) { NewIndex0 = INVALID_INDEX; NextVertexIndex++; continue; }
				if( NewIndex1 != INVALID_INDEX && NextVertexIndex - NewIndex1 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) { NewIndex1 = INVALID_INDEX; NextVertexIndex++; continue; }
				if( NewIndex2 != INVALID_INDEX && NextVertexIndex - NewIndex2 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) { NewIndex2 = INVALID_INDEX; NextVertexIndex++; continue; }
				break;
			}

			uint32 NewTriangleIndex = Context.NumTriangles;
			uint32 NumNewVertices = ( NewIndex0 == INVALID_INDEX ) + ( NewIndex1 == INVALID_INDEX ) + ( NewIndex2 == INVALID_INDEX );
			if( bStart )
			{
				check( ( NewIndex2 == INVALID_INDEX ) >= ( NewIndex1 == INVALID_INDEX ) );
				check( ( NewIndex1 == INVALID_INDEX ) >= ( NewIndex0 == INVALID_INDEX ) );


				uint32 NumWrittenIndices = 3u - NumNewVertices;
				uint32 LowBit = NumWrittenIndices & 1u;
				uint32 HighBit = (NumWrittenIndices >> 1) & 1u;

				Context.StripBitmasks[ NewTriangleIndex >> 5 ][ 0 ] |= ( 1u << ( NewTriangleIndex & 31u ) );
				Context.StripBitmasks[ NewTriangleIndex >> 5 ][ 1 ] |= ( HighBit << ( NewTriangleIndex & 31u ) );
				Context.StripBitmasks[ NewTriangleIndex >> 5 ][ 2 ] |= ( LowBit << ( NewTriangleIndex & 31u ) );
			}
			else
			{
				check( NewIndex0 != INVALID_INDEX );
				check( NewIndex1 != INVALID_INDEX );
				if( !bRight )
				{
					Context.StripBitmasks[ NewTriangleIndex >> 5 ][ 1 ] |= ( 1 << ( NewTriangleIndex & 31u ) );
				}

				if(NewIndex2 != INVALID_INDEX)
				{
					Context.StripBitmasks[ NewTriangleIndex >> 5 ][ 2 ] |= ( 1 << ( NewTriangleIndex & 31u ) );
				}
			}

			if( NewIndex0 == INVALID_INDEX ) { NewIndex0 = uint16(Context.NumVertices++); Context.NewToOldVertex[ NewIndex0 ] = uint16(OldIndex0); }
			if( NewIndex1 == INVALID_INDEX ) { NewIndex1 = uint16(Context.NumVertices++); Context.NewToOldVertex[ NewIndex1 ] = uint16(OldIndex1); }
			if( NewIndex2 == INVALID_INDEX ) { NewIndex2 = uint16(Context.NumVertices++); Context.NewToOldVertex[ NewIndex2 ] = uint16(OldIndex2); }

			// Output triangle
			Context.NumTriangles++;

			// Disable selected triangle
			const uint32 OldTriangleIndex = CornerToTriangle( TriangleCorner );
			Context.TrianglesEnabled[ OldTriangleIndex >> 5 ] &= ~( 1 << ( OldTriangleIndex & 31u ) );
			return NumNewVertices;
		};

		Cluster.StripIndexData.Empty();
		FBitWriter BitWriter( Cluster.StripIndexData );
		FStripDesc& StripDesc = Cluster.StripDesc;
		FMemory::Memset(StripDesc, 0);
		uint32 NumNewVerticesInDword[ 4 ] = {};
		uint32 NumRefVerticesInDword[ 4 ] = {};

		uint32 RangeStart = 0;
		for( const FMaterialRange& MaterialRange : Cluster.MaterialRanges )
		{
			check( RangeStart == MaterialRange.RangeStart );
			uint32 RangeLength = MaterialRange.RangeLength;

			// Enable triangles from current range
			for( uint32 i = 0; i < MAX_CLUSTER_TRIANGLES_IN_DWORDS; i++ )
			{
				int32 RangeStartRelativeToDword = (int32)RangeStart - (int32)i * 32;
				int32 BitStart = FMath::Max( RangeStartRelativeToDword, 0 );
				int32 BitEnd = FMath::Max( RangeStartRelativeToDword + (int32)RangeLength, 0 );
				uint32 StartMask = BitStart < 32 ? ( ( 1u << BitStart ) - 1u ) : 0xFFFFFFFFu;
				uint32 EndMask = BitEnd < 32 ? ( ( 1u << BitEnd ) - 1u ) : 0xFFFFFFFFu;
				Context.TrianglesEnabled[ i ] |= StartMask ^ EndMask;
			}

			// While a strip can be started
			while( true )
			{
				// Pick a start location for the strip
				uint32 StartCorner = INVALID_CORNER;
				int32 BestScore = -1;
				float BestPriority = INT_MIN;
				{
					for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MAX_CLUSTER_TRIANGLES_IN_DWORDS; TriangleDwordIndex++ )
					{
						uint32 CandidateMask = Context.TrianglesEnabled[ TriangleDwordIndex ];
						while( CandidateMask )
						{
							uint32 TriangleIndex = ( TriangleDwordIndex << 5 ) + FMath::CountTrailingZeros( CandidateMask );
							CandidateMask &= CandidateMask - 1u;

							for( uint32 Corner = 0; Corner < 3; Corner++ )
							{
								uint32 TriangleCorner = SetCorner( TriangleIndex, Corner );

								{
									// Is it viable WRT the constraint that new vertices should always be at the end.
									uint32 OldIndex0 = Cluster.Indexes[ CornerToIndex( NextCorner( TriangleCorner ) ) ];
									uint32 OldIndex1 = Cluster.Indexes[ CornerToIndex( PrevCorner( TriangleCorner ) ) ];
									uint32 OldIndex2 = Cluster.Indexes[ CornerToIndex( TriangleCorner ) ];

									uint32 NewIndex0 = Context.OldToNewVertex[ OldIndex0 ];
									uint32 NewIndex1 = Context.OldToNewVertex[ OldIndex1 ];
									uint32 NewIndex2 = Context.OldToNewVertex[ OldIndex2 ];
									uint32 NumVerts = Context.NumVertices + ( NewIndex0 == INVALID_INDEX ) + ( NewIndex1 == INVALID_INDEX ) + ( NewIndex2 == INVALID_INDEX );
									while(true)
									{
										if( NewIndex0 != INVALID_INDEX && NumVerts - NewIndex0 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) { NewIndex0 = INVALID_INDEX; NumVerts++; continue; }
										if( NewIndex1 != INVALID_INDEX && NumVerts - NewIndex1 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) { NewIndex1 = INVALID_INDEX; NumVerts++; continue; }
										if( NewIndex2 != INVALID_INDEX && NumVerts - NewIndex2 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) { NewIndex2 = INVALID_INDEX; NumVerts++; continue; }
										break;
									} 

									uint32 Mask = ( NewIndex0 == INVALID_INDEX ? 1u : 0u ) | ( NewIndex1 == INVALID_INDEX ? 2u : 0u ) | ( NewIndex2 == INVALID_INDEX ? 4u : 0u );

									if( Mask != 0u && Mask != 4u && Mask != 6u && Mask != 7u )
									{
										continue;
									}	
								}


								uint32 Opposite = OppositeCorner[ CornerToIndex( TriangleCorner ) ];
								uint32 LeftCorner = OppositeCorner[ CornerToIndex( NextCorner( TriangleCorner ) ) ];
								uint32 RightCorner = OppositeCorner[ CornerToIndex( PrevCorner( TriangleCorner ) ) ];

								bool bHasOpposite = Opposite != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( Opposite ) );
								bool bHasLeft = LeftCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( LeftCorner ) );
								bool bHasRight = RightCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( RightCorner ) );

								int32 Score = NewScoreTriangle( Context, TriangleIndex, true, bHasOpposite, bHasLeft, bHasRight );
								if( Score > BestScore )
								{
									StartCorner = TriangleCorner;
									BestScore = Score;
								}
								else if( Score == BestScore )
								{
									float Priority = TrianglePriorities[ TriangleIndex ];
									if( Priority > BestPriority )
									{
										StartCorner = TriangleCorner;
										BestScore = Score;
										BestPriority = Priority;
									}
								}
							}
						}
					}

					if( StartCorner == INVALID_CORNER )
						break;
				}

				uint32 StripLength = 1;

				{
					uint32 TriangleDword = Context.NumTriangles >> 5;
					uint32 BaseVertex = Context.NumVertices - 1;
					uint32 NumNewVertices = VisitTriangle( Context, StartCorner, true, false );

					if( NumNewVertices < 3 )
					{
						uint32 Index = Context.OldToNewVertex[ Cluster.Indexes[ CornerToIndex( NextCorner( StartCorner ) ) ] ];
						BitWriter.PutBits( BaseVertex - Index, 5 );
					}
					if( NumNewVertices < 2 )
					{
						uint32 Index = Context.OldToNewVertex[ Cluster.Indexes[ CornerToIndex( PrevCorner( StartCorner ) ) ] ];
						BitWriter.PutBits( BaseVertex - Index, 5 );
					}
					if( NumNewVertices < 1 )
					{
						uint32 Index = Context.OldToNewVertex[ Cluster.Indexes[ CornerToIndex( StartCorner ) ] ];
						BitWriter.PutBits( BaseVertex - Index, 5 );
					}
					NumNewVerticesInDword[ TriangleDword ] += NumNewVertices;
					NumRefVerticesInDword[ TriangleDword ] += 3u - NumNewVertices;
				}

				// Extend strip as long as we can
				uint32 CurrentCorner = StartCorner;
				while( true )
				{
					if( ( Context.NumTriangles & 31u ) == 0u )
						break;

					uint32 LeftCorner = OppositeCorner[ CornerToIndex( NextCorner( CurrentCorner ) ) ];
					uint32 RightCorner = OppositeCorner[ CornerToIndex( PrevCorner( CurrentCorner ) ) ];
					CurrentCorner = INVALID_CORNER;

					int32 LeftScore = INT_MIN;
					if( LeftCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( LeftCorner ) ) )
					{
						uint32 LeftLeftCorner = OppositeCorner[ CornerToIndex( NextCorner( LeftCorner ) ) ];
						uint32 LeftRightCorner = OppositeCorner[ CornerToIndex( PrevCorner( LeftCorner ) ) ];
						bool bLeftLeftCorner = LeftLeftCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( LeftLeftCorner ) );
						bool bLeftRightCorner = LeftRightCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( LeftRightCorner ) );

						LeftScore = NewScoreTriangle( Context, CornerToTriangle( LeftCorner ), false, true, bLeftLeftCorner, bLeftRightCorner );
						CurrentCorner = LeftCorner;
					}

					bool bIsRight = false;
					if( RightCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( RightCorner ) ) )
					{
						uint32 RightLeftCorner = OppositeCorner[ CornerToIndex( NextCorner( RightCorner ) ) ];
						uint32 RightRightCorner = OppositeCorner[ CornerToIndex( PrevCorner( RightCorner ) ) ];
						bool bRightLeftCorner = RightLeftCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( RightLeftCorner ) );
						bool bRightRightCorner = RightRightCorner != INVALID_CORNER && Context.TriangleEnabled( CornerToTriangle( RightRightCorner ) );

						int32 Score = NewScoreTriangle( Context, CornerToTriangle( RightCorner ), false, false, bRightLeftCorner, bRightRightCorner );
						if( Score > LeftScore )
						{
							CurrentCorner = RightCorner;
							bIsRight = true;
						}
					}

					if( CurrentCorner == INVALID_CORNER )
						break;

					{
						const uint32 OldIndex0 = Cluster.Indexes[ CornerToIndex( NextCorner( CurrentCorner ) ) ];
						const uint32 OldIndex1 = Cluster.Indexes[ CornerToIndex( PrevCorner( CurrentCorner ) ) ];
						const uint32 OldIndex2 = Cluster.Indexes[ CornerToIndex( CurrentCorner ) ];

						const uint32 NewIndex0 = Context.OldToNewVertex[ OldIndex0 ];
						const uint32 NewIndex1 = Context.OldToNewVertex[ OldIndex1 ];
						const uint32 NewIndex2 = Context.OldToNewVertex[ OldIndex2 ];

						check( NewIndex0 != INVALID_INDEX );
						check( NewIndex1 != INVALID_INDEX );
						const uint32 NextNumVertices = Context.NumVertices + ( ( NewIndex2 == INVALID_INDEX || Context.NumVertices - NewIndex2 >= CONSTRAINED_CLUSTER_CACHE_SIZE ) ? 1u : 0u );

						if( NextNumVertices - NewIndex0 >= CONSTRAINED_CLUSTER_CACHE_SIZE ||
							NextNumVertices - NewIndex1 >= CONSTRAINED_CLUSTER_CACHE_SIZE )
							break;
					}

					{
						uint32 TriangleDword = Context.NumTriangles >> 5;
						uint32 BaseVertex = Context.NumVertices - 1;
						uint32 NumNewVertices = VisitTriangle( Context, CurrentCorner, false, bIsRight );
						check(NumNewVertices <= 1u);
						if( NumNewVertices == 0 )
						{
							uint32 Index = Context.OldToNewVertex[ Cluster.Indexes[ CornerToIndex( CurrentCorner ) ] ];
							BitWriter.PutBits( BaseVertex - Index, 5 );
						}
						NumNewVerticesInDword[ TriangleDword ] += NumNewVertices;
						NumRefVerticesInDword[ TriangleDword ] += 1u - NumNewVertices;
					}

					StripLength++;
				}
			}
			RangeStart += RangeLength;
		}

		BitWriter.Flush(sizeof(uint32));
		
		// Reorder vertices
		const uint32 NumNewVertices = Context.NumVertices;

		TArray< float > OldVertices;
		Swap( OldVertices, Cluster.Verts );

		uint32 VertStride = Cluster.GetVertSize();
		Cluster.Verts.AddUninitialized( NumNewVertices * VertStride );
		for( uint32 i = 0; i < NumNewVertices; i++ )
		{
			FMemory::Memcpy( &Cluster.GetPosition(i), &OldVertices[ Context.NewToOldVertex[ i ] * VertStride ], VertStride * sizeof( float ) );
		}

		check( Context.NumTriangles == NumOldTriangles );

		Cluster.NumVerts = Context.NumVertices;
		
		uint32 NumPrevNewVerticesBeforeDwords1 = NumNewVerticesInDword[ 0 ];
		uint32 NumPrevNewVerticesBeforeDwords2 = NumNewVerticesInDword[ 1 ] + NumPrevNewVerticesBeforeDwords1;
		uint32 NumPrevNewVerticesBeforeDwords3 = NumNewVerticesInDword[ 2 ] + NumPrevNewVerticesBeforeDwords2;
		check(NumPrevNewVerticesBeforeDwords1 < 1024 && NumPrevNewVerticesBeforeDwords2 < 1024 && NumPrevNewVerticesBeforeDwords3 < 1024);
		StripDesc.NumPrevNewVerticesBeforeDwords = ( NumPrevNewVerticesBeforeDwords3 << 20 ) | ( NumPrevNewVerticesBeforeDwords2 << 10 ) | NumPrevNewVerticesBeforeDwords1;

		uint32 NumPrevRefVerticesBeforeDwords1 = NumRefVerticesInDword[0];
		uint32 NumPrevRefVerticesBeforeDwords2 = NumRefVerticesInDword[1] + NumPrevRefVerticesBeforeDwords1;
		uint32 NumPrevRefVerticesBeforeDwords3 = NumRefVerticesInDword[2] + NumPrevRefVerticesBeforeDwords2;
		check( NumPrevRefVerticesBeforeDwords1 < 1024 && NumPrevRefVerticesBeforeDwords2 < 1024 && NumPrevRefVerticesBeforeDwords3 < 1024);
		StripDesc.NumPrevRefVerticesBeforeDwords = (NumPrevRefVerticesBeforeDwords3 << 20) | (NumPrevRefVerticesBeforeDwords2 << 10) | NumPrevRefVerticesBeforeDwords1;

		static_assert(sizeof(StripDesc.Bitmasks) == sizeof(Context.StripBitmasks), "");
		FMemory::Memcpy( StripDesc.Bitmasks, Context.StripBitmasks, sizeof(StripDesc.Bitmasks) );

		const uint32 PaddedSize = Cluster.StripIndexData.Num() + 5;
		TArray<uint8> PaddedStripIndexData;
		PaddedStripIndexData.Reserve( PaddedSize );

		PaddedStripIndexData.Add( 0 );	// TODO: Workaround for empty list and reading from negative offset
		PaddedStripIndexData.Append( Cluster.StripIndexData );

		// UnpackTriangleIndices is 1:1 with the GPU implementation.
		// It can end up over-fetching because it is branchless. The over-fetched data is never actually used.
		// On the GPU index data is followed by other page data, so it is safe.
		
		// Here we have to pad to make it safe to perform a DWORD read after the end.
		PaddedStripIndexData.SetNumZeroed( PaddedSize );

		// Unpack strip
		for( uint32 i = 0; i < NumOldTriangles; i++ )
		{
			UnpackTriangleIndices( StripDesc, (const uint8*)(PaddedStripIndexData.GetData() + 1), i, &Cluster.Indexes[ i * 3 ] );
		}
	}
};

static void BuildClusterFromClusterTriangleRange( const FCluster& InCluster, FCluster& OutCluster, uint32 StartTriangle, uint32 NumTriangles )
{
	OutCluster = InCluster;
	OutCluster.Indexes.Empty();
	OutCluster.MaterialIndexes.Empty();
	OutCluster.MaterialRanges.Empty();

	// Copy triangle indices and material indices.
	// Ignore that some of the vertices will no longer be referenced as that will be cleaned up in ConstrainCluster* pass
	OutCluster.Indexes.SetNumUninitialized( NumTriangles * 3 );
	OutCluster.MaterialIndexes.SetNumUninitialized( NumTriangles );
	for( uint32 i = 0; i < NumTriangles; i++ )
	{
		uint32 TriangleIndex = StartTriangle + i;
			
		OutCluster.MaterialIndexes[ i ] = InCluster.MaterialIndexes[ TriangleIndex ];
		OutCluster.Indexes[ i * 3 + 0 ] = InCluster.Indexes[ TriangleIndex * 3 + 0 ];
		OutCluster.Indexes[ i * 3 + 1 ] = InCluster.Indexes[ TriangleIndex * 3 + 1 ];
		OutCluster.Indexes[ i * 3 + 2 ] = InCluster.Indexes[ TriangleIndex * 3 + 2 ];
	}

	OutCluster.NumTris = NumTriangles;

	// Rebuild material range and reconstrain 
	BuildMaterialRanges( OutCluster );
#if NANITE_USE_STRIP_INDICES
	FStripifier Stripifier;
	Stripifier.ConstrainAndStripifyCluster(OutCluster);
#else
	ConstrainClusterFIFO(OutCluster);
#endif
}

#if 0
// Dump Cluster to .obj for debugging
static void DumpClusterToObj( const char* Filename, const FCluster& Cluster)
{
	FILE* File = nullptr;
	fopen_s( &File, Filename, "wb" );

	for( const VertType& Vert : Cluster.Verts )
	{
		fprintf( File, "v %f %f %f\n", Vert.Position.X, Vert.Position.Y, Vert.Position.Z );
	}

	uint32 NumRanges = Cluster.MaterialRanges.Num();
	uint32 NumTriangles = Cluster.Indexes.Num() / 3;
	for( uint32 RangeIndex = 0; RangeIndex < NumRanges; RangeIndex++ )
	{
		const FMaterialRange& MaterialRange = Cluster.MaterialRanges[ RangeIndex ];
		fprintf( File, "newmtl range%d\n", RangeIndex );
		float r = ( RangeIndex + 0.5f ) / NumRanges;
		fprintf( File, "Kd %f %f %f\n", r, 0.0f, 0.0f );
		fprintf( File, "Ks 0.0, 0.0, 0.0\n" );
		fprintf( File, "Ns 18.0\n" );
		fprintf( File, "usemtl range%d\n", RangeIndex );
		for( uint32 i = 0; i < MaterialRange.RangeLength; i++ )
		{
			uint32 TriangleIndex = MaterialRange.RangeStart + i;
			fprintf( File, "f %d %d %d\n", Cluster.Indexes[ TriangleIndex * 3 + 0 ] + 1, Cluster.Indexes[ TriangleIndex * 3 + 1 ] + 1, Cluster.Indexes[ TriangleIndex * 3 + 2 ] + 1 );
		}
	}

	fclose( File );
}

static void DumpClusterNormals(const char* Filename, const FCluster& Cluster)
{
	uint32 NumVertices = Cluster.NumVerts;
	TArray<FIntPoint> Points;
	Points.SetNumUninitialized(NumVertices);
	for (uint32 i = 0; i < NumVertices; i++)
	{
		OctahedronEncodePreciseSIMD(Cluster.Verts[i].Normal, Points[i].X, Points[i].Y, NANITE_NORMAL_QUANTIZATION_BITS);
	}


	FILE* File = nullptr;
	fopen_s(&File, Filename, "wb");
	fputs(	"import numpy as np\n"
			"import matplotlib.pyplot as plt\n\n",
			File);
	fputs("x = [", File);
	for (uint32 i = 0; i < NumVertices; i++)
	{
		fprintf(File, "%d", Points[i].X);
		if (i + 1 != NumVertices)
			fputs(", ", File);
	}
	fputs("]\n", File);
	fputs("y = [", File);
	for (uint32 i = 0; i < NumVertices; i++)
	{
		fprintf(File, "%d", Points[i].Y);
		if (i + 1 != NumVertices)
			fputs(", ", File);
	}
	fputs("]\n", File);
	fputs(	"plt.xlim(0, 511)\n"
			"plt.ylim(0, 511)\n"
			"plt.scatter(x, y)\n"
			"plt.xlabel('x')\n"
			"plt.ylabel('y')\n"
			"plt.show()\n",
			File);
	fclose(File);
}

static void DumpClusterNormals(const char* Filename, const TArray<FCluster>& Clusters)
{
	for (int32 i = 0; i < Clusters.Num(); i++)
	{
		char Filename[128];
		static int Index = 0;
		sprintf(Filename, "D:\\NormalPlots\\plot%d.py", Index++);
		DumpClusterNormals(Filename, Clusters[i]);
	}
}
#endif

// Remove degenerate triangles
static void RemoveDegenerateTriangles(FCluster& Cluster)
{
	uint32 NumOldTriangles = Cluster.NumTris;
	uint32 NumNewTriangles = 0;

	for (uint32 OldTriangleIndex = 0; OldTriangleIndex < NumOldTriangles; OldTriangleIndex++)
	{
		uint32 i0 = Cluster.Indexes[OldTriangleIndex * 3 + 0];
		uint32 i1 = Cluster.Indexes[OldTriangleIndex * 3 + 1];
		uint32 i2 = Cluster.Indexes[OldTriangleIndex * 3 + 2];
		uint32 mi = Cluster.MaterialIndexes[OldTriangleIndex];

		if (i0 != i1 && i0 != i2 && i1 != i2)
		{
			Cluster.Indexes[NumNewTriangles * 3 + 0] = i0;
			Cluster.Indexes[NumNewTriangles * 3 + 1] = i1;
			Cluster.Indexes[NumNewTriangles * 3 + 2] = i2;
			Cluster.MaterialIndexes[NumNewTriangles] = mi;

			NumNewTriangles++;
		}
	}
	Cluster.NumTris = NumNewTriangles;
	Cluster.Indexes.SetNum(NumNewTriangles * 3);
	Cluster.MaterialIndexes.SetNum(NumNewTriangles);
}

static void RemoveDegenerateTriangles(TArray<FCluster>& Clusters)
{
	ParallelFor(TEXT("NaniteEncode.RemoveDegenerateTriangles.PF"), Clusters.Num(), 512,
		[&]( uint32 ClusterIndex )
		{
			RemoveDegenerateTriangles( Clusters[ ClusterIndex ] );
		} );
}

static void ConstrainClusters( TArray< FClusterGroup >& ClusterGroups, TArray< FCluster >& Clusters )
{
	// Calculate stats
	uint32 TotalOldTriangles = 0;
	uint32 TotalOldVertices = 0;
	for( const FCluster& Cluster : Clusters )
	{
		TotalOldTriangles += Cluster.NumTris;
		TotalOldVertices += Cluster.NumVerts;
	}

	ParallelFor(TEXT("NaniteEncode.ConstrainClusters.PF"), Clusters.Num(), 8,
		[&]( uint32 i )
		{
#if NANITE_USE_STRIP_INDICES
			FStripifier Stripifier;
			Stripifier.ConstrainAndStripifyCluster(Clusters[i]);
#else
			ConstrainClusterFIFO(Clusters[i]);
#endif
		} );
	
	uint32 TotalNewTriangles = 0;
	uint32 TotalNewVertices = 0;

	// Constrain clusters
	const uint32 NumOldClusters = Clusters.Num();
	for( uint32 i = 0; i < NumOldClusters; i++ )
	{
		TotalNewTriangles += Clusters[ i ].NumTris;
		TotalNewVertices += Clusters[ i ].NumVerts;
		
		// Split clusters with too many verts
		if( Clusters[ i ].NumVerts > 256 )
		{
			FCluster ClusterA, ClusterB;
			uint32 NumTrianglesA = Clusters[ i ].NumTris / 2;
			uint32 NumTrianglesB = Clusters[ i ].NumTris - NumTrianglesA;
			BuildClusterFromClusterTriangleRange( Clusters[ i ], ClusterA, 0, NumTrianglesA );
			BuildClusterFromClusterTriangleRange( Clusters[ i ], ClusterB, NumTrianglesA, NumTrianglesB );
			Clusters[ i ] = ClusterA;
			ClusterGroups[ ClusterB.GroupIndex ].Children.Add( Clusters.Num() );
			Clusters.Add( ClusterB );
		}
	}

	// Calculate stats
	uint32 TotalNewTrianglesWithSplits = 0;
	uint32 TotalNewVerticesWithSplits = 0;
	for( const FCluster& Cluster : Clusters )
	{
		TotalNewTrianglesWithSplits += Cluster.NumTris;
		TotalNewVerticesWithSplits += Cluster.NumVerts;
	}

	UE_LOG( LogStaticMesh, Log, TEXT("ConstrainClusters:") );
	UE_LOG( LogStaticMesh, Log, TEXT("  Input: %d Clusters, %d Triangles and %d Vertices"), NumOldClusters, TotalOldTriangles, TotalOldVertices );
	UE_LOG( LogStaticMesh, Log, TEXT("  Output without splits: %d Clusters, %d Triangles and %d Vertices"), NumOldClusters, TotalNewTriangles, TotalNewVertices );
	UE_LOG( LogStaticMesh, Log, TEXT("  Output with splits: %d Clusters, %d Triangles and %d Vertices"), Clusters.Num(), TotalNewTrianglesWithSplits, TotalNewVerticesWithSplits );
}

#if DO_CHECK
static void VerifyClusterContraints( const TArray< FCluster >& Clusters )
{
	ParallelFor(TEXT("NaniteEncode.VerifyClusterConstraints.PF"), Clusters.Num(), 1024,
		[&]( uint32 i )
		{
			VerifyClusterConstaints( Clusters[i] );
		} );
}
#endif

static uint32 CalculateMaxRootPages(uint32 TargetResidencyInKB)
{
	const uint64 SizeInBytes = uint64(TargetResidencyInKB) << 10;
	return (uint32)FMath::Clamp((SizeInBytes + NANITE_ROOT_PAGE_GPU_SIZE - 1u) >> NANITE_ROOT_PAGE_GPU_SIZE_BITS, 1llu, (uint64)MAX_uint32);
}

static void BuildVertReuseBatches(FCluster& Cluster)
{
	for (FMaterialRange& MaterialRange : Cluster.MaterialRanges)
	{
		TStaticBitArray<NANITE_MAX_CLUSTER_VERTICES> UsedVertMask;
		uint32 NumUniqueVerts = 0;
		uint32 NumTris = 0;
		const uint32 MaxBatchVerts = 32;
		const uint32 MaxBatchTris = 32;
		const uint32 TriIndexEnd = MaterialRange.RangeStart + MaterialRange.RangeLength;

		MaterialRange.BatchTriCounts.Reset();

		for (uint32 TriIndex = MaterialRange.RangeStart; TriIndex < TriIndexEnd; ++TriIndex)
		{
			const uint32 VertIndex0 = Cluster.Indexes[TriIndex * 3 + 0];
			const uint32 VertIndex1 = Cluster.Indexes[TriIndex * 3 + 1];
			const uint32 VertIndex2 = Cluster.Indexes[TriIndex * 3 + 2];

			auto Bit0 = UsedVertMask[VertIndex0];
			auto Bit1 = UsedVertMask[VertIndex1];
			auto Bit2 = UsedVertMask[VertIndex2];

			// If adding this tri to the current batch will result in too many unique verts, start a new batch
			const uint32 NumNewUniqueVerts = uint32(!Bit0) + uint32(!Bit1) + uint32(!Bit2);
			if (NumUniqueVerts + NumNewUniqueVerts > MaxBatchVerts)
			{
				check(NumTris > 0);
				MaterialRange.BatchTriCounts.Add(uint8(NumTris));
				NumUniqueVerts = 0;
				NumTris = 0;
				UsedVertMask = TStaticBitArray<NANITE_MAX_CLUSTER_VERTICES>();
				--TriIndex;
				continue;
			}

			Bit0 = true;
			Bit1 = true;
			Bit2 = true;
			NumUniqueVerts += NumNewUniqueVerts;
			++NumTris;

			if (NumTris == MaxBatchTris)
			{
				MaterialRange.BatchTriCounts.Add(uint8(NumTris));
				NumUniqueVerts = 0;
				NumTris = 0;
				UsedVertMask = TStaticBitArray<NANITE_MAX_CLUSTER_VERTICES>();
			}
		}

		if (NumTris > 0)
		{
			MaterialRange.BatchTriCounts.Add(uint8(NumTris));
		}
	}
}

static void BuildVertReuseBatches(TArray<FCluster>& Clusters)
{
	ParallelFor(TEXT("NaniteEncode.BuildVertReuseBatches.PF"), Clusters.Num(), 256,
		[&Clusters](uint32 ClusterIndex)
		{
			BuildVertReuseBatches(Clusters[ClusterIndex]);
		});
}

static uint32 RandDword()
{
	return FMath::Rand() ^ (FMath::Rand() << 13) ^ (FMath::Rand() << 26);
}

// Debug: Poison input attributes with random data
static void DebugPoisonVertexAttributes(TArray< FCluster >& Clusters)
{
	FMath::RandInit(0xDEADBEEF);

	for (FCluster& Cluster : Clusters)
	{
		for (uint32 VertexIndex = 0; VertexIndex < Cluster.NumVerts; VertexIndex++)
		{
			{
				FVector3f& Normal = Cluster.GetNormal(VertexIndex);
				*(uint32*)&Normal.X = RandDword();
				*(uint32*)&Normal.Y = RandDword();
				*(uint32*)&Normal.Z = RandDword();
			}

			if(Cluster.Settings.bHasColors)
			{
				FLinearColor& Color = Cluster.GetColor(VertexIndex);
				*(uint32*)&Color.R = RandDword();
				*(uint32*)&Color.G = RandDword();
				*(uint32*)&Color.B = RandDword();
				*(uint32*)&Color.A = RandDword();
			}

			for (uint32 UvIndex = 0; UvIndex < Cluster.Settings.NumTexCoords; UvIndex++)
			{
				FVector2f& UV = Cluster.GetUVs(VertexIndex)[UvIndex];
				*(uint32*)&UV.X = RandDword();
				*(uint32*)&UV.Y = RandDword();
			}
		}
	}
}

void Encode(
	FResources& Resources,
	const FMeshNaniteSettings& Settings,
	TArray< FCluster >& Clusters,
	TArray< FClusterGroup >& Groups,
	const FBounds3f& MeshBounds,
	uint32 NumMeshes,
	uint32 NumTexCoords,
	bool bHasTangents,
	bool bHasColors,
	uint32* OutTotalGPUSize)
{
	const uint32 MaxRootPages = CalculateMaxRootPages(Settings.TargetMinimumResidencyInKB);

	// DebugPoisonVertexAttributes(Clusters);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::SanitizeVertexData);
		for (FCluster& Cluster : Clusters)
		{
			Cluster.SanitizeVertexData();
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::RemoveDegenerateTriangles);	// TODO: is this still necessary?
		RemoveDegenerateTriangles( Clusters );
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::BuildMaterialRanges);
		BuildMaterialRanges( Clusters );
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::ConstrainClusters);
		ConstrainClusters( Groups, Clusters );
	}
#if DO_CHECK
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::VerifyClusterConstraints);
		VerifyClusterContraints( Clusters );
	}
#endif

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::BuildVertReuseBatches);
		BuildVertReuseBatches(Clusters);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::CalculateQuantizedPositions);
		Resources.PositionPrecision = CalculateQuantizedPositionsUniformGrid( Clusters, MeshBounds, Settings );	// Needs to happen after clusters have been constrained and split.
	}

	{
		// Select appropriate Auto precision for Normals and Tangents
		// Just use hard-coded defaults for now.
		Resources.NormalPrecision = (Settings.NormalPrecision < 0) ? 8 : FMath::Clamp(Settings.NormalPrecision, 0, NANITE_MAX_NORMAL_QUANTIZATION_BITS);

		if (bHasTangents)
		{
			Resources.TangentPrecision = (Settings.TangentPrecision < 0) ? 7 : FMath::Clamp(Settings.TangentPrecision, 0, NANITE_MAX_TANGENT_QUANTIZATION_BITS);
		}
		else
		{
			Resources.TangentPrecision = 0;
		}
	}
	

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::PrintMaterialRangeStats);
		PrintMaterialRangeStats( Clusters );
	}

	TArray<FPage> Pages;
	TArray<FClusterGroupPart> GroupParts;
	TArray<FEncodingInfo> EncodingInfos;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::CalculateEncodingInfos);
		CalculateEncodingInfos(EncodingInfos, Clusters, Resources.NormalPrecision, Resources.TangentPrecision, bHasTangents, bHasColors, NumTexCoords);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::AssignClustersToPages);
		AssignClustersToPages(Groups, Clusters, EncodingInfos, Pages, GroupParts, MaxRootPages);
		Resources.NumRootPages = FMath::Min((uint32)Pages.Num(), MaxRootPages);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::BuildHierarchyNodes);
		BuildHierarchies(Resources, Pages, Groups, GroupParts, NumMeshes);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::WritePages);
		WritePages(Resources, Pages, Groups, GroupParts, Clusters, EncodingInfos, bHasTangents, NumTexCoords, OutTotalGPUSize);
	}
}

} // namespace Nanite