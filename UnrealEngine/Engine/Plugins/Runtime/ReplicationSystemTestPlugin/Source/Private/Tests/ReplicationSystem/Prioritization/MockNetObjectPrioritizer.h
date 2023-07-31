// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/Prioritization/NetObjectPrioritizer.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Containers/BitArray.h"
#include "MockNetObjectPrioritizer.generated.h"

namespace UE::Net::Private
{
	typedef uint32 FInternalNetHandle;
}

const UE::Net::FRepTag RepTag_NetTest_Priority = 0x22C6880A98E628EDULL;

UCLASS()
class UMockNetObjectPrioritizerConfig : public UNetObjectPrioritizerConfig
{
	GENERATED_BODY()

};

UCLASS()
class UMockNetObjectPrioritizer : public UNetObjectPrioritizer
{
	GENERATED_BODY()

public:
	struct FFunctionCallStatus
	{
		struct
		{
			uint32 Init;
			uint32 AddObject;
			uint32 RemoveObject;
			uint32 UpdateObjects;
			uint32 Prioritize;
		} CallCounts, SuccessfulCallCounts;
	};

	struct FFunctionCallSetup
	{
		FFunctionCallSetup() : AddObject({}) {}

		// AddObject
		struct FAddObject
		{
			bool ReturnValue;
		};

		FAddObject AddObject;
	};

	inline void SetFunctionCallSetup(const FFunctionCallSetup& Setup) { CallSetup = Setup; }
	inline const FFunctionCallStatus& GetFunctionCallStatus() const { return CallStatus; }
	inline void ResetFunctionCallStatus() { CallStatus = FFunctionCallStatus({}); }

	float GetPriority(UE::Net::Private::FInternalNetHandle ObjectIndex) const;

protected:
	virtual void Init(FNetObjectPrioritizerInitParams& Params) override;
	virtual bool AddObject(uint32 ObjectIndex, FNetObjectPrioritizerAddObjectParams& Params) override;
	virtual void RemoveObject(uint32 ObjectIndex, const FNetObjectPrioritizationInfo& Info) override;
	virtual void UpdateObjects(FNetObjectPrioritizerUpdateParams&) override;
	virtual void Prioritize(FNetObjectPrioritizationParams&) override;

private:
	UMockNetObjectPrioritizer();

	FFunctionCallStatus CallStatus;
	FFunctionCallSetup CallSetup;
	TBitArray<> AddedIndices;
	SIZE_T AddedCount;
	TMap<uint32, UPTRINT> ObjectToPriorityOffset;
	TMap<uint32, float> ObjectToPriority;

	static constexpr float DefaultPriority = 1.0f;
};
