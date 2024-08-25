// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDVolVolumeTranslator.h"

#include "USDAssetImportData.h"
#include "USDAssetUserData.h"
#include "USDConversionUtils.h"
#include "USDDrawModeComponent.h"
#include "USDIntegrationUtils.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDPrimConversion.h"
#include "USDProjectSettings.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"

#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/Level.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"

#if WITH_EDITOR
#include "AssetImportTask.h"
#include "OpenVDBImportOptions.h"
#include "SparseVolumeTextureFactory.h"
#endif	  // WITH_EDITOR

#if USE_USD_SDK

#include "USDIncludesStart.h"
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>
#include "USDIncludesEnd.h"

static bool bRemoveDuplicates = true;
static FAutoConsoleVariableRef CVarRemoveDuplicates(
	TEXT("USD.Volume.RemoveDuplicateAnimatedFrames"),
	bRemoveDuplicates,
	TEXT(
		"If this is true (default), the contents of a .VDB file are added only once to animated Sparse Volume Textures (SVT), even if the same file shows in multiple different time samples. If this is false, every OpenVDBAsset prim filePath time sample is parse as a new frame on the animated SVT."
	)
);

namespace UE::UsdVolVolumeTranslator::Private
{
#if WITH_EDITOR
	struct FSparseVolumeTextureInfo
	{
		FString SourceVDBFilePath;

		TArray<FString> SourceOpenVDBAssetPrimPaths;

		TArray<double> TimeSamplePathTimeCodes;
		TArray<int32> TimeSamplePathIndices;
		TArray<FString> TimeSamplePaths;	// First item may equal SourceVDBFilePath. May have duplicate entries

		// Fields on the Volume prim that were connected to this .vdb file
		TArray<FString> VolumeFieldNames;

		// e.g. {'velocity': {'AttributesA.R': 'X', 'AttributesA.G': 'Y', 'AttributesA.B': 'Z'}}
		TMap<FString, TMap<FString, FString>> GridNameToChannelComponentMapping;
		TOptional<ESparseVolumeAttributesFormat> AttributesAFormat;
		TOptional<ESparseVolumeAttributesFormat> AttributesBFormat;

		USparseVolumeTexture* SparseVolumeTexture = nullptr;
		FString PrefixedAssetHash;
	};

	static const int32 SparseVolumeTextureChannelCount = 8;

	// Collect all the .vdb files that this prim wants to parse, and the desired fields/grids from them.
	//
	// In VDB terminology a "grid" is essentially a 3D texture, and can have formats like float, double3, half, etc.
	// In USD the analogous term is "field", but essentially means the same thing. Possibly the terminology is abstracted
	// to also fit the Field3D library, which we don't support it. Field/grid will be used interchangeably here.
	//
	// USD is very flexible and allows the user to reference specific grids from of each .vdb file. The syntax makes it
	// difficult to find out at once all the grids we'll need to parse from each the .vdb files, so here we need to group them
	// up first before deferring to the SparseVolumeTextureFactory.
	//
	// Note that USD allows a single Volume prim to reference grids from multiple .vdb files, and to also "timeSample" the
	// file reference to allow for volume animations. This means that in UE a "Volume" prim corresponds to a single
	// HeterogeneousVolumeActor, but which in turn can have any number of Sparse Volume Textures (one for each .vdb file referenced).
	TMap<FString, FSparseVolumeTextureInfo> CollectSparseVolumeTextureInfoByFilePath(const pxr::UsdVolVolume& Volume)
	{
		TMap<FString, FSparseVolumeTextureInfo> FilePathHashToInfo;

		FScopedUsdAllocs UsdAllocs;

		pxr::UsdStageRefPtr Stage = Volume.GetPrim().GetStage();

		const std::map<pxr::TfToken, pxr::SdfPath>& FieldMap = Volume.GetFieldPaths();
		for (std::map<pxr::TfToken, pxr::SdfPath>::const_iterator Iter = FieldMap.cbegin(); Iter != FieldMap.end(); ++Iter)
		{
			// This field name is the name of the field for the Volume prim, which can be anything and differ from the
			// grid name within the .vdb files
			const pxr::TfToken& FieldName = Iter->first;
			const pxr::SdfPath& AssetPrimPath = Iter->second;

			pxr::UsdPrim OpenVDBPrim = Stage->GetPrimAtPath(AssetPrimPath);
			pxr::UsdVolOpenVDBAsset OpenVDBPrimSchema = pxr::UsdVolOpenVDBAsset{OpenVDBPrim};
			if (OpenVDBPrimSchema)
			{
				pxr::UsdAttribute FilePathAttr = OpenVDBPrimSchema.GetFilePathAttr();

				FString ResolvedVDBPath = UsdUtils::GetResolvedAssetPath(FilePathAttr, pxr::UsdTimeCode::Default());

				// Find timesampled paths, if any
				TArray<double> TimeSamplePathTimeCodes;
				TArray<int32> TimeSamplePathIndices;
				TArray<FString> TimeSamplePaths;

				TMap<FString, int32> PathToIndex;

				std::vector<double> TimeSamples;
				if (FilePathAttr.GetTimeSamples(&TimeSamples) && TimeSamples.size() > 0)
				{
					UE::FSdfLayerOffset CombinedOffset = UsdUtils::GetPrimToStageOffset(UE::FUsdPrim{OpenVDBPrim});

					TimeSamplePathTimeCodes.Reserve(TimeSamples.size());
					TimeSamplePaths.Reserve(TimeSamples.size());
					TimeSamplePathIndices.Reserve(TimeSamples.size());
					for (double TimeSample : TimeSamples)
					{
						// We always want to store on the AssetUserData (which is where this stuff will end up in)
						// the time codes in the layer where the actual OpenVDBPrim is authored. If that layer is referenced
						// by a parent layer through an offset and scale, TimeSample will contain that offset and scale here,
						// which we need to undo
						double LayerLocalTimeCode = (TimeSample - CombinedOffset.Offset) / CombinedOffset.Scale;
						TimeSamplePathTimeCodes.Add(LayerLocalTimeCode);

						FString ResolvedFramePath = UsdUtils::GetResolvedAssetPath(FilePathAttr, TimeSample);

						// If we had no default time sample to act as the "main file", take the first frame
						if (ResolvedVDBPath.IsEmpty())
						{
							ResolvedVDBPath = ResolvedFramePath;
						}

						if (bRemoveDuplicates)
						{
							if (int32* FoundIndex = PathToIndex.Find(ResolvedFramePath))
							{
								TimeSamplePathIndices.Add(*FoundIndex);
							}
							else
							{
								TimeSamplePaths.Add(ResolvedFramePath);

								const int32 NewIndex = TimeSamplePaths.Num() - 1;
								PathToIndex.Add(ResolvedFramePath, NewIndex);
								TimeSamplePathIndices.Add(NewIndex);
							}
						}
						else
						{
							TimeSamplePaths.Add(ResolvedFramePath);

							const int32 NewIndex = TimeSamplePaths.Num() - 1;
							TimeSamplePathIndices.Add(NewIndex);
						}
					}
				}

				// Hash all the relevant file paths here: The collection of file paths to parse determines the SVT, and
				// we want one FSparseVolumeTextureInfo per SVT
				FString FilePathHashString;
				{
					FSHA1 SHA1;
					SHA1.UpdateWithString(*ResolvedVDBPath, ResolvedVDBPath.Len());
					for (const FString& TimeSamplePath : TimeSamplePaths)
					{
						SHA1.UpdateWithString(*TimeSamplePath, TimeSamplePath.Len());
					}
					SHA1.Final();

					FSHAHash FilePathHash;
					SHA1.GetHash(&FilePathHash.Hash[0]);

					FilePathHashString = FilePathHash.ToString();
				}

				if (!ResolvedVDBPath.IsEmpty())
				{
					FSparseVolumeTextureInfo& SparseVolumeTextureInfo = FilePathHashToInfo.FindOrAdd(FilePathHashString);
					SparseVolumeTextureInfo.SourceOpenVDBAssetPrimPaths.AddUnique(UsdToUnreal::ConvertPath(AssetPrimPath));
					SparseVolumeTextureInfo.SourceVDBFilePath = ResolvedVDBPath;
					SparseVolumeTextureInfo.TimeSamplePathTimeCodes = MoveTemp(TimeSamplePathTimeCodes);
					SparseVolumeTextureInfo.TimeSamplePathIndices = MoveTemp(TimeSamplePathIndices);
					SparseVolumeTextureInfo.TimeSamplePaths = MoveTemp(TimeSamplePaths);

					FString FieldNameStr = UsdToUnreal::ConvertToken(FieldName);
					SparseVolumeTextureInfo.VolumeFieldNames.AddUnique(FieldNameStr);

					pxr::TfToken GridName;
					pxr::UsdAttribute Attr = OpenVDBPrimSchema.GetFieldNameAttr();
					if (Attr && Attr.Get<pxr::TfToken>(&GridName))
					{
						FString GridNameStr = UsdToUnreal::ConvertToken(GridName);

						// Note we want this to add an entry to SparseVolumeTexture.GridNameToChannelNames even if we won't
						// find the schema on the prim, as we'll use these entries to make sure the generated Sparse Volume
						// Texture contains theses desired fields
						TMap<FString, FString>& ChannelToComponent = SparseVolumeTextureInfo.GridNameToChannelComponentMapping.FindOrAdd(GridNameStr);

						if (UsdUtils::PrimHasSchema(OpenVDBPrim.GetPrim(), UnrealIdentifiers::SparseVolumeTextureAPI))
						{
							// Parse desired data types for AttributesA and AttributesB channels
							TFunction<void(pxr::TfToken, TOptional<ESparseVolumeAttributesFormat>&)> HandleAttribute =
								[&OpenVDBPrim,
								 &AssetPrimPath,
								 &ResolvedVDBPath](pxr::TfToken AttrName, TOptional<ESparseVolumeAttributesFormat>& AttributeFormat)
							{
								using FormatMapType = std::unordered_map<pxr::TfToken, ESparseVolumeAttributesFormat, pxr::TfHash>;
								using InverseFormatMapType = std::unordered_map<ESparseVolumeAttributesFormat, FString>;

								// clang-format off
								const static FormatMapType FormatMap = {
									{pxr::TfToken("unorm8"),  ESparseVolumeAttributesFormat::Unorm8},
									{pxr::TfToken("float16"), ESparseVolumeAttributesFormat::Float16},
									{pxr::TfToken("float32"), ESparseVolumeAttributesFormat::Float32},
								};
								const static InverseFormatMapType InverseFormatMap = {
									{ESparseVolumeAttributesFormat::Unorm8,  TEXT("unorm8")},
									{ESparseVolumeAttributesFormat::Float16, TEXT("float16")},
									{ESparseVolumeAttributesFormat::Float32, TEXT("float32")},
								};
								// clang-format on

								pxr::TfToken DataType;
								pxr::UsdAttribute AttrA = OpenVDBPrim.GetAttribute(AttrName);
								if (AttrA && AttrA.Get<pxr::TfToken>(&DataType))
								{
									FormatMapType::const_iterator Iter = FormatMap.find(DataType);
									if (Iter != FormatMap.end())
									{
										ESparseVolumeAttributesFormat TargetFormat = Iter->second;

										// Check in case multiple OpenVDBAsset prims want different values for the data type
										const bool bIsSet = AttributeFormat.IsSet();
										if (bIsSet && AttributeFormat.GetValue() != TargetFormat)
										{
											const FString* ExistingFormat = nullptr;
											InverseFormatMapType::const_iterator ExistingIter = InverseFormatMap.find(AttributeFormat.GetValue());
											if (ExistingIter != InverseFormatMap.end())
											{
												ExistingFormat = &ExistingIter->second;
											}

											UE_LOG(
												LogUsd,
												Warning,
												TEXT(
													"OpenVDBAsset prims disagree on the attribute channel format for the Sparse Volume Texture generated for VDB file '%s' (encountered '%s' and '%s'). If there are multiple opinions for the attribute channel formats from different OpenVDBAsset prims, they must all agree!"
												),
												*ResolvedVDBPath,
												*UsdToUnreal::ConvertToken(DataType),
												ExistingFormat ? **ExistingFormat : TEXT("unknown")
											);
										}
										else if (!bIsSet)
										{
											AttributeFormat = TargetFormat;
										}
									}
									else
									{
										UE_LOG(
											LogUsd,
											Warning,
											TEXT(
												"Invalid Sparse Volume Texture attribute channel format '%s'. Available formats: 'unorm8', 'float16' and 'float32'."
											),
											*UsdToUnreal::ConvertToken(DataType),
											*UsdToUnreal::ConvertPath(AssetPrimPath)
										);
									}
								}
							};
							HandleAttribute(UnrealIdentifiers::UnrealSVTAttributesADataType, SparseVolumeTextureInfo.AttributesAFormat);
							HandleAttribute(UnrealIdentifiers::UnrealSVTAttributesBDataType, SparseVolumeTextureInfo.AttributesBFormat);

							// Parse desired channel assignment
							pxr::VtArray<pxr::TfToken> DesiredChannels;
							pxr::VtArray<pxr::TfToken> DesiredComponents;
							pxr::UsdAttribute ComponentsAttr = OpenVDBPrim.GetAttribute(UnrealIdentifiers::UnrealSVTMappedGridComponents);
							pxr::UsdAttribute ChannelsAttr = OpenVDBPrim.GetAttribute(UnrealIdentifiers::UnrealSVTMappedAttributeChannels);
							if (ChannelsAttr &&														 //
								ComponentsAttr &&													 //
								ChannelsAttr.Get<pxr::VtArray<pxr::TfToken>>(&DesiredChannels) &&	 //
								ComponentsAttr.Get<pxr::VtArray<pxr::TfToken>>(&DesiredComponents))
							{
								// These must always match of course
								if (DesiredChannels.size() == DesiredComponents.size())
								{
									// If we have more than one OpenVDBAsset prim reading from the same VDB file, the declared component to
									// channel mappings must be compatible
									for (uint32 Index = 0; Index < DesiredChannels.size(); ++Index)
									{
										FString Channel = UsdToUnreal::ConvertString(DesiredChannels[Index]);
										FString Component = UsdToUnreal::ConvertString(DesiredComponents[Index]);

										if (const FString* ExistingComponentMapping = ChannelToComponent.Find(Channel))
										{
											if (Component != *ExistingComponentMapping)
											{
												UE_LOG(
													LogUsd,
													Warning,
													TEXT(
														"Found multiple OpenVDBAsset prims (including '%s') targetting the same grid '%s', but with with conflicting grid component to Sparse Volume Texture attribute channel mapping (for example, both components '%s' and '%s' are mapped to the same channel '%s', which is not allowed)"
													),
													*UsdToUnreal::ConvertPath(AssetPrimPath),
													*GridNameStr,
													**ExistingComponentMapping,
													*Component,
													*Channel
												);
											}
										}
										else
										{
											ChannelToComponent.Add(Channel, Component);
										}
									}
								}
								else
								{
									UE_LOG(
										LogUsd,
										Warning,
										TEXT(
											"Failed to parse custom component to attribute mapping from OpenVDBAsset prim '%s': The '%s' and '%s' attributes should have the same number of entries, but the former has %u entries while the latter has %u"
										),
										*UsdToUnreal::ConvertPath(AssetPrimPath),
										*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealSVTMappedGridComponents),
										*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealSVTMappedAttributeChannels),
										DesiredComponents.size(),
										DesiredChannels.size()
									);
								}
							}
						}
					}
				}
				else
				{
					UE_LOG(
						LogUsd,
						Warning,
						TEXT("Failed to find the VDB file '%s' referenced by OpenVDBAsset prim at path '%s'"),
						*ResolvedVDBPath,
						*UsdToUnreal::ConvertPath(AssetPrimPath)
					);
				}
			}
			else
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Failed to find an OpenVDBAsset prim at path '%s' for field '%s' of prim '%s'"),
					*UsdToUnreal::ConvertPath(AssetPrimPath),
					*UsdToUnreal::ConvertToken(FieldName),
					*UsdToUnreal::ConvertPath(Volume.GetPrim().GetPath())
				);
			}
		}

		return FilePathHashToInfo;
	}

	// Here we must stash into InOutPreviewData.ImportOptions the desired channel mapping for this SVT given all the GridNameToChannelNames
	// mappings we pulled out of the prims if they had any instances of our SparseVolumeTextureAPI schema
	void SetVDBImportOptions(const FSparseVolumeTextureInfo& ParsedTexture, FOpenVDBPreviewData& InOutPreviewData)
	{
		// Tweak the collected filenames for other frames: The OpenVDB importer will scan for similar filenames
		// in the same folder as the main file, but through USD we expect the user to manually pick file paths
		// for each time sample (which may or may not come from the same folder, or be in any particular order)
		InOutPreviewData.SequenceFilenames = ParsedTexture.TimeSamplePaths;
		InOutPreviewData.DefaultImportOptions.bIsSequence = ParsedTexture.TimeSamplePaths.Num() > 1;

		// Apply manually specified channel formats, if any
		check(InOutPreviewData.DefaultImportOptions.Attributes.Num() == 2);
		if (ParsedTexture.AttributesAFormat.IsSet())
		{
			InOutPreviewData.DefaultImportOptions.Attributes[0].Format = ParsedTexture.AttributesAFormat.GetValue();
		}
		if (ParsedTexture.AttributesBFormat.IsSet())
		{
			InOutPreviewData.DefaultImportOptions.Attributes[1].Format = ParsedTexture.AttributesBFormat.GetValue();
		}

		static const TMap<FString, int32> ChannelIndexMap = {
			{TEXT("AttributesA.R"), 0},
			{TEXT("AttributesA.G"), 1},
			{TEXT("AttributesA.B"), 2},
			{TEXT("AttributesA.A"), 3},
			{TEXT("AttributesB.R"), 4},
			{TEXT("AttributesB.G"), 5},
			{TEXT("AttributesB.B"), 6},
			{TEXT("AttributesB.A"), 7}
		  };

		static const TMap<FString, int32> ComponentIndexMap = {
			{TEXT("X"), 0},
			{TEXT("Y"), 1},
			{TEXT("Z"), 2},
			{TEXT("W"), 3},
			{TEXT("R"), 0},
			{TEXT("G"), 1},
			{TEXT("B"), 2},
			{TEXT("A"), 3}
		  };

		// We'll use this to make sure we only try assigning one thing to each available attribute channel
		FString UsedChannels[SparseVolumeTextureChannelCount];

		TMap<FString, const FOpenVDBGridInfo*> GridInfoByName;
		for (const FOpenVDBGridInfo& GridInfo : InOutPreviewData.GridInfo)
		{
			GridInfoByName.Add(GridInfo.Name, &GridInfo);
		}

		FString AvailableGrids;
		static FString AvailableComponentNames;
		static FString AvailableChannelNames;

		// We'll collect our new mapping here and only apply to InOutPreviewData if we have a valid mapping,
		// so that we don't wipe it clean if we don't have anything valid to add back anyway
		TStaticArray<FOpenVDBSparseVolumeAttributesDesc, 2> NewChannelMapping;
		bool bHadValidMapping = false;

		for (const TPair<FString, TMap<FString, FString>>& GridToMapping : ParsedTexture.GridNameToChannelComponentMapping)
		{
			const FString& GridName = GridToMapping.Key;
			if (const FOpenVDBGridInfo* FoundGridInfo = GridInfoByName.FindRef(GridName))
			{
				int32 GridIndex = static_cast<int32>(FoundGridInfo->Index);

				for (const TPair<FString, FString>& ChannelToComponent : GridToMapping.Value)
				{
					// Get and validate the desired component as an index (e.g. whether this mapping refers to 'velocity.X' (index 0) or 'velocity.Y'
					// (index 1), etc.)
					const FString& DesiredComponent = ChannelToComponent.Value;
					int32 ComponentIndex = INDEX_NONE;
					if (const int32* FoundComponentIndex = ComponentIndexMap.Find(DesiredComponent))
					{
						ComponentIndex = *FoundComponentIndex;

						check(ComponentIndex < SparseVolumeTextureChannelCount);
						if (ComponentIndex < 0 || static_cast<uint32>(ComponentIndex) >= FoundGridInfo->NumComponents)
						{
							UE_LOG(
								LogUsd,
								Warning,
								TEXT("Invalid component name '%s' for grid '%s' in VDB file '%s', as that particular grid only has %u components."),
								*DesiredComponent,
								*GridName,
								*ParsedTexture.SourceVDBFilePath,
								FoundGridInfo->NumComponents
							);
							ComponentIndex = INDEX_NONE;
						}
					}
					else
					{
						if (AvailableComponentNames.IsEmpty())
						{
							TArray<FString> ComponentNames;
							ComponentIndexMap.GetKeys(ComponentNames);

							AvailableComponentNames = TEXT("'") + FString::Join(ComponentNames, TEXT("', '")) + TEXT("'");
						}

						UE_LOG(
							LogUsd,
							Warning,
							TEXT(
								"Desired component name '%s' for grid '%s' in VDB file '%s' is not a valid component name. Avaliable component names: %s"
							),
							*DesiredComponent,
							*GridName,
							*ParsedTexture.SourceVDBFilePath,
							*AvailableComponentNames
						);
					}

					// Get and validate desired channel (e.g. whether this mapping means to put something on 'AttributesA.R' or 'AttributesB.A', etc.)
					const FString& DesiredChannel = ChannelToComponent.Key;
					int32 ChannelIndex = INDEX_NONE;
					if (const int32* FoundChannelIndex = ChannelIndexMap.Find(DesiredChannel))
					{
						ChannelIndex = *FoundChannelIndex;

						const bool bIsSet = !UsedChannels[ChannelIndex].IsEmpty();
						const FString GridAndComponent = GridName + TEXT(".") + DesiredComponent;

						if (bIsSet && UsedChannels[ChannelIndex] != GridAndComponent)
						{
							UE_LOG(
								LogUsd,
								Warning,
								TEXT(
									"Cannot use attribute channel '%s' for grid '%s' in VDB file '%s', as the channel is already being used for the grid and component '%s'"
								),
								*DesiredChannel,
								*GridName,
								*ParsedTexture.SourceVDBFilePath,
								*UsedChannels[ChannelIndex]
							);
							ChannelIndex = INDEX_NONE;
						}
						else
						{
							ChannelIndex = *FoundChannelIndex;
							UsedChannels[ChannelIndex] = GridAndComponent;
						}
					}
					else
					{
						if (AvailableChannelNames.IsEmpty())
						{
							TArray<FString> ChannelNames;
							ChannelIndexMap.GetKeys(ChannelNames);

							AvailableChannelNames = TEXT("'") + FString::Join(ChannelNames, TEXT("', '")) + TEXT("'");
						}

						UE_LOG(
							LogUsd,
							Warning,
							TEXT(
								"Desired attribute channel '%s' for grid '%s' in VDB file '%s' is not a valid channel name. Avaliable channel names: %s"
							),
							*DesiredChannel,
							*GridName,
							*ParsedTexture.SourceVDBFilePath,
							*AvailableChannelNames
						);
					}

					// Finally actually assign the desired grid/component mapping
					if (ComponentIndex != INDEX_NONE && GridIndex != INDEX_NONE && ChannelIndex != INDEX_NONE)
					{
						// We track ChannelIndex from 0 through 7, but it's actually two groups of 4
						int32 AttributeGroupIndex = ChannelIndex / 4;
						int32 AttributeChannelIndex = ChannelIndex % 4;

						FOpenVDBSparseVolumeComponentMapping& ComponentMapping = NewChannelMapping[AttributeGroupIndex]
																					 .Mappings[AttributeChannelIndex];
						ComponentMapping.SourceGridIndex = GridIndex;
						ComponentMapping.SourceComponentIndex = ComponentIndex;

						bHadValidMapping = true;
					}
				}
			}
			else
			{
				if (AvailableGrids.IsEmpty())
				{
					TArray<FString> GridNames;
					GridInfoByName.GetKeys(GridNames);

					if (GridNames.Num() > 0)
					{
						AvailableGrids = TEXT("'") + FString::Join(GridNames, TEXT("', '")) + TEXT("'");
					}
				}

				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Faild to find grid named '%s' inside VDB file '%s'. Avaliable grid names: %s"),
					*GridName,
					*ParsedTexture.SourceVDBFilePath,
					*AvailableGrids
				);
			}
		}

		if (bHadValidMapping)
		{
			InOutPreviewData.DefaultImportOptions.Attributes[0].Mappings = NewChannelMapping[0].Mappings;
			InOutPreviewData.DefaultImportOptions.Attributes[1].Mappings = NewChannelMapping[1].Mappings;
		}
	}

	void HashForSparseVolumeTexture(const FOpenVDBPreviewData& PreviewData, FSHA1& InOutHash)
	{
		InOutHash.Update(
			reinterpret_cast<const uint8*>(PreviewData.LoadedFile.GetData()),
			PreviewData.LoadedFile.Num() * PreviewData.LoadedFile.GetTypeSize()
		);

		// Hash other files
		{
			FMD5 MD5;

			if (PreviewData.SequenceFilenames.Num() > 1)
			{
				// Skip first one as that should always be the "main" file, that we just hashed on PreviewData.LoadedFile above.
				// Note: This could become a performance issue if we have many large frames
				for (int32 Index = 1; Index < PreviewData.SequenceFilenames.Num(); ++Index)
				{
					const FString& FrameFilePath = PreviewData.SequenceFilenames[Index];

					// Copied from FMD5Hash::HashFileFromArchive as it doesn't expose its FMD5
					if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileReader(*FrameFilePath)})
					{
						TArray<uint8> LocalScratch;
						LocalScratch.SetNumUninitialized(1024 * 64);

						const int64 Size = Ar->TotalSize();
						int64 Position = 0;

						while (Position < Size)
						{
							const auto ReadNum = FMath::Min(Size - Position, (int64)LocalScratch.Num());
							Ar->Serialize(LocalScratch.GetData(), ReadNum);
							MD5.Update(LocalScratch.GetData(), ReadNum);

							Position += ReadNum;
						}
					}
				}

				FMD5Hash Hash;
				Hash.Set(MD5);
				InOutHash.Update(Hash.GetBytes(), Hash.GetSize());
			}
		}

		// The only other thing that affects the SVT asset hash is the grid to attribute channel mapping.
		// i.e. if we have another Volume prim with entirely different field names but that ends up with the same grid names
		// mapped to the same attribute channels, we want to reuse the generated SVT asset

		InOutHash.Update(
			reinterpret_cast<const uint8*>(&PreviewData.DefaultImportOptions.bIsSequence),
			sizeof(PreviewData.DefaultImportOptions.bIsSequence)
		);

		const TStaticArray<FOpenVDBSparseVolumeAttributesDesc, 2>& AttributeMapping = PreviewData.DefaultImportOptions.Attributes;
		for (int32 AttributesIndex = 0; AttributesIndex < AttributeMapping.Num(); ++AttributesIndex)
		{
			const FOpenVDBSparseVolumeAttributesDesc& AttributesDesc = AttributeMapping[AttributesIndex];

			InOutHash.Update(reinterpret_cast<const uint8*>(&AttributesDesc.Format), sizeof(AttributesDesc.Format));

			const TStaticArray<FOpenVDBSparseVolumeComponentMapping, 4>& ChannelMapping = AttributesDesc.Mappings;
			for (int32 ChannelIndex = 0; ChannelIndex < ChannelMapping.Num(); ++ChannelIndex)
			{
				const FOpenVDBSparseVolumeComponentMapping& Map = ChannelMapping[ChannelIndex];
				InOutHash.Update(reinterpret_cast<const uint8*>(&Map.SourceGridIndex), sizeof(Map.SourceGridIndex));
				InOutHash.Update(reinterpret_cast<const uint8*>(&Map.SourceComponentIndex), sizeof(Map.SourceComponentIndex));
			}
		}
	}

	void HashForVolumetricMaterial(
		const UMaterialInterface* ReferenceMaterial,
		const TMap<FString, FSparseVolumeTextureInfo*>& MaterialParameterToTexture,
		FSHA1& InOutHash
	)
	{
		FString ReferenceMaterialPathName = ReferenceMaterial->GetPathName();
		InOutHash.UpdateWithString(*ReferenceMaterialPathName, ReferenceMaterialPathName.Len());

		TArray<TPair<FString, FSparseVolumeTextureInfo*>> MaterialParameterPairs;
		MaterialParameterPairs.Reserve(MaterialParameterToTexture.Num());
		for (const TPair<FString, FSparseVolumeTextureInfo*>& Pair : MaterialParameterToTexture)
		{
			MaterialParameterPairs.Add(Pair);
		}

		// Make sure we hash our SVTs deterministically, whether they have a specific material assignment due
		// to the schema or not
		MaterialParameterPairs.Sort(
			[](const TPair<FString, FSparseVolumeTextureInfo*>& LHS, const TPair<FString, FSparseVolumeTextureInfo*>& RHS) -> bool
			{
				if (LHS.Key == RHS.Key)
				{
					return LHS.Value->PrefixedAssetHash < RHS.Value->PrefixedAssetHash;
				}
				return LHS.Key < RHS.Key;
			}
		);

		for (const TPair<FString, FSparseVolumeTextureInfo*>& Pair : MaterialParameterPairs)
		{
			InOutHash.UpdateWithString(*Pair.Key, Pair.Key.Len());
			InOutHash.UpdateWithString(*Pair.Value->PrefixedAssetHash, Pair.Value->PrefixedAssetHash.Len());
		}
	}

	// This collects a mapping describing which Sparse Volume Texture (SVT) should be assigned to each SVT material parameter
	// of the ReferenceMaterial.
	// It will prefer checking the VolumePrim for a custom schema where that is manually described, then it will fall back
	// to trying to map Volume prim field names to material parameter names, and finally will just distribute the SVTs over all
	// available parameters in alphabetical order
	TMap<FString, FSparseVolumeTextureInfo*> CollectMaterialParameterTextureAssignment(
		const pxr::UsdPrim& VolumePrim,
		const UMaterial* ReferenceMaterial,
		const TMap<FString, FSparseVolumeTextureInfo>& FilePathHashToTextureInfo
	)
	{
		TMap<FString, FSparseVolumeTextureInfo*> ResultParameterToInfo;

		if (!VolumePrim || !ReferenceMaterial || FilePathHashToTextureInfo.Num() == 0)
		{
			return ResultParameterToInfo;
		}

		FScopedUsdAllocs Allocs;

		// Collect which field was mapped to which .VDB asset (and so SVT asset)
		// A field can only be mapped to a single .VDB, but multiple fields can be mapped to the same .VDB
		TMap<FString, FSparseVolumeTextureInfo*> FieldNameToInfo;
		FieldNameToInfo.Reserve(FilePathHashToTextureInfo.Num());
		for (const TPair<FString, FSparseVolumeTextureInfo>& FilePathToInfo : FilePathHashToTextureInfo)
		{
			for (const FString& FieldName : FilePathToInfo.Value.VolumeFieldNames)
			{
				// We won't be modifying FSparseVolumeTextureInfo in this function anyway, it's just so that we can
				// return non-const pointers
				FieldNameToInfo.Add(FieldName, const_cast<FSparseVolumeTextureInfo*>(&FilePathToInfo.Value));
			}
		}

		// Collect material parameter assignments manually specified via the custom schema, if any
		bool bHadManualAssignment = false;
		if (UsdUtils::PrimHasSchema(VolumePrim, UnrealIdentifiers::SparseVolumeTextureAPI))
		{
			pxr::VtArray<pxr::TfToken> MappedFields;
			pxr::VtArray<pxr::TfToken> MappedParameters;
			pxr::UsdAttribute FieldsAttr = VolumePrim.GetAttribute(UnrealIdentifiers::UnrealSVTMappedFields);
			pxr::UsdAttribute ParametersAttr = VolumePrim.GetAttribute(UnrealIdentifiers::UnrealSVTMappedMaterialParameters);
			if (FieldsAttr &&													//
				ParametersAttr &&												//
				FieldsAttr.Get<pxr::VtArray<pxr::TfToken>>(&MappedFields) &&	//
				ParametersAttr.Get<pxr::VtArray<pxr::TfToken>>(&MappedParameters))
			{
				// These must always match of course
				if (MappedFields.size() == MappedParameters.size())
				{
					for (uint32 Index = 0; Index < MappedFields.size(); ++Index)
					{
						FString FieldName = UsdToUnreal::ConvertString(MappedFields[Index]);
						FString MaterialParameter = UsdToUnreal::ConvertString(MappedParameters[Index]);
						bHadManualAssignment = true;

						if (FSparseVolumeTextureInfo* FoundParsedTexture = FieldNameToInfo.FindRef(FieldName))
						{
							if (const FSparseVolumeTextureInfo* JobAtParam = ResultParameterToInfo.FindRef(MaterialParameter))
							{
								if (JobAtParam->SparseVolumeTexture != FoundParsedTexture->SparseVolumeTexture)
								{
									UE_LOG(
										LogUsd,
										Warning,
										TEXT(
											"Trying to assign different Sparse Volume Textures to the same material parameter '%s' on the material instantiated for Volume prim '%s' and field name '%s'! Only a single material can be assigned to a material parameter at a time."
										),
										*MaterialParameter,
										*UsdToUnreal::ConvertPath(VolumePrim.GetPrimPath()),
										*FieldName
									);
								}
							}
							else
							{
								// We won't be modifying anything in here, it's just so that we can return non-const pointers
								ResultParameterToInfo.Add(MaterialParameter, FoundParsedTexture);
							}
						}
					}
				}
				else
				{
					UE_LOG(
						LogUsd,
						Warning,
						TEXT(
							"Failed to parse custom parsed texture to material parameter mapping from volume prim '%s': The '%s' and '%s' attributes should have the same number of entries, but the former has %u entries while the latter has %u"
						),
						*UsdToUnreal::ConvertPath(VolumePrim.GetPrimPath()),
						*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealSVTMappedFields),
						*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealSVTMappedMaterialParameters),
						MappedFields.size(),
						MappedParameters.size()
					);
				}
			}
		}

		// Collect available parameter names on this material instance
		TSet<FString> SparseVolumeTextureParameterNames;
		{
			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> SparseVolumeTextureParameters;
			ReferenceMaterial->GetAllParametersOfType(EMaterialParameterType::SparseVolumeTexture, SparseVolumeTextureParameters);

			SparseVolumeTextureParameterNames.Reserve(SparseVolumeTextureParameters.Num());
			for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& ParameterPair : SparseVolumeTextureParameters)
			{
				const FMaterialParameterInfo& ParameterInfo = ParameterPair.Key;
				FString ParameterName = ParameterInfo.Name.ToString();

				SparseVolumeTextureParameterNames.Add(ParameterName);
			}
		}

		// Validate that all parameters exist on the material, or else emit a warning
		for (const TPair<FString, FSparseVolumeTextureInfo*>& AssignmentPair : ResultParameterToInfo)
		{
			if (!SparseVolumeTextureParameterNames.Contains(AssignmentPair.Key))
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Failed to assign Sparse Volume Texture '%s' to material '%s' as the desired material parameter '%s' doesn't exist on it"),
					*AssignmentPair.Value->SparseVolumeTexture->GetPathName(),
					*ReferenceMaterial->GetPathName(),
					*AssignmentPair.Key
				);
			}
		}

		// No manual material parameter assignment specified via custom schema: First let's assume that the field names match material parameter names
		bool bHadParameterNameMatch = false;
		if (!bHadManualAssignment)
		{
			ensure(ResultParameterToInfo.Num() == 0);

			TMap<FString, FString> CaseInsensitiveToSensitiveParameterNames;
			CaseInsensitiveToSensitiveParameterNames.Reserve(SparseVolumeTextureParameterNames.Num());
			for (const FString& ParameterName : SparseVolumeTextureParameterNames)
			{
				CaseInsensitiveToSensitiveParameterNames.Add(ParameterName.ToLower(), ParameterName);
			}

			for (const TPair<FString, FSparseVolumeTextureInfo*>& TextureJobPair : FieldNameToInfo)
			{
				const FString& FieldName = TextureJobPair.Key;

				if (const FString* CaseSensitiveParameterName = CaseInsensitiveToSensitiveParameterNames.Find(FieldName.ToLower()))
				{
					ResultParameterToInfo.Add(*CaseSensitiveParameterName, TextureJobPair.Value);
					bHadParameterNameMatch = true;
				}
			}
		}

		// Nothing yet, let's fall back to just distributing the SVTs across the available material parameter slots in alphabetical order
		if (!bHadManualAssignment && !bHadParameterNameMatch)
		{
			ensure(ResultParameterToInfo.Num() == 0);

			// If there aren't enough parameters, let the user know
			if ((SparseVolumeTextureParameterNames.Num() < FilePathHashToTextureInfo.Num()) && FilePathHashToTextureInfo.Num() > 0)
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT(
						"Material '%s' used for prim '%s' doesn't have enough Sparse Volume Texture params to fit all of its %d parsed textures! Some may be left unassigned."
					),
					*ReferenceMaterial->GetPathName(),
					*UsdToUnreal::ConvertPath(VolumePrim.GetPrimPath()),
					FilePathHashToTextureInfo.Num()
				);
			}

			TArray<FString> SortedFieldNames;
			FieldNameToInfo.GetKeys(SortedFieldNames);
			SortedFieldNames.Sort();

			TArray<FString> SortedParameterNames = SparseVolumeTextureParameterNames.Array();
			SortedParameterNames.Sort();

			for (int32 Index = 0; Index < SortedFieldNames.Num() && Index < SortedParameterNames.Num(); ++Index)
			{
				const FString& FieldName = SortedFieldNames[Index];
				const FString& ParameterName = SortedParameterNames[Index];

				FSparseVolumeTextureInfo* FoundTextureJob = FieldNameToInfo.FindRef(FieldName);
				if (ensure(FoundTextureJob))
				{
					ResultParameterToInfo.Add(ParameterName, FoundTextureJob);
				}
			}
		}

		return ResultParameterToInfo;
	}

	UMaterialInstance* InstantiateMaterial(
		FName InstanceName,
		UMaterialInterface* ReferenceMaterial,
		const FUsdSchemaTranslationContext& TranslationContext
	)
	{
		if (!ReferenceMaterial)
		{
			return nullptr;
		}

		// Create an UMaterialInstanceConstant
		if (GIsEditor)	  // Also have to prevent Standalone game from going with MaterialInstanceConstants
		{
			UMaterialInstanceConstant* NewMaterial = NewObject<UMaterialInstanceConstant>(
				GetTransientPackage(),
				InstanceName,
				TranslationContext.ObjectFlags | EObjectFlags::RF_Transient
			);

			if (NewMaterial)
			{
				if (ensure(ReferenceMaterial))
				{
					// We can't blindly recreate all component render states when a level is being added, because we may end up first creating
					// render states for some components, and UWorld::AddToWorld calls FScene::AddPrimitive which expects the component to not
					// have primitives yet
					FMaterialUpdateContext::EOptions::Type Options = FMaterialUpdateContext::EOptions::Default;
					if (TranslationContext.Level && TranslationContext.Level->bIsAssociatingLevel)
					{
						Options = (FMaterialUpdateContext::EOptions::Type)(Options & ~FMaterialUpdateContext::EOptions::RecreateRenderStates);
					}
					FMaterialUpdateContext UpdateContext(Options, GMaxRHIShaderPlatform);
					UpdateContext.AddMaterialInstance(NewMaterial);

					NewMaterial->SetParentEditorOnly(ReferenceMaterial);

					NewMaterial->PreEditChange(nullptr);
					NewMaterial->PostEditChange();

					return NewMaterial;
				}
			}
		}
		else
		// Create an UMaterialInstanceDynamic
		{
			// SparseVolumeTextures can't be created at runtime so this branch should never really be taken for now, but anyway...
			// Note: Some code in FNiagaraBakerRenderer::RenderSparseVolumeTexture suggests that this workflow wouldn't really work
			// because the HeterogeneousVolumeComponent always creates its own MID from the material we give it, and creating a MID
			// from another MID doesn't really work
			if (ensure(ReferenceMaterial))
			{
				UMaterialInstanceDynamic* NewMaterial = UMaterialInstanceDynamic::Create(ReferenceMaterial, GetTransientPackage(), InstanceName);
				if (NewMaterial)
				{
					NewMaterial->SetFlags(RF_Transient);
				}
				return NewMaterial;
			}
		}

		return nullptr;
	}

	void AssignMaterialParameters(UMaterialInstance* MaterialInstance, const TMap<FString, FSparseVolumeTextureInfo*>& ParameterToTexture)
	{
		// Now that we finally have the parameter assignment for each SVT, assign them to the materials
		for (const TPair<FString, FSparseVolumeTextureInfo*>& Pair : ParameterToTexture)
		{
			if (UMaterialInstanceConstant* Constant = Cast<UMaterialInstanceConstant>(MaterialInstance))
			{
				FMaterialParameterInfo Info;
				Info.Name = *Pair.Key;
				Constant->SetSparseVolumeTextureParameterValueEditorOnly(Info, Pair.Value->SparseVolumeTexture);
			}
			else if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(MaterialInstance))
			{
				Dynamic->SetSparseVolumeTextureParameterValue(*Pair.Key, Pair.Value->SparseVolumeTexture);
			}
		}
	}
#endif	  // WITH_EDITOR
}	 // namespace UE::UsdVolVolumeTranslator::Private

void FUsdVolVolumeTranslator::CreateAssets()
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdVolVolumeTranslator::CreateAssets);

	using namespace UE::UsdVolVolumeTranslator::Private;

	if (!Context->AssetCache || !Context->InfoCache)
	{
		return;
	}

	// Don't bother generating assets if we're going to just draw some bounds for this prim instead
	EUsdDrawMode DrawMode = UsdUtils::GetAppliedDrawMode(GetPrim());
	if (DrawMode != EUsdDrawMode::Default)
	{
		CreateAlternativeDrawModeAssets(DrawMode);
		return;
	}

	if (!Context->bAllowParsingSparseVolumeTextures)
	{
		return;
	}

	const FString VolumePrimPathString = PrimPath.GetString();
	pxr::UsdPrim VolumePrim = GetPrim();
	pxr::UsdVolVolume Volume{VolumePrim};
	if (!Volume)
	{
		return;
	}
	pxr::UsdStageRefPtr Stage = VolumePrim.GetStage();

	FString VolumePrimHashPrefix = UsdUtils::GetAssetHashPrefix(GetPrim(), Context->bReuseIdenticalAssets);

	// Create the info structs from the requested files
	TMap<FString, FSparseVolumeTextureInfo> FilePathHashToTextureInfo = CollectSparseVolumeTextureInfoByFilePath(Volume);

	// Create SVT assets from the info structs
	for (TPair<FString, FSparseVolumeTextureInfo>& FilePathHashToInfo : FilePathHashToTextureInfo)
	{
		const FString& VDBFilePath = FilePathHashToInfo.Value.SourceVDBFilePath;
		FSparseVolumeTextureInfo& ParsedTexture = FilePathHashToInfo.Value;

		if (!FPaths::FileExists(VDBFilePath))
		{
			UE_LOG(
				LogUsd,
				Warning,
				TEXT("Failed to find a VDB file at path '%s' when parsing Volume prim '%s'"),
				*VDBFilePath,
				*VolumePrimPathString
			);
			continue;
		}

		// Here we're going to pick how to map between the grids from the .vdb files (each one is a separate volumetric texture,
		// like "density" or "temperature", etc.) into the SVT texture's 8 attribute channels (AttributesA.RGBA and AttributesB.RGBA),
		// and also pick the attribute channel data types.
		//
		// By default we'll defer to LoadOpenVDBPreviewData which has some heuristics based on the grid names and data types.
		// In practice these .vdb files should only have 1-3 grids each with some common names so the heuristics should hopefully
		// be fine for a sensible result.
		//
		// Users can also add a custom schema to the OpenVDBAsset prims in order to manually control how to map the grids to the SVT
		// attributes, in a similar way to how blendshapes are mapped. We'll check for those in SetVDBImportOptions
		TStrongObjectPtr<UOpenVDBImportOptionsObject> ImportOptions{NewObject<UOpenVDBImportOptionsObject>()};
		LoadOpenVDBPreviewData(VDBFilePath, &ImportOptions->PreviewData);
		SetVDBImportOptions(ParsedTexture, ImportOptions->PreviewData);

		// Collect a hash for this VDB asset
		FSHAHash VDBAndAssignmentHash;
		{
			FSHA1 SHA1;
			HashForSparseVolumeTexture(ImportOptions->PreviewData, SHA1);

			SHA1.Final();
			SHA1.GetHash(&VDBAndAssignmentHash.Hash[0]);
		}
		ParsedTexture.PrefixedAssetHash = VolumePrimHashPrefix + VDBAndAssignmentHash.ToString();

		USparseVolumeTexture* SparseVolumeTexture = Cast<USparseVolumeTexture>(Context->AssetCache->GetCachedAsset(ParsedTexture.PrefixedAssetHash));

		// Need to create a brand new asset
		if (!SparseVolumeTexture)
		{
			TStrongObjectPtr<USparseVolumeTextureFactory> SparseVolumeTextureFactory{NewObject<USparseVolumeTextureFactory>()};

			// We use the asset import task to indicate it's an automated import, and also to transmit our import options
			TStrongObjectPtr<UAssetImportTask> AssetImportTask{NewObject<UAssetImportTask>()};
			AssetImportTask->Filename = VDBFilePath;
			AssetImportTask->bAutomated = true;
			AssetImportTask->bSave = false;
			AssetImportTask->Options = ImportOptions.Get();
			AssetImportTask->Factory = SparseVolumeTextureFactory.Get();

			bool bOperationCanceled = false;
			const TCHAR* Parms = nullptr;
			const FName AssetName = MakeUniqueObjectName(
				GetTransientPackage(),
				USparseVolumeTexture::StaticClass(),
				*IUsdClassesModule::SanitizeObjectName(FPaths::GetBaseFilename(VDBFilePath))	// File path instead of prim path in case we have
																								// multiple .vdb files in the same Volume prim
			);

			// Call FactoryCreateFile directly here or else the usual AssetToolsModule.Get().ImportAssetTasks()
			// workflow would end up creating a package for every asset, which we don't care about since
			// we'll rename them into the asset cache anyway
			SparseVolumeTextureFactory->SetAssetImportTask(AssetImportTask.Get());
			SparseVolumeTexture = Cast<USparseVolumeTexture>(SparseVolumeTextureFactory->FactoryCreateFile(
				USparseVolumeTexture::StaticClass(),
				GetTransientPackage(),
				AssetName,
				Context->ObjectFlags | EObjectFlags::RF_Public | EObjectFlags::RF_Transient,
				VDBFilePath,
				Parms,
				GWarn,
				bOperationCanceled
			));
			if (!SparseVolumeTexture)
			{
				UE_LOG(LogUsd, Error, TEXT("Failed to generate Sparse Volume Texture from OpenVDB file '%s'"), *VDBFilePath);
				return;
			}

			SparseVolumeTexture->PostEditChange();

			if (UStreamableSparseVolumeTexture* StreamableTexture = Cast<UStreamableSparseVolumeTexture>(SparseVolumeTexture))
			{
				// Set an asset import data into the texture as it won't do that on its own, and we would otherwise
				// lose the source .vdb file information downstream
				UUsdAssetImportData* ImportData = NewObject<UUsdAssetImportData>(SparseVolumeTexture);
				ImportData->UpdateFilenameOnly(VDBFilePath);

				StreamableTexture->AssetImportData = ImportData;
			}

			Context->AssetCache->CacheAsset(ParsedTexture.PrefixedAssetHash, SparseVolumeTexture);
		}

		if (SparseVolumeTexture)
		{
			Context->InfoCache->LinkAssetToPrim(PrimPath, SparseVolumeTexture);

			if (UUsdSparseVolumeTextureAssetUserData* UserData = UsdUtils::GetOrCreateAssetUserData<UUsdSparseVolumeTextureAssetUserData>(
					SparseVolumeTexture
				))
			{
				UserData->PrimPaths.AddUnique(VolumePrimPathString);
				UserData->SourceOpenVDBAssetPrimPaths = ParsedTexture.SourceOpenVDBAssetPrimPaths;
				UserData->TimeSamplePaths = ParsedTexture.TimeSamplePaths;
				UserData->TimeSamplePathIndices = ParsedTexture.TimeSamplePathIndices;
				UserData->TimeSamplePathTimeCodes = ParsedTexture.TimeSamplePathTimeCodes;

				if (Context->MetadataOptions.bCollectMetadata)
				{
					UsdToUnreal::ConvertMetadata(
						VolumePrim,
						UserData,
						Context->MetadataOptions.BlockedPrefixFilters,
						Context->MetadataOptions.bInvertFilters,
						Context->MetadataOptions.bCollectFromEntireSubtrees
					);
				}
				else
				{
					UserData->StageIdentifierToMetadata.Remove(UsdToUnreal::ConvertString(Stage->GetRootLayer()->GetIdentifier()));
				}
			}
		}

		ParsedTexture.SparseVolumeTexture = SparseVolumeTexture;
	}

	// Create SVT materials
	// Sparse volume textures are really 3D textures, and our actor essentially has a 3D cube mesh and will
	// draw these textures on the level. There's one step missing: The material to use.
	//
	// By default we'll spawn an instance of a reference material that we ship, that is basically just a simple
	// volume domain material with "add" blend mode, that connects AttributesA.R to the "extinction" material output,
	// and AttributesB.RGB into "albedo".
	//
	// The default material should be enough to get "something to show up", but realistically for the correct look the
	// user would need to set up a custom material. Even more so because these .vdb files can contain grids that are
	// meant to be drawn as level sets, or float values that are meant to go through look-up tables, usually don't have
	// any color, etc.
	//
	// Volume prims are Gprims however, and can have material bindings, which is what we'll fetch below. If this happens
	// to be an UnrealMaterial, we'll try to use it as the SVT material instead of our default. We'll only need to find
	// the correct material parameter to put our SVT assets in, and once again we can use the custom schema to let the
	// user specify the correct material parameter name for each SVT, in case there are more than one option.

	UMaterialInterface* ReferenceMaterial = nullptr;

	// Check to see if the Volume prim has a material binding to an UnrealMaterial we can use
	static FName UnrealRenderContext = *UsdToUnreal::ConvertToken(UnrealIdentifiers::Unreal);
	if (Context->RenderContext == UnrealRenderContext)
	{
		pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
		if (!Context->MaterialPurpose.IsNone())
		{
			MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context->MaterialPurpose.ToString()).Get();
		}

		pxr::UsdShadeMaterialBindingAPI BindingAPI{VolumePrim};
		if (pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial(MaterialPurposeToken))
		{
			if (TOptional<FString> UnrealMaterial = UsdUtils::GetUnrealSurfaceOutput(ShadeMaterial.GetPrim()))
			{
				ReferenceMaterial = Cast<UMaterialInterface>(FSoftObjectPath{UnrealMaterial.GetValue()}.TryLoad());
			}
		}
	}
	// Fall back to the default SVT material instead
	const UUsdProjectSettings* ProjectSettings = nullptr;
	if (!ReferenceMaterial)
	{
		ProjectSettings = GetDefault<UUsdProjectSettings>();
		if (!ProjectSettings)
		{
			return;
		}

		ReferenceMaterial = Cast<UMaterialInterface>(ProjectSettings->ReferenceDefaultSVTMaterial.TryLoad());
	}

	const UMaterial* Material = nullptr;
	if (ReferenceMaterial)
	{
		Material = ReferenceMaterial->GetMaterial();

		// Warn in case the used material can't be used for SVTs
		if (Material && Material->MaterialDomain != EMaterialDomain::MD_Volume)
		{
			UE_LOG(
				LogUsd,
				Warning,
				TEXT("The material '%s' used for the Volume prim '%s' may not be capable of using Sparse Volume Textures as it does not have the "
					 "Volume material domain."),
				*ReferenceMaterial->GetPathName(),
				*VolumePrimPathString
			);
		}
	}
	if (!Material)
	{
		return;
	}

	TMap<FString, FSparseVolumeTextureInfo*> MaterialParameterToTexture = CollectMaterialParameterTextureAssignment(
		VolumePrim,
		Material,
		FilePathHashToTextureInfo
	);

	FSHAHash MaterialHash;
	{
		FSHA1 SHA1;
		HashForVolumetricMaterial(ReferenceMaterial, MaterialParameterToTexture, SHA1);
		if (ProjectSettings)
		{
			FString ReferencePathString = ProjectSettings->ReferenceDefaultSVTMaterial.ToString();
			SHA1.UpdateWithString(*ReferencePathString, ReferencePathString.Len());
		}
		SHA1.Final();
		SHA1.GetHash(&MaterialHash.Hash[0]);
	}
	const FString PrefixedMaterialHash = VolumePrimHashPrefix + MaterialHash.ToString();

	UMaterialInstance* MaterialInstance = nullptr;

	if (Context->AssetCache)
	{
		MaterialInstance = Cast<UMaterialInstance>(Context->AssetCache->GetCachedAsset(PrefixedMaterialHash));
	}

	// Create new material instance
	if (!MaterialInstance)
	{
		const FName InstanceName = MakeUniqueObjectName(
			GetTransientPackage(),
			UMaterialInstance::StaticClass(),
			*IUsdClassesModule::SanitizeObjectName(FPaths::GetBaseFilename(VolumePrimPathString))
		);

		MaterialInstance = InstantiateMaterial(InstanceName, ReferenceMaterial, Context.Get());

		if (MaterialInstance)
		{
			UUsdAssetUserData* UserData = NewObject<UUsdAssetUserData>(MaterialInstance, TEXT("USDAssetUserData"));
			UserData->PrimPaths = {PrimPath.GetString()};
			MaterialInstance->AddAssetUserData(UserData);

			AssignMaterialParameters(MaterialInstance, MaterialParameterToTexture);

			Context->AssetCache->CacheAsset(PrefixedMaterialHash, MaterialInstance);
		}
	}

	if (MaterialInstance)
	{
		Context->InfoCache->LinkAssetToPrim(PrimPath, MaterialInstance);

		if (UUsdAssetUserData* UserData = UsdUtils::GetOrCreateAssetUserData<UUsdAssetUserData>(MaterialInstance))
		{
			UserData->PrimPaths.AddUnique(VolumePrimPathString);

			if (Context->MetadataOptions.bCollectMetadata)
			{
				UsdToUnreal::ConvertMetadata(
					VolumePrim,
					UserData,
					Context->MetadataOptions.BlockedPrefixFilters,
					Context->MetadataOptions.bInvertFilters,
					Context->MetadataOptions.bCollectFromEntireSubtrees
				);
			}
			else
			{
				UserData->StageIdentifierToMetadata.Remove(UsdToUnreal::ConvertString(Stage->GetRootLayer()->GetIdentifier()));
			}
		}
	}
#endif	  // WITH_EDITOR
}

USceneComponent* FUsdVolVolumeTranslator::CreateComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdVolVolumeTranslator::CreateComponents);

	USceneComponent* SceneComponent = nullptr;

#if WITH_EDITOR
	EUsdDrawMode DrawMode = UsdUtils::GetAppliedDrawMode(GetPrim());
	if (DrawMode == EUsdDrawMode::Default)
	{
		if (Context->bAllowParsingSparseVolumeTextures)
		{
			const bool bNeedsActor = true;
			SceneComponent = CreateComponentsEx({UHeterogeneousVolumeComponent::StaticClass()}, bNeedsActor);
		}
	}
	else
	{
		SceneComponent = CreateAlternativeDrawModeComponents(DrawMode);
	}

	UpdateComponents(SceneComponent);
#endif	  // WITH_EDITOR

	return SceneComponent;
}

void FUsdVolVolumeTranslator::UpdateComponents(USceneComponent* SceneComponent)
{
#if WITH_EDITOR
	// Set volumetric material onto the spawned component
	if (UHeterogeneousVolumeComponent* VolumeComponent = Cast<UHeterogeneousVolumeComponent>(SceneComponent))
	{
		const int32 ElementIndex = 0;
		UMaterialInterface* CurrentMaterial = VolumeComponent->GetMaterial(ElementIndex);

		if (UMaterialInstance* MaterialForPrim = Context->InfoCache->GetSingleAssetForPrim<UMaterialInstance>(PrimPath))
		{
			if (MaterialForPrim != CurrentMaterial)
			{
				// We need to call PostLoad here or else it won't render the material properly (reference:
				// SNiagaraVolumeTextureViewport::Construct)
				VolumeComponent->SetMaterial(ElementIndex, MaterialForPrim);
				VolumeComponent->PostLoad();

				CurrentMaterial = MaterialForPrim;
			}
		}

		// Animate first SVT parameter if we have an animated one
		if (CurrentMaterial && !Context->bSequencerIsAnimating)
		{
			TArray<FMaterialParameterInfo> ParameterInfo;
			TArray<FGuid> ParameterIds;
			CurrentMaterial->GetAllSparseVolumeTextureParameterInfo(ParameterInfo, ParameterIds);

			for (const FMaterialParameterInfo& Info : ParameterInfo)
			{
				USparseVolumeTexture* SparseVolumeTexture = nullptr;
				if (CurrentMaterial->GetSparseVolumeTextureParameterValue(Info, SparseVolumeTexture) && SparseVolumeTexture)
				{
					if (SparseVolumeTexture->GetNumFrames() > 1)
					{
						if (UUsdSparseVolumeTextureAssetUserData* UserData = Cast<UUsdSparseVolumeTextureAssetUserData>(
								UsdUtils::GetAssetUserData(SparseVolumeTexture)
							))
						{
							UE::FUsdPrim VolumePrim = GetPrim();
							UE::FUsdStage Stage = VolumePrim.GetStage();

							UE::FUsdPrim PrimForOffsetCalculation = VolumePrim;
							if (UserData->SourceOpenVDBAssetPrimPaths.Num() > 0)
							{
								const FString& FirstAssetPrimPath = UserData->SourceOpenVDBAssetPrimPaths[0];
								UE::FUsdPrim FirstAssetPrim = Stage.GetPrimAtPath(UE::FSdfPath{*FirstAssetPrimPath});
								if (FirstAssetPrim)
								{
									PrimForOffsetCalculation = FirstAssetPrim;
								}
							}

							UE::FSdfLayerOffset CombinedOffset = UsdUtils::GetPrimToStageOffset(PrimForOffsetCalculation);
							const double LayerTimeCode = ((Context->Time - CombinedOffset.Offset) / CombinedOffset.Scale);

							// The SVTs will have all the volume frames packed next to each other with no time information,
							// and are indexed by a "frame index" where 0 is the first frame and N-1 is the last frame.
							// These is also no linear interpolation: The frame index is basically floor()'d and the integer
							// value is used as the index into the Frames array
							int32 TargetIndex = 0;
							for (; TargetIndex + 1 < UserData->TimeSamplePathTimeCodes.Num(); ++TargetIndex)
							{
								if (UserData->TimeSamplePathTimeCodes[TargetIndex + 1] > LayerTimeCode)
								{
									break;
								}
							}
							TargetIndex = FMath::Clamp(TargetIndex, 0, UserData->TimeSamplePathTimeCodes.Num() - 1);

							// At this point TargetIndex points at the index of the biggest timeCode that is
							// still <= LayerTimeCode. We may have an index mapping though, like when the bRemoveDuplicates
							// cvar is true

							if (UserData->TimeSamplePathIndices.IsValidIndex(TargetIndex))
							{
								TargetIndex = UserData->TimeSamplePathIndices[TargetIndex];
							}

							// Now TargetIndex should be pointing at the index of the desired frame within the SVT

							const bool bPlaying = false;
							VolumeComponent->SetPlaying(bPlaying);
							VolumeComponent->SetFrame(static_cast<float>(TargetIndex));
						}
					}
				}

				break;
			}
		}
	}
#endif	  // WITH_EDITOR

	Super::UpdateComponents(SceneComponent);
}

bool FUsdVolVolumeTranslator::CollapsesChildren(ECollapsingType CollapsingType) const
{
	// If we have a custom draw mode, it means we should draw bounds/cards/etc. instead
	// of our entire subtree, which is basically the same thing as collapsing
	EUsdDrawMode DrawMode = UsdUtils::GetAppliedDrawMode(GetPrim());
	if (DrawMode != EUsdDrawMode::Default)
	{
		return true;
	}

	return false;
}

bool FUsdVolVolumeTranslator::CanBeCollapsed(ECollapsingType CollapsingType) const
{
	return false;
}

TSet<UE::FSdfPath> FUsdVolVolumeTranslator::CollectAuxiliaryPrims() const
{
	if (!Context->bIsBuildingInfoCache)
	{
		return Context->InfoCache->GetAuxiliaryPrims(PrimPath);
	}

	TSet<UE::FSdfPath> Result;
#if WITH_EDITOR
	{
		FScopedUsdAllocs UsdAllocs;

		if (pxr::UsdVolVolume Volume{GetPrim()})
		{
			const std::map<pxr::TfToken, pxr::SdfPath>& FieldMap = Volume.GetFieldPaths();
			for (std::map<pxr::TfToken, pxr::SdfPath>::const_iterator Iter = FieldMap.cbegin(); Iter != FieldMap.end(); ++Iter)
			{
				const pxr::SdfPath& AssetPrimPath = Iter->second;

				Result.Add(UE::FSdfPath{*UsdToUnreal::ConvertPath(AssetPrimPath)});
			}
		}
	}
#endif	  // WITH_EDITOR
	return Result;
}

#endif	  // #if USE_USD_SDK
