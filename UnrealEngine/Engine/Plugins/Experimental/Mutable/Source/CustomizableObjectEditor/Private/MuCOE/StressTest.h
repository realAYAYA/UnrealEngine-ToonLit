// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Math/IntVector.h"
#include "Math/UnrealMathSSE.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCOE/StatePerformanceTesting.h"
#include "PixelFormat.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "StressTest.generated.h"

class UCustomizableObject;
class UCustomizableObjectInstance;
struct FFrame;


struct MaterialBriefInfo
{
	MaterialBriefInfo()
	{

	}

	MaterialBriefInfo(FString& NameParam, TArray<FString>& ArrayTextureNameParam, TArray<FIntVector> ArrayTextureSizeParam, TArray<EPixelFormat> ArrayTextureFormatParam) :
		Name(NameParam)
		, ArrayTextureName(ArrayTextureNameParam)
		, ArrayTextureSize(ArrayTextureSizeParam)
		, ArrayTextureFormat(ArrayTextureFormatParam)
	{

	}

	FString Name;
	TArray<FString> ArrayTextureName;
	TArray<FIntVector> ArrayTextureSize;
	TArray<EPixelFormat> ArrayTextureFormat;
};


struct MostExpensiveInstanceData
{
	MostExpensiveInstanceData():
		  CurrentLODNumFaces(0)
		, CurrentLODTextureSize(0)
	{

	}

	FString Name;
	uint32 CurrentLODNumFaces;
	uint32 CurrentLODTextureSize;
	TMap<FString, MaterialBriefInfo> MapMaterial;
	TArray<struct FCustomizableObjectBoolParameterValue> BoolParameters;
	TArray<struct FCustomizableObjectIntParameterValue> IntParameters;
	TArray<struct FCustomizableObjectFloatParameterValue> FloatParameters;
	TArray<struct FCustomizableObjectTextureParameterValue> TextureParameters;
	TArray<struct FCustomizableObjectVectorParameterValue> VectorParameters;
	TArray<struct FCustomizableObjectProjectorParameterValue> ProjectorParameters;
};


typedef TMap<FString, MeasuredData> MeasureDataMap;
typedef TMap<FString, MaterialBriefInfo> MaterialInfoMap;


// Callback to add update information to test
DECLARE_DELEGATE_SixParams(FStressTestInstanceUpdate, MeasureDataMap, MaterialInfoMap, uint32, uint32, uint32, UCustomizableObjectInstance*);
// Callbacks to notify of worst cases found when running the test. It can be called multiple times per test, as worse cases pop up.
DECLARE_DELEGATE_FourParams(FStressTestWorstNumFacesFound, uint32, uint32, UCustomizableObjectInstance*, MeasuredData*);
DECLARE_DELEGATE_FiveParams(FStressTestWorstTextureSizeFound, MaterialInfoMap, uint32, uint32, UCustomizableObjectInstance*, MeasuredData*);
// Callback to notify that the test has finished, before data is cleared
DECLARE_DELEGATE(FStressTestEnded);

UCLASS()
class ULiveInstance : public UObject
{
public:
	GENERATED_BODY()

	/** Method to assign for the callback */
	UFUNCTION()
	void DelegatedCallback(UCustomizableObjectInstance* Instance);

	/** Add to ArrayInformation the relevant information from Instance */
	void AddInstanceInformation();

	/** Init elements in MapMeasuredData the elevant information from Instance */
	static TMap<FString, MeasuredData> InitInstanceInformation();

	/** Amount of time an instance is kept before destroying it (the instance needs to complete at least the first skeletal mesh update) */
	int TimeToDie;

	/** After an instance has been updated, if this value is negative, a new skeleta lmesh update with random values is requested */
	int TimeToUpdate;

	/** Flag to control whether the instance has been updated */
	bool Updated;

	/** Instace being updated with random values */
	UCustomizableObjectInstance* Instance;

	/** Number of instances updated */
	uint32 NumInstanceUpdated;

	/** Delgate to call the owning test and add results */
	FStressTestInstanceUpdate TestDelegate;
};


class FRunningStressTest
{
public:
	/** Default constructor */
	FRunningStressTest();

	/** Ticker used to contorl logic during stress test */
	bool RunStressTestTick(float DeltaTime);

	/** Setup map to gather data and ticker */
	void InitMeasureTest();

	/** Show in the console log the test results */
	void WriteTestResults();

	/** Callback for new instance update information */
	void AddInstanceData(MeasureDataMap MapMeasuredParam, MaterialInfoMap MapMaterialParam, uint32 CurrentLODTextureSize, uint32 CurrentLODNumFaces, uint32 LOD, UCustomizableObjectInstance* Instance);

	/** Finish current test */
	void FinishTest();

	/** Will retrieve the parameters present in Instance and set them in Destination */
	void CaptureInstanceParameters(UCustomizableObjectInstance* Instance, MostExpensiveInstanceData* Destination);

	/** Getter of LiveInstances */
	TMap<class UCustomizableObjectInstance*, ULiveInstance*>& GetLiveInstances();

	/** Getter of StressTestReadyToReset */
	bool GetStressTestReadyToReset();

	/** Getter of TestInCourse */
	bool GetTestInCourse();

	/** Setter of CreateInstanceTimeMs */
	void SetCreateInstanceTimeMs(int32 CreateInstanceTimeMsParam);

	/** Setter of CreateInstanceTimeMsVar */
	void SetCreateInstanceTimeMsVar(int32 CreateInstanceTimeMsVarParam);

	/** Setter of InstanceUpdateTimeMs */
	void SetInstanceUpdateTimeMs(int32 InstanceUpdateTimeMsParam);

	/** Setter of InstanceUpdateTimeMsVar */
	void SetInstanceUpdateTimeMsVar(int32 InstanceUpdateTimeMsVarParam);

	/** Setter of InstanceLifeTimeMs */
	void SetInstanceLifeTimeMs(int32 InstanceLifeTimeMsParam);

	/** Setter of InstanceLifeTimeMsVar */
	void SetInstanceLifeTimeMsVar(int32 InstanceLifeTimeMsVarParam);

	/** Setter of PendingInstanceCount */
	void SetPendingInstanceCount(int32 PendingInstanceCountParam);

	/** Setter of CustomizableObject */
	void SetCustomizableObject(UCustomizableObject* CustomizableObjectParam);

	/** Setter of StressTestReadyToReset */
	void SetStressTestReadyToReset(bool StressTestReadyToResetParam);

	/** Setter of SetVerifyInstancesThreshold */
	void SetVerifyInstancesThreshold(float VerifyInstancesThresholdParam);

	/** Setter of NextInstanceTimeMs */
	void SetNextInstanceTimeMs(float NextInstanceTimeMsParam);

	/** Delegate called when the worst case for number of faces is found UP UNTIL NOW, with its info and a pointer to the instance. It can be called multiple times on a single test run. */
	FStressTestWorstNumFacesFound WorstFacesDelegate;

	/** Delegate called when the worst case for texture size is found UP UNTIL NOW, with its info and a pointer to the instance. It can be called multiple times on a single test run. */
	FStressTestWorstTextureSizeFound WorstTextureDelegate;

	/** Delegate called when the stress test finishes, before any data is erased. */
	FStressTestEnded StressTestEnded;

	/** Return the string verison of the pixel format enum given as parameter */
	static FString StringyfyEPixelFormat(EPixelFormat Value);

protected:
	/** Append Amount whitespace characters to Target */
	void AppendWhitespace(FString& Target, int32 Amount);

	/** Output face, texture size and material information in ArrayLODMostExpensive */
	void PrintMostExpensivePerLOD();

	/** Print parameter data in Data */
	void PrintParameterData(const MostExpensiveInstanceData& Data);

	/** Utility method to find the length of the longest parameter name in Data */
	uint32 FindLongestParameterName(const MostExpensiveInstanceData& Data);

	/** Utility method to add the headers used in PrintMostExpensivePerLOD */
	void PrintParameterHeader(FString Param, int32 Value, bool AddNewLine);

	/** Utility method to print the parameter information */
	void PrintMostExpensiveInstanceData(const MostExpensiveInstanceData& Data, uint32 LOD);

	/** Value used to determine how frequently a new instance should be made */
	int32 CreateInstanceTimeMs;

	/** Value used to determine how frequently a new instance should be made */
	int32 CreateInstanceTimeMsVar;

	/** Value used to determine how frequently an instance should be updated (the ULiveInstance::Updated flag is used as restriction) */
	int32 InstanceUpdateTimeMs;

	/** Value used to determine how frequently an instance should be updated (the ULiveInstance::Updated flag is used as restriction) */
	int32 InstanceUpdateTimeMsVar;

	/** Value used to set the maximum amount of time an instance is updated before being destroyed (the ULiveInstance::Updated flag is used as restriction) */
	int32 InstanceLifeTimeMs;

	/** Value used to set the maximum amount of time an instance is updated before being destroyed (the ULiveInstance::Updated flag is used as restriction) */
	int32 InstanceLifeTimeMsVar;

	/** Number of instances for the test */
	int32 PendingInstanceCount;

	/** Elapsed time in the current test */
	int64 TotalTimeMs = 0;

	/** Threshold time to build a new instance in the ticker method */
	int NextInstanceTimeMs = 0;

	/** Ticker delegate handle */
	FTSTicker::FDelegateHandle StressTestTickDelegateHandle;

	/** Ticker handle */
	FTickerDelegate StressTestTickDelegate;

	/** Map with the customizable object instances and the corresponding data */
	UPROPERTY()
	TMap<class UCustomizableObjectInstance*, ULiveInstance*> LiveInstances;

	/** Threshold time to verify all instances in the test have finished updating */
	float VerifyInstancesThreshold;

	/** Customizable object to build the instances */
	UCustomizableObject* CustomizableObject;

	/** True when all the instances in the stress test have finished being updated */
	bool StressTestReadyToReset;

	/** Counter to know the total number of instance update requested */
	uint32 NumUpdateRequested;

	/** Counter to know the total number of instance update already done */
	uint32 NumUpdatedDone;

	/** Information about each random values skeleton update carried out by this instance, each array element covers per-LOD data */
	TArray<TMap<FString, MeasuredData>> ArrayMeasuredDataPerLOD;

	/** To have a per-material breakdown information */
	TArray<TMap<FString, MaterialBriefInfo>> ArrayMaterialInfoPerLOD;

	/** Smallest texture found in the test */
	FIntVector TextureMinSize;

	/** Biggest texture found in the test */
	FIntVector TextureMaxSize;

	/** Flag to know if a test is in course */
	bool TestInCourse;

	/** Min texture size in bytes found in the test */
	uint32 MinTextureSize;

	/** Mean texture size in bytes found in the test */
	uint32 MeanTextureSize;

	/** Max texture size in bytes found in the test */
	uint32 MaxTextureSize;

	/** Array with per-LOD detailed information of the most expensive instance built regarding geometry data */
	TArray<MostExpensiveInstanceData> ArrayLODMostExpensiveGeometry;

	/** Array with per-LOD detailed information of the most expensive instance built regarding texture size data */
	TArray<MostExpensiveInstanceData> ArrayLODMostExpensiveTexture;

	/** Flag to control when to initialize the number of elements of ArrayLODMostExpensive in each test */
	bool ArrayLODResized;

	/** Number of LOD of the Customizable Object */
	uint32 NumLOD;
};
