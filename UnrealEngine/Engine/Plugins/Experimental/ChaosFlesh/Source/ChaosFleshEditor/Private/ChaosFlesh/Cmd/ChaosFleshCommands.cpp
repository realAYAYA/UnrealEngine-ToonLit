// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosFlesh/Cmd/ChaosFleshCommands.h"

#include "ChaosFlesh/Asset/FleshAssetFactory.h"
#include "ChaosFlesh/Cmd/FleshAssetConversion.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshCollection.h"
#include "ChaosFlesh/FleshCollectionUtility.h"
#include "ChaosFlesh/FleshComponent.h"
#include "Chaos/Tetrahedron.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GeometryCollection/TransformCollection.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(UChaosFleshCommandsLogging, NoLogging, All);


void FChaosFleshCommands::ImportFile(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 1)
	{
		//FString BasePath = FPaths::ProjectDir() + "Imports/default.geo";// +FString(Args[0]);
		if (FPaths::FileExists(Args[0]))
		{
			auto Factory = NewObject<UFleshAssetFactory>();
			UPackage* Package = CreatePackage(TEXT("/Game/FleshAsset"));
			//UPackage* Package = CreatePackage( *FPaths::ProjectContentDir() );

			UFleshAsset* FleshAsset = static_cast<UFleshAsset*>(Factory->FactoryCreateNew(UFleshAsset::StaticClass(), Package, FName("FleshAsset"), RF_Standalone | RF_Public, NULL, GWarn));
			FAssetRegistryModule::AssetCreated(FleshAsset);
			{
				FFleshAssetEdit EditObject = FleshAsset->EditCollection();
				if (FFleshCollection* Collection = EditObject.GetFleshCollection())
				{
					UE_LOG(UChaosFleshCommandsLogging, Log, TEXT("FChaosFleshCommands::ImportFile"));
					if (TUniquePtr<FFleshCollection> InCollection = FFleshAssetConversion::ImportTetFromFile(Args[0]))
					{
						Collection->CopyMatchingAttributesFrom(*InCollection);
					}
				}
				Package->SetDirtyFlag(true);
			}
		}
	}
	else
	{
		UE_LOG(UChaosFleshCommandsLogging, Error, TEXT("Failed to import file for flesh asset."));
	}
}

void
FChaosFleshCommands::FindQualifyingTetrahedra(const TArray<FString>& Args, UWorld* World)
{
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator ActorIt(*SelectedActors); ActorIt; ++ActorIt)
		{
			if (AActor* Actor = Cast<AActor>(*ActorIt))
			{
				const TSet<UActorComponent*>& Components = Actor->GetComponents();
				for (TSet<UActorComponent*>::TConstIterator CompIt = Components.CreateConstIterator(); CompIt; ++CompIt)
				{
					if (UFleshComponent* FleshComponent = Cast<UFleshComponent>(*CompIt))
					{
						if (const UFleshAsset* RestCollection = FleshComponent->GetRestCollection())
						{
							const FFleshCollection* FleshCollection = RestCollection->GetCollection();

							const TManagedArray<FIntVector4>* TetMesh =
								FleshCollection->FindAttribute<FIntVector4>(
									FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
							const TManagedArray<int32>* TetrahedronStart =
								FleshCollection->FindAttribute<int32>(
									FTetrahedralCollection::TetrahedronStartAttribute, FGeometryCollection::GeometryGroup);
							const TManagedArray<int32>* TetrahedronCount =
								FleshCollection->FindAttribute<int32>(
									FTetrahedralCollection::TetrahedronCountAttribute, FGeometryCollection::GeometryGroup);

							const UFleshDynamicAsset* DynamicCollection = FleshComponent->GetDynamicCollection();
							const TManagedArray<FVector3f>* Vertex =
								DynamicCollection && DynamicCollection->FindPositions() && DynamicCollection->FindPositions()->Num() ?
								DynamicCollection->FindPositions() :
								FleshCollection->FindAttribute<FVector3f>("Vertex", "Vertices");

							float MaxAR = TNumericLimits<float>::Max();
							float MinVol = -TNumericLimits<float>::Max();
							float XCoordGT = TNumericLimits<float>::Max();
							float YCoordGT = TNumericLimits<float>::Max();
							float ZCoordGT = TNumericLimits<float>::Max();
							float XCoordLT = -TNumericLimits<float>::Max();
							float YCoordLT = -TNumericLimits<float>::Max();
							float ZCoordLT = -TNumericLimits<float>::Max();
							bool bHideTets = false;
							for (int32 Idx = 0; Idx < Args.Num(); Idx++)
							{
								if (Args[Idx].Equals(FString(TEXT("MaxAR"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										MaxAR = FCString::Atof(*Args[Idx]);
									}
								}
								else if (Args[Idx].Equals(FString(TEXT("MinVol"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										MinVol = FCString::Atof(*Args[Idx]);
									}
								}

								// XYZ greater than
								else if (Args[Idx].Equals(FString(TEXT("XCoordGT"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										XCoordGT = FCString::Atof(*Args[Idx]);
									}
								}
								else if (Args[Idx].Equals(FString(TEXT("YCoordGT"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										YCoordGT = FCString::Atof(*Args[Idx]);
									}
								}
								else if (Args[Idx].Equals(FString(TEXT("ZCoordGT"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										ZCoordGT = FCString::Atof(*Args[Idx]);
									}
								}

								// XYZ less than
								else if (Args[Idx].Equals(FString(TEXT("XCoordLT"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										XCoordLT = FCString::Atof(*Args[Idx]);
									}
								}
								else if (Args[Idx].Equals(FString(TEXT("YCoordLT"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										YCoordLT = FCString::Atof(*Args[Idx]);
									}
								}
								else if (Args[Idx].Equals(FString(TEXT("ZCoordLT"))))
								{
									if (Args.IsValidIndex(Idx + 1))
									{
										Idx++;
										ZCoordLT = FCString::Atof(*Args[Idx]);
									}
								}

								else if (Args[Idx].Equals(FString(TEXT("HideTets"))))
								{
									bHideTets = true;
								}
							}

							TArray<int32> Indices;
							for (int32 TetMeshIdx = 0; TetMeshIdx < TetrahedronStart->Num(); TetMeshIdx++)
							{
								const int32 TetMeshStart = (*TetrahedronStart)[TetMeshIdx];
								const int32 TetMeshCount = (*TetrahedronCount)[TetMeshIdx];

								for (int32 i = 0; i < TetMeshCount; i++)
								{
									const int32 Idx = TetMeshStart + i;
									const FIntVector4& Tet = (*TetMesh)[Idx];

									const int32 MaxIdx = FGenericPlatformMath::Max(
										FGenericPlatformMath::Max(Tet[0], Tet[1]),
										FGenericPlatformMath::Max(Tet[2], Tet[3]));
									if (MaxIdx >= Vertex->Num())
									{
										continue;
									}

									Chaos::TTetrahedron<Chaos::FReal> Tetrahedron(
										(*Vertex)[Tet[0]],
										(*Vertex)[Tet[1]],
										(*Vertex)[Tet[2]],
										(*Vertex)[Tet[3]]);

									if (MinVol != -TNumericLimits<float>::Max())
									{
										float Vol = Tetrahedron.GetSignedVolume();
										if (Vol < MinVol)
										{
											Indices.Add(Idx);
											continue;
										}
									}
									if (MaxAR != TNumericLimits<float>::Max())
									{
										float AR = Tetrahedron.GetAspectRatio();
										if (AR > MaxAR)
										{
											Indices.Add(Idx);
											continue;
										}
									}
									if (XCoordGT != TNumericLimits<float>::Max() ||
										YCoordGT != TNumericLimits<float>::Max() ||
										ZCoordGT != TNumericLimits<float>::Max())
									{
										bool bAdd = true;
										if (XCoordGT != TNumericLimits<float>::Max())
										{
											for (int32 j = 0; j < 4; j++) bAdd &= (*Vertex)[Tet[j]][0] > XCoordGT;
										}
										if (YCoordGT != TNumericLimits<float>::Max())
										{
											for (int32 j = 0; j < 4; j++) bAdd &= (*Vertex)[Tet[j]][1] > YCoordGT;
										}
										if (ZCoordGT != TNumericLimits<float>::Max())
										{
											for (int32 j = 0; j < 4; j++) bAdd &= (*Vertex)[Tet[j]][2] > ZCoordGT;
										}
										if (bAdd)
										{
											Indices.Add(Idx);
											continue;
										}
									}
									if (XCoordLT != -TNumericLimits<float>::Max() ||
										YCoordLT != -TNumericLimits<float>::Max() ||
										ZCoordLT != -TNumericLimits<float>::Max())
									{
										bool bAdd = true;
										if (XCoordLT != -TNumericLimits<float>::Max())
										{
											for (int32 j = 0; j < 4; j++) bAdd &= (*Vertex)[Tet[j]][0] < XCoordLT;
										}
										if (YCoordLT != -TNumericLimits<float>::Max())
										{
											for (int32 j = 0; j < 4; j++) bAdd &= (*Vertex)[Tet[j]][1] < YCoordLT;
										}
										if (ZCoordLT != -TNumericLimits<float>::Max())
										{
											for (int32 j = 0; j < 4; j++) bAdd &= (*Vertex)[Tet[j]][2] < ZCoordLT;
										}
										if (bAdd)
										{
											Indices.Add(Idx);
											continue;
										}
									}
								}
							}

							if (bHideTets)
							{
								FleshComponent->HideTetrahedra.Append(Indices);
							}

							if (Indices.Num())
							{
								FString IndicesStr(TEXT("["));
								int32 i = 0;
								for (; i < Indices.Num() - 1; i++)
								{
									IndicesStr.Append(FString::Printf(TEXT("%d "), Indices[i]));
								}
								if (i < Indices.Num())
								{
									IndicesStr.Append(FString::Printf(TEXT("%d]"), Indices[i]));
								}
								else
								{
									IndicesStr.Append(TEXT("]"));
								}
								UE_LOG(UChaosFleshCommandsLogging, Log,
									TEXT("ChaosDeformableCommands.FindQualifyingTetrahedra - '%s.%s' Found %d qualifying tetrahedra: \n%s"),
									*Actor->GetName(),
									*FleshComponent->GetName(),
									Indices.Num(),
									*IndicesStr);
							}

						} // end if RestCollection
					}
				}
			}
		}
	}
}