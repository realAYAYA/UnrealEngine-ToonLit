// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuCOE/StatePerformanceTesting.h"

#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/EngineTypes.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformTime.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "UObject/Class.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UnrealNames.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FTestingCustomizableObject::FTestingCustomizableObject(TestType TestParameter)
	: Instance(nullptr)
	, BatchTestInCourse(false)
	, BatchTestFinished(false)
	, ActiveBatchIndex(-1)
	, ActiveBatch(nullptr)
	, TestResourcesPresent(false)
	, Test(TestParameter)
{
	HelperCallback = NewObject<UUpdateClassHelper>(GetTransientPackage());
	HelperCallback->StateData = this;
}


FTestingCustomizableObject::~FTestingCustomizableObject()
{
	if (BatchTestInCourse)
	{
		FinishTest();
	}

	HelperCallback = nullptr;
	Instance = nullptr;	
}


void FTestingCustomizableObject::InitTest()
{

}


void FTestingCustomizableObject::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Instance);
	Collector.AddReferencedObject(HelperCallback);

	for (UCustomizableSkeletalComponent* Elem : CustomizableSkeletalComponents)
	{
		Collector.AddReferencedObject(Elem);
	}
	for (USkeletalMeshComponent* Elem : SkeletalMeshComponents)
	{
		Collector.AddReferencedObject(Elem);
	}
}


FString FTestingCustomizableObject::GetReferencerName() const
{
	return TEXT("FTestingCustomizableObject");
}


void FTestingCustomizableObject::StartTest(UCustomizableObject* CustomizableObject)
{
	if (BatchTestInCourse)
	{
		FinishTest();
	}
	
	if (!CustomizableObject->IsCompiled())
	{
		FText Msg(LOCTEXT("StateTestCompileRequired","Please compile the Customizable Object before starting the test"));
		FMessageLog MessageLog("Mutable");
		MessageLog.Notify(Msg, EMessageSeverity::Info, true);
		return;
	}

	TestResourcesPresent = true;

	Instance = CustomizableObject->CreateInstance();

	const int32 NumComponents = CustomizableObject->ReferenceSkeletalMeshes.Num();
	SkeletalMeshComponents.Reset(NumComponents);
	CustomizableSkeletalComponents.Reset(NumComponents);

	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = NewObject<UCustomizableSkeletalComponent>(UCustomizableSkeletalComponent::StaticClass());
		if (CustomizableSkeletalComponent)
		{
			CustomizableSkeletalComponents.Add(CustomizableSkeletalComponent);
			
			CustomizableSkeletalComponent->CustomizableObjectInstance = Instance;
			CustomizableSkeletalComponent->ComponentIndex = ComponentIndex;

			USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			if (SkeletalMeshComponent)
			{
				SkeletalMeshComponents.Add(SkeletalMeshComponent);

				CustomizableSkeletalComponent->AttachToComponent(SkeletalMeshComponent, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}

	if (SkeletalMeshComponents.Num() && SkeletalMeshComponents.Num() == CustomizableSkeletalComponents.Num())
	{
		Instance->UpdatedDelegate.AddDynamic(HelperCallback, &UUpdateClassHelper::DelegatedCallback);
		Instance->UpdateSkeletalMeshAsync(true);
		InitStateAndParameterSequence();
	}
}


void FTestingCustomizableObject::InitStateAndParameterSequence()
{
	BatchTestFinished = false;
	BatchTestInCourse = true;

	ArrayBatch.Empty();

	InitTest();
	
	TickDelegate = FTickerDelegate::CreateRaw(this, &FTestingCustomizableObject::RunStateTestTick);
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 0.f);

	if (ArrayBatch.Num() > 0)
	{
		ActiveBatchIndex = 0;
		ActiveBatch = &ArrayBatch[0];
	}
}


void FTestingCustomizableObject::FinishTest()
{
	WorstTimeDelegate.Unbind();
	StateTestEnded.ExecuteIfBound();
	if (Instance)
	{
		//Instance->UpdateBeginDelegate.Unbind();
		Instance->UpdatedDelegate.RemoveDynamic(HelperCallback, &UUpdateClassHelper::DelegatedCallback);
	}

	for(UCustomizableSkeletalComponent* CustomizableSkeletalComponent : CustomizableSkeletalComponents)
	{
		if (CustomizableSkeletalComponent)
		{
			CustomizableSkeletalComponent->DestroyComponent();
		}
	}

	for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent)
		{
			SkeletalMeshComponent->DestroyComponent();
		}
	}

	CustomizableSkeletalComponents.Empty();
	SkeletalMeshComponents.Empty();

	Instance = nullptr; // NOTE: is this the right thing? doing like in the CO editor destructor

	TestResourcesPresent = false;

	TickDelegate = nullptr;
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	GEditor->ForceGarbageCollection();

	BatchTestInCourse = false;
	BatchTestFinished = true;
}


void FTestingCustomizableObject::InstanceUpdateEnd()
{
	const int32 MutableRuntimeCycles = Instance->LastUpdateMutableRuntimeCycles;

	if ((ActiveBatch != nullptr) && !ActiveBatch->TestFinished)
	{
		ActiveBatch->TestFinished = true;
		ActiveBatch->ElapsedTime = FPlatformTime::ToMilliseconds(MutableRuntimeCycles);
	}
}


bool FTestingCustomizableObject::RunStateTestTick(float DeltaTime)
{
	if (!ActiveBatch->TestStarted)
	{
		ActiveBatch->TestStarted = true;
		LaunchNextBatch();
		Instance->UpdateSkeletalMeshAsync(true);
	}
	
	if ((ActiveBatch->TestStarted && ActiveBatch->TestFinished && ((ActiveBatchIndex + 1) < ArrayBatch.Num())))
	{
		InstanceTestUpdated(ActiveBatch);
		ActiveBatchIndex++;
		ActiveBatch = &ArrayBatch[ActiveBatchIndex];
	}
	
	if (((ActiveBatchIndex + 1) == ArrayBatch.Num()) && ActiveBatch->TestStarted && ActiveBatch->TestFinished)
	{
		FinishTest();
		return false;
	}

	return true;
}


bool FTestingCustomizableObject::GetTestResourcesPresent()
{
	return TestResourcesPresent;
}


int32 FTestingCustomizableObject::GetCurrentActiveBatchIndex()
{
	return ActiveBatchIndex;
}


int32 FTestingCustomizableObject::GetNumBatches()
{
	return ArrayBatch.Num();
}

bool FTestingCustomizableObject::GetBatchTestInCourse()
{
	return BatchTestInCourse;
}

void FTestingCustomizableObject::LaunchNextBatch()
{
	if (ActiveBatch == nullptr)
	{
		return;
	}

	if (ActiveBatch->IsStateChange)
	{
		Instance->SetState(ActiveBatch->State);
	}
	else
	{
		switch (ActiveBatch->ParameterType)
		{
			case EMutableParameterType::Bool:
			{
				Instance->SetBoolParameterSelectedOption(ActiveBatch->Name, ActiveBatch->ValueBool);
				break;
			}
			case EMutableParameterType::Int:
			{
				Instance->SetIntParameterSelectedOption(ActiveBatch->Name, ActiveBatch->OptionName);
				break;
			}
			case EMutableParameterType::Float:
			{
				Instance->SetFloatParameterSelectedOption(ActiveBatch->Name, ActiveBatch->ValueFloat);
				break;
			}
			case EMutableParameterType::Color:
			{
				Instance->SetColorParameterSelectedOption(ActiveBatch->Name, ActiveBatch->ColorValue);
				break;
			}
			case EMutableParameterType::Projector:
			{
				// For projectors we have small random offsets to apply to the position
				FVector Position = Instance->GetProjectorPosition(ActiveBatch->Name);
				Position *= ActiveBatch->VectorValue;
				Instance->SetProjectorPosition(ActiveBatch->Name, (FVector3f)Position);
				break;
			}
			case EMutableParameterType::Texture:
			{
				// What to do in this case?
				break;
			}
			default:
			{
				break;
			}
		}
	}
}


void FTestingCustomizableObject::InstanceTestUpdated(BatchParameterChange* BatchChange)
{

}


void UUpdateClassHelper::DelegatedCallback(UCustomizableObjectInstance* Instance)
{
	if (StateData != nullptr)
	{
		StateData->InstanceUpdateEnd();
	}
}


FRuntimeTest::FRuntimeTest() : FTestingCustomizableObject(TestType::RuntimeParameter)
{
}


void FRuntimeTest::LoadStateData(int32 StateIndex)
{
	UCustomizableObject* CustomizableObject = Instance->GetCustomizableObject(); 

	StateWholeInformation StateData;
	StateData.State = StateIndex;
	StateData.StateName = CustomizableObject->GetStateName(StateIndex);

	ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName));

	uint32 MaxParamChanges = 16;
	
	// TODO: test no-state available cases
	uint32 StateParameterCount = CustomizableObject->GetStateParameterCount(StateIndex);
	for (uint32 i = 0; i < StateParameterCount; ++i)
	{
		uint32 ParamIndexInObject = CustomizableObject->GetStateParameterIndex(StateIndex, i);
		FString RunTimeParameter = CustomizableObject->GetParameterName(ParamIndexInObject);
		EMutableParameterType Temp = CustomizableObject->GetParameterType(ParamIndexInObject);
		switch (CustomizableObject->GetParameterType(ParamIndexInObject))
		{
		case EMutableParameterType::None:
		{
			break;
		}
		case EMutableParameterType::Bool:
		{
			TArray<FCustomizableObjectBoolParameterValue>& BoolParameters = Instance->GetBoolParameters();
			FCustomizableObjectBoolParameterValue Parameter;
			Parameter.ParameterName = RunTimeParameter;
			const uint32 MaxIndex = BoolParameters.Num();
			for (uint32 j = 0; j < MaxIndex; ++j)
			{
				if (BoolParameters[j].ParameterName == Parameter.ParameterName)
				{
					Parameter.ParameterValue = BoolParameters[j].ParameterValue;
					StateData.BoolParametersIndex.Add(j);
					for (uint32 k = 0; k < (MaxParamChanges / 2); ++k)
					{
						ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName, RunTimeParameter, Parameter.ParameterValue));
						ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName, RunTimeParameter, !Parameter.ParameterValue));
					}
				}
			}
			StateData.BoolParameters.Add(Parameter);
			break;
		}
		case EMutableParameterType::Int:
		{
			TArray<FCustomizableObjectIntParameterValue>& IntParameters = Instance->GetIntParameters();
			FCustomizableObjectIntParameterValue Parameter;
			Parameter.ParameterName = RunTimeParameter;
			const uint32 MaxIndex = IntParameters.Num();
			for (uint32 j = 0; j < MaxIndex; ++j)
			{
				if (IntParameters[j].ParameterName == Parameter.ParameterName)
				{
					//Parameter.ParameterValue = Descriptor.IntParameters[j].ParameterValue;
					Parameter.ParameterValueName = IntParameters[j].ParameterValueName;
					StateData.IntParametersIndex.Add(j);

					int32 ParameterIndex = CustomizableObject->FindParameter(IntParameters[j].ParameterName);

					uint32 NumOptions = CustomizableObject->GetIntParameterNumOptions(ParameterIndex);
					for (uint32 l = 0; l < NumOptions && l < MaxParamChanges; ++l)
					{
						FString PossibleValue = CustomizableObject->GetIntParameterAvailableOption(ParameterIndex, l);
						ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName, RunTimeParameter, PossibleValue));
					}
				}
			}
			StateData.IntParameters.Add(Parameter);
			break; 
		}
		case EMutableParameterType::Float:
		{
			TArray<FCustomizableObjectFloatParameterValue>& FloatParameters = Instance->GetFloatParameters();
			FCustomizableObjectFloatParameterValue Parameter;
			Parameter.ParameterName = RunTimeParameter;
			const uint32 MaxIndex = FloatParameters.Num();
			for (uint32 j = 0; j < MaxIndex; ++j)
			{
				if (FloatParameters[j].ParameterName == Parameter.ParameterName)
				{
					Parameter.ParameterValue = FloatParameters[j].ParameterValue;
					StateData.FloatParametersIndex.Add(j);

					for (uint32 k = 0; k < MaxParamChanges; ++k)
					{
						ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName, RunTimeParameter, FMath::RandRange(0.0f, 1.0f)));
					}
					break;
				}
			}
			StateData.FloatParameters.Add(Parameter);
			break;
		}
		case EMutableParameterType::Color:
		{
			TArray<FCustomizableObjectVectorParameterValue>& VectorParameters = Instance->GetVectorParameters();
			FCustomizableObjectVectorParameterValue Parameter;
			Parameter.ParameterName = RunTimeParameter;
			const uint32 MaxIndex = VectorParameters.Num();
			for (uint32 j = 0; j < MaxIndex; ++j)
			{
				if (VectorParameters[j].ParameterName == Parameter.ParameterName)
				{
					Parameter.ParameterValue = VectorParameters[j].ParameterValue;
					StateData.VectorParametersIndex.Add(j);
					for (uint32 k = 0; k < MaxParamChanges; ++k)
					{
						ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName, RunTimeParameter, FLinearColor(FMath::RandRange(0.0f, 1.0f), FMath::RandRange(0.0f, 1.0f), FMath::RandRange(0.0f, 1.0f), 1.0f)));
					}
					break;
				}
			}
			StateData.VectorParameters.Add(Parameter);
			break;
		}
		case EMutableParameterType::Projector:
		{
			TArray<FCustomizableObjectVectorParameterValue>& VectorParameters = Instance->GetVectorParameters();
			FCustomizableObjectVectorParameterValue Parameter;
			Parameter.ParameterName = RunTimeParameter;
			const uint32 MaxIndex = VectorParameters.Num();
			for (uint32 j = 0; j < MaxIndex; ++j)
			{
				if (VectorParameters[j].ParameterName == Parameter.ParameterName)
				{
					Parameter.ParameterValue = VectorParameters[j].ParameterValue;
					StateData.VectorParametersIndex.Add(j);
					for (uint32 k = 0; k < MaxParamChanges; ++k)
					{
						// For projector we multiply the position with a small factor around 1
						ArrayBatch.Add(BatchParameterChange(StateIndex, StateData.StateName, RunTimeParameter, FVector(FMath::RandRange(0.9f, 1.1f), FMath::RandRange(0.9f, 1.1f), FMath::RandRange(0.9f, 1.1f))));
					}
					break;
				}
			}
			StateData.VectorParameters.Add(Parameter);
			break;
		}
		case EMutableParameterType::Texture:
		{
			TArray<FCustomizableObjectTextureParameterValue>& TextureParameters = Instance->GetTextureParameters();
			FCustomizableObjectTextureParameterValue Parameter;
			Parameter.ParameterName = RunTimeParameter;
			const uint32 MaxIndex = TextureParameters.Num();
			for (uint32 j = 0; j < MaxIndex; ++j)
			{
				if (TextureParameters[j].ParameterName == Parameter.ParameterName)
				{
					Parameter.ParameterValue = TextureParameters[j].ParameterValue;
					StateData.TextureParametersIndex.Add(j);
					// How would be the testing here done?
					break;
				}
			}
			StateData.TextureParameters.Add(Parameter);
			break;
		}
		default:
		{
			break;
		}
		}
	}

	ArrayState.Add(StateData);
}


void FRuntimeTest::InitTest()
{
	MapStateEnterData.Empty();
	MapStateUpdateData.Empty();
	MapStateRuntimeData.Empty();

	// Build a batch of changes for each state
	const int32 NumStates = Instance->GetCustomizableObject()->GetStateCount();

	// Enter each state and, for each runtime parameter, set random values (in the case
	// of integer parameter, set all possible values)
	for (int32 i = 0; i < NumStates; ++i)
	{
		LoadStateData(i);
	}

	const int32 NumStateChanges = NumStates * 50;
	uint32 IndexState;
	for (int32 i = 0; i < NumStateChanges; ++i)
	{
		IndexState = FMath::Rand() % NumStates;
		ArrayBatch.Add(BatchParameterChange(IndexState, ArrayState[IndexState].StateName));
	}
}


TMap<uint32, MeasuredData> FRuntimeTest::GetMapStateEnterData()
{
	return MapStateEnterData;
}


TMap<uint32, MeasuredData> FRuntimeTest::GetMapStateUpdateData()
{
	return MapStateUpdateData;
}


TMap<uint32, TArray<MeasuredData>> FRuntimeTest::GetMapStateRuntimeData()
{
	return MapStateRuntimeData;
}


void FRuntimeTest::InstanceTestUpdated(BatchParameterChange* BatchChange)
{
	if (BatchChange->IsStateChange)
	{
		MeasuredData* DataPtr = MapStateEnterData.Find(BatchChange->State);

		if (DataPtr == nullptr)
		{
			MeasuredData& Data = MapStateEnterData.Add(BatchChange->State, MeasuredData(BatchChange->StateName));
			AddMeasuredDataAndReportTime(&Data, BatchChange->ElapsedTime, BatchChange->State);
			TArray<MeasuredData> ArrayMeasuredData;
			MapStateRuntimeData.Add(BatchChange->State, ArrayMeasuredData);
		}
		else
		{
			AddMeasuredDataAndReportTime(DataPtr, BatchChange->ElapsedTime, BatchChange->State);
		}
	}
	else
	{
		// Add current change in course to statistics about the instance being previewed
		MeasuredData* DataPtr = FindStateRuntimeParameter(BatchChange->State, BatchChange->Name);

		if (DataPtr == nullptr)
		{
			MeasuredData Data = MeasuredData(BatchChange->Name);
			AddMeasuredDataAndReportTime(&Data, BatchChange->ElapsedTime, BatchChange->State, BatchChange->Name);
			TArray<MeasuredData>* ArrayMeasuredData = MapStateRuntimeData.Find(BatchChange->State);
			ArrayMeasuredData->Add(Data);
		}
		else
		{
			AddMeasuredDataAndReportTime(DataPtr, BatchChange->ElapsedTime, BatchChange->State, BatchChange->Name);
		}

		DataPtr = MapStateUpdateData.Find(BatchChange->State);
		if (DataPtr == nullptr)
		{
			MeasuredData Data = MeasuredData(BatchChange->StateName);
			AddMeasuredDataAndReportTime(&Data, BatchChange->ElapsedTime, BatchChange->State, BatchChange->Name);
			MapStateUpdateData.Add(BatchChange->State, Data);
		}
		else
		{
			AddMeasuredDataAndReportTime(DataPtr, BatchChange->ElapsedTime, BatchChange->State, BatchChange->Name);
		}
	}
}


MeasuredData* FRuntimeTest::FindStateRuntimeParameter(uint32 State, FString& Name)
{
	TArray<MeasuredData>* ArrayData = MapStateRuntimeData.Find(State);

	const uint32 MaxIndex = ArrayData->Num();
	for (uint32 i = 0; i < MaxIndex; ++i)
	{
		if ((*ArrayData)[i].Name == Name)
		{
			return &(*ArrayData)[i];
		}
	}

	return nullptr;
}


void FRuntimeTest::AddMeasuredDataAndReportTime(MeasuredData* DataPtr, float ElapsedTime, int32 StateIndex, FString ParameterName)
{
	if (DataPtr)
	{
		if (ElapsedTime > DataPtr->MaxTime)
		{
			if (ParameterName.IsEmpty())
			{
				WorstTimeDelegate.ExecuteIfBound(ElapsedTime, StateIndex, INDEX_NONE, 0/* TODO: Get lod!*/, Instance, DataPtr);
			}
			else
			{
				int32 ParameterIndexInCO = INDEX_NONE;

				if (Instance)
				{
					if (const UCustomizableObject* Object = Instance->GetCustomizableObject())
					{
						ParameterIndexInCO = Object->FindParameter(ParameterName);
					}
				}

				if (ParameterIndexInCO != INDEX_NONE)
				{
					WorstTimeDelegate.ExecuteIfBound(ElapsedTime, StateIndex, ParameterIndexInCO, 0/* TODO: Get lod!*/, Instance, DataPtr);
				}
			}
		}
		DataPtr->AddMeasureData(ElapsedTime);
	}
}


#undef LOCTEXT_NAMESPACE
