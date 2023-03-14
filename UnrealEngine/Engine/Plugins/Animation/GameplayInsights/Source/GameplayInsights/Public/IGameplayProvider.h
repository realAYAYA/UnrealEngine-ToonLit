// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Containers/Timelines.h"

struct FClassInfo
{	
	uint64 Id = 0;
	uint64 SuperId = 0;
	const TCHAR* Name = nullptr;
	const TCHAR* PathName = nullptr;
};

struct FObjectInfo
{	
	uint64 Id = 0;
	uint64 OuterId = 0;
	uint64 ClassId = 0;
	const TCHAR* Name = nullptr;
	const TCHAR* PathName = nullptr;
};

struct FObjectPropertiesMessage
{
	int64 PropertyValueStartIndex = INDEX_NONE;
	int64 PropertyValueEndIndex = INDEX_NONE;
};

struct FObjectPropertyValue
{
	const TCHAR* Value = nullptr;
	int32 ParentId = 0;
	uint32 TypeStringId = 0;
	uint32 KeyStringId = 0;
	float ValueAsFloat = 0.0f;
};

struct FObjectEventMessage
{
	uint64 Id = 0;
	const TCHAR* Name = nullptr;
};

struct FRecordingInfoMessage
{
	uint64 WorldId;
	double ProfileTime;
	double ElapsedTime;
	uint32 FrameIndex;
	uint32 RecordingIndex;
};

struct FViewMessage
{
	uint64 PlayerId;
	FVector Position;
	FRotator Rotation;
	float Fov;
	float AspectRatio;
};

struct FWorldInfo
{
	/** Types of worlds that we know about - synced with EngineTypes.h */
	enum class EType : uint8
	{
		None,
		Game,
		Editor,
		PIE,
		EditorPreview,
		GamePreview,
		GameRPC,
		Inactive
	};

	/** Types of net modes that we know about - synced with EngineBaseTypes.h */
	enum class ENetMode : uint8
	{
		Standalone,
		DedicatedServer,
		ListenServer,
		Client,

		MAX,
	};

	uint64 Id = 0;
	int32 PIEInstanceId = 0;
	EType Type = EType::None;
	ENetMode NetMode = ENetMode::Standalone;
	bool bIsSimulating = false;
};

// Delegate fired when an object receives an end play event
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnObjectEndPlay, uint64 /*ObjectId*/, double /*Time*/, const FObjectInfo& /*ObjectInfo*/);

class IGameplayProvider : public TraceServices::IProvider
{
public:
	typedef TraceServices::ITimeline<FObjectEventMessage> ObjectEventsTimeline;
	typedef TraceServices::ITimeline<FObjectPropertiesMessage> ObjectPropertiesTimeline;
	typedef TraceServices::ITimeline<FRecordingInfoMessage> RecordingInfoTimeline;
	typedef TraceServices::ITimeline<FViewMessage> ViewTimeline;

	virtual bool ReadObjectEventsTimeline(uint64 InObjectId, TFunctionRef<void(const ObjectEventsTimeline&)> Callback) const = 0;
	virtual bool ReadObjectEvent(uint64 InObjectId, uint64 InMessageId, TFunctionRef<void(const FObjectEventMessage&)> Callback) const = 0;
	virtual bool ReadObjectPropertiesTimeline(uint64 InObjectId, TFunctionRef<void(const ObjectPropertiesTimeline&)> Callback) const = 0;
	virtual void EnumerateObjectPropertyValues(uint64 InObjectId, const FObjectPropertiesMessage& InMessage, TFunctionRef<void(const FObjectPropertyValue&)> Callback) const = 0;
	virtual void EnumerateObjects(TFunctionRef<void(const FObjectInfo&)> Callback) const = 0;
	virtual void EnumerateObjects(double StartTime, double EndTime, TFunctionRef<void(const FObjectInfo&)> Callback) const = 0;
	virtual void EnumerateSubobjects(uint64 ObjectId, TFunctionRef<void(uint64 SubobjectId)> Callback) const = 0;
	virtual const FClassInfo* FindClassInfo(uint64 InClassId) const = 0;
	virtual const UClass* FindClass(uint64 InClassId) const = 0;
	virtual const FClassInfo* FindClassInfo(const TCHAR* InClassPath) const = 0;
	virtual const FObjectInfo* FindObjectInfo(uint64 InObjectId) const = 0;
	virtual const FWorldInfo* FindWorldInfo(uint64 InObjectId) const = 0;
	virtual const FWorldInfo* FindWorldInfoFromObject(uint64 InObjectId) const = 0;
	virtual bool IsWorld(uint64 InObjectId) const = 0;
	virtual const FClassInfo& GetClassInfo(uint64 InClassId) const = 0;
	virtual const FClassInfo& GetClassInfoFromObject(uint64 InObjectId) const = 0;
	virtual const FObjectInfo& GetObjectInfo(uint64 InObjectId) const = 0;
	virtual FOnObjectEndPlay& OnObjectEndPlay() = 0;
	virtual const TCHAR* GetPropertyName(uint32 InPropertyStringId) const = 0;
	virtual const RecordingInfoTimeline* GetRecordingInfo(uint32 RecordingId) const = 0; 
	virtual void ReadViewTimeline(TFunctionRef<void(const ViewTimeline&)> Callback) const = 0;
	virtual uint64 FindPossessingController(uint64 Pawn, double Time) const = 0;
	virtual TRange<double> GetObjectTraceLifetime(uint64 ObjectId) const = 0;
	virtual TRange<double> GetObjectRecordingLifetime(uint64 ObjectId) const = 0;
};