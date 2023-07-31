// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <vector>

namespace DatasmithNavisworksUtilImpl {

	struct FGeometrySettings
	{
		double TriangleSizeThreshold = 0;
		double PositionThreshold = 0;
		double NormalThreshold = 0;
	};
	
	class FGeometry
	{
	public:
		class FTriangleReaderNative& TriangleReader;

		typedef float NormalType;
		typedef float UvType;

		uint32_t VertexCount = 0;
		std::vector<double> Coords;
		std::vector<NormalType> Normals;
		std::vector<UvType> UVs;

		uint32_t TriangleCount = 0;
		std::vector<uint32_t> Indices;

		FGeometry(FTriangleReaderNative& TriangleReader);
		~FGeometry();
		
		uint64_t ComputeHash();
		void Optimize();

		// Designate block of code where geometry is being modified(to track allocation statistics)
		void ModificationStarted();
		void ModificationEnded();
	};

	struct FAllocationStats
	{
		uint64_t GeometryCount = 0;
		
		uint64_t TriangleCount = 0;
		uint64_t VertexCount = 0;

		uint64_t CoordBytesUsed = 0;
		uint64_t NormalBytesUsed = 0;
		uint64_t UvBytesUsed = 0;
		uint64_t IndexBytesUsed = 0;
		
		uint64_t CoordBytesReserved = 0;
		uint64_t NormalBytesReserved = 0;
		uint64_t UvBytesReserved = 0;
		uint64_t IndexBytesReserved = 0;
		
		void Add(FGeometry& Geometry);
		void Remove(FGeometry& Geometry);
	};

	class FTriangleReaderNative
	{
	public:
		FTriangleReaderNative();
		~FTriangleReaderNative();

		void Read(void* FragmentIUnknownPtr, FGeometry& Geom, FGeometrySettings& Settings);

		FGeometry* GetNewGeometry();
		void ReleaseGeometry(FGeometry* Geometry);

		// Copy buffers without leaving any 'slack' bytes(capacity==size)
		FGeometry* MakeLeanCopy(FGeometry* Geometry);
		
		void ClearBuffer(FGeometry* Geometry);

		static FAllocationStats AllocationStats;
	};

}
