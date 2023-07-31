// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithNavisworksUtil.h"

#include "TriangleReaderNative.h"

#include <clocale>


DatasmithNavisworksUtil::GeometrySettings::GeometrySettings()
	: Handle(new DatasmithNavisworksUtilImpl::FGeometrySettings)
	, bIsDisposed(false)
{
}

DatasmithNavisworksUtil::GeometrySettings::~GeometrySettings()
{
	if (bIsDisposed)
	{
		return;
	}
	this->!GeometrySettings();
	bIsDisposed = true;
}

void DatasmithNavisworksUtil::GeometrySettings::!GeometrySettings()
{
	delete Handle;
}

void DatasmithNavisworksUtil::GeometrySettings::SetTriangleSizeThreshold(double Value)
{
	Handle->TriangleSizeThreshold = Value;
}

void DatasmithNavisworksUtil::GeometrySettings::SetPositionThreshold(double Value)
{
	Handle->PositionThreshold = Value;
}

void DatasmithNavisworksUtil::GeometrySettings::SetNormalThreshold(double Value)
{
	Handle->NormalThreshold = Value;
}

DatasmithNavisworksUtil::Geometry::Geometry(TriangleReader^ InOwner, DatasmithNavisworksUtilImpl::FGeometry* InGeometry)
	: Owner(InOwner)
	, NativeGeometry(InGeometry)
	, bIsDisposed(false)
{
	Update();

	Hash = InGeometry->ComputeHash();
}

DatasmithNavisworksUtil::Geometry::~Geometry()
{
	if (bIsDisposed)
	{
		return;
	}
	this->!Geometry();
	bIsDisposed = true;
}

void DatasmithNavisworksUtil::Geometry::!Geometry()
{
	Owner->ReleaseGeometry(this);
}

bool DatasmithNavisworksUtil::Geometry::Equals(Geometry^ Other)
{
	return (NativeGeometry->VertexCount == Other->NativeGeometry->VertexCount)
		&& (NativeGeometry->Coords == Other->NativeGeometry->Coords)
		&& (NativeGeometry->Normals == Other->NativeGeometry->Normals)
		&& (NativeGeometry->UVs == Other->NativeGeometry->UVs)
		&& (NativeGeometry->TriangleCount == Other->NativeGeometry->TriangleCount)
		&& (NativeGeometry->Indices == Other->NativeGeometry->Indices)
		;
}

int DatasmithNavisworksUtil::Geometry::GetHashCode()
{
	return Hash % INT_MAX;
}

void DatasmithNavisworksUtil::Geometry::Update()
{
	VertexCount = NativeGeometry->VertexCount;
	Coords = NativeGeometry->Coords.data();
	Normals = NativeGeometry->Normals.data();
	UVs = NativeGeometry->UVs.data();

	TriangleCount = NativeGeometry->TriangleCount;
	Indices = NativeGeometry->Indices.data();
}

void DatasmithNavisworksUtil::Geometry::Optimize()
{
	NativeGeometry->Optimize();

	Update();
}

bool DatasmithNavisworksUtil::Geometry::Append(Geometry^ Other)
{
	if (NativeGeometry->Coords.size() + Other->NativeGeometry->VertexCount * 3 > NativeGeometry->Coords.capacity())
	{
		return false;
	}
	if (NativeGeometry->Indices.size() + Other->NativeGeometry->TriangleCount * 3 > NativeGeometry->Indices.capacity())
	{
		return false;
	}

	NativeGeometry->ModificationStarted();
	NativeGeometry->Coords.insert(NativeGeometry->Coords.end(), Other->NativeGeometry->Coords.begin(), Other->NativeGeometry->Coords.end());
	NativeGeometry->Normals.insert(NativeGeometry->Normals.end(), Other->NativeGeometry->Normals.begin(), Other->NativeGeometry->Normals.end());
	NativeGeometry->UVs.insert(NativeGeometry->UVs.end(), Other->NativeGeometry->UVs.begin(), Other->NativeGeometry->UVs.end());

	for (uint32_t Index : Other->NativeGeometry->Indices)
	{
		NativeGeometry->Indices.push_back(Index + NativeGeometry->VertexCount);
	}

	NativeGeometry->VertexCount += Other->NativeGeometry->VertexCount;
	NativeGeometry->TriangleCount += Other->NativeGeometry->TriangleCount;

	Update();
	NativeGeometry->ModificationEnded();
	return true;
}

DatasmithNavisworksUtil::TriangleReader::TriangleReader()
	: NativeTriangleReader(new DatasmithNavisworksUtilImpl::FTriangleReaderNative)
	, bIsDisposed(false)
	, BufferOwnerGeometry(nullptr)
{
	BufferGeometry = NativeTriangleReader->GetNewGeometry();
}

DatasmithNavisworksUtil::TriangleReader::~TriangleReader()
{
	if (bIsDisposed)
	{
		return;
	}
	this->!TriangleReader();
	bIsDisposed = true;
}

void DatasmithNavisworksUtil::TriangleReader::!TriangleReader()
{
	NativeTriangleReader->ReleaseGeometry(BufferGeometry);
	delete NativeTriangleReader;
}

DatasmithNavisworksUtil::Geometry^ DatasmithNavisworksUtil::TriangleReader::ReadGeometry(Autodesk::Navisworks::Api::Interop::ComApi::InwOaFragment3^ Fragment, GeometrySettings^ Settings)
{
	// If there's a buffer owner Geometry object and it's not disposed(disposed had its Geometry released)
	// disown the Buffer by making a lean(without 'slack' bytes) copy of it
	Geometry^ BufferOwner = safe_cast<Geometry^>(BufferOwnerGeometry.Target);
	if (BufferOwner != nullptr && !BufferOwner->IsDisposed())
	{
		BufferOwner->NativeGeometry = NativeTriangleReader->MakeLeanCopy(BufferGeometry);
		BufferOwner->Update();
	}
	NativeTriangleReader->ClearBuffer(BufferGeometry);

	NativeTriangleReader->Read(System::Runtime::InteropServices::Marshal::GetIUnknownForObject(Fragment).ToPointer(), *BufferGeometry, *Settings->Handle);
	
	Geometry^ Result = gcnew Geometry(this, BufferGeometry); // Make new geometry own the Buffer(it will be disowned when next geometry is read)
	BufferOwnerGeometry.Target = Result; // Store buffer owner weak reference
	return Result;
}

DatasmithNavisworksUtil::Geometry^ DatasmithNavisworksUtil::TriangleReader::ReserveGeometry(uint32_t VertexCount, uint32_t TriangleCount)
{
	DatasmithNavisworksUtilImpl::FGeometry* Geom = NativeTriangleReader->GetNewGeometry();
	Geom->Coords.reserve(VertexCount * 3);
	Geom->Normals.reserve(VertexCount * 3);
	Geom->UVs.reserve(VertexCount * 2);
	Geom->Indices.reserve(TriangleCount * 3);
	return gcnew Geometry(this, Geom);
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetGeometryCount()
{
	return NativeTriangleReader->AllocationStats.GeometryCount;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetTriangleCount()
{
	return NativeTriangleReader->AllocationStats.TriangleCount;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetVertexCount()
{
	return NativeTriangleReader->AllocationStats.VertexCount;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetCoordBytesUsed()
{
	return NativeTriangleReader->AllocationStats.CoordBytesUsed;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetNormalBytesUsed()
{
	return NativeTriangleReader->AllocationStats.NormalBytesUsed;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetUvBytesUsed()
{
	return NativeTriangleReader->AllocationStats.UvBytesUsed;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetIndexBytesUsed()
{
	return NativeTriangleReader->AllocationStats.IndexBytesUsed;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetCoordBytesReserved()
{
	return NativeTriangleReader->AllocationStats.CoordBytesReserved;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetNormalBytesReserved()
{
	return NativeTriangleReader->AllocationStats.NormalBytesReserved;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetUvBytesReserved()
{
	return NativeTriangleReader->AllocationStats.UvBytesReserved;
}

uint64_t DatasmithNavisworksUtil::TriangleReader::GetIndexBytesReserved()
{
	return NativeTriangleReader->AllocationStats.IndexBytesReserved;
}

void DatasmithNavisworksUtil::TriangleReader::ReleaseGeometry(Geometry^ Geometry)
{
	if (BufferGeometry != Geometry->NativeGeometry) // Buffer will be released when TriangleReader is disposed
	{
		NativeTriangleReader->ReleaseGeometry(Geometry->NativeGeometry);
	}
}

DatasmithNavisworksUtil::UnrealLocale::UnrealLocale()
{
	// Store current numeric locale and install default, which has '.'(dot) as decimal separator
	wchar_t* LocaleNamePtr = _wsetlocale(LC_NUMERIC, nullptr);
	StoredLocale = _wcsdup(LocaleNamePtr);
	_wsetlocale(LC_NUMERIC, L"C");
}

DatasmithNavisworksUtil::UnrealLocale::~UnrealLocale()
{
	this->!UnrealLocale();
	StoredLocale = nullptr;
}

DatasmithNavisworksUtil::UnrealLocale::!UnrealLocale()
{
	free(StoredLocale);
}

void DatasmithNavisworksUtil::UnrealLocale::RestoreLocale()
{
	_wsetlocale(LC_NUMERIC, StoredLocale);
}
