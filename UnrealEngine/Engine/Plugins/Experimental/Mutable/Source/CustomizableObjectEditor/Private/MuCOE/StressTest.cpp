// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/StressTest.h"

#include "Containers/IndirectArray.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformCrt.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Math/Color.h"
#include "Math/NumericLimits.h"
#include "Math/Vector.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "Templates/Casts.h"
#include "Trace/Detail/Channel.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UnrealNames.h"

class FSkeletalMeshLODRenderData;
class FSkeletalMeshRenderData;


void ULiveInstance::DelegatedCallback(UCustomizableObjectInstance* UpdatedInstance)
{
	AddInstanceInformation();
	Updated = true;
}


void ULiveInstance::AddInstanceInformation()
{
	if(!Instance->HasAnySkeletalMesh())
	{
		UE_LOG(LogMutable, Warning, TEXT("ERROR: no skeletal mesh in ULiveInstance::AddInstanceInformataion"));
		return;
	}

	FSkeletalMeshRenderData* RenderData = Helper_GetResourceForRendering(Instance->GetSkeletalMesh());
	TIndirectArray<FSkeletalMeshLODRenderData>* ArrayLODRenderData = Helper_GetLODDataPtr(RenderData);
	int32 MaxIndexLOD = Helper_GetLODDataPtr(RenderData)->Num();

	for (int32 i = 0; i < MaxIndexLOD; ++i)
	{
		TMap<FString, MaterialBriefInfo> MapMaterialInfo;
		uint32 CurrentLODTextureSize = 0;
		uint32 CurrentLODNumFaces = 0;

		TMap<FString, MeasuredData> MapMeasuredData = ULiveInstance::InitInstanceInformation();

		for (int32 c = 0; c < Instance->SkeletalMeshes.Num(); ++c)
		{
			RenderData = Helper_GetResourceForRendering(Instance->GetSkeletalMesh(c));
			ArrayLODRenderData = Helper_GetLODDataPtr(RenderData);
			MaxIndexLOD = Helper_GetLODDataPtr(RenderData)->Num();
			CurrentLODNumFaces = Helper_LODGetTotalFaces(&((*ArrayLODRenderData)[i]));
			const TArray<struct FSkeletalMeshLODInfo>& LODInfoArray = Helper_GetLODInfoArray(Instance->GetSkeletalMesh(c));
			if (LODInfoArray.IsValidIndex(i))
			{
				const FSkeletalMeshLODInfo& LODInfo = LODInfoArray[i];

				MapMeasuredData["Number of faces"].AddMeasureData(Helper_LODGetTotalFaces(&((*ArrayLODRenderData)[i])));
				MapMeasuredData["Number of texture coordinates"].AddMeasureData(Helper_LODGetNumTexCoords(&((*ArrayLODRenderData)[i])));
				const TArray<FSkeletalMaterial>& Materials = Instance->GetSkeletalMesh(c)->GetMaterials();
				MapMeasuredData["Number of materials"].AddMeasureData(LODInfo.LODMaterialMap.Num());

				for (int32 j = 0; j < LODInfo.LODMaterialMap.Num(); ++j)
				{
					if (Materials.IsValidIndex(LODInfo.LODMaterialMap[j]))
					{
						UMaterialInterface* MaterialInterface = Materials[LODInfo.LODMaterialMap[j]].MaterialInterface;
						UMaterialInstance* BaseMaterial = Cast<UMaterialInstance>(MaterialInterface);
						FString MaterialPath = MaterialInterface->GetPathName();
						MaterialBriefInfo Info;

						Info.Name = BaseMaterial ? BaseMaterial->GetName() : MaterialInterface->GetName();

						if (BaseMaterial && MaterialPath.Contains("Transient"))
						{
							for (int32 k = 0; k < BaseMaterial->TextureParameterValues.Num(); ++k)
							{
								if (UTexture* Texture = BaseMaterial->TextureParameterValues[k].ParameterValue)
								{
									FString TextPath = Texture->GetPathName();
									if (Texture->GetPathName().Contains("Transient"))
									{
										FIntVector Size = FIntVector(0, 0, 0);
										Info.ArrayTextureName.Add(Texture->GetName());
										Size.X = Texture->GetSurfaceWidth();
										Size.Y = Texture->GetSurfaceHeight();
										uint32 CurrentTextureSize = Texture->CalcTextureMemorySizeEnum(ETextureMipCount::TMC_AllMips);
										CurrentLODTextureSize += CurrentTextureSize;
										MapMeasuredData["Texture size"].AddMeasureData(float(CurrentTextureSize));
										Info.ArrayTextureSize.Add(Size);
										Info.ArrayTextureFormat.Add(((UTexture2D*)Texture)->GetPixelFormat());
									}
								}
							}
							MapMaterialInfo.Add(Info.Name, Info);
						}
					}
				}
				MapMeasuredData["Instance texture size"].AddMeasureData(CurrentLODTextureSize);
			}
		}

		TestDelegate.ExecuteIfBound(MapMeasuredData, MapMaterialInfo, CurrentLODTextureSize, CurrentLODNumFaces, uint32(i), Instance);

		NumInstanceUpdated++;
	}
}


TMap<FString, MeasuredData> ULiveInstance::InitInstanceInformation()
{
	TMap<FString, MeasuredData> MapResult;

	TArray<FString> ArrayParam;

	ArrayParam.Add("Number of faces");
	ArrayParam.Add("Number of materials");
	ArrayParam.Add("Number of texture coordinates");
	ArrayParam.Add("Texture size");
	ArrayParam.Add("Instance texture size");

	int32 MaxIndex = ArrayParam.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		MapResult.Add(ArrayParam[i], MeasuredData(ArrayParam[i]));
	}

	return MapResult;
}


FRunningStressTest::FRunningStressTest()
{
	NumUpdateRequested = 0;
}


bool FRunningStressTest::RunStressTestTick(float DeltaTime)
{
	if (PendingInstanceCount <= 0)
	{
		VerifyInstancesThreshold -= DeltaTime;

		if (VerifyInstancesThreshold > 0.0f)
		{
			return true;
		}

		// Verify all instances in the test made their last update before finising the test properly
		for (TPair<UCustomizableObjectInstance*, ULiveInstance*>& it : LiveInstances)
		{
			if (!it.Value->Updated)
			{
				VerifyInstancesThreshold = 5.0f;
				return true;
			}
		}

		if (NumUpdatedDone == NumUpdateRequested)
		{
			UE_LOG(LogMutable, Warning, TEXT("\t\t\t Final Stress Test results for model %s"), *CustomizableObject->GetName());
		}

		WriteTestResults();

		PrintMostExpensivePerLOD();

		UE_LOG(LogMutable, Warning, TEXT("///////////////////////////////////////////////////////////////////////////////////"));

		UE_LOG(LogMutable, Warning, TEXT("Finished stress test for model [%s]."), *CustomizableObject->GetName());
		UE_LOG(LogMutable, Warning, TEXT("\n"));

		FinishTest();

		return false;
	}
	else
	{
		int thisFrameTime = int(DeltaTime*1000.0f);
		TotalTimeMs += thisFrameTime;

		// Create a new instance if necessary
		int remainingTime = thisFrameTime;
		NextInstanceTimeMs -= remainingTime;
		while (NextInstanceTimeMs < 0)
		{
			--PendingInstanceCount;

			// Remaining time
			remainingTime = -NextInstanceTimeMs;

			UCustomizableObjectInstance* Instance = CustomizableObject->CreateInstance();

			if (!ArrayLODResized)
			{
				NumLOD = CustomizableObject->GetNumLODs();
				ArrayLODMostExpensiveGeometry.SetNum(NumLOD);
				ArrayLODMostExpensiveTexture.SetNum(NumLOD);
				ArrayMeasuredDataPerLOD.SetNum(NumLOD);
				ArrayMaterialInfoPerLOD.SetNum(NumLOD);

				for (uint32 i = 0; i < NumLOD; ++i)
				{
					ArrayMeasuredDataPerLOD[i] = ULiveInstance::InitInstanceInformation();
				}

				ArrayLODResized = true;
			}

			Instance->SetRandomValues();

			UCustomizableSkeletalComponent* Component = NewObject<UCustomizableSkeletalComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			Component->CustomizableObjectInstance = Instance;

			ULiveInstance* Data = NewObject<ULiveInstance>(GetTransientPackage());
			Instance->UpdatedDelegate.AddDynamic(Data, &ULiveInstance::DelegatedCallback);
			Instance->UpdateSkeletalMeshAsync(true);
			Data->Updated = false;

			Data->TimeToDie = FMath::Max(InstanceLifeTimeMs, InstanceLifeTimeMs + (FMath::Rand() % (InstanceLifeTimeMsVar * 2)) - InstanceLifeTimeMsVar);
			Data->TimeToUpdate = InstanceUpdateTimeMs
				+ (FMath::Rand() % (InstanceUpdateTimeMsVar * 2))
				- InstanceUpdateTimeMsVar;

			Data->Instance = Instance;
			Data->NumInstanceUpdated = 0;
			Data->TestDelegate.BindRaw(this, &FRunningStressTest::AddInstanceData);

			LiveInstances.Add(Instance, Data);
			NumUpdateRequested++;

			NextInstanceTimeMs = CreateInstanceTimeMs
				+ (FMath::Rand() % (CreateInstanceTimeMsVar * 2))
				- CreateInstanceTimeMsVar;

			if (remainingTime > 0)
			{
				NextInstanceTimeMs -= remainingTime;
			}

			UE_LOG(LogMutable, Warning, TEXT("Stress test created instance. [%d active]"), LiveInstances.Num());
		}

		// Update instances that need it
		for (TPair<UCustomizableObjectInstance*, ULiveInstance*>& it : LiveInstances)
		{
			if (!it.Value->Updated)
			{
				continue;
			}

			it.Value->TimeToUpdate -= thisFrameTime;
			if (it.Value->TimeToUpdate < 0)
			{
				it.Key->SetRandomValues();
				it.Key->UpdateSkeletalMeshAsync(true);

				it.Value->Updated = false;
				it.Value->TimeToUpdate = InstanceUpdateTimeMs
					+ (FMath::Rand() % (InstanceUpdateTimeMsVar * 2))
					- InstanceUpdateTimeMsVar;
			}
		}

		// Destroy instances that expired
		TArray<UCustomizableObjectInstance*> ToRemove;
		for (TPair<UCustomizableObjectInstance*, ULiveInstance*>& it : LiveInstances)
		{
			if (!it.Value) continue;

			it.Value->TimeToDie -= thisFrameTime;
			if (it.Value->TimeToDie < 0 && it.Value->Updated)
			{
				it.Key->UpdatedDelegate.RemoveDynamic(it.Value, &ULiveInstance::DelegatedCallback);
				it.Value = nullptr;
				ToRemove.Add(it.Key);
			}
		}

		for (UCustomizableObjectInstance* i : ToRemove)
		{
			LiveInstances.Remove(i);
		}

		if (ToRemove.Num())
		{
			UE_LOG(LogMutable, Warning, TEXT("Stress test removed instance. [%d active]"), LiveInstances.Num());
		}
	}

	return true;
}


void FRunningStressTest::InitMeasureTest()
{
	StressTestReadyToReset = false;
	NumUpdateRequested = 0;
	NumUpdatedDone = 0;
	ArrayMeasuredDataPerLOD.Empty();
	ArrayMaterialInfoPerLOD.Empty();

	int32 MaxValue = TNumericLimits<int32>::Max();
	int32 MinValue = TNumericLimits<int32>::Min();

	TextureMinSize = FIntVector(MaxValue, MaxValue, 0);
	TextureMaxSize = FIntVector(MinValue, MinValue, 0);

	TestInCourse = true;

	MinTextureSize = TNumericLimits<uint32>::Max();
	MeanTextureSize = 0;
	MaxTextureSize = 0;

	StressTestTickDelegate = FTickerDelegate::CreateRaw(this, &FRunningStressTest::RunStressTestTick);
	StressTestTickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(StressTestTickDelegate, 0.f);

	ArrayLODMostExpensiveGeometry.Empty();
	ArrayLODMostExpensiveTexture.Empty();

	ArrayLODResized = false;
	NumLOD = 0;
}


void FRunningStressTest::WriteTestResults()
{
	FString Line;
	Line = "///////////////////////////////////////////////////////////////////////////////////";
	UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

	for (uint32 i = 0; i < NumLOD; ++i)
	{
		int32 MaxOffsetName = 0;
		for (TPair<FString, MeasuredData>& it : ArrayMeasuredDataPerLOD[i])
		{
			MaxOffsetName = FMath::Max(MaxOffsetName, it.Key.Len());
		}
		MaxOffsetName += 5;

		MinTextureSize = uint32(ArrayMeasuredDataPerLOD[i]["Texture size"].MinTime);
		MeanTextureSize = uint32(ArrayMeasuredDataPerLOD[i]["Texture size"].MeanTime);
		MaxTextureSize = uint32(ArrayMeasuredDataPerLOD[i]["Texture size"].MaxTime);

		Line = "Results for LOD level ";
		Line += FString::Printf(TEXT("%d"), i);
		UE_LOG(LogMutable, Warning, TEXT("%s:"), *Line);

		uint32 MaxLength1 = MaxOffsetName + 25;
		uint32 MaxLength2 = MaxLength1 + 25;

		Line = "\t Parameter";
		AppendWhitespace(Line, MaxOffsetName - Line.Len());
		Line += "Min";
		AppendWhitespace(Line, MaxLength1 - Line.Len());
		Line += "Mean";
		AppendWhitespace(Line, MaxLength2 - Line.Len());
		Line += "Max";
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

		Line = "\t ---------------------------------------------------------------------------------------------";
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

		for (TPair<FString, MeasuredData>& it : ArrayMeasuredDataPerLOD[i])
		{
			if (it.Key == "Texture size")
			{
				continue;
			}

			Line = "\t ";
			Line += FString::Printf(TEXT("%s: "), *(it.Key));
			AppendWhitespace(Line, MaxOffsetName - Line.Len());
			Line += FString::Printf(TEXT("%d"), int32(it.Value.MinTime));
			AppendWhitespace(Line, MaxLength1 - Line.Len());
			Line += FString::Printf(TEXT("%d"), int32(it.Value.MeanTime));
			AppendWhitespace(Line, MaxLength2 - Line.Len());
			Line += FString::Printf(TEXT("%d"), int32(it.Value.MaxTime));
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}

		// Texture size information
		Line = "\t Texture size ";
		AppendWhitespace(Line, MaxOffsetName - Line.Len());
		float Value = (MinTextureSize > 1048576) ? float(MinTextureSize) / 1048576.0f : float(MinTextureSize) / 1024.0f;
		Line += FString::Printf(TEXT("%.2f"), Value);
		Line += (MinTextureSize > 1048576) ? "(MB)" : "(KB)";
		AppendWhitespace(Line, MaxLength1 - Line.Len());
		Value = (MeanTextureSize > 1048576) ? float(MeanTextureSize) / 1048576.0f : float(MeanTextureSize) / 1024.0f;
		Line += FString::Printf(TEXT("%.2f"), Value);
		Line += (MeanTextureSize > 1048576) ? "(MB)" : "(KB)";
		AppendWhitespace(Line, MaxLength2 - Line.Len());
		Value = (MaxTextureSize > 1048576) ? float(MaxTextureSize) / 1048576.0f : float(MaxTextureSize) / 1024.0f;
		Line += FString::Printf(TEXT("%.2f"), Value);
		Line += (MaxTextureSize > 1048576) ? "(MB)" : "(KB)";
		UE_LOG(LogMutable, Warning, TEXT("%s\n"), *Line);
	}
}


void FRunningStressTest::AddInstanceData(MeasureDataMap MapMeasuredParam, MaterialInfoMap MapMaterialParam, uint32 CurrentLODTextureSize, uint32 CurrentLODNumFaces, uint32 LOD, UCustomizableObjectInstance* Instance)
{
	for (TPair<FString, MeasuredData>& it : MapMeasuredParam)
	{
		MeasuredData* Data = ArrayMeasuredDataPerLOD[LOD].Find(it.Key);

		if (Data != nullptr)
		{
			*Data += it.Value;
		}
	}

	if (ArrayLODMostExpensiveGeometry[LOD].CurrentLODNumFaces <= CurrentLODNumFaces)
	{
		CaptureInstanceParameters(Instance, &ArrayLODMostExpensiveGeometry[LOD]);
		ArrayLODMostExpensiveGeometry[LOD].CurrentLODNumFaces = CurrentLODNumFaces;
		ArrayLODMostExpensiveGeometry[LOD].CurrentLODTextureSize = CurrentLODTextureSize;
		ArrayLODMostExpensiveGeometry[LOD].MapMaterial = MapMaterialParam;
	}

	WorstFacesDelegate.ExecuteIfBound(CurrentLODNumFaces, LOD, Instance, &ArrayMeasuredDataPerLOD[LOD]["Number of faces"]);

	if (ArrayLODMostExpensiveTexture[LOD].CurrentLODTextureSize <= CurrentLODTextureSize)
	{
		CaptureInstanceParameters(Instance, &ArrayLODMostExpensiveTexture[LOD]);
		ArrayLODMostExpensiveTexture[LOD].CurrentLODNumFaces = CurrentLODNumFaces;
		ArrayLODMostExpensiveTexture[LOD].CurrentLODTextureSize = CurrentLODTextureSize;
		ArrayLODMostExpensiveTexture[LOD].MapMaterial = MapMaterialParam;
	}

	WorstTextureDelegate.ExecuteIfBound(MapMaterialParam, CurrentLODTextureSize, LOD, Instance, &ArrayMeasuredDataPerLOD[LOD]["Instance texture size"]);

	for (TPair<FString, MaterialBriefInfo>& it : MapMaterialParam)
	{
		MaterialBriefInfo* Data = ArrayMaterialInfoPerLOD[LOD].Find(it.Key);

		if (Data == nullptr)
		{
			ArrayMaterialInfoPerLOD[LOD].Add(it.Key, it.Value);

			const int32 MaxIndex = it.Value.ArrayTextureSize.Num();
			for (int32 i = 0; i < MaxIndex; ++i)
			{
				FIntVector& Value = it.Value.ArrayTextureSize[i];

				if ((Value.X <= TextureMinSize.X) || (Value.Y <= TextureMinSize.Y))
				{
					TextureMinSize = Value;
				}

				if ((Value.X >= TextureMaxSize.X) || (Value.Y >= TextureMaxSize.Y))
				{
					TextureMaxSize = Value;
				}
			}
		}
	}

	NumUpdatedDone++;

	FString Line = "Test update instance number ";
	Line += FString::Printf(TEXT("%d"), NumUpdatedDone);
	UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

	WriteTestResults();
}


void FRunningStressTest::FinishTest()
{
	WorstFacesDelegate.Unbind();
	WorstTextureDelegate.Unbind();
	StressTestEnded.ExecuteIfBound();
	FTSTicker::GetCoreTicker().RemoveTicker(StressTestTickDelegateHandle);
	StressTestTickDelegate = nullptr;

	StressTestReadyToReset = true;

	for (TPair<UCustomizableObjectInstance*, ULiveInstance*>& it : LiveInstances)
	{
		it.Value->TestDelegate.Unbind();
	}

	LiveInstances.Empty();
	TestInCourse = false;
}


void FRunningStressTest::CaptureInstanceParameters(UCustomizableObjectInstance* Instance, MostExpensiveInstanceData* Destination)
{
	Destination->BoolParameters = Instance->GetBoolParameters();
	Destination->IntParameters = Instance->GetIntParameters();
	Destination->FloatParameters = Instance->GetFloatParameters();
	Destination->TextureParameters = Instance->GetTextureParameters();
	Destination->VectorParameters = Instance->GetVectorParameters();
	Destination->ProjectorParameters = Instance->GetProjectorParameters();
}


TMap<class UCustomizableObjectInstance*, ULiveInstance*>& FRunningStressTest::GetLiveInstances()
{
	return LiveInstances;
}


bool FRunningStressTest::GetStressTestReadyToReset()
{
	return StressTestReadyToReset;
}


bool FRunningStressTest::GetTestInCourse()
{
	return TestInCourse;
}


void FRunningStressTest::SetCreateInstanceTimeMs(int32 CreateInstanceTimeMsParam)
{
	CreateInstanceTimeMs = CreateInstanceTimeMsParam;
}


void FRunningStressTest::SetCreateInstanceTimeMsVar(int32 CreateInstanceTimeMsVarParam)
{
	CreateInstanceTimeMsVar = CreateInstanceTimeMsVarParam;
}


void FRunningStressTest::SetInstanceUpdateTimeMs(int32 InstanceUpdateTimeMsParam)
{
	InstanceUpdateTimeMs = InstanceUpdateTimeMsParam;
}


void FRunningStressTest::SetInstanceUpdateTimeMsVar(int32 InstanceUpdateTimeMsVarParam)
{
	InstanceUpdateTimeMsVar = InstanceUpdateTimeMsVarParam;
}


void FRunningStressTest::SetInstanceLifeTimeMs(int32 InstanceLifeTimeMsParam)
{
	InstanceLifeTimeMs = InstanceLifeTimeMsParam;
}


void FRunningStressTest::SetInstanceLifeTimeMsVar(int32 InstanceLifeTimeMsVarParam)
{
	InstanceLifeTimeMsVar = InstanceLifeTimeMsVarParam;
}


void FRunningStressTest::SetPendingInstanceCount(int32 PendingInstanceCountParam)
{
	PendingInstanceCount = PendingInstanceCountParam;
}


void FRunningStressTest::SetCustomizableObject(UCustomizableObject* CustomizableObjectParam)
{
	CustomizableObject = CustomizableObjectParam;
}


void FRunningStressTest::SetStressTestReadyToReset(bool StressTestReadyToResetParam)
{
	StressTestReadyToReset = StressTestReadyToResetParam;
}


void FRunningStressTest::SetVerifyInstancesThreshold(float VerifyInstancesThresholdParam)
{
	VerifyInstancesThreshold = VerifyInstancesThresholdParam;
}


void FRunningStressTest::SetNextInstanceTimeMs(float NextInstanceTimeMsParam)
{
	NextInstanceTimeMs = NextInstanceTimeMsParam;
}


void FRunningStressTest::AppendWhitespace(FString& Target, int32 Amount)
{
	for (int32 i = 0; i < Amount; ++i)
	{
		Target += " ";
	}
}


void FRunningStressTest::PrintMostExpensivePerLOD()
{
	const uint32 MaxIndexLOD = ArrayLODMostExpensiveGeometry.Num();

	// For each LOD level
	for (uint32 i = 0; i < MaxIndexLOD; ++i)
	{
		FString Line = "///////////////////////////////////////////////////////////////////////////////////";
		UE_LOG(LogMutable, Warning, TEXT("%s:"), *Line);

		Line = "Instance detailed information for LOD level ";
		Line += FString::Printf(TEXT("%d:"), i);
		UE_LOG(LogMutable, Warning, TEXT("%s\n"), *Line);

		Line = "\t Information for most expensive instance in terms of geometry size";
		UE_LOG(LogMutable, Warning, TEXT("%s:"), *Line);
		MostExpensiveInstanceData& DataGeometry = ArrayLODMostExpensiveGeometry[i];
		PrintMostExpensiveInstanceData(DataGeometry, i);

		Line = "\t Information for most expensive instance in terms of material texture size";
		UE_LOG(LogMutable, Warning, TEXT("%s:"), *Line);
		MostExpensiveInstanceData& DataTexture = ArrayLODMostExpensiveTexture[i];
		PrintMostExpensiveInstanceData(DataTexture, i);
	}
}


void FRunningStressTest::PrintParameterData(const MostExpensiveInstanceData& Data)
{
	uint32 MaxLength = FindLongestParameterName(Data);
	MaxLength += 5;

	FString Line;

	if (Data.BoolParameters.Num() > 0)
	{
		PrintParameterHeader(FString("\t\t Bool Parameters"), MaxLength, false);
		for (const FCustomizableObjectBoolParameterValue& it : Data.BoolParameters)
		{
			Line = FString::Printf(TEXT("\t\t\t %s"), *it.ParameterName);
			AppendWhitespace(Line, MaxLength - Line.Len());
			Line += FString::Printf(TEXT("%d"), it.ParameterValue);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}

	if (Data.IntParameters.Num() > 0)
	{
		PrintParameterHeader(FString("\t\t Integer Parameters"), MaxLength, true);
		for (const FCustomizableObjectIntParameterValue& it : Data.IntParameters)
		{
			Line = FString::Printf(TEXT("\t\t\t %s"), *it.ParameterName);
			AppendWhitespace(Line, MaxLength - Line.Len());
			Line += FString::Printf(TEXT("%s"), *it.ParameterValueName);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}

	if (Data.FloatParameters.Num() > 0)
	{
		PrintParameterHeader(FString("\t\t Float Parameters"), MaxLength, true);
		for (const FCustomizableObjectFloatParameterValue& it : Data.FloatParameters)
		{
			Line = FString::Printf(TEXT("\t\t\t %s"), *it.ParameterName);
			AppendWhitespace(Line, MaxLength - Line.Len());
			Line += FString::Printf(TEXT("%.2f"), it.ParameterValue);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}

	if (Data.TextureParameters.Num() > 0)
	{
		PrintParameterHeader(FString("\t\t Texture Parameters"), MaxLength, true);
		for (const FCustomizableObjectTextureParameterValue& it : Data.TextureParameters)
		{
			Line = FString::Printf(TEXT("\t\t\t %s"), *it.ParameterName);
			AppendWhitespace(Line, MaxLength - Line.Len());
			Line += FString::Printf(TEXT("%d"), it.ParameterValue);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}

	if (Data.VectorParameters.Num() > 0)
	{
		PrintParameterHeader(FString("\t\t Vector Parameters"), MaxLength, true);
		for (const FCustomizableObjectVectorParameterValue& it : Data.VectorParameters)
		{
			Line = FString::Printf(TEXT("\t\t\t %s"), *it.ParameterName);
			AppendWhitespace(Line, MaxLength - Line.Len());
			Line += FString::Printf(TEXT("(%.2f,"), it.ParameterValue.R);
			Line += FString::Printf(TEXT("%.2f,"), it.ParameterValue.G);
			Line += FString::Printf(TEXT("%.2f,"), it.ParameterValue.B);
			Line += FString::Printf(TEXT("%.2f)"), it.ParameterValue.A);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}

	if (Data.ProjectorParameters.Num() > 0)
	{
		PrintParameterHeader(FString("\t\t Projector Parameters"), MaxLength, true);
		for (const FCustomizableObjectProjectorParameterValue& it : Data.ProjectorParameters)
		{
			Line = FString::Printf(TEXT("\t\t\t %s"), *it.ParameterName);
			AppendWhitespace(Line, MaxLength - Line.Len());
			Line += FString::Printf(TEXT("(%.2f,"), it.Value.Position.X);
			Line += FString::Printf(TEXT("%.2f,"), it.Value.Position.Y);
			Line += FString::Printf(TEXT("%.2f)"), it.Value.Position.Z);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}
}

uint32 FRunningStressTest::FindLongestParameterName(const MostExpensiveInstanceData& Data)
{
	int32 Result = 0;
	for (const FCustomizableObjectBoolParameterValue& it : Data.BoolParameters)
	{
		Result = FMath::Max(Result, it.ParameterName.Len());
	}

	for (const FCustomizableObjectIntParameterValue& it : Data.IntParameters)
	{
		Result = FMath::Max(Result, it.ParameterName.Len());
	}

	for (const FCustomizableObjectFloatParameterValue& it : Data.FloatParameters)
	{
		Result = FMath::Max(Result, it.ParameterName.Len());
	}

	for (const FCustomizableObjectTextureParameterValue& it : Data.TextureParameters)
	{
		Result = FMath::Max(Result, it.ParameterName.Len());
	}

	for (const FCustomizableObjectVectorParameterValue& it : Data.VectorParameters)
	{
		Result = FMath::Max(Result, it.ParameterName.Len());
	}

	for (const FCustomizableObjectProjectorParameterValue& it : Data.ProjectorParameters)
	{
		Result = FMath::Max(Result, it.ParameterName.Len());
	}

	return Result;
}


void FRunningStressTest::PrintParameterHeader(FString Param, int32 Value, bool AddNewLine)
{
	if (AddNewLine)
	{
		UE_LOG(LogMutable, Warning, TEXT("\n"));
	}
	FString Line = Param;
	UE_LOG(LogMutable, Warning, TEXT("\t\t\t %s"), *Line);
	Line = "\t\t\t Name";
	AppendWhitespace(Line, Value - Line.Len());
	Line += "Value";
	UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
	UE_LOG(LogMutable, Warning, TEXT("\t\t\t ------------------------------------------"));
}


void FRunningStressTest::PrintMostExpensiveInstanceData(const MostExpensiveInstanceData& Data, uint32 LOD)
{
	FString Line;
	Line = FString::Printf(TEXT("\t\t Number of faces: "));
	Line += FString::Printf(TEXT("%d"), Data.CurrentLODNumFaces);
	UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

	// Texture size information
	Line = "\t\t Texture size: ";
	float Value = (MinTextureSize > 1048576) ? float(MinTextureSize) / 1048576.0f : float(MinTextureSize) / 1024.0f;
	Line += FString::Printf(TEXT("min=%.2f"), Value);
	Line += (MinTextureSize > 1048576) ? "(MB)" : "(KB)";
	Value = (MaxTextureSize > 1048576) ? float(MaxTextureSize) / 1048576.0f : float(MaxTextureSize) / 1024.0f;
	Line += FString::Printf(TEXT(", max=%.2f"), Value);
	Line += (MaxTextureSize > 1048576) ? "(MB)" : "(KB)";
	UE_LOG(LogMutable, Warning, TEXT("%s\n"), *Line);

	// Material information
	const TMap<FString, MaterialBriefInfo>& MapMaterialLOD = Data.MapMaterial;

	Line = FString::Printf(TEXT("\t\t Material information"));

	if (ArrayMaterialInfoPerLOD[LOD].Num() == 1)
	{
		Line += FString::Printf(TEXT(" (1 element):"));
	}
	else
	{
		Line += FString::Printf(TEXT(" (%d elements):"), MapMaterialLOD.Num());
	}

	UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

	int32 MaxOffsetName = 0;
	for (const TPair<FString, MaterialBriefInfo>& itMaterial : MapMaterialLOD)
	{
		MaxOffsetName = FMath::Max(MaxOffsetName, itMaterial.Key.Len());
	}
	MaxOffsetName += 20;
	int32 SecondOffsetTexture = MaxOffsetName + 20;

	FString TextureFormatString;

	for (const TPair<FString, MaterialBriefInfo>& itMaterial : MapMaterialLOD)
	{
		Line = FString::Printf(TEXT("\t\t\t Material %s: "), *(itMaterial.Key));
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

		int32 MaxIndex = itMaterial.Value.ArrayTextureName.Num();
		for (int32 i = 0; i < MaxIndex; ++i)
		{
			Line = FString::Printf(TEXT("\t\t\t\t Texture %s"), *(itMaterial.Value.ArrayTextureName[i]));
			AppendWhitespace(Line, MaxOffsetName - Line.Len());
			Line += FString::Printf(TEXT("size=(%d,"), itMaterial.Value.ArrayTextureSize[i].X);
			Line += FString::Printf(TEXT("%d)"), itMaterial.Value.ArrayTextureSize[i].Y);
			AppendWhitespace(Line, SecondOffsetTexture - Line.Len());
			TextureFormatString = StringyfyEPixelFormat(itMaterial.Value.ArrayTextureFormat[i]);
			Line += FString::Printf(TEXT("format=%s"), *TextureFormatString);
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);
		}
	}

	UE_LOG(LogMutable, Warning, TEXT("\n"));

	if ((Data.BoolParameters.Num() > 0) || (Data.IntParameters.Num() > 0) || (Data.FloatParameters.Num() > 0) ||
		(Data.TextureParameters.Num() > 0) || (Data.VectorParameters.Num() > 0) || (Data.ProjectorParameters.Num() > 0))
	{
		Line = FString::Printf(TEXT("\t\t Parameter information"));
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Line);

		// Parameter information
		PrintParameterData(Data);
	}
}


FString FRunningStressTest::StringyfyEPixelFormat(EPixelFormat Value)
{
	switch (Value)
	{
		case PF_Unknown:
		{
			return FString("PF_Unknown");
		}
		case PF_A32B32G32R32F:
		{
			return FString("PF_A32B32G32R32F");
		}
		case PF_B8G8R8A8:
		{
			return FString("PF_B8G8R8A8");
		}
		case PF_G8:
		{
			return FString("PF_G8");
		}
		case PF_G16:
		{
			return FString("PF_G16");
		}
		case PF_DXT1:
		{
			return FString("PF_DXT1");
		}
		case PF_DXT3:
		{
			return FString("PF_DXT3");
		}
		case PF_DXT5:
		{
			return FString("PF_DXT5");
		}
		case PF_UYVY:
		{
			return FString("PF_UYVY");
		}
		case PF_FloatRGB:
		{
			return FString("PF_FloatRGB");
		}
		case PF_FloatRGBA:
		{
			return FString("PF_FloatRGBA");
		}
		case PF_DepthStencil:
		{
			return FString("PF_DepthStencil");
		}
		case PF_ShadowDepth:
		{
			return FString("PF_ShadowDepth");
		}
		case PF_R32_FLOAT:
		{
			return FString("PF_R32_FLOAT");
		}
		case PF_G16R16:
		{
			return FString("PF_G16R16");
		}
		case PF_G16R16F:
		{
			return FString("PF_G16R16F");
		}
		case PF_G16R16F_FILTER:
		{
			return FString("PF_G16R16F_FILTER");
		}
		case PF_G32R32F:
		{
			return FString("PF_G32R32F");
		}
		case PF_A2B10G10R10:
		{
			return FString("PF_A2B10G10R10");
		}
		case PF_A16B16G16R16:
		{
			return FString("PF_A16B16G16R16");
		}
		case PF_D24:
		{
			return FString("PF_D24");
		}
		case PF_R16F:
		{
			return FString("PF_R16F");
		}
		case PF_R16F_FILTER:
		{
			return FString("PF_R16F_FILTER");
		}
		case PF_BC5:
		{
			return FString("PF_BC5");
		}
		case PF_V8U8:
		{
			return FString("PF_V8U8");
		}
		case PF_A1:
		{
			return FString("PF_A1");
		}
		case PF_FloatR11G11B10:
		{
			return FString("PF_FloatR11G11B10");
		}
		case PF_A8:
		{
			return FString("PF_A8");
		}
		case PF_R32_UINT:
		{
			return FString("PF_R32_UINT");
		}
		case PF_R32_SINT:
		{
			return FString("PF_R32_SINT");
		}
		case PF_PVRTC2:
		{
			return FString("PF_PVRTC2");
		}
		case PF_PVRTC4:
		{
			return FString("PF_PVRTC4");
		}
		case PF_R16_UINT:
		{
			return FString("PF_R16_UINT");
		}
		case PF_R16_SINT:
		{
			return FString("PF_R16_SINT");
		}
		case PF_R16G16B16A16_UINT:
		{
			return FString("PF_R16G16B16A16_UINT");
		}
		case PF_R16G16B16A16_SINT:
		{
			return FString("PF_R16G16B16A16_SINT");
		}
		case PF_R5G6B5_UNORM:
		{
			return FString("PF_R5G6B5_UNORM");
		}
		case PF_R8G8B8A8:
		{
			return FString("PF_R8G8B8A8");
		}
		case PF_A8R8G8B8:
		{
			return FString("PF_A8R8G8B8");
		}
		case PF_BC4:
		{
			return FString("PF_BC4");
		}
		case PF_R8G8:
		{
			return FString("PF_R8G8");
		}
		case PF_ATC_RGB:
		{
			return FString("PF_ATC_RGB");
		}
		case PF_ATC_RGBA_E:
		{
			return FString("PF_ATC_RGBA_E");
		}
		case PF_ATC_RGBA_I:
		{
			return FString("PF_ATC_RGBA_I");
		}
		case PF_X24_G8:
		{
			return FString("PF_X24_G8");
		}
		case PF_ETC1:
		{
			return FString("PF_ETC1");
		}
		case PF_ETC2_RGB:
		{
			return FString("PF_ETC2_RGB");
		}
		case PF_ETC2_RGBA:
		{
			return FString("PF_ETC2_RGBA");
		}
		case PF_R32G32B32A32_UINT:
		{
			return FString("PF_R32G32B32A32_UINT");
		}
		case PF_R16G16_UINT:
		{
			return FString("PF_R16G16_UINT");
		}
		case PF_ASTC_4x4:
		{
			return FString("PF_ASTC_4x4");
		}
		case PF_ASTC_6x6:
		{
			return FString("PF_ASTC_6x6");
		}
		case PF_ASTC_8x8:
		{
			return FString("PF_ASTC_8x8");
		}
		case PF_ASTC_10x10:
		{
			return FString("PF_ASTC_10x10");
		}
		case PF_ASTC_12x12:
		{
			return FString("PF_ASTC_12x12");
		}
		case PF_BC6H:
		{
			return FString("PF_BC6H");
		}
		case PF_BC7:
		{
			return FString("PF_BC7");
		}
		case PF_R8_UINT:
		{
			return FString("PF_R8_UINT");
		}
		case PF_L8:
		{
			return FString("PF_L8");
		}
		case PF_MAX:
		{
			return FString("PF_MAX");
		}
		default:
		{
			return FString("Error");
		}
	}
}
