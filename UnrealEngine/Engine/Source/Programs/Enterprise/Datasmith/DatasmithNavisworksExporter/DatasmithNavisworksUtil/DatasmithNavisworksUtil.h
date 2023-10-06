// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include <cstdint>

namespace DatasmithNavisworksUtilImpl {
	struct FGeometrySettings;
	class FTriangleReaderNative;
	class FGeometry;
}

namespace DatasmithNavisworksUtil {

	public ref struct GeometrySettings
	{
		explicit GeometrySettings();
		~GeometrySettings();
		!GeometrySettings();

		void SetTriangleSizeThreshold(double);
		void SetPositionThreshold(double);
		void SetNormalThreshold(double);
		
		DatasmithNavisworksUtilImpl::FGeometrySettings* Handle; // Handle to the native class holding data, so it can be released on time
	private:
		bool bIsDisposed;
	};

	ref class TriangleReader;
	
	public ref class Geometry : System::IEquatable<Geometry^>
	{
	public:

		explicit Geometry(TriangleReader^ InOwner, DatasmithNavisworksUtilImpl::FGeometry* InGeometry);
		~Geometry();
		!Geometry();

		bool Equals(Object^ Obj) override
		{
			return Equals(dynamic_cast<Geometry^>(Obj));
		}
		virtual bool Equals(Geometry^ Other);
		virtual int GetHashCode() override;
		void Update();

		void Optimize();

		bool Append(Geometry^ Other);

		uint32_t VertexCount;
		double* Coords;
		float *Normals;
		float* UVs;

		uint32_t TriangleCount;
		uint32_t* Indices;

		DatasmithNavisworksUtilImpl::FGeometry* NativeGeometry; // Native class holding data, so it can be released on time

		bool IsDisposed()
		{
			return bIsDisposed;
		}
		
	private:
		TriangleReader^ Owner;

		bool bIsDisposed;
		
		uint64_t Hash;
	};
	
	public ref class TriangleReader
	{
	public:
		explicit TriangleReader();
		~TriangleReader();
		!TriangleReader();

		Geometry^ ReadGeometry(Autodesk::Navisworks::Api::Interop::ComApi::InwOaFragment3^ Fragment, GeometrySettings^ Params);

		// Tell that this Geometry is not used(for early deallocation, instead of relying on GC as Geometry can keep huge amounts of unmanaged memory
		void ReleaseGeometry(Geometry^ Geometry);
		
		/**
		 * \brief Allocate empty geometry, empty but with space reserved for known vertex/triangle counts
		 */
		Geometry^ ReserveGeometry(uint32_t VertexCount, uint32_t TriangleCount);

		// Stats
		uint64_t GetGeometryCount();
		uint64_t GetTriangleCount();
		uint64_t GetVertexCount();

		// Bytes used (i.e. std::vector's "size")
		uint64_t GetCoordBytesUsed();
		uint64_t GetNormalBytesUsed();
		uint64_t GetUvBytesUsed();
		uint64_t GetIndexBytesUsed();
		
		// Bytes actually allocated ("capacity")
		uint64_t GetCoordBytesReserved();
		uint64_t GetNormalBytesReserved();
		uint64_t GetUvBytesReserved();
		uint64_t GetIndexBytesReserved();
		
	private:

		// Buffer where new geometry data is read into initially
		// This is done so that it's the only buffer which grows
		// Trivia - Navisworks API doesn't tell us in advance how many triangles a Fragment has -
		// so we either need to grow(read - reallocate) buffers for each Fragment geometry or
		// have single buffer to read into
		DatasmithNavisworksUtilImpl::FGeometry* BufferGeometry;

		// Buffer geometry is assigned to the last read Geometry object(returned from ReadGeometry)
		// When we need to read another fragment geometry - the Buffer is reassigned to the newly create Geometry
		// leaving lean copy of the Buffer data on the previous Geometry
		System::WeakReference BufferOwnerGeometry;
		
		DatasmithNavisworksUtilImpl::FTriangleReaderNative* NativeTriangleReader;
		
		bool bIsDisposed;
	};

	public ref struct UnrealLocale
	{
		explicit UnrealLocale();
		~UnrealLocale();
		!UnrealLocale();

		void RestoreLocale();

	private:
		wchar_t* StoredLocale;
	};
}
