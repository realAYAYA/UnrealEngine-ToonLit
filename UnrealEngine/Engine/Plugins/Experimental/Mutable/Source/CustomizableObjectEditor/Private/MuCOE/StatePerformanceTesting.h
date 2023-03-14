// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "UObject/GCObject.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "StatePerformanceTesting.generated.h"

class UCustomizableObject;
class UCustomizableObjectInstance;
class UCustomizableSkeletalComponent;
class USkeletalMeshComponent;
class UUpdateClassHelper;
struct FFrame;


enum class TestType
{
	RuntimeParameter,
	CustomizableObjectMemory
};


struct MeasuredData
{
	MeasuredData() :
		Name("")
		, MeanTime(0.0f)
		, MinTime(FLT_MAX)
		, MaxTime(FLT_MIN)
		, Accumulated(0.0f)
		, NumMeasures(0.0f)
	{

	}

	MeasuredData(FString& NameParam) :
		Name(NameParam)
		, MeanTime(0.0f)
		, MinTime(FLT_MAX)
		, MaxTime(FLT_MIN)
		, Accumulated(0.0f)
		, NumMeasures(0.0f)
	{

	}

	void AddMeasureData(float ElapsedTime)
	{
		Accumulated += ElapsedTime;
		MinTime = FMath::Min(MinTime, ElapsedTime);
		MaxTime = FMath::Max(MaxTime, ElapsedTime);
		NumMeasures += 1.0f;
		MeanTime = Accumulated / NumMeasures;
	}


	MeasuredData operator+=(const MeasuredData& Other)
	{
		if (this->Name != Other.Name)
		{
			return *this;
		}

		if ((HasInitialValues(*this) && HasInitialValues(Other)) ||
			(!HasInitialValues(*this) && HasInitialValues(Other)))
		{
			return *this;
		}
		else if (HasInitialValues(*this) && !HasInitialValues(Other))
		{
			*this = Other;
			return *this;
		}

		this->Accumulated += Other.Accumulated;
		this->MaxTime = FMath::Max(this->MaxTime, Other.MaxTime);
		this->MinTime = FMath::Min(this->MinTime, Other.MinTime);
		this->NumMeasures += Other.NumMeasures;
		this->MeanTime = this->Accumulated / this->NumMeasures;
		return *this;
	}

	bool HasInitialValues(const MeasuredData& Data)
	{
		if ((Data.MeanTime == 0.0f) &&
			(Data.MinTime == FLT_MAX) &&
			(Data.MaxTime == FLT_MIN) &&
			(Data.Accumulated == 0.0f) &&
			(Data.NumMeasures == 0.0f))
			{
				return true;
			}

		return false;
	}


	FString Name;
	float MeanTime;
	float MinTime;
	float MaxTime;
	float Accumulated;
	float NumMeasures;
};


struct BatchParameterChange
{
	/** Float change constructor */
	BatchParameterChange(int32 StateParam, FString& StateName, FString& NameParam, float ValueFloatParam) :
		  State(StateParam)
		, StateName(StateName)
		, ParameterType(EMutableParameterType::Float)
		, Name(NameParam)
		, ValueFloat(ValueFloatParam)
		, ValueBool(false)
		, IsStateChange(false)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** Integer parameter change constructor */
	BatchParameterChange(int32 StateParam, FString& StateName, FString& NameParam, FString ParameterOptionParam) :
		  State(StateParam)
		, StateName(StateName)
		, ParameterType(EMutableParameterType::Int)
		, Name(NameParam)
		, ValueFloat(-1.0f)
		, OptionName(ParameterOptionParam)
		, ValueBool(false)
		, IsStateChange(false)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** Bool change constructor */
	BatchParameterChange(int32 StateParam, FString& StateName, FString& NameParam, bool ValueBoolParam) :
		  State(StateParam)
		, StateName(StateName)
		, ParameterType(EMutableParameterType::Bool)
		, Name(NameParam)
		, ValueFloat(-1.0f)
		, ValueBool(ValueBoolParam)
		, IsStateChange(false)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** Color change constructor */
	BatchParameterChange(int32 StateParam, FString& StateName, FString& NameParam, FLinearColor ColorValueParam) :
		  State(StateParam)
		, StateName(StateName)
		, ParameterType(EMutableParameterType::Color)
		, Name(NameParam)
		, ValueFloat(-1.0f)
		, ValueBool(false)
		, ColorValue(ColorValueParam)
		, IsStateChange(false)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** Projector position change constructor */
	BatchParameterChange(int32 StateParam, FString& StateName, FString& NameParam, FVector VectorValueParam) :
		  State(StateParam)
		, StateName(StateName)
		, ParameterType(EMutableParameterType::Projector)
		, Name(NameParam)
		, ValueFloat(-1.0f)
		, ValueBool(false)
		, VectorValue(VectorValueParam)
		, IsStateChange(false)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** State change constructor */
	BatchParameterChange(int32 StateParam, FString& NameParam) :
		  State(StateParam)
		, StateName(NameParam)
		, ParameterType(EMutableParameterType::None)
		, Name(NameParam)
		, ValueFloat(-1.0f)
		, ValueBool(false)
		, IsStateChange(true)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** Default constructor */
	BatchParameterChange() :
		  State(-1)
		, ParameterType(EMutableParameterType::None)
		, ValueFloat(-1.0f)
		, ValueBool(false)
		, IsStateChange(false)
		, ElapsedTime(0.0f)
		, TestStarted(false)
		, TestFinished(false)
	{

	}

	/** State of the parameter, in case this change in the batch is for a parameter. Otherwise, is the state to set (as a batch change) */
	int32 State = -1;

	/** State name (parameter name in the case of a parameter, or state name in the case of a state) */
	FString StateName;

	/** Parameter type, in case this change in the batch is for a parameter */
	EMutableParameterType ParameterType = EMutableParameterType::None;

	/** Name (parameter name in the case of a parameter, or state name in the case of a state) */
	FString Name;

	/** Value to change the parameter to, in case it's a float parameter */
	float ValueFloat = -1.0f;

	/** Value to change the parameter to, in case it's an integer parameter */
	FString OptionName;

	/** Value to change the parameter to, in case it's a bool parameter */
	bool ValueBool = false;

	/** Value to change the parameter to, in case it's a color parameter */
	FLinearColor ColorValue;

	/** Value to change the parameter to, in case it's a projector parameter: offset the position */
	FVector VectorValue;

	/** Flag to distinguish between state and parameter changes */
	bool IsStateChange = false;

	/** Th elpased time to perform this batch change */
	float ElapsedTime = 0.0f;

	/** Flag to know if the test for this batch test started */
	bool TestStarted = false;

	/** Flag to know if the test for this batch test finished */
	bool TestFinished = false;
};


struct StateWholeInformation
{
	/** State index */
	int32 State;

	/** State name */
	FString StateName;

	/** Copy of run time boolean parameters */
	TArray<struct FCustomizableObjectBoolParameterValue> BoolParameters;

	/** Index in the Customizable Object of the parameters in BoolParameters */
	TArray<int32> BoolParametersIndex;

	/** Copy of run time integer parameters */
	TArray<struct FCustomizableObjectIntParameterValue> IntParameters;

	/** Index in the Customizable Object of the parameters in IntParameters */
	TArray<int32> IntParametersIndex;

	/** Copy of run time float parameters */
	TArray<struct FCustomizableObjectFloatParameterValue> FloatParameters;

	/** Index in the Customizable Object of the parameters in FloatParameters */
	TArray<int32> FloatParametersIndex;

	/** Copy of run time texture parameters */
	TArray<struct FCustomizableObjectTextureParameterValue> TextureParameters;

	/** Index in the Customizable Object of the parameters in TextureParameters */
	TArray<int32> TextureParametersIndex;

	/** Copy of run time vector parameters */
	TArray<struct FCustomizableObjectVectorParameterValue> VectorParameters;

	/** Index in the Customizable Object of the parameters in VectorParameters */
	TArray<int32> VectorParametersIndex;

	/** Copy of run time projector parameters */
	TArray<struct FCustomizableObjectProjectorParameterValue> ProjectorParameters;

	/** Index in the Customizable Object of the parameters in ProjectorParameters */
	TArray<int32> ProjectorParametersIndex;
};


// Callbacks to notify of worst cases found when running the test. It can be called multiple times per test, as worse cases pop up.
DECLARE_DELEGATE_SixParams(FStateTestWorstTimeFound, float, int32, int32, uint32, UCustomizableObjectInstance*, MeasuredData*);
// Callback to notify that the test has finished, before data is cleared
DECLARE_DELEGATE(FStateTestEnded);


class FTestingCustomizableObject : public FGCObject
{
public:

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

public:
	/** Default constructor */
	FTestingCustomizableObject(TestType TestParameter);

	/** Default destructor */
	virtual ~FTestingCustomizableObject();

	/** Init test */
	virtual void InitTest();

	/** Start the test */
	void StartTest(UCustomizableObject* CustomizableObject);

	/** Inits the loop through all states and runtime parameters */
	void InitStateAndParameterSequence();

	/** Finish the test */
	void FinishTest();

	/** Callback for instance update end delegate */
	void InstanceUpdateEnd();

	/** Getter of ActiveBatchIndex */
	int32 GetCurrentActiveBatchIndex();

	/** Getter of ArrayBatch.Num() */
	int32 GetNumBatches();

	/** Getter of BatchTestInCourse */
	bool GetBatchTestInCourse();

	/** Coordinates the advaces of each test of the batch test */
	bool RunStateTestTick(float DeltaTime);

	/** Instance to build to perform the test */
	UCustomizableObjectInstance* Instance;

	/** Instance skeletal component */
	TArray<UCustomizableSkeletalComponent*> CustomizableSkeletalComponents;

	/** Instance skeletal mesh component */
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;

	/** UObject class to be able to use the update callback */
	UPROPERTY()
	UUpdateClassHelper* HelperCallback;

	/** Getter of TestResourcesPresent */
	bool GetTestResourcesPresent();

	/** Delegate called when the worst generation time is found UP UNTIL NOW, with its info and a pointer to the instance. It can be called multiple times on a single test run. */
	FStateTestWorstTimeFound WorstTimeDelegate;

	/** Delegate called when the stress test finishes, before any data is erased. */
	FStateTestEnded StateTestEnded;

protected:
	/** Performs the change in state or runtime parameter according to the information in ActiveBatch */
	void LaunchNextBatch();

	/** To be called when a running test updates its instance parameters after a BatchParameterChange execution */
	virtual void InstanceTestUpdated(BatchParameterChange* BatchChange);

	/** Flag to know if there's a batch test running */
	bool BatchTestInCourse;

	/** Flag to know if the last ran batch test finished */
	bool BatchTestFinished;

	/** All Customizable Object state information to generate a proper batch of changes */
	TArray<StateWholeInformation> ArrayState;

	/** Array with the batch of changes to apply */
	TArray<BatchParameterChange> ArrayBatch;

	/** Active batch change index */
	int32 ActiveBatchIndex;

	/** Active batch change */
	BatchParameterChange* ActiveBatch;

	/** Handle for the delegate to run the Tick method in charge of coordinating the test */
	FTSTicker::FDelegateHandle TickDelegateHandle;

	/** Delegate to run the Tick method in charge of coordinating the test */
	FTickerDelegate TickDelegate;

	/** Flag to control when test resources are built (for AddReferencedObjects method) */
	bool TestResourcesPresent;

	/** Enum to know the type of test is being carried out by this instance */
	TestType Test;
};


UCLASS()
class UUpdateClassHelper : public UObject
{
public:
	GENERATED_BODY()

	/** Method to assign for the callback */
	UFUNCTION()
	void DelegatedCallback(UCustomizableObjectInstance* Instance);

	/** Pointer to existing class to notify */
	FTestingCustomizableObject* StateData = nullptr;
};


/** Class used to make the runtime parameter and state enter tests */
class FRuntimeTest : public FTestingCustomizableObject
{
public:
	/** Default constructor */
	FRuntimeTest();

	/** Loads the state and runtime parameter data of the instance */
	void LoadStateData(int32 StateIndex);

	/** Init test */
	void InitTest() final override;

	/** Getter of MapStateEnterData (copy to avoid any possible thread issues) */
	TMap<uint32, MeasuredData> GetMapStateEnterData();

	/** Getter of MapStateUpdateData (copy to avoid any possible thread issues) */
	TMap<uint32, MeasuredData> GetMapStateUpdateData();

	/** Getter of MapStateRuntimeData (copy to avoid any possible thread issues) */
	TMap<uint32, TArray<MeasuredData>> GetMapStateRuntimeData();

protected:
	/** To be called when a running test updates its instance parameters after a BatchParameterChange execution */
	void InstanceTestUpdated(BatchParameterChange* BatchChange) final override;

	/** Retrieve pointer to runtime parameter of name given as parameter, for state given as parameter */
	MeasuredData* FindStateRuntimeParameter(uint32 State, FString& Name);

	void AddMeasuredDataAndReportTime(MeasuredData* DataPtr, float ElapsedTime, int32 StateIndex, FString ParameterName = FString());

	/** Map storing state id -> state entering time data */
	TMap<uint32, MeasuredData> MapStateEnterData;

	/** Map storing state id -> state update time data */
	TMap<uint32, MeasuredData> MapStateUpdateData;

	/** Map storing state id -> state runtime parameters measure data */
	TMap<uint32, TArray<MeasuredData>> MapStateRuntimeData;
};