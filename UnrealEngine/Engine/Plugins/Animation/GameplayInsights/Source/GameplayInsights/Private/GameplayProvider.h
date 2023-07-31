// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IGameplayProvider.h"
#include "Model/PointTimeline.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Containers/StringView.h"
#include "Model/IntervalTimeline.h"

namespace TraceServices { class IAnalysisSession; }

class FGameplayProvider : public IGameplayProvider
{
public:
	static FName ProviderName;

	FGameplayProvider(TraceServices::IAnalysisSession& InSession);

	/** IGameplayProvider interface */
	virtual bool ReadObjectEventsTimeline(uint64 InObjectId, TFunctionRef<void(const ObjectEventsTimeline&)> Callback) const override;
	virtual bool ReadObjectEvent(uint64 InObjectId, uint64 InMessageId, TFunctionRef<void(const FObjectEventMessage&)> Callback) const override;
	virtual bool ReadObjectPropertiesTimeline(uint64 InObjectId, TFunctionRef<void(const ObjectPropertiesTimeline&)> Callback) const override;
	virtual void EnumerateObjectPropertyValues(uint64 InObjectId, const FObjectPropertiesMessage& InMessage, TFunctionRef<void(const FObjectPropertyValue&)> Callback) const override;
	virtual void EnumerateObjects(TFunctionRef<void(const FObjectInfo&)> Callback) const override;
	virtual void EnumerateObjects(double StartTime, double EndTime, TFunctionRef<void(const FObjectInfo&)> Callback) const override;
	virtual void EnumerateSubobjects(uint64 ObjectId, TFunctionRef<void(uint64 SubobjectId)> Callback) const override;
	virtual const FClassInfo* FindClassInfo(uint64 InClassId) const override;
	virtual const UClass* FindClass(uint64 InClassId) const override;
	virtual const FClassInfo* FindClassInfo(const TCHAR* InClassPath) const override;
	virtual const FObjectInfo* FindObjectInfo(uint64 InObjectId) const override;
	virtual const FWorldInfo* FindWorldInfo(uint64 InObjectId) const override;
	virtual const FWorldInfo* FindWorldInfoFromObject(uint64 InObjectId) const override;
	virtual bool IsWorld(uint64 InObjectId) const override;
	virtual const FClassInfo& GetClassInfo(uint64 InClassId) const override;
	virtual const FClassInfo& GetClassInfoFromObject(uint64 InObjectId) const override;
	virtual const FObjectInfo& GetObjectInfo(uint64 InObjectId) const override;
	virtual FOnObjectEndPlay& OnObjectEndPlay() override { return OnObjectEndPlayDelegate; }
	virtual const TCHAR* GetPropertyName(uint32 InPropertyStringId) const override;
	virtual const RecordingInfoTimeline* GetRecordingInfo(uint32 RecordingId) const override; 
	virtual void ReadViewTimeline(TFunctionRef<void(const ViewTimeline&)> Callback) const override;


	/** Add a class message */
	void AppendClass(uint64 InClassId, uint64 InSuperId, const TCHAR* InClassName, const TCHAR* InClassPathName);

	/** Add an object message */
	void AppendObject(uint64 InObjectId, uint64 InOuterId, uint64 InClassId, const TCHAR* InObjectName, const TCHAR* InObjectPathName);

	/** Add an object create message */
	void AppendObjectLifetimeBegin(uint64 InObjectId, double InProfileTime, double InRecordingTime);
	
	/** Add an object destroy message */
	void AppendObjectLifetimeEnd(uint64 InObjectId, double InProfileTime, double InRecordingTime);	

	/** Add an object event message */
	void AppendObjectEvent(uint64 InObjectId, double InTime, const TCHAR* InEvent);
	
	/** Add a Controller Attach message */
	void AppendPawnPossess(uint64 InControllerId, uint64 InPawnId, double InTime);

	/** Add a view message */
	void AppendView(uint64 InObjectId, double InTime, const FVector& InPosition, const FRotator& InRotation, float InFov, float InAspectRatio);

	/** Add a world message */
	void AppendWorld(uint64 InObjectId, int32 InPIEInstanceId, uint8 InType, uint8 InNetMode, bool bInIsSimulating);

	/** Add a recording info message */
	void AppendRecordingInfo(uint64 InWorldId, double InProfileTime, uint32 InRecordingIndex, uint32 InFrameIndex, double InElapsedTime);

	/** Add a class property string ID message */
	void AppendClassPropertyStringId(uint32 InStringId, const FStringView& InString);

	/** Add a properties start message */
	void AppendPropertiesStart(uint64 InObjectId, double InTime, uint64 InEventId);

	/** Add a properties end message */
	void AppendPropertiesEnd(uint64 InObjectId, double InTime);

	/** Add a property value message */
	void AppendPropertyValue(uint64 InObjectId, double InTime, uint64 InEventId, int32 InParentId, uint32 InTypeStringId, uint32 InKeyStringId, const FStringView& InValue);

	/** Check whether we have any data */
	bool HasAnyData() const;

	/** Check whether we have any object property data */
	bool HasObjectProperties() const;

	/** Search PawnPossession timeline to find the Controller Object Id for a Pawn (or 0 if none)*/
	virtual uint64 FindPossessingController(uint64 Pawn, double Time) const override;
	

	/** find the Trace time range for which an object existed */
	virtual TRange<double> GetObjectTraceLifetime(uint64 ObjectId) const override;

	/** find the Recording time range for which an object existed */
	virtual TRange<double> GetObjectRecordingLifetime(uint64 ObjectId) const override;

private:
	TraceServices::IAnalysisSession& Session;

	/** All class info, grow only for stable indices */
	TArray<FClassInfo> ClassInfos;

	/** All object info, grow only for stable indices */
	TArray<FObjectInfo> ObjectInfos;

	/** All world info, grow only for stable indices */
	TArray<FWorldInfo> WorldInfos;

	/** Classes that are in use. Map from Id to ClassInfo index */
	TMap<uint64, int32> ClassIdToIndexMap;

	/** Objects that are in use. Map from Id to ObjectInfo index */
	TMap<uint64, int32> ObjectIdToIndexMap;

	/** Worlds that are in use. Map from Id to WorldInfo index */
	TMap<uint64, int32> WorldIdToIndexMap;

	/** Map from object Ids to timeline index */
	TMap<uint64, uint32> ObjectIdToEventTimelines;
	TMap<uint64, uint32> ObjectIdToPropertiesStorage;

	struct FPawnPossessMessage
	{
		uint64 ControllerId = 0;
		uint64 PawnId = 0;
	};
	
	struct FObjectExistsMessage
	{
		uint64 ObjectId = 0;
	};

	struct FObjectPropertiesStorage
	{
		double OpenStartTime;
		uint64 OpenEventId;
		FObjectPropertiesMessage OpenEvent;
		TSharedPtr<TraceServices::TIntervalTimeline<FObjectPropertiesMessage>> Timeline;
		TArray<FObjectPropertyValue> Values;
	};

	/** Message storage */
	TArray<TSharedRef<TraceServices::TPointTimeline<FObjectEventMessage>>> EventTimelines;
	TArray<TSharedRef<FObjectPropertiesStorage>> PropertiesStorage;
	TSharedPtr<TraceServices::TPointTimeline<FViewMessage>> ViewTimeline;

	/** Map of class path name to ClassInfo index */
	TMap<FStringView, int32> ClassPathNameToIndexMap;

	/** EndPlay event text */
	const TCHAR* EndPlayEvent;

	/** Delegate fired when an object receives an end play event */
	FOnObjectEndPlay OnObjectEndPlayDelegate;

	/** Map from string ID to stored string */
	TMap<uint32, const TCHAR*> PropertyStrings;

	/** Map of RecordingInfo Timelines by RecordingId - Each timeline is a mapping from Gameplay Elapsed time to Profiler Elapsed Time for one recording session */
	TMap<uint32, TSharedRef<TraceServices::TPointTimeline<FRecordingInfoMessage>>> Recordings;

	// Timeline containing intervals where a controller is attached to a pawn
	TraceServices::TIntervalTimeline<FPawnPossessMessage> PawnPossession;
	
	// Timeline containing intervals where an object exists
	TraceServices::TIntervalTimeline<FObjectExistsMessage> ObjectLifetimes;
	
	// Timeline containing intervals where an object exists - In Rewind Debugger recording time units, to avoid excessive conversions
	TraceServices::TIntervalTimeline<FObjectExistsMessage> ObjectRecordingLifetimes;


	// map from controller id to index in the PawnPossession timeline, for lookup when ending events
	TMap<uint64, uint64> ActivePawnPossession;

	// map from object id to index in ObjectLifetimes, for lookup when objects are destroyed
	TMap<uint64, uint64> ActiveObjectLifetimes;
	
	// map from object id to index in ObjectRecordingLifetimes, for lookup
	TMap<uint64, uint64> ActiveObjectRecordingLifetimes;

	/** Whether we have any data */
	bool bHasAnyData;

	/** Whether we have any object properties */
	bool bHasObjectProperties;

	/** MultiMap from parent object to sub objects, to avoid slow reverse lookups */
	TMultiMap<uint64, uint64> ObjectHierarchy;
};
