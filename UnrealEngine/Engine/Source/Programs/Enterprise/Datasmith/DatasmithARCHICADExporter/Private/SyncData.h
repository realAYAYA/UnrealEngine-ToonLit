// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"
#include "Utils/TimeStat.h"

#include "Light.hpp"

#include "Array.h"

namespace ModelerAPI
{
class Transformation;
}

BEGIN_NAMESPACE_UE_AC

class FElementID;
class FSyncContext;
class FSyncDatabase;
class FSynchronizer;

/* Class that keep synchronization data of Archicad elements.
 * Take care of object hierarchy (By synthetizing layer an scene) */
class FSyncData
{
  public:
	class FScene;
	class FActor;
	class FLayer;
	class FElement;
	class FCameraSet;
	class FCamera;
	class FLight;

	class FHotLinksRoot;
	class FHotLinkNode;
	class FHotLinkInstance;

	// Class use for scanning db to find element not yet registered.
	class FInterator;
	class FProcessMetadata;
	class FAttachObservers;

	// Constructor
	FSyncData(const GS::Guid& InGuid);

	// Destructor
	virtual ~FSyncData();

	// Update data from a 3d element
	void Update(const FElementID& IoElementId);

	// Return the element index in current 3d context
	GS::Int32 GetIndex3D() const { return Index3D; }

	// Return true if object have been modified (3d or API)
	bool IsModified() const { return bIsModified; }

	// Check modification stamp
	bool CheckModificationStamp(UInt64 InModificationStamp)
	{
		if (ModificationStamp != InModificationStamp)
		{
			ModificationStamp = InModificationStamp;
			bIsModified = true;
		}
		return bIsModified;
	}

	// Set modification state to true
	void MarkAsModified() { bIsModified = true; }

	// Before a scan, we presume object as deletable and not modified
	void ResetBeforeScan()
	{
		Index3D = 0;
		bIsModified = false;
	}

	void MarkAsExisting() { Index3D = -1; }

	// Recursively clean. Delete element that hasn't 3d geometry related to it
	void CleanAfterScan(FSyncDatabase* IOSyncDatabase);

	void SetParent(FSyncData* InParent);

	void SetIsAComponent(bool bInIsAComponent = true) { bIsAComponent = bInIsAComponent; }

	bool HasParent() const { return Parent != nullptr; }

	void SetDefaultParent(const FElementID& InElementID);

	// Working class that contain data to process elements and it's childs
	class FProcessInfo;

	// Process (Sync Datasmith element from Archicad element)
	virtual void Process(FProcessInfo* IOProcessInfo) = 0;

	// Attach observer for Auto Sync
	virtual bool AttachObserver(FAttachObservers* /* IOAttachObservers */) { return false; }

	// Return true if this element need to update tags and metadata
	virtual bool NeedTagsAndMetaDataUpdate() { return false; }

	// Process meta data. Return true if meta data was updated
	virtual bool ProcessMetaData(FSyncDatabase* /* IOSyncDatabase */) { return false; };

	// Delete this sync data
	virtual TSharedPtr< IDatasmithElement > GetElement() const = 0;

	// Return the Id
	const GS::Guid& GetId() const { return ElementId; }

	virtual void SetMesh(FSyncDatabase* /* IOSyncDatabase */, const TSharedPtr< IDatasmithMeshElement >& /* InMesh */)
	{
	}

  protected:
	// Add a child
	virtual void AddChildActor(const TSharedPtr< IDatasmithActorElement >& InActor) = 0;

	// Remove a child
	virtual void RemoveChildActor(const TSharedPtr< IDatasmithActorElement >& InActor) = 0;

	// Set (or replace) datasmith actor element related to this sync data
	virtual void SetActorElement(const TSharedPtr< IDatasmithActorElement >& InElement) = 0;

	// Return Element as an actor
	virtual const TSharedPtr< IDatasmithActorElement >& GetActorElement() const = 0;

	// Delete this sync data
	virtual void DeleteMe(FSyncDatabase* IOSyncDatabase);

	// Add a child to this sync data
	void AddChild(FSyncData* InChild);

	// Remove a child from this sync data
	void RemoveChild(FSyncData* InChild);

	// Return true if this element and all it's childs have been cut out
	virtual bool CheckAllCutOut();

	// Permanent id of the element (Synthethized elements, like layers, have synthetized guid).
	GS::Guid ElementId = GS::NULLGuid;

	// Temporary 3d index of the element
	GS::Int32 Index3D = 0;

	// 3d generation id, change when 3d geometry of the object is changed
	GS::UInt32 GenId = 0;

	// modification stamp
	UInt64 ModificationStamp = 0;

	// If GenId have changed or object is newly renderered
	bool bIsModified = false;

	// If this object is a component
	bool bIsAComponent = false;

	// Parent of this element
	FSyncData* Parent = nullptr;

	// Childs of this element
	typedef TArray< FSyncData* > FChildsArray;
	FChildsArray				 Childs;
};

class FSyncData::FScene : public FSyncData
{
  public:
	// Guid given to the scene element.
	static const GS::Guid SceneGUID;

	FScene();

	// Delete this sync data
	virtual TSharedPtr< IDatasmithElement > GetElement() const override { return SceneElement; };

  protected:
	// Set the element to the scene element
	virtual void Process(FProcessInfo* IOProcessInfo) override;

	// Delete this sync data
	virtual void DeleteMe(FSyncDatabase* IOSyncDatabase) override;

	// Update scene metadata
	void UpdateInfo(FProcessInfo* IOProcessInfo);

	// Add an child actor to my scene
	virtual void AddChildActor(const TSharedPtr< IDatasmithActorElement >& InActor) override;

	// Remove an child actor from my scene
	virtual void RemoveChildActor(const TSharedPtr< IDatasmithActorElement >& InActor) override;

	// Call this a FScene has no meaning... calling it will throw an exception.
	virtual void SetActorElement(const TSharedPtr< IDatasmithActorElement >& InActor) override;

	// Return an invalid actor shared ptr
	virtual const TSharedPtr< IDatasmithActorElement >& GetActorElement() const override;

	// The mesh element if this element is a mesh actor
	TSharedPtr< IDatasmithScene > SceneElement;

	// Empty actor that will contain matedata info on the scene
	TSharedPtr< IDatasmithActorElement > SceneInfoActorElement;

	// The mesh element if this element is a mesh actor
	TSharedPtr< IDatasmithMetaDataElement > SceneInfoMetaData;
};

class FSyncData::FActor : public FSyncData
{
	// Delete this sync data
	virtual TSharedPtr< IDatasmithElement > GetElement() const override { return ActorElement; };

  protected:
	FActor(const GS::Guid& InGuid);

	// Delete this sync data
	virtual void DeleteMe(FSyncDatabase* IOSyncDatabase) override;

	// Add an child actor to my element
	virtual void AddChildActor(const TSharedPtr< IDatasmithActorElement >& InActor) override;

	// Remove an child actor to my element
	virtual void RemoveChildActor(const TSharedPtr< IDatasmithActorElement >& InActor) override;

	// Set (or replace) datasmith actor element related to this sync data
	virtual void SetActorElement(const TSharedPtr< IDatasmithActorElement >& InActor) override;

	// Return Element as an actor
	virtual const TSharedPtr< IDatasmithActorElement >& GetActorElement() const override { return ActorElement; }

	typedef GS::Array< GS::UniString > FTagsArray;

	// Update tags data
	bool UpdateTags(const FTagsArray& InTags);

	void ReplaceMetaData(IDatasmithScene& IOScene, const TSharedPtr< IDatasmithMetaDataElement >& InNewMetaData);

	TSharedPtr< IDatasmithActorElement > ActorElement;

	// The mesh element if this element is a mesh actor
	TSharedPtr< IDatasmithMetaDataElement > MetaData;
};

class FSyncData::FLayer : public FSyncData::FActor
{
  public:
	// Guid used to synthetize layer guid
	static const GS::Guid LayerGUID;

	// Return the synthetized layer guid.
	static GS::Guid GetLayerGUID(short Layer);

	// Return true if this guid is for a layer
	static short IsLayerGUID(GS::Guid LayerID);

	// Return the layer index
	static short GetLayerIndex(const GS::Guid& InLayerID);

	FLayer(const GS::Guid& InGuid);

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;
};

class FSyncData::FElement : public FSyncData::FActor
{
  public:
	FElement(const GS::Guid& InGuid, const FSyncContext& InSyncContext);

	// Mesh has changed, update the actor accordingly
	void MeshElementChanged();

	// Access to the element mesh handle
	TSharedPtr< IDatasmithMeshElement >& GetMeshElementRef() { return MeshElement; }

	// Add tags data
	bool AddTags(FSyncDatabase* IOSyncDatabase);

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;

	// Delete this sync data
	virtual void DeleteMe(FSyncDatabase* IOSyncDatabase) override;

	// Attach observer for Auto Sync
	virtual bool AttachObserver(FAttachObservers* IOAttachObservers) override;

	// Return true if this element need to update tags and metadata
	virtual bool NeedTagsAndMetaDataUpdate() override;

	// Process meta data. Return true if meta data was updated
	virtual bool ProcessMetaData(FSyncDatabase* IOSyncDatabase) override;

	virtual void SetMesh(FSyncDatabase* IOSyncDatabase, const TSharedPtr< IDatasmithMeshElement >& InMesh) override;

	// Rebuild the meta data of this element
	bool UpdateMetaData(IDatasmithScene* InScene);

	// Return true if this element and all it's childs have been cut out
	bool CheckAllCutOut() override;

	// The mesh element if this element is a mesh actor
	TSharedPtr< IDatasmithMeshElement > MeshElement;

	// True if metadata is updated
	bool bMetadataProcessed = false;

	// True if we observe this element
	bool bIsObserved = false;

	// Type of this element
	API_ElemTypeID TypeID = API_ZombieElemID;
};

class FSyncData::FCameraSet : public FSyncData::FActor
{
  public:
	FCameraSet(const GS::Guid& InGuid, const GS::UniString& InName, bool bInOpenedPath)
		: FSyncData::FActor(InGuid)
		, Name(InName)
		, bOpenedPath(bInOpenedPath)
	{
	}

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;

	const GS::UniString Name;
	bool				bOpenedPath;
};

class FSyncData::FCamera : public FSyncData::FActor
{
  public:
	// Guid given to the current view.
	static const GS::Guid CurrentViewGUID;

	FCamera(const GS::Guid& InGuid, GS::Int32 InIndex)
		: FSyncData::FActor(InGuid)
		, Index(InIndex)
	{
	}

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;

	void InitWithCurrentView();

	void InitWithCameraElement();

	GS::Int32 Index;
};

class FSyncData::FLight : public FSyncData::FActor
{
  public:
	class FLightGDLParameters
	{
	  public:
		FLightGDLParameters();
		FLightGDLParameters(const API_Guid& InLightGuid, const class FLibPartInfo* InLibPartInfo);

		bool operator!=(const FLightGDLParameters& InOther) const;

		enum EC4dDetAreaShape
		{
			kNoShape = 0,
			kDisc,
			kRectangle,
			kSphere,
			kCylinder,
			kCube,
			kHemisphere,
			kLine,
			kPerpendicularCylinder
		};

		ModelerAPI::Color	 GS_Color;
		unsigned char		 ColorComponentCount = 0;
		double				 Intensity = 1.0;
		bool				 bUsePhotometric = false;
		EDatasmithLightUnits Units = EDatasmithLightUnits::Unitless;
		double				 DetRadius = 0.0; // Meter
		GS::UniString		 IESFileName;
		bool				 bIsAreaLight = false;
		EC4dDetAreaShape	 AreaShape = kNoShape;
		Geometry::Vector3D	 AreaSize = {};
		double				 WindowLightAngle = 0.0;
		double				 SunAzimuthAngle = 0.0;
		double				 SunAltitudeAngle = 0.0;
		bool				 bIsParallelLight = false;
		bool				 bGenShadow = false;
	};

	class FLightData
	{
	  public:
		FLightData();
		FLightData(const ModelerAPI::Light& InLight);
		bool operator!=(const FLightData& InOther) const;

		ModelerAPI::Light::Type LightType = ModelerAPI::Light::Type::UndefinedLight;
		float					InnerConeAngle = 15.0;
		float					OuterConeAngle = 75.0;
		FLinearColor			Color = FLinearColor::White;
		FVector					Position = FVector::ZeroVector;
		FQuat					Rotation = FQuat::Identity;
	};

	FLight(const GS::Guid& InGuid, GS::Int32 InIndex)
		: FSyncData::FActor(InGuid)
		, Index(InIndex)
	{
	}

	void SetLightData(const FLightData& InLightData)
	{
		if (LightData != InLightData)
		{
			LightData = InLightData;
			bIsModified = true;
		}
	}

	void SetValuesFromParameters(const FLightGDLParameters& InParameters)
	{
		if (Parameters != InParameters)
		{
			Parameters = InParameters;
			bIsModified = true;
		}
	}

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;

	GS::Int32			Index;
	FLightGDLParameters Parameters;
	FLightData			LightData;
};

class FSyncData::FHotLinksRoot : public FSyncData::FActor
{
  public:
	// Guid given to the current view.
	static const GS::Guid HotLinksRootGUID;

	FHotLinksRoot()
		: FSyncData::FActor(HotLinksRootGUID)
	{
	}

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;
};

class FSyncData::FHotLinkNode : public FSyncData::FActor
{
  public:
	FHotLinkNode(const GS::Guid& InGuid)
		: FSyncData::FActor(InGuid)
	{
	}

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;
};

class FSyncData::FHotLinkInstance : public FSyncData::FActor
{
  public:
	FHotLinkInstance(const GS::Guid& InGuid, FSyncDatabase* IOSyncDatabase);

	const API_Tranmat& GetTransformation() { return Transformation; }

  protected:
	virtual void Process(FProcessInfo* IOProcessInfo) override;

	API_Tranmat Transformation;
};

class FSyncData::FInterator
{
  public:
	// Destructor
	virtual ~FInterator() {}

	// Start the process with this root observer
	void Start(FSyncData* Root);

	// Stop processing
	void Stop();

	// Return true if we need to process
	bool NeedProcess() const { return Stack.Num() != 0; }

	// Process attachment until done or until time slice finish
	enum EProcessControl
	{
		kDone, // Task is terminated
		kInterrupted, // Task is interrupted -> need restart to resume
		kContinue // Time slice end, wait for another idle
	};
	EProcessControl ProcessUntil(double TimeSliceEnd);

	EProcessControl ProcessAll() { return ProcessUntil(std::numeric_limits< double >::max()); }

	virtual EProcessControl Process(FSyncData* InCurrent) = 0;

	// Return the next FSyncData
	FSyncData* Next();

	// Return the index of the current.
	FChildsArray::SizeType GetCurrentIndex();

	// Return the current count of item processed
	int32 GetProcessedCount() const { return ProcessedCount; }

	// Return the cumulated process time
	double GetProcessedTime() const { return ProcessTime; }

  private:
	// Stack element
	class FEntry
	{
	  public:
		FSyncData*			   Parent;
		FChildsArray::SizeType ChildIndex;
	};

	typedef TArray< FEntry > FEntriesArray;
	FEntriesArray			 Stack;
	int32					 ProcessedCount = 0;
	double					 ProcessTime = 0.0;
};

// Class to process metadata as idle task (Only for Direct Link synchronization)
class FSyncData::FProcessMetadata : public FSyncData::FInterator
{
  public:
	// Constructor
	FProcessMetadata(FSynchronizer* InSynchronizer)
		: Synchronizer(InSynchronizer)
	{
	}

	// Start the process with this root observer
	void Start(FSyncData* Root);

	// Call ProcessMetaData for the sync data
	virtual EProcessControl Process(FSyncData* InCurrent) override;

	// Return true if at least one sync data updated it's meta data
	bool HasMetadataUpdated() const { return bMetadataUpdated; }

	// Tell that we already synced previously processed sync data
	void CleardMetadataUpdated() { bMetadataUpdated = false; }

	// Count of processed
	int GetProcessedCount() const { return MetadataProcessedCount; }

  private:
	// My synchronizer
	FSynchronizer* Synchronizer = nullptr;
	// Count of processed
	int MetadataProcessedCount = 0;
	// True if at least one sync data updated it's meta data
	bool bMetadataUpdated = false;
};

#define ATTACH_ONSERVER_STAT 1

// Class use for scanning db to find element not yet registered.
class FSyncData::FAttachObservers : public FSyncData::FInterator
{
  public:
	// Constructor
	FAttachObservers();

	// Start the process with this root observer
	void Start(FSyncData* Root);

	// Call AttachObserver for the sync data
	virtual EProcessControl Process(FSyncData* InCurrent) override;

	// Process attachment until done or until time slice finish
	bool ProcessAttachUntil(double TimeSliceEnd);

#if ATTACH_ONSERVER_STAT
	void CumulateStats(const FTimeStat& SlotStart, double AfterAttachObserver);

	// Log attach observer statistics
	void PrintStat();
#endif

  private:
#if ATTACH_ONSERVER_STAT
	FTimeStat AttachObserverProcessTimeStart;
	FTimeStat AttachObserverProcessTimeEnd;
	double	  AttachObserverStartTime = 0.0;
	double	  AttachObserverTime = 0.0;
	double	  GetHeaderTime = 0.0;
	int		  AttachCount = 0;
#endif
};

END_NAMESPACE_UE_AC
