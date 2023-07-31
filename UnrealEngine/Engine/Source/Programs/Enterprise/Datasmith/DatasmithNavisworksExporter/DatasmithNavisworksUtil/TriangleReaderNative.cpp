// Copyright Epic Games, Inc. All Rights Reserved.

#include "TriangleReaderNative.h"

#include <Unknwn.h>

#include <vector>
#include <utility>
#include <unordered_map>

#if !defined(Navisworks_API)
#error "Navisworks_API is undefined - won't find Navisworks assemblies and typelibs"
#endif

#define STRINGIFY_(x) #x
#define lcodieD(x) STRINGIFY_(x)
#import lcodieD(Navisworks_API/lcodieD.dll) rename_namespace("NavisworksIntegratedAPI") // renaming to omit API version(e.g. NavisworksIntegratedAPI17) 

using namespace DatasmithNavisworksUtilImpl;

bool DoesTriangleHaveInvalidNormals(FGeometry& Geom, uint32_t TriangleIndex, double NormalThreshold)
{
	const uint32_t TriangleBaseVertexIndex = TriangleIndex * 3;
	FGeometry::NormalType TypedThreshold = static_cast<FGeometry::NormalType>(NormalThreshold);
	FGeometry::NormalType* NormalPtr = Geom.Normals.data() + TriangleBaseVertexIndex * 3;

	// Find out if any triangle normal is invalid
	for (uint32_t I = 0; I < 3; ++I, NormalPtr += 3)
	{
		if ((abs(NormalPtr[0]) <= TypedThreshold)
			&& (abs(NormalPtr[1]) <= TypedThreshold)
			&& (abs(NormalPtr[2]) <= TypedThreshold))
		{
			return true;
		}
	}
	return false;
}

// Implementation of callback object that goes to Naviswork's GenerateSimplePrimitives
// Although it's an IDispatch interface we don't need its mechanics implemented when
// calling GenerateSimplePrimitives directly from C++
class SimplePrimitivesCallback : public NavisworksIntegratedAPI::InwSimplePrimitivesCB {
public:

	DatasmithNavisworksUtilImpl::FGeometry* Geometry;
	bool bReadNormals;
	bool bReadUVs;
	
	DatasmithNavisworksUtilImpl::FGeometrySettings GeometrySettings;

	struct Vector3d
	{
		double X, Y, Z;
		
		Vector3d(double X, double Y, double Z) : X(X), Y(Y), Z(Z)
		{
		}
		
		bool AlmostEqual(const Vector3d Other, double Threshold) const
		{
			return (abs(X - Other.X) <= Threshold)
				&& (abs(Y - Other.Y) <= Threshold)
				&& (abs(Z - Other.Z) <= Threshold);
		}

		Vector3d Cross(const Vector3d V) const
		{
			return {
				Y * V.Z - Z * V.Y,
				Z * V.X - X * V.Z,
				X * V.Y - Y * V.X
			};
		}

		double LengthSquared() const
		{
			return X * X + Y * Y + Z * Z;
		}
		
		Vector3d operator -(const Vector3d V) const
		{
			return {
				X - V.X,
				Y - V.Y,
				Z - V.Z
			};
		}
		
		Vector3d operator *(const double S) const
		{
			return {
				X * S,
				Y * S,
				Z * S
			};
		}
	};

	// Returns added position vector
	Vector3d ConvertCoord(NavisworksIntegratedAPI::InwSimpleVertex* SimpleVertex, std::vector<double>& Result)
	{
		ExtractVectorFromVariant(SimpleVertex->coord, Result, 3);
		return Vector3d{ Result[Result.size() - 3], Result[Result.size() - 2], Result[Result.size() - 1] };
	}

	void ConvertNormal(NavisworksIntegratedAPI::InwSimpleVertex* SimpleVertex, std::vector<DatasmithNavisworksUtilImpl::FGeometry::NormalType>& Result)
	{
		ExtractVectorFromVariant(SimpleVertex->normal, Result, 3);
	}

	void ConvertUV(NavisworksIntegratedAPI::InwSimpleVertex* SimpleVertex, std::vector<DatasmithNavisworksUtilImpl::FGeometry::UvType>& Result)
	{
		ExtractVectorFromVariant(SimpleVertex->tex_coord, Result, 2);
	}

	template<typename ComponentType>
	static void ExtractVectorFromVariant(const _variant_t& Variant, std::vector<ComponentType>& Result, const int Count)
	{
		SAFEARRAY* ComArray = Variant.parray;
		HRESULT Hr;
		if (SUCCEEDED(Hr = SafeArrayLock(ComArray)))
		{
			FLOAT* Array = static_cast<FLOAT*>(ComArray->pvData);
			for(int I=0; I != Count; ++I)
			{
				Result.push_back(Array[I]);
			}
			Hr = SafeArrayUnlock(ComArray);
		}
	}

	HRESULT raw_Triangle(NavisworksIntegratedAPI::InwSimpleVertex* V1, NavisworksIntegratedAPI::InwSimpleVertex* V2, NavisworksIntegratedAPI::InwSimpleVertex* V3) override
	{
		const int BaseIndex = Geometry->VertexCount;

		const Vector3d P0 = ConvertCoord(V1, Geometry->Coords);
		const Vector3d P1 = ConvertCoord(V2, Geometry->Coords);
		const Vector3d P2 = ConvertCoord(V3, Geometry->Coords);

		// Test Degenerate triangle
		if (P0.AlmostEqual(P1, GeometrySettings.PositionThreshold)
			|| P0.AlmostEqual(P2, GeometrySettings.PositionThreshold)
			|| P1.AlmostEqual(P2, GeometrySettings.PositionThreshold))
		{
			// Rollback added coords, we are skipping degenerate triangle
			Geometry->Coords.resize(Geometry->Coords.size() - 3 * 3);
			return S_OK;
		}
		
		if (bReadNormals)
		{
			ConvertNormal(V1, Geometry->Normals);
			ConvertNormal(V2, Geometry->Normals);
			ConvertNormal(V3, Geometry->Normals);
		}
		else
		{
			Geometry->Normals.resize(Geometry->Normals.size() + 3 * 3, 0);
		}

		// Try repairing normals
		// In case normals are not read or read as near zero.
		// Importing invalid normals into Unreal will make Unreal regenerate them and display warnings which we can fix on export.
		// Also merging meshes with valid normals will invalidate those merged meshes and make Unreal regenerate normals for the whole merged mesh making it faceted even for partch that were smooth without merge
		// Geometry without normals is faceted in Navisworks anyway so we are making it explicit, early, simple(we know topology) and fixing merged meshes
		uint32_t TriangleIndex = Geometry->TriangleCount;
		if (DoesTriangleHaveInvalidNormals(*Geometry, TriangleIndex, GeometrySettings.NormalThreshold))
		{
			Vector3d TriangleNormal = (P1 - P0).Cross(P2 - P0);
			double TriangleArea = 0.5 * TriangleNormal.LengthSquared();
			if (TriangleArea < GeometrySettings.TriangleSizeThreshold)
			{
				// Skip triangle that is too small(degenerate)
				Geometry->Coords.resize(Geometry->Coords.size() - 3 * 3);
				Geometry->Normals.resize(Geometry->Normals.size() - 3 * 3);
				return S_OK;
			}

			double Scale = 1.0 / sqrt(TriangleArea);
			Vector3d Normal = TriangleNormal * Scale;
			FGeometry::NormalType* NormalPtr = Geometry->Normals.data() + BaseIndex * 3;
			for (int I = 0; I < 3; ++I, NormalPtr += 3)
			{
				NormalPtr[0] = static_cast<FGeometry::NormalType>(Normal.X);
				NormalPtr[1] = static_cast<FGeometry::NormalType>(Normal.Y);
				NormalPtr[2] = static_cast<FGeometry::NormalType>(Normal.Z);
			}
		}

		if (bReadUVs)
		{
			ConvertUV(V1, Geometry->UVs);
			ConvertUV(V2, Geometry->UVs);
			ConvertUV(V3, Geometry->UVs);
		}

		Geometry->VertexCount += 3;

		for (int I = 0; I < 3; ++I)
		{
			Geometry->Indices.push_back(BaseIndex + I);
		}
		Geometry->TriangleCount++;

		return S_OK;
	}

	HRESULT raw_Line(NavisworksIntegratedAPI::InwSimpleVertex* v1, NavisworksIntegratedAPI::InwSimpleVertex* v2) override
	{
		return S_OK;
	}

	HRESULT raw_Point(NavisworksIntegratedAPI::InwSimpleVertex* v1) override
	{
		return S_OK;
	}

	HRESULT raw_SnapPoint(NavisworksIntegratedAPI::InwSimpleVertex* v1) override
	{
		return S_OK;
	}

	// IDispatch implementation - just simple stubs - these methods are not called anyway
	HRESULT QueryInterface(const IID& riid, void** ppvObject) override
	{
		return S_OK;
	}
	
	ULONG AddRef() override
	{
		return 1;
	}
	
	ULONG Release() override
	{
		return 1;
	}
	
	HRESULT GetTypeInfoCount(UINT* pctinfo) override
	{
		return S_OK;
	}
	
	HRESULT GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override
	{
		return S_OK;
	}
	
	HRESULT GetIDsOfNames(const IID& riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) override
	{
		return S_OK;
	}
	
	HRESULT Invoke(DISPID dispIdMember, const IID& riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
		VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) override
	{
		return S_OK;
	}
	// end IDispatch implementation
};

DatasmithNavisworksUtilImpl::FTriangleReaderNative::FTriangleReaderNative()
{
}

DatasmithNavisworksUtilImpl::FTriangleReaderNative::~FTriangleReaderNative()
{
}

DatasmithNavisworksUtilImpl::FAllocationStats DatasmithNavisworksUtilImpl::FTriangleReaderNative::AllocationStats;

void DatasmithNavisworksUtilImpl::FTriangleReaderNative::Read(void* FragmentIUnknownPtr, DatasmithNavisworksUtilImpl::FGeometry& Geom, FGeometrySettings& Settings)
{
	Geom.ModificationStarted();
	NavisworksIntegratedAPI::InwOaFragment3Ptr Fragment(static_cast<IUnknown*>(FragmentIUnknownPtr));
	SimplePrimitivesCallback Callback;
	Callback.Geometry = &Geom;

	// TODO: Interesting, what is this UserOffset?
	NavisworksIntegratedAPI::InwLVec3fPtr InwLVec3F = Fragment->GetUserOffset();
	double UserOffset[] = { InwLVec3F->data1, InwLVec3F->data2, InwLVec3F->data1 };

	NavisworksIntegratedAPI::nwEVertexProperty VertexProperty = Fragment->GetVertexProps();
	Callback.bReadNormals = VertexProperty & NavisworksIntegratedAPI::nwEVertexProperty::eNORMAL;
	Callback.bReadUVs = VertexProperty & NavisworksIntegratedAPI::nwEVertexProperty::eTEX_COORD;
	Callback.GeometrySettings = Settings;
	// Callback will be called for each triangle in the fragment mesh
	Fragment->GenerateSimplePrimitives(VertexProperty, &Callback);

	if (!Callback.bReadUVs)
	{
		Geom.UVs.resize(Geom.VertexCount * 2, 0.0);
	}
	Geom.ModificationEnded();
}

DatasmithNavisworksUtilImpl::FGeometry* DatasmithNavisworksUtilImpl::FTriangleReaderNative::GetNewGeometry()
{
	return new FGeometry(*this);
}

void DatasmithNavisworksUtilImpl::FTriangleReaderNative::ReleaseGeometry(FGeometry* Geometry)
{
	delete Geometry;
}

DatasmithNavisworksUtilImpl::FGeometry* DatasmithNavisworksUtilImpl::FTriangleReaderNative::MakeLeanCopy(FGeometry* Geometry)
{
	FGeometry* Result = new FGeometry(*this);

	Result->ModificationStarted();
	Result->TriangleCount = Geometry->TriangleCount;
	Result->VertexCount = Geometry->VertexCount;

	Result->Coords = Geometry->Coords;
	Result->Normals = Geometry->Normals;
	Result->UVs = Geometry->UVs;
	Result->Indices = Geometry->Indices;

	Result->ModificationEnded();
	
	return Result;
}

void FTriangleReaderNative::ClearBuffer(FGeometry* Geom)
{
	// Clear geometry buffer but keep allocations (std::vector::clear doesn't reallocate)
	Geom->ModificationStarted();
	Geom->TriangleCount = 0;
	Geom->VertexCount = 0;
	Geom->Coords.clear();
	Geom->Normals.clear();
	Geom->UVs.clear();
	Geom->Indices.clear();
	Geom->ModificationEnded();
}

inline void CombineHash(std::size_t& A, const std::size_t& B)
{
	A = A ^ (B + 0x9e3779b9 + (A << 6) + (A >> 2));
}

DatasmithNavisworksUtilImpl::FGeometry::FGeometry(FTriangleReaderNative& TriangleReader)
	: TriangleReader(TriangleReader)
{
	FTriangleReaderNative::AllocationStats.Add(*this);
}

DatasmithNavisworksUtilImpl::FGeometry::~FGeometry()
{
	FTriangleReaderNative::AllocationStats.Remove(*this);
}

uint64_t DatasmithNavisworksUtilImpl::FGeometry::ComputeHash()
{
	std::size_t Hash = 0;

	CombineHash(Hash, std::hash<uint32_t>()(VertexCount));

	for (double Value : Coords)
	{
		CombineHash(Hash, std::hash<double>()(Value));
	}

	for (NormalType Value : Normals)
	{
		CombineHash(Hash, std::hash<NormalType>()(Value));
	}

	for (UvType Value : UVs)
	{
		CombineHash(Hash, std::hash<UvType>()(Value));
	}

	CombineHash(Hash, std::hash<uint32_t>()(TriangleCount));

	for (uint32_t Value : Indices)
	{
		CombineHash(Hash, std::hash<uint32_t>()(Value));
	}
	return Hash;
}

void DatasmithNavisworksUtilImpl::FGeometry::Optimize()
{
	std::vector<std::size_t> VertexHashes(VertexCount);

	// TODO: parallel this loop and add tolerance to vertex values
	for (uint32_t VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		std::size_t Hash = 0;
		for (int I = 0; I < 3; ++I)
		{
			CombineHash(Hash, std::hash<double>()(Coords[VertexIndex * 3 + I]));
		}
		for (int I = 0; I < 3; ++I)
		{
			CombineHash(Hash, std::hash<NormalType>()(Normals[VertexIndex * 3 + I]));
		}
		for (int I = 0; I < 2; ++I)
		{
			CombineHash(Hash, std::hash<UvType>()(UVs[VertexIndex * 2 + I]));
		}
		VertexHashes[VertexIndex] = Hash;
	}
	auto VertexEquals = [this](const uint32_t& A, const uint32_t& B)
	{
		return std::equal(Coords.begin() + A * 3, Coords.begin() + A * 3 + 3, Coords.begin() + B * 3)
			&& std::equal(Normals.begin() + A * 3, Normals.begin() + A * 3 + 3, Normals.begin() + B * 3)
			&& std::equal(UVs.begin() + A * 2, UVs.begin() + A * 2 + 2, UVs.begin() + B * 2)
			;
	};

	auto VertexHash = [&VertexHashes](const uint32_t& VertexIndex)
	{
		return VertexHashes[VertexIndex];
	};

	std::unordered_map<uint32_t, uint32_t, decltype(VertexHash), decltype(VertexEquals)> VerticesMap(VertexCount, VertexHash, VertexEquals);

	std::vector<uint32_t> OldVertexIndexToNew(VertexCount);
	std::vector<uint32_t> NewVertices;
	NewVertices.reserve(VertexCount);

	for (uint32_t VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		uint32_t NextNewVertexIndex = static_cast<uint32_t>(NewVertices.size()); // if current vertex is not yet in VerticesMap this will be its new index
		const auto Insert = VerticesMap.try_emplace(VertexIndex, NextNewVertexIndex);
		const bool bHasInsertedNewVertex = Insert.second;
		if (bHasInsertedNewVertex)
		{
			NewVertices.push_back(VertexIndex);
		}
		const auto OldAndNew = Insert.first;
		OldVertexIndexToNew[VertexIndex] = OldAndNew->second;
	}

	// Move vertices
	for (uint32_t NewIndex = 0; NewIndex < NewVertices.size(); ++NewIndex)
	{
		const uint32_t OldIndex = NewVertices[NewIndex];

		for (int I = 0; I < 3; ++I)
		{
			Coords[NewIndex * 3 + I] = Coords[OldIndex * 3 + I];
		}
		for (int I = 0; I < 3; ++I)
		{
			Normals[NewIndex * 3 + I] = Normals[OldIndex * 3 + I];
		}
		for (int I = 0; I < 2; ++I)
		{
			UVs[NewIndex * 2 + I] = UVs[OldIndex * 2 + I];
		}
	}

	// Update VertexCount and dependants
	VertexCount = static_cast<uint32_t>(NewVertices.size());
	Coords.resize(VertexCount * 3);
	Normals.resize(VertexCount * 3);
	UVs.resize(VertexCount * 2);

	for (auto& Index : Indices)
	{
		Index = OldVertexIndexToNew[Index];
	}
}

void DatasmithNavisworksUtilImpl::FGeometry::ModificationStarted()
{
	FTriangleReaderNative::AllocationStats.Remove(*this);
}

void DatasmithNavisworksUtilImpl::FGeometry::ModificationEnded()
{
	FTriangleReaderNative::AllocationStats.Add(*this);
}

void DatasmithNavisworksUtilImpl::FAllocationStats::Add(DatasmithNavisworksUtilImpl::FGeometry& Geometry)
{
	GeometryCount++;
	
	TriangleCount += Geometry.TriangleCount;
	VertexCount += Geometry.VertexCount;

	CoordBytesUsed += Geometry.Coords.size() * sizeof(Geometry.Coords[0]);
	NormalBytesUsed += Geometry.Normals.size() * sizeof(Geometry.Normals[0]);
	UvBytesUsed += Geometry.UVs.size() * sizeof(Geometry.UVs[0]);
	IndexBytesUsed += Geometry.Indices.size() * sizeof(Geometry.Indices[0]);

	CoordBytesReserved += Geometry.Coords.capacity() * sizeof(Geometry.Coords[0]);
	NormalBytesReserved += Geometry.Normals.capacity() * sizeof(Geometry.Normals[0]);
	UvBytesReserved += Geometry.UVs.capacity() * sizeof(Geometry.UVs[0]);
	IndexBytesReserved += Geometry.Indices.capacity() * sizeof(Geometry.Indices[0]);
}

void DatasmithNavisworksUtilImpl::FAllocationStats::Remove(DatasmithNavisworksUtilImpl::FGeometry& Geometry)
{
	GeometryCount--;
	
	TriangleCount -= Geometry.TriangleCount;
	VertexCount -= Geometry.VertexCount;

	CoordBytesUsed -= Geometry.Coords.size() * sizeof(Geometry.Coords[0]);
	NormalBytesUsed -= Geometry.Normals.size() * sizeof(Geometry.Normals[0]);
	UvBytesUsed -= Geometry.UVs.size() * sizeof(Geometry.UVs[0]);
	IndexBytesUsed -= Geometry.Indices.size() * sizeof(Geometry.Indices[0]);

	CoordBytesReserved -= Geometry.Coords.capacity() * sizeof(Geometry.Coords[0]);
	NormalBytesReserved -= Geometry.Normals.capacity() * sizeof(Geometry.Normals[0]);
	UvBytesReserved -= Geometry.UVs.capacity() * sizeof(Geometry.UVs[0]);
	IndexBytesReserved -= Geometry.Indices.capacity() * sizeof(Geometry.Indices[0]);
}


