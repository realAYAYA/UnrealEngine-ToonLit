// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include "SyncContext.h"
#include "SyncData.h"
#include "Utils/TAssValueName.h"

#include "ModelElement.hpp"

BEGIN_NAMESPACE_UE_AC

class FMeshClass;

// Class that contain element id and related
class FElementID
{
  public:
	// Contructor
	FElementID(const FSyncContext& InSyncContext);

	// Initialize with 3D element
	void InitElement(GS::Int32 InIndex3D);

	// Initialize with sync data
	void InitElement(FSyncData* IOSyncdata);

	// Return true if object is valid (i.e. not recently deleted)
	bool IsInvalid() const { return Element3D.IsInvalid(); }

	// Return the element index (in 3D list)
	GS::Int32 GetIndex3D() const { return Index3D; }

	// Return the 3D element
	const ModelerAPI::Element& GetElement3D() const { return Element3D; }

	// Return the 3D type name
	static const utf8_t* GetTypeName(ModelerAPI::Element::Type InType)
	{
		return TAssEnumName< ModelerAPI::Element::Type >::GetName(InType);
	}

	// Return element 3D type name
	const utf8_t* GetTypeName() const { return GetTypeName(Element3D.GetType()); }

	// Initialize element header from 3D element
	bool InitHeader();

	// Initialize element header with element guid
	void InitHeader(const API_Guid& InGuid)
	{
		Zap(&APIElementHeader);
		APIElementHeader.guid = InGuid;
		UE_AC_TestGSError(ACAPI_Element_GetHeader(&APIElementHeader, 0));
	}

	// Set the Sync Data associated to the current element
	void SetSyncData(FSyncData* InSyncData) { SyncData = InSyncData; }

	// Return the Sync Data associated to the current element
	FSyncData* GetSyncData() const { return SyncData; }

	// Connect to parent or childs
	void HandleDepedencies() const;

	// Return true if element is a morph type body (will need double side)
	bool IsSurface() const;

	// Return the element's header
	const API_Elem_Head& GetHeader() const { return APIElementHeader; }

	// Return the element's typeID
	const API_ElemTypeID& GetTypeID() const
	{
#if AC_VERSION < 26
		return APIElementHeader.typeID;
#else
		return APIElementHeader.type.typeID;
#endif
	}

	// Return the mesh class based on hash of 3D ModelerAPI::BaseElemId
	FMeshClass* GetMeshClass();

	// Return the lib part info if this element come from it
	const FLibPartInfo* GetLibPartInfo();

	// Current synchronisation context
	const FSyncContext& SyncContext;

	// Return the element name
	const utf8_t* GetElementName();

  private:
	// Connect childs of this parent
	void CollectDependantElementsType(API_ElemTypeID TypeID) const;

	// 3D element index
	GS::Int32 Index3D;
	// 3D element
	ModelerAPI::Element Element3D;

	// Basic AC API Element header
	API_Elem_Head APIElementHeader;

	// Sync data associated to the current element
	FSyncData* SyncData;

	// Mesh class based on hash of 3D ModelerAPI::BaseElemId
	FMeshClass* MeshClass;

	// Lib part info have been fetched
	bool bLibPartInfoFetched;
	// LibPartInfo != nullptr if element come from it
	const FLibPartInfo* LibPartInfo;

	// The element name
	utf8_string ElementName;
};

// SyncData tree iterator and synchronization process context
class FSyncData::FProcessInfo : public FSyncData::FInterator
{
  public:
	// Contructor
	FProcessInfo(const FSyncContext& InSyncContext)
		: SyncContext(InSyncContext)
		, ProgessValue(0)
		, ElementID(InSyncContext)
	{
		bProcessMetaData = !SyncContext.IsSynchronizer();
		Start(&SyncContext.GetSyncDatabase().GetSceneSyncData());
	}

	// Process syncronization
	virtual EProcessControl Process(FSyncData* InCurrent) override
	{
		if (InCurrent == nullptr)
		{
			return FInterator::kDone;
		}

		InCurrent->Process(this);

		return FInterator::kContinue;
	}

	// Current synchronization context
	const FSyncContext& SyncContext;
	// True if we must process metadata immediately. (Export->true, DirectLink->False)
	bool bProcessMetaData = false;
	// Value to pass to progress progression
	int ProgessValue = 0;
	// Current element
	FElementID ElementID;
};

END_NAMESPACE_UE_AC
