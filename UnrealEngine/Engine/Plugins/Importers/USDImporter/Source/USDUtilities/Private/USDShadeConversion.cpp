// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDShadeConversion.h"

#if USE_USD_SDK

#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDAttributeUtils.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPrim.h"

#include "Algo/AllOf.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#include "RHI.h"

#if WITH_EDITOR
	#include "Factories/TextureFactory.h"
	#include "IMaterialBakingModule.h"
	#include "MaterialBakingStructures.h"
	#include "MaterialEditingLibrary.h"
	#include "MaterialOptions.h"
	#include "MaterialUtilities.h"
	#include "ObjectTools.h"
#endif // WITH_EDITOR

#include "USDIncludesStart.h"
	#include "pxr/base/tf/token.h"
	#include "pxr/usd/ar/ar.h"
	#include "pxr/usd/ar/asset.h"
	#include "pxr/usd/ar/resolvedPath.h"
	#include "pxr/usd/ar/resolver.h"
	#include "pxr/usd/ar/resolverScopedCache.h"
	#include "pxr/usd/sdf/layer.h"
	#include "pxr/usd/sdf/layerUtils.h"
	#include "pxr/usd/usdShade/material.h"
#include "USDIncludesEnd.h"

// Required for packaging
class UTextureFactory;

#define LOCTEXT_NAMESPACE "USDShadeConversion"

namespace UE
{
	namespace UsdShadeConversion
	{
		namespace Private
		{
			// Temporarily here as we header files can't be modified on patch versions (5.1.1)
			const static pxr::TfToken RawColorSpaceToken = pxr::TfToken{"raw"};
			const static pxr::TfToken SRGBColorSpaceToken = pxr::TfToken{"sRGB"};
			const static pxr::TfToken SourceColorSpaceToken = pxr::TfToken{"sourceColorSpace"};

#if WITH_EDITOR
			using FlattenToMatEntry = TPairInitializer<const EFlattenMaterialProperties&, const EMaterialProperty&>;
			const static TMap<EFlattenMaterialProperties, EMaterialProperty> FlattenToMaterialProperty
			({
				FlattenToMatEntry( EFlattenMaterialProperties::Diffuse, EMaterialProperty::MP_BaseColor ),
				FlattenToMatEntry( EFlattenMaterialProperties::Metallic, EMaterialProperty::MP_Metallic ),
				FlattenToMatEntry( EFlattenMaterialProperties::Specular, EMaterialProperty::MP_Specular ),
				FlattenToMatEntry( EFlattenMaterialProperties::Roughness, EMaterialProperty::MP_Roughness ),
				FlattenToMatEntry( EFlattenMaterialProperties::Anisotropy, EMaterialProperty::MP_Anisotropy ),
				FlattenToMatEntry( EFlattenMaterialProperties::Normal, EMaterialProperty::MP_Normal ),
				FlattenToMatEntry( EFlattenMaterialProperties::Tangent, EMaterialProperty::MP_Tangent ),
				FlattenToMatEntry( EFlattenMaterialProperties::Opacity, EMaterialProperty::MP_Opacity ),
				FlattenToMatEntry( EFlattenMaterialProperties::Emissive, EMaterialProperty::MP_EmissiveColor ),
				FlattenToMatEntry( EFlattenMaterialProperties::SubSurface, EMaterialProperty::MP_SubsurfaceColor ),
				FlattenToMatEntry( EFlattenMaterialProperties::OpacityMask, EMaterialProperty::MP_OpacityMask ),
				FlattenToMatEntry( EFlattenMaterialProperties::AmbientOcclusion, EMaterialProperty::MP_AmbientOcclusion ),
			});

			/** Simple wrapper around the FBakeOutput data that we can reuse for data coming in from a FFlattenMaterial */
			struct FBakedMaterialView
			{
				TMap<EMaterialProperty, TArray<FColor>*> PropertyData;
				TMap<EMaterialProperty, TArray<FFloat16Color>*> HDRPropertyData;
				TMap<EMaterialProperty, FIntPoint> PropertySizes;

				explicit FBakedMaterialView( FBakeOutput& BakeOutput )
					: PropertySizes( BakeOutput.PropertySizes )
				{
					HDRPropertyData.Reserve( BakeOutput.HDRPropertyData.Num() );
					for ( TPair<EMaterialProperty, TArray<FFloat16Color>>& BakedPair : BakeOutput.HDRPropertyData )
					{
						HDRPropertyData.Add( BakedPair.Key, &BakedPair.Value );
					}

					PropertyData.Reserve( BakeOutput.PropertyData.Num() );
					for ( TPair<EMaterialProperty, TArray<FColor>>& BakedPair : BakeOutput.PropertyData )
					{
						PropertyData.Add( BakedPair.Key, &BakedPair.Value );
					}
				}

				explicit FBakedMaterialView( FFlattenMaterial& FlattenMaterial )
				{
					PropertyData.Reserve( static_cast< uint8 >( EFlattenMaterialProperties::NumFlattenMaterialProperties ) );
					for ( const TPair< EFlattenMaterialProperties, EMaterialProperty>& FlattenToProperty : FlattenToMaterialProperty )
					{
						PropertyData.Add( FlattenToProperty.Value, &FlattenMaterial.GetPropertySamples( FlattenToProperty.Key ) );
					}

					PropertySizes.Reserve( static_cast< uint8 >( EFlattenMaterialProperties::NumFlattenMaterialProperties ) );
					for ( const TPair< EFlattenMaterialProperties, EMaterialProperty>& FlattenToProperty : FlattenToMaterialProperty )
					{
						PropertySizes.Add( FlattenToProperty.Value, FlattenMaterial.GetPropertySize( FlattenToProperty.Key ) );
					}
				}
			};
#endif // WITH_EDITOR

			// Given an AssetPath, resolve it to an actual file path
			FString ResolveAssetPath( const pxr::SdfLayerHandle& LayerHandle, const FString& AssetPath )
			{
				FScopedUsdAllocs UsdAllocs;

				FString AssetPathToResolve = AssetPath;
				AssetPathToResolve.ReplaceInline( TEXT("<UDIM>"), TEXT("1001") ); // If it's a UDIM path, we replace the UDIM tag with the start tile

				pxr::ArResolverScopedCache ResolverCache;
				pxr::ArResolver& Resolver = pxr::ArGetResolver();

				const FString RelativePathToResolve =
					LayerHandle
					? UsdToUnreal::ConvertString( pxr::SdfComputeAssetPathRelativeToLayer( LayerHandle, UnrealToUsd::ConvertString( *AssetPathToResolve ).Get() ) )
					: AssetPathToResolve;

				std::string AssetIdentifier = Resolver.Resolve( UnrealToUsd::ConvertString( *RelativePathToResolve ).Get().c_str() );

				// Don't normalize an empty path as the result will be "."
				if ( AssetIdentifier.size() > 0 )
				{
					AssetIdentifier = Resolver.CreateIdentifier( AssetIdentifier );
				}

				FString ResolvedAssetPath = UsdToUnreal::ConvertString( AssetIdentifier );

				return ResolvedAssetPath;
			}

			// If ResolvedTexturePath is in a format like "C:/MyFiles/scene.usdz[0/texture.png]", this will place "png" in OutExtensionNoDot, and return true
			bool IsInsideUsdzArchive(const FString& ResolvedTexturePath, FString& OutExtensionNoDot)
			{
				// We need at least an opening and closing bracket
				if ( ResolvedTexturePath.Len() < 3 )
				{
					return false;
				}

				int32 OpenBracketPos = ResolvedTexturePath.Find( TEXT( "[" ), ESearchCase::IgnoreCase, ESearchDir::FromEnd );
				int32 CloseBracketPos = ResolvedTexturePath.Find( TEXT( "]" ), ESearchCase::IgnoreCase, ESearchDir::FromEnd );
				if ( OpenBracketPos != INDEX_NONE && CloseBracketPos != INDEX_NONE && CloseBracketPos > OpenBracketPos - 1 )
				{
					// Should be like "C:/MyFiles/scene.usdz"
					FString UsdzFilePath = ResolvedTexturePath.Left( OpenBracketPos );

					// Should be like "0/texture.png"
					FString TextureInUsdzPath = ResolvedTexturePath.Mid( OpenBracketPos + 1, CloseBracketPos - OpenBracketPos - 1 );

					if ( FPaths::GetExtension( UsdzFilePath ).ToLower() == TEXT( "usdz" ) )
					{
						// Should be something like "png"
						OutExtensionNoDot = FPaths::GetExtension(TextureInUsdzPath);
						return true;
					}
				}

				return false;
			}

			TUsdStore<std::shared_ptr<const char>> ReadTextureBufferFromUsdzArchive( const FString& ResolvedTexturePath, uint64& OutBufferSize )
			{
				pxr::ArResolver& Resolver = pxr::ArGetResolver();
				std::shared_ptr<pxr::ArAsset> Asset = Resolver.OpenAsset(
					pxr::ArResolvedPath( UnrealToUsd::ConvertString( *ResolvedTexturePath ).Get() ) );

				TUsdStore<std::shared_ptr<const char>> Buffer;

				if ( Asset )
				{
					OutBufferSize = static_cast< uint64 >( Asset->GetSize() );
					{
						FScopedUsdAllocs Allocs;
						Buffer = Asset->GetBuffer();
					}
				}

				return Buffer;
			}

			/** If ResolvedTexturePath points at a texture inside an usdz file, this will use USD to pull the asset from the file, and TextureFactory to import it directly from the binary buffer */
			UTexture* ReadTextureFromUsdzArchiveEditor(const FString& ResolvedTexturePath, const FString& TextureExtension, UTextureFactory* TextureFactory, UObject* Outer, EObjectFlags ObjectFlags )
			{
#if WITH_EDITOR
				uint64 BufferSize = 0;
				TUsdStore<std::shared_ptr<const char>> Buffer = ReadTextureBufferFromUsdzArchive(ResolvedTexturePath, BufferSize);
				const uint8* BufferStart = reinterpret_cast< const uint8* >( Buffer.Get().get() );

				if ( BufferSize == 0 || BufferStart == nullptr )
				{
					return nullptr;
				}

				FScopedUnrealAllocs UEAllocs;

				FName TextureName = MakeUniqueObjectName( Outer, UTexture::StaticClass(), *FPaths::GetBaseFilename( ResolvedTexturePath ) );

				return Cast<UTexture>(TextureFactory->FactoryCreateBinary(
					UTexture::StaticClass(),
					Outer,
					TextureName,
					ObjectFlags,
					nullptr,
					*TextureExtension,
					BufferStart,
					BufferStart + BufferSize,
					nullptr));
#endif // WITH_EDITOR
				return nullptr;
			}

			UTexture* ReadTextureFromUsdzArchiveRuntime( const FString& ResolvedTexturePath )
			{
				uint64 BufferSize = 0;
				TUsdStore<std::shared_ptr<const char>> Buffer = ReadTextureBufferFromUsdzArchive( ResolvedTexturePath, BufferSize );
				const uint8* BufferStart = reinterpret_cast< const uint8* >( Buffer.Get().get() );

				if ( BufferSize == 0 || BufferStart == nullptr )
				{
					return nullptr;
				}

				FScopedUnrealAllocs UEAllocs;

				// Copied from FImageUtils::ImportBufferAsTexture2D( Buffer ) so that we can avoid a copy into the TArray<uint8> that it takes as parameter
				IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>( TEXT( "ImageWrapper" ) );
				EImageFormat Format = ImageWrapperModule.DetectImageFormat( BufferStart, BufferSize );

				UTexture2D* NewTexture = nullptr;
				EPixelFormat PixelFormat = PF_Unknown;
				if ( Format != EImageFormat::Invalid )
				{
					TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( Format );

					int32 BitDepth = 0;
					int32 Width = 0;
					int32 Height = 0;

					if ( ImageWrapper->SetCompressed( ( void* ) BufferStart, BufferSize ) )
					{
						PixelFormat = PF_Unknown;

						ERGBFormat RGBFormat = ERGBFormat::Invalid;

						BitDepth = ImageWrapper->GetBitDepth();

						Width = ImageWrapper->GetWidth();
						Height = ImageWrapper->GetHeight();

						if ( BitDepth == 16 )
						{
							PixelFormat = PF_FloatRGBA;
							RGBFormat = ERGBFormat::BGRA;
						}
						else if ( BitDepth == 8 )
						{
							PixelFormat = PF_B8G8R8A8;
							RGBFormat = ERGBFormat::BGRA;
						}
						else
						{
							UE_LOG( LogUsd, Warning, TEXT( "Error creating texture. Bit depth is unsupported. (%d)" ), BitDepth );
							return nullptr;
						}

						TArray64<uint8> UncompressedData;
						ImageWrapper->GetRaw( RGBFormat, BitDepth, UncompressedData );

						NewTexture = UTexture2D::CreateTransient( Width, Height, PixelFormat );
						if ( NewTexture )
						{
							NewTexture->bNotOfflineProcessed = true;
							uint8* MipData = static_cast< uint8* >( NewTexture->GetPlatformData()->Mips[ 0 ].BulkData.Lock( LOCK_READ_WRITE ) );

							// Bulk data was already allocated for the correct size when we called CreateTransient above
							FMemory::Memcpy( MipData, UncompressedData.GetData(), NewTexture->GetPlatformData()->Mips[ 0 ].BulkData.GetBulkDataSize() );

							NewTexture->GetPlatformData()->Mips[ 0 ].BulkData.Unlock();

							NewTexture->UpdateResource();
						}
					}
				}
				else
				{
					UE_LOG( LogUsd, Warning, TEXT( "Error creating texture. Couldn't determine the file format" ) );
				}

				return NewTexture;
			}

			// Computes and returns the hash string for the texture at the given path.
			// Handles regular texture asset paths as well as asset paths identifying textures inside Usdz archives.
			// Returns an empty string if the texture could not be hashed.
			FString GetTextureHash(
				const FString& ResolvedTexturePath,
				bool bSRGB,
				TextureCompressionSettings CompressionSettings,
				TextureAddress AddressX,
				TextureAddress AddressY
			)
			{
				FMD5 MD5;

				// Hash the actual texture
				FString TextureExtension;
				if (IsInsideUsdzArchive(ResolvedTexturePath, TextureExtension))
				{
					uint64 BufferSize = 0;
					TUsdStore<std::shared_ptr<const char>> Buffer = ReadTextureBufferFromUsdzArchive(ResolvedTexturePath, BufferSize);
					const uint8* BufferStart = reinterpret_cast<const uint8*>(Buffer.Get().get());

					if (BufferSize > 0 && BufferStart != nullptr)
					{
						MD5.Update( BufferStart, BufferSize );
					}
				}
				// Copied from FMD5Hash::HashFileFromArchive as it doesn't expose its FMD5
				else if ( TUniquePtr<FArchive> Ar{ IFileManager::Get().CreateFileReader( *ResolvedTexturePath ) } )
				{
					TArray<uint8> LocalScratch;
					LocalScratch.SetNumUninitialized( 1024 * 64 );

					const int64 Size = Ar->TotalSize();
					int64 Position = 0;

					// Read in BufferSize chunks
					while ( Position < Size )
					{
						const auto ReadNum = FMath::Min( Size - Position, ( int64 ) LocalScratch.Num() );
						Ar->Serialize( LocalScratch.GetData(), ReadNum );
						MD5.Update( LocalScratch.GetData(), ReadNum );

						Position += ReadNum;
					}
				}
				else
				{
					UE_LOG( LogUsd, Warning, TEXT( "Failed to find texture at path '%s' when trying to generate a hash for it" ), *ResolvedTexturePath );
				}

				// Hash the additional data
				MD5.Update( reinterpret_cast< uint8* >( &bSRGB ), sizeof( bool ) );
				MD5.Update( reinterpret_cast< uint8* >( &CompressionSettings ), sizeof( CompressionSettings ) );
				MD5.Update( reinterpret_cast< uint8* >( &AddressX ), sizeof( AddressX ) );
				MD5.Update( reinterpret_cast< uint8* >( &AddressY ), sizeof( AddressY ) );

				FMD5Hash Hash;
				Hash.Set( MD5 );
				return LexToString( Hash );
			}

			// Will traverse the shade material graph backwards looking for a string/token value and return it.
			// Returns the empty string if it didn't find anything.
			FString RecursivelySearchForStringValue( pxr::UsdShadeInput Input )
			{
				if ( !Input )
				{
					return {};
				}

				FScopedUsdAllocs Allocs;

				if ( Input.HasConnectedSource() )
				{
					pxr::UsdShadeConnectableAPI Souce;
					pxr::TfToken SourceName;
					pxr::UsdShadeAttributeType SourceType;
					if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( Input.GetAttr(), &Souce, &SourceName, &SourceType ) )
					{
						for ( const pxr::UsdShadeInput& DeeperInput : Souce.GetInputs() )
						{
							FString Result = RecursivelySearchForStringValue( DeeperInput );
							if ( !Result.IsEmpty() )
							{
								return Result;
							}
						}
					}
				}
				else
				{
					std::string StringValue;
					if ( Input.Get< std::string >( &StringValue ) )
					{
						return UsdToUnreal::ConvertString( StringValue );
					}

					pxr::TfToken TokenValue;
					if ( Input.Get< pxr::TfToken >( &TokenValue ) )
					{
						return UsdToUnreal::ConvertToken( TokenValue );
					}
				}

				return {};
			}

			/** Receives a UsdUVTexture shader, and returns the name of the primvar that it is using as 'st' */
			FString GetPrimvarUsedAsST( pxr::UsdShadeConnectableAPI& UsdUVTexture )
			{
				FScopedUsdAllocs Allocs;

				pxr::UsdShadeInput StInput = UsdUVTexture.GetInput( UnrealIdentifiers::St );
				if ( !StInput )
				{
					return {};
				}

				pxr::UsdShadeConnectableAPI PrimvarReader;
				pxr::TfToken PrimvarReaderId;
				pxr::UsdShadeAttributeType AttributeType;

				// Connected to a PrimvarReader
				if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( StInput.GetAttr(), &PrimvarReader, &PrimvarReaderId, &AttributeType ) )
				{
					if ( pxr::UsdShadeInput VarnameInput = PrimvarReader.GetInput( UnrealIdentifiers::Varname ) )
					{
						// PrimvarReader may have a string literal with the primvar name, or it may be connected to
						// e.g. an attribute defined elsewhere
						return RecursivelySearchForStringValue( VarnameInput );
					}
				}

				return {};
			}

			void CheckForUVIndexConflicts( TMap<FString, int32>& PrimvarToUVIndex, const FString& ShadeMaterialPath )
			{
				TMap<int32, TArray<FString>> InvertedMap;
				for ( const TPair< FString, int32 >& PrimvarAndUVIndex : PrimvarToUVIndex )
				{
					InvertedMap.FindOrAdd( PrimvarAndUVIndex.Value ).Add( PrimvarAndUVIndex.Key );
				}

				for ( const TPair< int32, TArray<FString> >& UVIndexAndPrimvars : InvertedMap )
				{
					const TArray<FString>& Primvars = UVIndexAndPrimvars.Value;
					if ( Primvars.Num() > 1 )
					{
						FString PrimvarNames = FString::Join(Primvars, TEXT(", "));
						UE_LOG(LogUsd, Warning, TEXT("For shade material '%s', multiple primvars (%s) target the same UV index '%d'"), *ShadeMaterialPath, *PrimvarNames, UVIndexAndPrimvars.Key);
					}
				}
			}

			struct FTextureParameterValue
			{
				UTexture* Texture;
				int32 UVIndex;
				int32 OutputIndex = 0;
			};
			struct FPrimvarReaderParameterValue
			{
				std::string PrimvarName;
				FVector FallbackValue;
			};
			using FParameterValue = TVariant<float, FVector, FTextureParameterValue, FPrimvarReaderParameterValue, bool>;

			bool GetTextureParameterValue( pxr::UsdShadeInput& ShadeInput, TextureGroup LODGroup, FParameterValue& OutValue,
				UMaterialInterface* Material = nullptr, UUsdAssetCache* TexturesCache = nullptr, TMap<FString, int32>* PrimvarToUVIndex = nullptr )
			{
				FScopedUsdAllocs UsdAllocs;

				const bool bIsNormalInput = ( ShadeInput.GetTypeName() == pxr::SdfValueTypeNames->Normal3f
					|| ShadeInput.GetTypeName() == pxr::SdfValueTypeNames->Normal3fArray
					|| ShadeInput.GetBaseName() == UnrealIdentifiers::Normal
				);

				pxr::UsdShadeConnectableAPI UsdUVTextureSource;
				pxr::TfToken UsdUVTextureSourceName;
				pxr::UsdShadeAttributeType UsdUVTextureAttributeType;

				// Clear it, as we'll signal that it has a valid texture bound by setting it with an FTextureParameterValue
				OutValue.Set<float>(0.0f);

				if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( ShadeInput.GetAttr(), &UsdUVTextureSource, &UsdUVTextureSourceName, &UsdUVTextureAttributeType ) )
				{
					pxr::UsdShadeInput FileInput;

					// UsdUVTexture: Get its file input
					if ( UsdUVTextureAttributeType == pxr::UsdShadeAttributeType::Output )
					{
						FileInput = UsdUVTextureSource.GetInput( UnrealIdentifiers::File );
					}
					// Check if we are being directly passed an asset
					else
					{
						FileInput = UsdUVTextureSource.GetInput( UsdUVTextureSourceName );
					}

					// Recursively traverse "inputs:file" connections until we stop finding other connected prims
					pxr::UsdShadeConnectableAPI TextureFileSource;
					pxr::TfToken TextureFileSourceName;
					pxr::UsdShadeAttributeType TextureFileAttributeType;
					while ( FileInput )
					{
						if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( FileInput.GetAttr(), &TextureFileSource, &TextureFileSourceName, &TextureFileAttributeType ) )
						{
							// Another connection, get its file input
							if ( TextureFileAttributeType == pxr::UsdShadeAttributeType::Output )
							{
								FileInput = TextureFileSource.GetInput( UnrealIdentifiers::File );
							}
							// Check if we are being directly passed an asset
							else
							{
								FileInput = TextureFileSource.GetInput( TextureFileSourceName );
							}
						}
						else
						{
							break;
						}
					}

					// Get desired texture wrapping modes
					TextureAddress AddressX = TextureAddress::TA_Wrap;
					TextureAddress AddressY = TextureAddress::TA_Wrap;
					if ( pxr::UsdAttribute WrapSAttr = UsdUVTextureSource.GetInput( UnrealIdentifiers::WrapS ) )
					{
						pxr::TfToken WrapS;
						if ( WrapSAttr.Get( &WrapS ) )
						{
							AddressX = WrapS == UnrealIdentifiers::Repeat
								? TextureAddress::TA_Wrap
								: WrapS == UnrealIdentifiers::Mirror
									? TextureAddress::TA_Mirror
									: TextureAddress::TA_Clamp;  // We also consider the "black" wrap mode as clamp
																 // as that is the closest we can get
						}
					}
					if ( pxr::UsdAttribute WrapTAttr = UsdUVTextureSource.GetInput( UnrealIdentifiers::WrapT ) )
					{
						pxr::TfToken WrapT;
						if ( WrapTAttr.Get( &WrapT ) )
						{
							AddressY = WrapT == UnrealIdentifiers::Repeat
								? TextureAddress::TA_Wrap
								: WrapT == UnrealIdentifiers::Mirror
									? TextureAddress::TA_Mirror
									: TextureAddress::TA_Clamp;  // We also consider the "black" wrap mode as clamp
																 // as that is the closest we can get
						}
					}

					if ( FileInput && FileInput.GetTypeName() == pxr::SdfValueTypeNames->Asset ) // Check that FileInput is of type Asset
					{
						const FString TexturePath = UsdUtils::GetResolvedTexturePath( FileInput.GetAttr() );
						// We will assume the texture is valid, and show a warning if we fail to parse this later.
						// Note that we don't even check that the file exists: If we have a texture bound to opacity then we
						// assume this material is meant to be translucent, even if the texture path is invalid (or points inside an USDZ archive)
						if ( !TexturePath.IsEmpty() )
						{
							OutValue.Set<FTextureParameterValue>( FTextureParameterValue{} );
						}
						else
						{
							return false;
						}

						// We'll add these to the hash because the materials are built to try and reuse the same textures for multiple channels,
						// and those may expect linear or sRGB values. Without this we may parse a texture as linear because we hit the opacity channel first,
						// and then reuse it as linear for the base color channel even though it should have been sRGB.
						// Plus we may have something weird like a normal map being plugged into the base color and the normal channel.
						bool bSRGB = true;
						TextureCompressionSettings CompressionSettings = TextureCompressionSettings::TC_Default;
						if ( bIsNormalInput )
						{
							// Disable SRGB when parsing float textures, as they're likely specular/roughness maps
							bSRGB = false;
							CompressionSettings = TextureCompressionSettings::TC_Normalmap;
						}
						else
						{
							if ( LODGroup == TEXTUREGROUP_WorldSpecular )
							{
								bSRGB = false;
							}

							// Override the sRGB flag with whatever is specified on the texture shader, if it has anything specific.
							if ( pxr::UsdAttribute SourceColorSpaceAttr = UsdUVTextureSource.GetInput( SourceColorSpaceToken ) )
							{
								pxr::TfToken SourceColorSpaceValue;
								if ( SourceColorSpaceAttr.HasAuthoredValue() && SourceColorSpaceAttr.Get( &SourceColorSpaceValue ) )
								{
									bSRGB = SourceColorSpaceValue == RawColorSpaceToken
										? false
										: SourceColorSpaceValue == SRGBColorSpaceToken
											? true
											: bSRGB;
								}
							}
						}

						const FString TextureHash = GetTextureHash( TexturePath, bSRGB, CompressionSettings, AddressX, AddressY );

						// We only actually want to retrieve the textures if we have a cache to put them in
						if ( TexturesCache )
						{
							UTexture* Texture = Cast< UTexture >( TexturesCache->GetCachedAsset( TextureHash ) );
							if ( !Texture )
							{
								// Give the same prim path to the texture, so that it ends up imported right next to the material
								FString MaterialPrimPath;
#if WITH_EDITOR
								if ( Material )
								{
									if ( UUsdAssetImportData* ImportData = Cast< UUsdAssetImportData >( Material->AssetImportData ) )
									{
										MaterialPrimPath = ImportData->PrimPath;
									}
								}
#endif // WITH_EDITOR

								Texture = UsdUtils::CreateTexture( FileInput.GetAttr(), MaterialPrimPath, LODGroup, TexturesCache );

								if ( Texture )
								{
									Texture->SRGB = bSRGB;
									Texture->CompressionSettings = CompressionSettings;
									if ( UTexture2D* Texture2D = Cast<UTexture2D>( Texture ) )
									{
										Texture2D->AddressX = AddressX;
										Texture2D->AddressY = AddressY;
									}
									Texture->UpdateResource();

									TexturesCache->CacheAsset( TextureHash, Texture );
								}
							}

							if ( Texture )
							{
								FString PrimvarName = GetPrimvarUsedAsST( UsdUVTextureSource );
								int32 UVIndex = 0;
								if ( !PrimvarName.IsEmpty() && PrimvarToUVIndex )
								{
									UVIndex = UsdUtils::GetPrimvarUVIndex(PrimvarName);
									PrimvarToUVIndex->Add(PrimvarName, UVIndex);
								}
								else if ( PrimvarToUVIndex )
								{
									FUsdLogManager::LogMessage(
										EMessageSeverity::Warning,
										FText::Format( LOCTEXT( "FailedToParsePrimVar", "Failed to find primvar used as st input for texture '{0}' in material '{1}'. Will use UV index 0 instead" ),
											FText::FromString( TexturePath ),
											FText::FromString( Material ? *Material->GetName() : TEXT( "nullptr" ) )
										)
									);
								}

								FTextureParameterValue OutTextureValue;
								OutTextureValue.Texture = Texture;
								OutTextureValue.UVIndex = UVIndex;

								if ( UsdUVTextureAttributeType == pxr::UsdShadeAttributeType::Output )
								{
									const FName OutputName( UsdToUnreal::ConvertToken( UsdUVTextureSourceName ) );

									if ( OutputName == TEXT("rgb") )
									{
										OutTextureValue.OutputIndex = 0;
									}
									else if ( OutputName == TEXT("r") )
									{
										OutTextureValue.OutputIndex = 1;
									}
									else if ( OutputName == TEXT("g") )
									{
										OutTextureValue.OutputIndex = 2;
									}
									else if ( OutputName == TEXT("b") )
									{
										OutTextureValue.OutputIndex = 3;
									}
									else if ( OutputName == TEXT("a") )
									{
										OutTextureValue.OutputIndex = 4;
									}
								}

								OutValue.Set<FTextureParameterValue>( OutTextureValue );
							}
							else
							{
								FUsdLogManager::LogMessage(
									EMessageSeverity::Warning,
									FText::Format( LOCTEXT( "FailedToParseTexture", "Failed to parse texture at path '{0}'" ),
										FText::FromString( TexturePath )
									)
								);
							}
						}
					}
				}

				// We may be calling this from IsMaterialTranslucent, when we have no TexturesCache: We wouldn't be able to import textures without the
				// cache as we may repeatedly parse the same texture.
				// Because of this, and how we clear OutValue to float before we begin, we can say that if we have a FTextureParameterValue at all, then there
				// is a valid texture that *can* be parsed, and so we return true.
				return OutValue.IsType<FTextureParameterValue>();
			}

			bool GetFloatParameterValue( pxr::UsdShadeConnectableAPI& Connectable, const pxr::TfToken& InputName, float DefaultValue, FParameterValue& OutValue,
				UMaterialInterface* Material = nullptr, UUsdAssetCache* TexturesCache = nullptr, TMap<FString, int32>* PrimvarToUVIndex = nullptr)
			{
				FScopedUsdAllocs Allocs;

				pxr::UsdShadeInput Input = Connectable.GetInput( InputName );
				if ( !Input )
				{
					return false;
				}

				// If we have another shader/node connected
				pxr::UsdShadeConnectableAPI Source;
				pxr::TfToken SourceName;
				pxr::UsdShadeAttributeType AttributeType;
				if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( Input.GetAttr(), &Source, &SourceName, &AttributeType ) )
				{
					if ( !GetTextureParameterValue( Input, TEXTUREGROUP_WorldSpecular, OutValue, Material, TexturesCache, PrimvarToUVIndex ) )
					{
						// Check if we have a fallback input that we can use instead, since we don't have a valid texture value
						if ( const pxr::UsdShadeInput FallbackInput = Source.GetInput( UnrealIdentifiers::Fallback ) )
						{
							float UsdFallbackFloat;
							if ( FallbackInput.Get<float>( &UsdFallbackFloat ) )
							{
								OutValue.Set<float>( UsdFallbackFloat );
								return true;
							}
						}

						// Recurse because the attribute may just be pointing at some other attribute that has the data
						// (e.g. when shader input is just "hoisted" and connected to the parent material input)
						return GetFloatParameterValue( Source, SourceName, DefaultValue, OutValue, Material, TexturesCache, PrimvarToUVIndex );
					}
				}
				// No other node connected, so we must have some value
				else
				{
					float InputValue = DefaultValue;
					Input.Get< float >( &InputValue );

					OutValue.Set<float>( InputValue );
				}

				return true;
			}

			bool GetPrimvarReaderParameterValue( const pxr::UsdShadeInput& Input, const pxr::TfToken& PrimvarReaderShaderId, const FLinearColor& DefaultValue, FParameterValue& OutValue )
			{
				FScopedUsdAllocs Allocs;

				if ( !Input )
				{
					return false;
				}

				const bool bShaderOutputsOnly = true;
				const pxr::UsdShadeAttributeVector ValProdAttrs = Input.GetValueProducingAttributes( bShaderOutputsOnly );
				for ( const pxr::UsdAttribute& ValProdAttr : ValProdAttrs )
				{
					const pxr::UsdShadeShader ValProdShader( ValProdAttr.GetPrim() );
					if ( !ValProdShader )
					{
						continue;
					}

					pxr::TfToken ShaderId;
					if ( !ValProdShader.GetShaderId( &ShaderId ) || ShaderId != PrimvarReaderShaderId )
					{
						continue;
					}

					const pxr::UsdShadeInput VarnameInput = ValProdShader.GetInput( UnrealIdentifiers::Varname );
					if ( !VarnameInput )
					{
						continue;
					}

					// The schema for UsdPrimvarReader specifies that the "varname" input should be
					// string-typed, but some assets might be set up token-typed, so we'll consider
					// either type.
					std::string PrimvarName;
					if ( VarnameInput.GetTypeName() == pxr::SdfValueTypeNames->String )
					{
						if ( !VarnameInput.Get( &PrimvarName ) )
						{
							continue;
						}
					}
					else if ( VarnameInput.GetTypeName() == pxr::SdfValueTypeNames->Token )
					{
						pxr::TfToken PrimvarNameToken;
						if ( !VarnameInput.Get( &PrimvarNameToken ) )
						{
							continue;
						}
						PrimvarName = PrimvarNameToken.GetString();
					}

					if ( PrimvarName.empty() )
					{
						continue;
					}

					FLinearColor FallbackColor = DefaultValue;
					const pxr::UsdShadeInput FallbackInput = ValProdShader.GetInput( UnrealIdentifiers::Fallback );
					pxr::GfVec3f UsdFallbackColor;
					if ( FallbackInput && FallbackInput.Get<pxr::GfVec3f>( &UsdFallbackColor ) )
					{
						FallbackColor = UsdToUnreal::ConvertColor( UsdFallbackColor );
					}

					OutValue.Set<FPrimvarReaderParameterValue>( FPrimvarReaderParameterValue{ PrimvarName, FVector(FallbackColor) } );
					return true;
				}

				return false;
			}

			bool GetVec3ParameterValue( pxr::UsdShadeConnectableAPI& Connectable, const pxr::TfToken& InputName, const FLinearColor& DefaultValue, FParameterValue& OutValue,
				bool bIsNormalMap = false, UMaterialInterface* Material = nullptr, UUsdAssetCache* TexturesCache = nullptr, TMap<FString, int32>* PrimvarToUVIndex = nullptr )
			{
				FScopedUsdAllocs Allocs;

				pxr::UsdShadeInput Input = Connectable.GetInput( InputName );
				if ( !Input )
				{
					return false;
				}

				// If we have another shader/node connected
				pxr::UsdShadeConnectableAPI Source;
				pxr::TfToken SourceName;
				pxr::UsdShadeAttributeType AttributeType;
				if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( Input.GetAttr(), &Source, &SourceName, &AttributeType ) )
				{
					if ( !GetTextureParameterValue( Input, bIsNormalMap ? TEXTUREGROUP_WorldNormalMap : TEXTUREGROUP_World, OutValue, Material, TexturesCache, PrimvarToUVIndex ) )
					{
						// Check whether this input receives its value through a connection to a
						// primvar reader shader.
						if ( GetPrimvarReaderParameterValue( Input, UnrealIdentifiers::UsdPrimvarReader_float3, DefaultValue, OutValue ) )
						{
							return true;
						}

						// Check if we have a fallback input that we can use instead, since we don't have a valid texture value
						if ( const pxr::UsdShadeInput FallbackInput = Source.GetInput( UnrealIdentifiers::Fallback ) )
						{
							pxr::GfVec3f UsdFallbackVec3;
							if ( FallbackInput.Get<pxr::GfVec3f>( &UsdFallbackVec3 ) )
							{
								OutValue.Set<FVector>( UsdToUnreal::ConvertVector( UsdFallbackVec3 ) );
								return true;
							}

							pxr::GfVec4f UsdFallbackVec4;
							if ( FallbackInput.Get<pxr::GfVec4f>( &UsdFallbackVec4 ) )
							{
								if ( !FMath::IsNearlyEqual( UsdFallbackVec4[ 3 ], 1.0f ) )
								{
									UE_LOG( LogUsd, Warning, TEXT( "Ignoring alpha value from fallback GfVec4f [%f, %f, %f, %f] used for Shader '%s'" ),
										UsdFallbackVec4[ 0 ], UsdFallbackVec4[ 1 ], UsdFallbackVec4[ 2 ], UsdFallbackVec4[ 3 ], *UsdToUnreal::ConvertPath( Source.GetPrim().GetPrimPath() )
									);
								}

								OutValue.Set<FVector>( FVector{ UsdFallbackVec4[ 0 ], UsdFallbackVec4[ 1 ], UsdFallbackVec4[ 2 ] } );
								return true;
							}
						}

						// This shader doesn't have anything: Traverse into the input connectable itself
						return GetVec3ParameterValue( Source, SourceName, DefaultValue, OutValue, bIsNormalMap, Material, TexturesCache, PrimvarToUVIndex );
					}
				}
				// No other node connected, so we must have some value
				else if ( InputName != UnrealIdentifiers::Normal )
				{
					FLinearColor DiffuseColor = DefaultValue;
					pxr::GfVec3f UsdDiffuseColor;
					if ( Input.Get< pxr::GfVec3f >( &UsdDiffuseColor ) )
					{
						DiffuseColor = UsdToUnreal::ConvertColor( UsdDiffuseColor );
					}

					OutValue.Set<FVector>( FVector(DiffuseColor) );
				}

				return true;
			}

			bool GetBoolParameterValue( pxr::UsdShadeConnectableAPI& Connectable, const pxr::TfToken& InputName, bool DefaultValue, FParameterValue& OutValue, UMaterialInterface* Material = nullptr, UUsdAssetCache* TexturesCache = nullptr, TMap<FString, int32>* PrimvarToUVIndex = nullptr)
			{
				FScopedUsdAllocs Allocs;

				pxr::UsdShadeInput Input = Connectable.GetInput( InputName );
				if ( !Input )
				{
					return false;
				}

				// If we have another shader/node connected
				pxr::UsdShadeConnectableAPI Source;
				pxr::TfToken SourceName;
				pxr::UsdShadeAttributeType AttributeType;
				if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( Input.GetAttr(), &Source, &SourceName, &AttributeType ) )
				{
					// Recurse because the attribute may just be pointing at some other attribute that has the data
					// (e.g. when shader input is just "hoisted" and connected to the parent material input)
					return GetBoolParameterValue( Source, SourceName, DefaultValue, OutValue, Material, TexturesCache, PrimvarToUVIndex );
				}
				// No other node connected, so we must have some value
				else
				{
					bool InputValue = DefaultValue;
					Input.Get< bool >( &InputValue );

					OutValue.Set< bool >( InputValue );
				}

				return true;
			}

#if WITH_EDITOR
			/**
			 * Intended to be used with Visit(), this will create a UMaterialExpression in InMaterial, set it with the
			 * value stored in the current variant of an FParameterValue, and return it.
			 */
			struct FGetExpressionForValueVisitor
			{
				FGetExpressionForValueVisitor(UMaterial& InMaterial)
					: Material(InMaterial)
				{}

				UMaterialExpression* operator()( const float FloatValue ) const
				{
					UMaterialExpressionConstant* Expression = Cast< UMaterialExpressionConstant >( UMaterialEditingLibrary::CreateMaterialExpression( &Material, UMaterialExpressionConstant::StaticClass() ) );
					Expression->R = FloatValue;

					return Expression;
				}

				UMaterialExpression* operator()( const FVector& VectorValue ) const
				{
					UMaterialExpressionConstant4Vector* Expression = Cast< UMaterialExpressionConstant4Vector >( UMaterialEditingLibrary::CreateMaterialExpression( &Material, UMaterialExpressionConstant4Vector::StaticClass() ) );
					Expression->Constant = FLinearColor( VectorValue );

					return Expression;
				}

				UMaterialExpression* operator()( const FTextureParameterValue& TextureValue ) const
				{
					UMaterialExpressionTextureSample* Expression = Cast< UMaterialExpressionTextureSample >( UMaterialEditingLibrary::CreateMaterialExpression( &Material, UMaterialExpressionTextureSample::StaticClass() ) );
					Expression->Texture = TextureValue.Texture;
					Expression->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture( TextureValue.Texture );
					Expression->ConstCoordinate = TextureValue.UVIndex;

					return Expression;
				}

				UMaterialExpression* operator()( const FPrimvarReaderParameterValue& PrimvarReaderValue ) const
				{
					UMaterialExpression* Expression = nullptr;

					// We currently only import the "displayColor" primvar in USD as vertex colors
					// on UE meshes, so that's the only primvar we can support here.
					if ( PrimvarReaderValue.PrimvarName == "displayColor" )
					{
						Expression = UMaterialEditingLibrary::CreateMaterialExpression( &Material, UMaterialExpressionVertexColor::StaticClass() );
					}
					else
					{
						Expression = UMaterialEditingLibrary::CreateMaterialExpression( &Material, UMaterialExpressionConstant4Vector::StaticClass() );

						UMaterialExpressionConstant4Vector* FallbackExpression = Cast< UMaterialExpressionConstant4Vector >( Expression );
						if ( FallbackExpression )
						{
							FallbackExpression->Constant = FLinearColor( PrimvarReaderValue.FallbackValue );
						}
					}

					return Expression;
				}

			private:
				UMaterial& Material;
			};
#endif // WITH_EDITOR

			UMaterialExpression* GetExpressionForValue( UMaterial& Material, const FParameterValue& ParameterValue )
			{
#if WITH_EDITOR
				FGetExpressionForValueVisitor Visitor{ Material };
				return Visit( Visitor, ParameterValue );
#else
				return nullptr;
#endif // WITH_EDITOR
			}

			void SetScalarParameterValue( UMaterialInstance& Material, const TCHAR* ParameterName, float ParameterValue )
			{
				FMaterialParameterInfo Info;
				Info.Name = ParameterName;

				if ( UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>( &Material ) )
				{
					Dynamic->SetScalarParameterValueByInfo( Info, ParameterValue );
				}
#if WITH_EDITOR
				else if ( UMaterialInstanceConstant* Constant = Cast<UMaterialInstanceConstant>( &Material ) )
				{
					Constant->SetScalarParameterValueEditorOnly( Info, ParameterValue );
				}
#endif // WITH_EDITOR
			}

			void SetVectorParameterValue( UMaterialInstance& Material, const TCHAR* ParameterName, FLinearColor ParameterValue )
			{
				FMaterialParameterInfo Info;
				Info.Name = ParameterName;

				if ( UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>( &Material ) )
				{
					Dynamic->SetVectorParameterValueByInfo( Info, ParameterValue );
				}
#if WITH_EDITOR
				else if ( UMaterialInstanceConstant* Constant = Cast<UMaterialInstanceConstant>( &Material ) )
				{
					Constant->SetVectorParameterValueEditorOnly( Info, ParameterValue );
				}
#endif // WITH_EDITOR
			}

			void SetTextureParameterValue( UMaterialInstance& Material, const TCHAR* ParameterName, UTexture* ParameterValue )
			{
				FMaterialParameterInfo Info;
				Info.Name = ParameterName;

				if ( UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>( &Material ) )
				{
					Dynamic->SetTextureParameterValueByInfo( Info, ParameterValue );
				}
#if WITH_EDITOR
				else if ( UMaterialInstanceConstant* Constant = Cast<UMaterialInstanceConstant>( &Material ) )
				{
					Constant->SetTextureParameterValueEditorOnly( Info, ParameterValue );
				}
#endif // WITH_EDITOR
			}

			void SetBoolParameterValue( UMaterialInstance& Material, const TCHAR* ParameterName, bool bParameterValue )
			{
				bool bFound = false;

#if WITH_EDITOR
				// Try the static parameters first
				if ( UMaterialInstanceConstant* Constant = Cast<UMaterialInstanceConstant>( &Material ) )
				{
					bool bNeedsUpdatePermutations = false;

					FStaticParameterSet StaticParameters;
					Constant->GetStaticParameterValues( StaticParameters );

					for ( FStaticSwitchParameter& StaticSwitchParameter : StaticParameters.EditorOnly.StaticSwitchParameters )
					{
						if ( StaticSwitchParameter.ParameterInfo.Name == ParameterName )
						{
							bFound = true;

							if ( StaticSwitchParameter.Value != bParameterValue )
							{
								StaticSwitchParameter.Value = bParameterValue;
								StaticSwitchParameter.bOverride = true;
								bNeedsUpdatePermutations = true;
							}

							break;
						}
					}

					if ( bNeedsUpdatePermutations )
					{
						FlushRenderingCommands();
						Constant->UpdateStaticPermutation( StaticParameters );
					}
				}
#endif // WITH_EDITOR

				// Try it as a scalar parameter
				if ( !bFound )
				{
					SetScalarParameterValue( Material, ParameterName, bParameterValue ? 1.0f : 0.0f );
				}
			}

			/**
			 * Intended to be used with Visit(), this will set an FParameterValue into InMaterial using InParameterName,
			 * depending on the variant active in FParameterValue
			 */
			struct FSetParameterValueVisitor
			{
				explicit FSetParameterValueVisitor( UMaterialInstance& InMaterial, const TCHAR* InParameterName )
					: Material(InMaterial)
					, ParameterName(InParameterName)
				{}

				void operator()( const float FloatValue ) const
				{
					SetScalarParameterValue( Material, ParameterName, FloatValue );
				}

				void operator()( const FVector& VectorValue ) const
				{
					SetVectorParameterValue( Material, ParameterName, FLinearColor(VectorValue) );
				}

				void operator()( const FTextureParameterValue& TextureValue ) const
				{
					SetTextureParameterValue( Material, ParameterName, TextureValue.Texture );
				}

				void operator()( const FPrimvarReaderParameterValue& PrimvarReaderValue ) const
				{
					SetVectorParameterValue( Material, ParameterName, FLinearColor(PrimvarReaderValue.FallbackValue) );
				}

				void operator()( const bool BoolValue ) const
				{
					SetBoolParameterValue( Material, ParameterName, BoolValue );
				}

			protected:
				UMaterialInstance& Material;
				const TCHAR* ParameterName;
			};

			/**
			 * Specialized version of FSetParameterValueVisitor for UE's UsdPreviewSurface base materials.
			 */
			struct FSetPreviewSurfaceParameterValueVisitor : private FSetParameterValueVisitor
			{
				using FSetParameterValueVisitor::FSetParameterValueVisitor;

				void operator()( const float FloatValue ) const
				{
					FSetParameterValueVisitor::operator()( FloatValue );
					SetScalarParameterValue( Material, *FString::Printf( TEXT( "Use%sTexture" ), ParameterName ), 0.0f );
				}

				void operator()( const FVector& VectorValue ) const
				{
					FSetParameterValueVisitor::operator()( VectorValue );
					SetScalarParameterValue( Material, *FString::Printf( TEXT( "Use%sTexture" ), ParameterName ), 0.0f );
				}

				void operator()( const FTextureParameterValue& TextureValue ) const
				{
					SetTextureParameterValue( Material, *FString::Printf( TEXT( "%sTexture" ), ParameterName ), TextureValue.Texture );
					SetScalarParameterValue( Material, *FString::Printf( TEXT( "Use%sTexture" ), ParameterName ), 1.0f );
				}

				void operator()( const FPrimvarReaderParameterValue& PrimvarReaderValue ) const
				{
					FSetParameterValueVisitor::operator()( PrimvarReaderValue.FallbackValue );

					// We currently only import the "displayColor" primvar in USD as vertex colors
					// on UE meshes, so that's the only primvar we can support here.
					if ( PrimvarReaderValue.PrimvarName == "displayColor" )
					{
						SetScalarParameterValue( Material, TEXT( "UseVertexColorForBaseColor" ), 1.0f );
					}
				}
			};

			void SetParameterValue( UMaterialInstance& Material, const TCHAR* ParameterName, const FParameterValue& ParameterValue, bool bForUsdPreviewSurface )
			{
				if ( bForUsdPreviewSurface )
				{
					FSetPreviewSurfaceParameterValueVisitor Visitor{ Material, ParameterName };
					Visit( Visitor, ParameterValue );
				}
				else
				{
					FSetParameterValueVisitor Visitor{ Material, ParameterName };
					Visit( Visitor, ParameterValue );
				}
			}

			UTexture* CreateTextureWithEditor( const pxr::UsdAttribute& TextureAssetPathAttr, const FString& PrimPath, TextureGroup LODGroup, UObject* Outer )
			{
				UTexture* Texture = nullptr;
#if WITH_EDITOR
				FScopedUsdAllocs UsdAllocs;

				pxr::SdfAssetPath TextureAssetPath;
				TextureAssetPathAttr.Get< pxr::SdfAssetPath >( &TextureAssetPath );

				FString TexturePath = UsdToUnreal::ConvertString( TextureAssetPath.GetAssetPath() );
				const bool bIsSupportedUdimTexture = TexturePath.Contains( TEXT( "<UDIM>" ) );
				FPaths::NormalizeFilename( TexturePath );

				FScopedUnrealAllocs UnrealAllocs;

				if ( !TexturePath.IsEmpty() )
				{
					bool bOutCancelled = false;
					UTextureFactory* TextureFactory = NewObject< UTextureFactory >();
					TextureFactory->SuppressImportOverwriteDialog();
					TextureFactory->bUseHashAsGuid = true;
					TextureFactory->LODGroup = LODGroup;
					TextureFactory->HDRImportShouldBeLongLatCubeMap = EAppReturnType::YesAll;

					if ( bIsSupportedUdimTexture )
					{
						FString BaseFileName = FPaths::GetBaseFilename( TexturePath );

						FString BaseFileNameBeforeUdim;
						FString BaseFileNameAfterUdim;
						BaseFileName.Split( TEXT( "<UDIM>" ), &BaseFileNameBeforeUdim, &BaseFileNameAfterUdim );

						FString UdimRegexPattern = FString::Printf( TEXT( R"((%s)(\d{4})(%s))" ), *BaseFileNameBeforeUdim, *BaseFileNameAfterUdim );
						TextureFactory->UdimRegexPattern = MoveTemp( UdimRegexPattern );
					}

					const FString ResolvedTexturePath = UsdUtils::GetResolvedTexturePath( TextureAssetPathAttr );
					if ( !ResolvedTexturePath.IsEmpty() )
					{
						EObjectFlags ObjectFlags = RF_Transactional;
						if ( !Outer )
						{
							Outer = GetTransientPackage();
						}

						if ( Outer == GetTransientPackage() )
						{
							ObjectFlags = ObjectFlags | RF_Transient;
						}

						FString TextureExtension;
						if ( IsInsideUsdzArchive( ResolvedTexturePath, TextureExtension ) )
						{
							// Always prefer using the TextureFactory if we can, as it may provide compression, which the runtime version never will
							Texture = ReadTextureFromUsdzArchiveEditor( ResolvedTexturePath, TextureExtension, TextureFactory, Outer, ObjectFlags );
						}
						// Not inside an USDZ archive, just a regular texture
						else
						{
							FName TextureName = MakeUniqueObjectName( Outer, UTexture::StaticClass(), *FPaths::GetBaseFilename( ResolvedTexturePath ) );
							Texture = Cast< UTexture >( TextureFactory->ImportObject( UTexture::StaticClass(), Outer, TextureName, ObjectFlags, ResolvedTexturePath, TEXT(""), bOutCancelled));
						}

						if ( Texture )
						{
							UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( Texture, TEXT( "USDAssetImportData" ) );
							ImportData->PrimPath = PrimPath;
							ImportData->UpdateFilenameOnly( ResolvedTexturePath );
							Texture->AssetImportData = ImportData;
						}
					}
				}
#endif // WITH_EDITOR
				return Texture;
			}

			UTexture* CreateTextureAtRuntime( const pxr::UsdAttribute& TextureAssetPathAttr, const FString& PrimPath, TextureGroup LODGroup )
			{
				FScopedUsdAllocs UsdAllocs;

				pxr::SdfAssetPath TextureAssetPath;
				TextureAssetPathAttr.Get< pxr::SdfAssetPath >( &TextureAssetPath );

				FString TexturePath = UsdToUnreal::ConvertString( TextureAssetPath.GetAssetPath() );
				FPaths::NormalizeFilename( TexturePath );

				FScopedUnrealAllocs UnrealAllocs;

				UTexture* Texture = nullptr;

				if ( !TexturePath.IsEmpty() )
				{
					const FString ResolvedTexturePath = UsdUtils::GetResolvedTexturePath( TextureAssetPathAttr );
					if ( !ResolvedTexturePath.IsEmpty() )
					{
						// Try checking if the texture is inside an USDZ archive first, or else TextureFactory throws an error
						FString TextureExtension;
						if ( IsInsideUsdzArchive( ResolvedTexturePath, TextureExtension ) )
						{
							Texture = ReadTextureFromUsdzArchiveRuntime( ResolvedTexturePath );
						}

						// Not inside an USDZ archive, just a regular texture
						if ( !Texture )
						{
							Texture = FImageUtils::ImportFileAsTexture2D( ResolvedTexturePath );
						}
					}
				}

				return Texture;
			}

#if WITH_EDITOR
			// Note that we will bake things that aren't supported on the default USD surface shader schema. These could be useful in case the user has a custom
			// renderer though, and they can pick which properties they want anyway
			bool BakeMaterial( const UMaterialInterface& Material, const TArray<FPropertyEntry>& InMaterialProperties, const FIntPoint& InDefaultTextureSize, FBakeOutput& OutBakedData )
			{
				const bool bAllQualityLevels = true;
				const bool bAllFeatureLevels = true;
				TArray<UTexture*> MaterialTextures;
				Material.GetUsedTextures( MaterialTextures, EMaterialQualityLevel::Num, bAllQualityLevels, GMaxRHIFeatureLevel, bAllFeatureLevels );

				// Precache all used textures, otherwise could get everything rendered with low-res textures.
				for ( UTexture* Texture : MaterialTextures )
				{
					if ( UTexture2D* Texture2D = Cast<UTexture2D>( Texture ) )
					{
						Texture2D->SetForceMipLevelsToBeResident( 30.0f );
						Texture2D->WaitForStreaming();
					}
				}

				FMaterialData MatSet;
				MatSet.Material = const_cast<UMaterialInterface*>(&Material); // We don't modify it and neither does the material baking module, it's just a bad signature

				for ( const FPropertyEntry& Entry : InMaterialProperties )
				{
					// No point in asking it to bake if we're going to use the user-supplied value
					if ( Entry.bUseConstantValue )
					{
						continue;
					}

					switch ( Entry.Property )
					{
					case MP_Normal:
						if ( !Material.GetMaterial()->HasNormalConnected() && !Material.GetMaterial()->bUseMaterialAttributes )
						{
							continue;
						}
						break;
					case MP_Tangent:
						if ( !Material.GetMaterial()->GetEditorOnlyData()->Tangent.IsConnected() && !Material.GetMaterial()->bUseMaterialAttributes )
						{
							continue;
						}
						break;
					case MP_EmissiveColor:
						if ( !Material.GetMaterial()->GetEditorOnlyData()->EmissiveColor.IsConnected() && !Material.GetMaterial()->bUseMaterialAttributes )
						{
							continue;
						}
						break;
					case MP_OpacityMask:
						if ( !Material.IsPropertyActive( MP_OpacityMask ) || Material.GetBlendMode() != BLEND_Masked )
						{
							continue;
						}
						break;
					case MP_Opacity:
						if ( !Material.IsPropertyActive( MP_Opacity ) || !IsTranslucentBlendMode( Material.GetBlendMode() ) )
						{
							continue;
						}
						break;
					case MP_MAX:
						continue;
						break;
					default:
						if ( !Material.IsPropertyActive( Entry.Property ) )
						{
							continue;
						}
						break;
					}

					MatSet.PropertySizes.Add( Entry.Property, Entry.bUseCustomSize ? Entry.CustomSize : InDefaultTextureSize );
				}

				FMeshData MeshSettings;
				MeshSettings.MeshDescription = nullptr;
				MeshSettings.TextureCoordinateBox = FBox2D( FVector2D( 0.0f, 0.0f ), FVector2D( 1.0f, 1.0f ) );
				MeshSettings.TextureCoordinateIndex = 0;

				TArray<FBakeOutput> BakeOutputs;
				IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>( "MaterialBaking" );
				const bool bLinearBake = true;
				Module.SetLinearBake( bLinearBake );
				const bool bEmissiveHDR = true;
				Module.SetEmissiveHDR( bEmissiveHDR );
				Module.BakeMaterials( { &MatSet }, { &MeshSettings }, BakeOutputs );

				if ( BakeOutputs.Num() < 1 )
				{
					return false;
				}

				OutBakedData = BakeOutputs[0];
				return true;
			}

			// Writes textures for all baked channels in BakedSamples that are larger than 1x1, and returns the filenames of the emitted textures for each channel
			TMap<EMaterialProperty, FString> WriteTextures( FBakedMaterialView& BakedSamples, const FString& TextureNamePrefix, const FDirectoryPath& TexturesFolder )
			{
				IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>( TEXT( "ImageWrapper" ) );
				TSharedPtr<IImageWrapper> EXRImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::EXR );
				TSharedPtr<IImageWrapper> PNGImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );

				TMap<EMaterialProperty, FString> WrittenTexturesPerChannel;

				// Write textures for HDR baked properties larger than 1x1
				for ( TPair<EMaterialProperty, TArray<FFloat16Color>*>& BakedDataPair : BakedSamples.HDRPropertyData )
				{
					const EMaterialProperty Property = BakedDataPair.Key;

					TArray<FFloat16Color>* Samples = BakedDataPair.Value;
					if ( !Samples || Samples->Num() < 2 )
					{
						continue;
					}

					const FIntPoint& FinalSize = BakedSamples.PropertySizes.FindChecked( Property );
					if ( FinalSize.GetMin() < 2 )
					{
						continue;
					}

					const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
					FName PropertyName = PropertyEnum->GetNameByValue( Property );
					FString TrimmedPropertyName = PropertyName.ToString();
					TrimmedPropertyName.RemoveFromStart( TEXT( "MP_" ) );

					// Final FilePath will be something like "C:/TexturesFolder/Game_ContentFolder_Materials_Red_BaseColor.png", which automatically guarantees
					// it won't overwrite another texture from the same export, but will overwrite old textures from previous exports
					FString TextureFileName = ObjectTools::SanitizeObjectName( FPaths::ChangeExtension( TextureNamePrefix, TEXT( "" ) ) );
					TextureFileName.RemoveFromStart( TEXT( "_" ) );
					FString TextureFilePath = FPaths::Combine( TexturesFolder.Path, FString::Printf( TEXT( "%s_%s.exr" ), *ObjectTools::SanitizeObjectName( TextureFileName ), *TrimmedPropertyName ) );

					// For some reason the baked samples always have zero alpha and there is nothing we can do about it... It seems like the material baking module is made
					// with the intent that the data ends up in UTexture2Ds, where they can be set to be compressed without alpha and have the value ignored.
					// Since we need to write these to file, we must set them back up to full alpha. This is potentially useless as USD handles these at most as color3f, but
					// it could be annoying for the user if they intend on using the textures for anything else
					for ( FFloat16Color& Sample : *Samples )
					{
						Sample.A = 1.0f;
					}

					EXRImageWrapper->SetRaw( Samples->GetData(), Samples->GetAllocatedSize(), FinalSize.X, FinalSize.Y, ERGBFormat::RGBAF, 16 );
					const TArray64<uint8> Data = EXRImageWrapper->GetCompressed( 100 );

					bool bWroteFile = FFileHelper::SaveArrayToFile( Data, *TextureFilePath );
					if ( bWroteFile )
					{
						WrittenTexturesPerChannel.Add( Property, TextureFilePath );
					}
					else
					{
						UE_LOG( LogUsd, Warning, TEXT( "Failed to write texture '%s', baked channel will be ignored." ), *TextureFilePath );
					}
				}

				// Write textures for baked properties larger than 1x1
				for ( TPair<EMaterialProperty, TArray<FColor>*>& BakedDataPair : BakedSamples.PropertyData )
				{
					const EMaterialProperty Property = BakedDataPair.Key;

					// The material baking module still generates and sends an SDR version of any HDR channel it also bakes,
					// so here let's skip this one in case we already generated an HDR texture for the channel
					if ( WrittenTexturesPerChannel.Contains( Property ) )
					{
						continue;
					}

					TArray<FColor>* Samples = BakedDataPair.Value;
					if ( !Samples || Samples->Num() < 2 )
					{
						continue;
					}

					const FIntPoint& FinalSize = BakedSamples.PropertySizes.FindChecked( Property );
					if ( FinalSize.GetMin() < 2 )
					{
						continue;
					}

					const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
					FName PropertyName = PropertyEnum->GetNameByValue( Property );
					FString TrimmedPropertyName = PropertyName.ToString();
					TrimmedPropertyName.RemoveFromStart( TEXT( "MP_" ) );

					// Final FilePath will be something like "C:/TexturesFolder/Game_ContentFolder_Materials_Red_BaseColor.png", which automatically guarantees
					// it won't overwrite another texture from the same export, but will overwrite old textures from previous exports
					FString TextureFileName = ObjectTools::SanitizeObjectName( FPaths::ChangeExtension( TextureNamePrefix, TEXT( "" ) ) );
					TextureFileName.RemoveFromStart( TEXT( "_" ) );
					FString TextureFilePath = FPaths::Combine( TexturesFolder.Path, FString::Printf( TEXT( "%s_%s.png" ), *ObjectTools::SanitizeObjectName( TextureFileName ), *TrimmedPropertyName ) );

					// For some reason the baked samples always have zero alpha and there is nothing we can do about it... It seems like the material baking module is made
					// with the intent that the data ends up in UTexture2Ds, where they can be set to be compressed without alpha and have the value ignored.
					// Since we need to write these to file, we must set them back up to full alpha. This is potentially useless as USD handles these at most as color3f, but
					// it could be annoying for the user if they intend on using the textures for anything else
					for ( FColor& Sample : *Samples )
					{
						Sample.A = 255;
					}

					PNGImageWrapper->SetRaw( Samples->GetData(), Samples->GetAllocatedSize(), FinalSize.X, FinalSize.Y, ERGBFormat::BGRA, 8 );
					const TArray64<uint8> PNGData = PNGImageWrapper->GetCompressed( 100 );

					bool bWroteFile = FFileHelper::SaveArrayToFile( PNGData, *TextureFilePath );
					if ( bWroteFile )
					{
						WrittenTexturesPerChannel.Add(Property, TextureFilePath);
					}
					else
					{
						UE_LOG(LogUsd, Warning, TEXT("Failed to write texture '%s', baked channel will be ignored."), *TextureFilePath);
					}
				}

				return WrittenTexturesPerChannel;
			}

			bool ConfigureShadePrim( const FBakedMaterialView& BakedData, const TMap<EMaterialProperty, FString>& WrittenTextures, const TMap<EMaterialProperty, float>& UserConstantValues, pxr::UsdShadeMaterial& OutUsdShadeMaterial )
			{
				FScopedUsdAllocs Allocs;

				pxr::UsdPrim MaterialPrim = OutUsdShadeMaterial.GetPrim();
				pxr::UsdStageRefPtr Stage = MaterialPrim.GetStage();
				if ( !MaterialPrim || !Stage )
				{
					return false;
				}

				FString UsdFilePath = UsdToUnreal::ConvertString( Stage->GetRootLayer()->GetRealPath() );

				FUsdStageInfo StageInfo{ Stage };

				pxr::SdfPath MaterialPath = MaterialPrim.GetPath();

				// Create surface shader
				pxr::UsdShadeShader ShadeShader = pxr::UsdShadeShader::Define( Stage, MaterialPath.AppendChild( UnrealToUsd::ConvertToken( TEXT( "SurfaceShader" ) ).Get() ) );
				ShadeShader.SetShaderId( UnrealIdentifiers::UsdPreviewSurface );
				pxr::UsdShadeOutput ShaderOutOutput = ShadeShader.CreateOutput( UnrealIdentifiers::Surface, pxr::SdfValueTypeNames->Token );

				// Connect material to surface shader
				pxr::UsdShadeOutput MaterialSurfaceOutput = OutUsdShadeMaterial.CreateSurfaceOutput();
				MaterialSurfaceOutput.ConnectToSource(ShaderOutOutput);

				pxr::UsdShadeShader PrimvarReaderShader;

				const pxr::TfToken PrimvarReaderShaderName = UnrealToUsd::ConvertToken(TEXT("PrimvarReader")).Get();
				const pxr::TfToken PrimvarVariableName = UnrealToUsd::ConvertToken(TEXT("stPrimvarName")).Get();

				// Collect all the properties we'll process, as some data is baked and some comes from values the user input as export options
				TSet<EMaterialProperty> PropertiesToProcess;
				{
					PropertiesToProcess.Reserve( BakedData.PropertyData.Num() + UserConstantValues.Num() + BakedData.HDRPropertyData.Num() );
					BakedData.PropertyData.GetKeys( PropertiesToProcess );

					TSet<EMaterialProperty> UsedProperties;
					UserConstantValues.GetKeys( UsedProperties );
					PropertiesToProcess.Append( UsedProperties );

					BakedData.HDRPropertyData.GetKeys( UsedProperties );
					PropertiesToProcess.Append( UsedProperties );
				}

				// Fill in outputs
				for ( const EMaterialProperty& Property : PropertiesToProcess )
				{
					const FString* TextureFilePath = WrittenTextures.Find( Property );
					const float* UserConstantValue = UserConstantValues.Find( Property );
					const FIntPoint* SampleSize = BakedData.PropertySizes.Find( Property );

					uint64 NumSamples = 0;

					bool bPropertyValueIsConstant = false;
					pxr::GfVec3f ConstantLinearValue;

					// Try HDR first: When baking a channel as HDR (like emissive) the MaterialBaking module
					// will still bake the SDR version of the channel too. We keep both in our FBakedMaterialView
					// because it only does the mechanism of decaying to a single value on the SDR data array
					bool bParsedHDR = false;
					if ( BakedData.HDRPropertyData.Contains( Property ) )
					{
						const TArray<FColor>* SDRSamples = BakedData.PropertyData.FindRef( Property );
						NumSamples = SDRSamples ? SDRSamples->Num() : 0;

						const bool bDecayedToSingleSample = NumSamples == 1;
						bPropertyValueIsConstant = ( UserConstantValue != nullptr || bDecayedToSingleSample );

						const TArray<FFloat16Color>* Samples = BakedData.HDRPropertyData.FindRef( Property );
						if ( bDecayedToSingleSample && Samples )
						{
							// If it decayed to single sample we know our SDR array only has one value,
							// and so should our HDR array. Get the value from the HDR one to avoid an
							// unnecessary quantization though
							const FFloat16Color& Sample = ( *Samples )[ 0 ];
							ConstantLinearValue = pxr::GfVec3f{ Sample.R, Sample.G, Sample.B };
						}

						if ( Samples && Samples->Num() > 0 )
						{
							bParsedHDR = true;
						}
					}

					if ( !bParsedHDR )
					{
						const TArray<FColor>* Samples = BakedData.PropertyData.FindRef( Property );
						NumSamples = Samples ? Samples->Num() : 0;

						bPropertyValueIsConstant = ( UserConstantValue != nullptr || NumSamples == 1 );
						if ( NumSamples == 1 )
						{
							switch ( Property )
							{
								case EMaterialProperty::MP_BaseColor:
								case EMaterialProperty::MP_SubsurfaceColor:
								{
									pxr::GfVec4f ConvertedColor = UnrealToUsd::ConvertColor( ( *Samples )[ 0 ] );
									ConstantLinearValue = pxr::GfVec3f{
										ConvertedColor[ 0 ],
										ConvertedColor[ 1 ],
										ConvertedColor[ 2 ]
									};
									break;
								}
								case EMaterialProperty::MP_Normal:
								case EMaterialProperty::MP_Tangent:
								{
									FVector ConvertedNormal{ ( *Samples )[ 0 ].ReinterpretAsLinear() };
									ConstantLinearValue = UnrealToUsd::ConvertVector( StageInfo, ConvertedNormal );
									break;
								}
								default:
								{
									const FColor& Sample = ( *Samples )[ 0 ];
									ConstantLinearValue = pxr::GfVec3f{
										Sample.R / 255.0f,
										Sample.G / 255.0f,
										Sample.B / 255.0f
									};
									break;
								}
							}
						}
					}

					if ( ( NumSamples == 0 || !SampleSize ) && !UserConstantValue )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Skipping material property %d as we have no valid data to use." ), Property );
						continue;
					}

					if ( !UserConstantValue && NumSamples > 0 && SampleSize )
					{
						if ( NumSamples != SampleSize->X * SampleSize->Y )
						{
							UE_LOG( LogUsd, Warning, TEXT( "Skipping material property %d as it has an unexpected number of samples (%d instead of %d)." ), Property, NumSamples, SampleSize->X * SampleSize->Y );
							continue;
						}
					}

					if ( !bPropertyValueIsConstant && ( TextureFilePath == nullptr || !FPaths::FileExists( *TextureFilePath ) ) )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Skipping material property %d as target texture '%s' was not found." ), Property, TextureFilePath ? **TextureFilePath : TEXT( "" ) );
						continue;
					}

					pxr::TfToken InputToken;
					pxr::SdfValueTypeName InputType;
					pxr::VtValue ConstantValue;
					pxr::GfVec4f FallbackValue;
					pxr::TfToken ColorSpaceToken = RawColorSpaceToken;
					switch ( Property )
					{
					case MP_BaseColor:
						InputToken = UnrealIdentifiers::DiffuseColor;
						InputType = pxr::SdfValueTypeNames->Color3f;
						if ( bPropertyValueIsConstant )
						{
							if ( UserConstantValue )
							{
								ConstantValue = pxr::GfVec3f( *UserConstantValue, *UserConstantValue, *UserConstantValue );
							}
							else
							{
								ConstantValue = ConstantLinearValue;
							}
						}
						FallbackValue = pxr::GfVec4f{ 0.0, 0.0, 0.0, 1.0f };
						ColorSpaceToken = SRGBColorSpaceToken;
						break;
					case MP_Specular:
						InputToken = UnrealIdentifiers::Specular;
						InputType = pxr::SdfValueTypeNames->Float;
						ConstantValue = UserConstantValue ? *UserConstantValue : ConstantLinearValue[ 0 ];
						FallbackValue = pxr::GfVec4f{ 0.5, 0.5, 0.5, 1.0f };
						break;
					case MP_Metallic:
						InputToken = UnrealIdentifiers::Metallic;
						InputType = pxr::SdfValueTypeNames->Float;
						ConstantValue = UserConstantValue ? *UserConstantValue : ConstantLinearValue[ 0 ];
						FallbackValue = pxr::GfVec4f{ 0.0, 0.0, 0.0, 1.0f };
						break;
					case MP_Roughness:
						InputToken = UnrealIdentifiers::Roughness;
						InputType = pxr::SdfValueTypeNames->Float;
						ConstantValue = UserConstantValue ? *UserConstantValue : ConstantLinearValue[ 0 ];
						FallbackValue = pxr::GfVec4f{ 0.5, 0.5, 0.5, 1.0f };
						break;
					case MP_Normal:
						InputToken = UnrealIdentifiers::Normal;
						InputType = pxr::SdfValueTypeNames->Normal3f;
						if ( bPropertyValueIsConstant )
						{
							// This doesn't make much sense here but it's an available option, so here we go
							if ( UserConstantValue )
							{
								ConstantValue = pxr::GfVec3f( *UserConstantValue, *UserConstantValue, *UserConstantValue );
							}
							else
							{
								ConstantValue = ConstantLinearValue;
							}
						}
						FallbackValue = pxr::GfVec4f{ 0.0, 1.0, 0.0, 1.0f };
						break;
					case MP_Tangent:
						InputToken = UnrealIdentifiers::Tangent;
						InputType = pxr::SdfValueTypeNames->Normal3f;
						if ( bPropertyValueIsConstant )
						{
							// This doesn't make much sense here but it's an available option, so here we go
							if ( UserConstantValue )
							{
								ConstantValue = pxr::GfVec3f( *UserConstantValue, *UserConstantValue, *UserConstantValue );
							}
							else
							{
								ConstantValue = ConstantLinearValue;
							}
						}
						FallbackValue = pxr::GfVec4f{ 0.0, 1.0, 0.0, 1.0f };
						break;
					case MP_EmissiveColor:
						InputToken = UnrealIdentifiers::EmissiveColor;
						InputType = pxr::SdfValueTypeNames->Color3f;
						if ( bPropertyValueIsConstant )
						{
							// This doesn't make much sense here but it's an available option, so here we go
							if ( UserConstantValue )
							{
								ConstantValue = pxr::GfVec3f( *UserConstantValue, *UserConstantValue, *UserConstantValue );
							}
							else
							{
								ConstantValue = ConstantLinearValue;
							}
						}
						FallbackValue = pxr::GfVec4f{ 0.0, 0.0, 0.0, 1.0f };
						// Emissive is also written out with RawColorSpaceToken as we write them as EXR files now
						break;
					case MP_Opacity: // It's OK that we use the same for both as these are mutually exclusive blend modes
					case MP_OpacityMask:
						InputToken = UnrealIdentifiers::Opacity;
						InputType = pxr::SdfValueTypeNames->Float;
						ConstantValue = UserConstantValue ? *UserConstantValue : ConstantLinearValue[ 0 ];
						FallbackValue = pxr::GfVec4f{ 1.0, 1.0, 1.0, 1.0f };
						break;
					case MP_Anisotropy:
						InputToken = UnrealIdentifiers::Anisotropy;
						InputType = pxr::SdfValueTypeNames->Float;
						ConstantValue = UserConstantValue ? *UserConstantValue : ConstantLinearValue[ 0 ];
						FallbackValue = pxr::GfVec4f{ 0.0, 0.0, 0.0, 1.0f };
						break;
					case MP_AmbientOcclusion:
						InputToken = UnrealIdentifiers::Occlusion;
						InputType = pxr::SdfValueTypeNames->Float;
						ConstantValue = UserConstantValue ? *UserConstantValue : ConstantLinearValue[ 0 ];
						FallbackValue = pxr::GfVec4f{ 1.0, 1.0, 1.0, 1.0f };
						break;
					case MP_SubsurfaceColor:
						InputToken = UnrealIdentifiers::SubsurfaceColor;
						InputType = pxr::SdfValueTypeNames->Color3f;
						if ( bPropertyValueIsConstant )
						{
							if ( UserConstantValue )
							{
								ConstantValue = pxr::GfVec3f( *UserConstantValue, *UserConstantValue, *UserConstantValue );
							}
							else
							{
								ConstantValue = ConstantLinearValue;
							}
						}
						FallbackValue = pxr::GfVec4f{ 1.0, 1.0, 1.0, 1.0f };
						ColorSpaceToken = SRGBColorSpaceToken;
						break;
					default:
						continue;
						break;
					}

					pxr::UsdShadeInput ShadeInput = ShadeShader.CreateInput( InputToken, InputType );
					if ( bPropertyValueIsConstant )
					{
						ShadeInput.Set( ConstantValue );
					}
					else // Its a texture
					{
						// Create the primvar/uv set reader on-demand. We'll be using the same UV set for everything for now
						if ( !PrimvarReaderShader )
						{
							PrimvarReaderShader = pxr::UsdShadeShader::Define( Stage, MaterialPath.AppendChild( PrimvarReaderShaderName ) );
							PrimvarReaderShader.SetShaderId( UnrealIdentifiers::UsdPrimvarReader_float2 );

							// Create the 'st' input directly on the material, as that seems to be preferred
							pxr::UsdShadeInput MaterialStInput = OutUsdShadeMaterial.CreateInput( PrimvarVariableName, pxr::SdfValueTypeNames->Token );
							MaterialStInput.Set( UnrealIdentifiers::St );

							pxr::UsdShadeInput VarnameInput = PrimvarReaderShader.CreateInput( UnrealIdentifiers::Varname, pxr::SdfValueTypeNames->String );
							VarnameInput.ConnectToSource( MaterialStInput );

							PrimvarReaderShader.CreateOutput( UnrealIdentifiers::Result, pxr::SdfValueTypeNames->Token );
						}

						FString TextureReaderName = UsdToUnreal::ConvertToken( InputToken );
						TextureReaderName.RemoveFromEnd( TEXT( "Color" ) );
						TextureReaderName += TEXT( "Texture" );

						pxr::UsdShadeShader UsdUVTextureShader = pxr::UsdShadeShader::Define( Stage, MaterialPath.AppendChild( UnrealToUsd::ConvertToken( *TextureReaderName ).Get() ) );
						UsdUVTextureShader.SetShaderId( UnrealIdentifiers::UsdUVTexture );

						pxr::UsdShadeInput TextureFileInput = UsdUVTextureShader.CreateInput( UnrealIdentifiers::File, pxr::SdfValueTypeNames->Asset );
						FString TextureRelativePath = *TextureFilePath;
						if ( !UsdFilePath.IsEmpty() )
						{
							FPaths::MakePathRelativeTo( TextureRelativePath, *UsdFilePath );
						}
						TextureFileInput.Set( pxr::SdfAssetPath( UnrealToUsd::ConvertString( *TextureRelativePath ).Get() ) );

						pxr::UsdShadeInput TextureStInput = UsdUVTextureShader.CreateInput( UnrealIdentifiers::St, pxr::SdfValueTypeNames->Float2 );
						TextureStInput.ConnectToSource( PrimvarReaderShader.GetOutput( UnrealIdentifiers::Result ) );

						pxr::UsdShadeInput TextureColorSpaceInput = UsdUVTextureShader.CreateInput( SourceColorSpaceToken, pxr::SdfValueTypeNames->Token );
						TextureColorSpaceInput.Set( ColorSpaceToken );

						pxr::UsdShadeInput TextureFallbackInput = UsdUVTextureShader.CreateInput( UnrealIdentifiers::Fallback, pxr::SdfValueTypeNames->Float4 );
						TextureFallbackInput.Set( FallbackValue );

						// In the general case it's impossible to set a "correct" wrapping value here because the
						// material we just baked may be using 3 different textures with UV transforms an all different
						// texture wrapping modes, and we're forced to pick a single value to wrap the baked texture
						// with, but let's at least write "repeat" out as that is the default for textures in UE and
						// the more likely to be correct, in case the mesh does things like have UVs outside [0, 1]
						pxr::UsdShadeInput TextureFileWrapSInput = UsdUVTextureShader.CreateInput(
							UnrealIdentifiers::WrapS,
							pxr::SdfValueTypeNames->Token
						);
						TextureFileWrapSInput.Set( UnrealIdentifiers::Repeat );
						pxr::UsdShadeInput TextureFileWrapTInput = UsdUVTextureShader.CreateInput(
							UnrealIdentifiers::WrapT,
							pxr::SdfValueTypeNames->Token
						);
						TextureFileWrapTInput.Set( UnrealIdentifiers::Repeat );

						pxr::UsdShadeOutput TextureOutput = UsdUVTextureShader.CreateOutput(
							InputType == pxr::SdfValueTypeNames->Float ? UnrealIdentifiers::R : UnrealIdentifiers::RGB,
							InputType
						);

						ShadeInput.ConnectToSource( TextureOutput );
					}
				}

				return true;
			}
#endif // WITH_EDITOR

			void HashShadeInput( const pxr::UsdShadeInput& ShadeInput, FSHA1& InOutHashState )
			{
				if ( !ShadeInput )
				{
					return;
				}

				FScopedUsdAllocs UsdAllocs;

				FString InputName = UsdToUnreal::ConvertToken( ShadeInput.GetBaseName() );
				InOutHashState.UpdateWithString( *InputName, InputName.Len() );

				FString InputTypeName = UsdToUnreal::ConvertToken( ShadeInput.GetTypeName().GetAsToken() );
				InOutHashState.UpdateWithString( *InputTypeName, InputTypeName.Len() );

				// Connected to something else, recurse
				pxr::UsdShadeConnectableAPI Source;
				pxr::TfToken SourceName;
				pxr::UsdShadeAttributeType AttributeType;
				if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( ShadeInput.GetAttr(), &Source, &SourceName, &AttributeType ) )
				{
					FString SourceOutputName = UsdToUnreal::ConvertToken( SourceName );
					InOutHashState.UpdateWithString( *SourceOutputName, SourceOutputName.Len() );

					// Skip in case our input is connected to an output on the same prim, or else we'll recurse forever
					if ( Source.GetPrim() == ShadeInput.GetPrim() )
					{
						return;
					}

					for ( const pxr::UsdShadeInput& ChildInput : Source.GetInputs() )
					{
						HashShadeInput( ChildInput, InOutHashState );
					}
				}
				// Not connected to anything, has a value (this could be a texture file path too)
				else
				{
					pxr::VtValue ShadeInputValue;
					ShadeInput.Get( &ShadeInputValue );

					// We have to manually resolve and hash these file paths or else resolvedpaths inside usdz archives will have upper case driver letters
					// when we first open the stage, but will switch to lower case drive letters if we reload them. This is not something we're doing,
					// as it happens with a pure USD python script. (this with USD 21.05 in June 2021)
					if ( ShadeInputValue.IsHolding<pxr::SdfAssetPath>() )
					{
						FString ResolvedPath = UsdUtils::GetResolvedTexturePath( ShadeInput.GetAttr() );
						InOutHashState.UpdateWithString( *ResolvedPath, ResolvedPath.Len() );
					}
					else
					{
						uint64 ValueHash = ( uint64 ) ShadeInputValue.GetHash();
						InOutHashState.Update( reinterpret_cast< uint8* >( &ValueHash ), sizeof( uint64 ) );
					}
				}
			}
		}
	}
}
namespace UsdShadeConversionImpl = UE::UsdShadeConversion::Private;

bool UsdToUnreal::ConvertMaterial( const pxr::UsdShadeMaterial& UsdShadeMaterial, UMaterialInstance& Material )
{
	TMap<FString, int32> PrimvarToUVIndex;
	return ConvertMaterial( UsdShadeMaterial, Material, nullptr, PrimvarToUVIndex );
}

bool UsdToUnreal::ConvertMaterial( const pxr::UsdShadeMaterial& UsdShadeMaterial, UMaterialInstance& Material, UUsdAssetCache* TexturesCache, TMap< FString, int32 >& PrimvarToUVIndex, const TCHAR* RenderContext )
{
	FScopedUsdAllocs UsdAllocs;

	bool bHasMaterialInfo = false;

	pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
	if ( RenderContext )
	{
		pxr::TfToken ProvidedRenderContextToken = UnrealToUsd::ConvertToken( RenderContext ).Get();

		// We won't ever create an asset for the Unreal render context material prim.
		// Check the universal context here in case this material should also generate an asset for that context too
		if ( ProvidedRenderContextToken != UnrealIdentifiers::Unreal )
		{
			RenderContextToken = ProvidedRenderContextToken;
		}
	}

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource( RenderContextToken );
	if ( !SurfaceShader )
	{
		return false;
	}

	pxr::UsdShadeConnectableAPI Connectable{ SurfaceShader };

	UsdShadeConversionImpl::FParameterValue ParameterValue;

	auto SetTextureComponentParamForScalarInput = [ &Material ]( const UsdShadeConversionImpl::FParameterValue& InParameterValue, const TCHAR* ParamName )
	{
		if ( const UsdShadeConversionImpl::FTextureParameterValue* TextureParameterValue = InParameterValue.TryGet< UsdShadeConversionImpl::FTextureParameterValue >() )
		{
			FLinearColor ComponentMask;
			switch( TextureParameterValue->OutputIndex )
			{
			case 0: // RGB, since we are handling scalars here plug red when RGB is requested
			case 1: // R
				ComponentMask = FLinearColor{ 1.f, 0.f, 0.f, 0.f };
				break;
			case 2: // G
				ComponentMask = FLinearColor{ 0.f, 1.f, 0.f, 0.f };
				break;
			case 3: // B
				ComponentMask = FLinearColor{ 0.f, 0.f, 1.f, 0.f };
				break;
			case 4: // A
				ComponentMask = FLinearColor{ 0.f, 0.f, 0.f, 1.f };
				break;
			}

			UsdShadeConversionImpl::SetVectorParameterValue( Material, ParamName, ComponentMask );
		}
	};

	const bool bForUsdPreviewSurface = true;

	// Base color
	{
		const bool bIsNormalMap = false;
		if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, UnrealIdentifiers::DiffuseColor, FLinearColor( 0, 0, 0 ), ParameterValue, bIsNormalMap, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "BaseColor" ), ParameterValue, bForUsdPreviewSurface );
			bHasMaterialInfo = true;
		}
	}

	// Emissive color
	{
		const bool bIsNormalMap = false;
		if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, UnrealIdentifiers::EmissiveColor, FLinearColor( 0, 0, 0 ), ParameterValue, bIsNormalMap, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "EmissiveColor" ), ParameterValue, bForUsdPreviewSurface );
			bHasMaterialInfo = true;
		}
	}

	// Metallic
	{
		if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Metallic, 0.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "Metallic" ), ParameterValue, bForUsdPreviewSurface );
			SetTextureComponentParamForScalarInput( ParameterValue, TEXT( "MetallicTextureComponent" ) );
			bHasMaterialInfo = true;
		}
	}

	// Roughness
	{
		if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Roughness, 1.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "Roughness" ), ParameterValue, bForUsdPreviewSurface );
			SetTextureComponentParamForScalarInput( ParameterValue, TEXT( "RoughnessTextureComponent" ) );
			bHasMaterialInfo = true;
		}
	}

	// Opacity
	{
		if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Opacity, 1.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "Opacity" ), ParameterValue, bForUsdPreviewSurface );
			SetTextureComponentParamForScalarInput( ParameterValue, TEXT( "OpacityTextureComponent" ) );
			bHasMaterialInfo = true;
		}
	}

	// Normal
	{
		const bool bIsNormalMap = true;
		if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, UnrealIdentifiers::Normal, FLinearColor( 0, 0, 1 ), ParameterValue, bIsNormalMap, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "Normal" ), ParameterValue, bForUsdPreviewSurface );
			bHasMaterialInfo = true;
		}
	}

	// Refraction
	{
		if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Refraction, 1.5f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "Refraction" ), ParameterValue, bForUsdPreviewSurface );
			SetTextureComponentParamForScalarInput( ParameterValue, TEXT( "RefractionTextureComponent" ) );
			bHasMaterialInfo = true;
		}
	}

	// Occlusion
	{
		if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Occlusion, 1.0f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			UsdShadeConversionImpl::SetParameterValue( Material, TEXT( "AmbientOcclusion" ), ParameterValue, bForUsdPreviewSurface );
			SetTextureComponentParamForScalarInput( ParameterValue, TEXT( "AmbientOcclusionTextureComponent" ) );
			bHasMaterialInfo = true;
		}
	}

	// Handle world space normals
	pxr::UsdPrim UsdPrim = UsdShadeMaterial.GetPrim();
	if ( pxr::UsdAttribute Attr = UsdPrim.GetAttribute( UnrealIdentifiers::WorldSpaceNormals ) )
	{
		bool bValue = false;
		if ( Attr.Get<bool>( &bValue ) && bValue )
		{
			UsdShadeConversionImpl::SetScalarParameterValue( Material, TEXT( "UseWorldSpaceNormals" ), 1.0f );
		}
	}

	FString ShadeMaterialPath = UsdToUnreal::ConvertPath( UsdPrim.GetPath() );
	UsdShadeConversionImpl::CheckForUVIndexConflicts( PrimvarToUVIndex, ShadeMaterialPath );

	return bHasMaterialInfo;
}

bool UsdToUnreal::ConvertMaterial( const pxr::UsdShadeMaterial& UsdShadeMaterial, UMaterial& Material )
{
	TMap<FString, int32> PrimvarToUVIndex;
	return ConvertMaterial( UsdShadeMaterial, Material, nullptr, PrimvarToUVIndex );
}

bool UsdToUnreal::ConvertMaterial( const pxr::UsdShadeMaterial& UsdShadeMaterial, UMaterial& Material, UUsdAssetCache* TexturesCache, TMap< FString, int32 >& PrimvarToUVIndex, const TCHAR* RenderContext )
{
	bool bHasMaterialInfo = false;

#if WITH_EDITOR
	pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
	if ( RenderContext )
	{
		pxr::TfToken ProvidedRenderContextToken = UnrealToUsd::ConvertToken( RenderContext ).Get();

		// We won't ever create an asset for the Unreal render context material prim.
		// Check the universal context here in case this material should also generate an asset for that context too
		if ( ProvidedRenderContextToken != UnrealIdentifiers::Unreal )
		{
			RenderContextToken = ProvidedRenderContextToken;
		}
	}

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource( RenderContextToken );
	if ( !SurfaceShader )
	{
		return false;
	}

	pxr::UsdShadeConnectableAPI Connectable{ SurfaceShader };

	UsdShadeConversionImpl::FParameterValue ParameterValue;

	auto SetOutputIndex = []( const UsdShadeConversionImpl::FParameterValue& InParameterValue, int32& OutputIndexToSet )
	{
		if ( const UsdShadeConversionImpl::FTextureParameterValue* TextureParameterValue = InParameterValue.TryGet< UsdShadeConversionImpl::FTextureParameterValue >() )
		{
			OutputIndexToSet = TextureParameterValue->OutputIndex;
		}
	};

	UMaterialEditorOnlyData* EditorOnly = Material.GetEditorOnlyData();

	// Base color
	{
		const bool bIsNormalMap = false;
		if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, UnrealIdentifiers::DiffuseColor, FLinearColor( 0, 0, 0 ), ParameterValue, bIsNormalMap, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			if (UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue(Material, ParameterValue))
			{
				EditorOnly->BaseColor.Expression = Expression;
				SetOutputIndex(ParameterValue, EditorOnly->BaseColor.OutputIndex);

				bHasMaterialInfo = true;
			}
		}
	}

	// Emissive color
	{
		const bool bIsNormalMap = false;
		if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, UnrealIdentifiers::EmissiveColor, FLinearColor( 0, 0, 0 ), ParameterValue, bIsNormalMap, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			if (UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue(Material, ParameterValue))
			{
				EditorOnly->EmissiveColor.Expression = Expression;
				SetOutputIndex(ParameterValue, EditorOnly->EmissiveColor.OutputIndex);

				bHasMaterialInfo = true;
			}
		}
	}

	// Metallic
	if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Metallic, 0.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
	{
		if ( UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue( Material, ParameterValue ) )
		{
			EditorOnly->Metallic.Expression = Expression;
			SetOutputIndex( ParameterValue, EditorOnly->Metallic.OutputIndex );

			bHasMaterialInfo = true;
		}
	}

	// Roughness
	if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Roughness, 1.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
	{
		if ( UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue( Material, ParameterValue ) )
		{
			EditorOnly->Roughness.Expression = Expression;
			SetOutputIndex( ParameterValue, EditorOnly->Roughness.OutputIndex );

			bHasMaterialInfo = true;
		}
	}

	// Opacity
	if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Opacity, 1.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
	{
		if ( UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue( Material, ParameterValue ) )
		{
			EditorOnly->Opacity.Expression = Expression;
			SetOutputIndex( ParameterValue, EditorOnly->Opacity.OutputIndex );

			Material.BlendMode = BLEND_Translucent;
			bHasMaterialInfo = true;
		}
	}

	// Normal
	{
		const bool bIsNormalMap = true;
		if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, UnrealIdentifiers::Normal, FLinearColor( 0, 0, 1 ), ParameterValue, bIsNormalMap, &Material, TexturesCache, &PrimvarToUVIndex ) )
		{
			if ( UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue( Material, ParameterValue ) )
			{
				EditorOnly->Normal.Expression = Expression;
				SetOutputIndex(ParameterValue, EditorOnly->Normal.OutputIndex);

				bHasMaterialInfo = true;
			}
		}
	}

	// Refraction
	const bool bHasRefractionValue = UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Refraction, 1.5f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex );
	if ( bHasRefractionValue || Material.BlendMode == BLEND_Translucent ) // Force a 1.5 IOR if USD didn't specify a value, as it's USD's fallback value
	{
		if ( UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue( Material, ParameterValue ) )
		{
			EditorOnly->Refraction.Expression = Expression;
			SetOutputIndex( ParameterValue, EditorOnly->Refraction.OutputIndex );

			bHasMaterialInfo = true;
		}
	}

	// Occlusion
	if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Occlusion, 1.f, ParameterValue, &Material, TexturesCache, &PrimvarToUVIndex ) )
	{
		if ( UMaterialExpression* Expression = UsdShadeConversionImpl::GetExpressionForValue( Material, ParameterValue ) )
		{
			EditorOnly->AmbientOcclusion.Expression = Expression;
			SetOutputIndex( ParameterValue, EditorOnly->AmbientOcclusion.OutputIndex );

			bHasMaterialInfo = true;
		}
	}

	// Handle world space normals
	pxr::UsdPrim UsdPrim = UsdShadeMaterial.GetPrim();
	if ( pxr::UsdAttribute Attr = UsdPrim.GetAttribute( UnrealIdentifiers::WorldSpaceNormals ) )
	{
		bool bValue = false;
		if ( Attr.Get<bool>( &bValue ) && bValue )
		{
			Material.bTangentSpaceNormal = false;
		}
	}

	FString ShadeMaterialPath = UsdToUnreal::ConvertPath( UsdPrim.GetPath() );
	UsdShadeConversionImpl::CheckForUVIndexConflicts( PrimvarToUVIndex, ShadeMaterialPath );

#endif // WITH_EDITOR
	return bHasMaterialInfo;
}

bool UsdToUnreal::ConvertShadeInputsToParameters( const pxr::UsdShadeMaterial& UsdShadeMaterial, UMaterialInstance& MaterialInstance, UUsdAssetCache* TexturesCache, const TCHAR* RenderContext )
{
	FScopedUsdAllocs UsdAllocs;

	bool bHasMaterialInfo = false;

	pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
	if ( RenderContext )
	{
		RenderContextToken = UnrealToUsd::ConvertToken( RenderContext ).Get();
	}

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource( RenderContextToken );
	if ( !SurfaceShader )
	{
		return false;
	}

	pxr::UsdShadeConnectableAPI Connectable{ SurfaceShader };

	for ( pxr::UsdShadeInput& ShadeInput : SurfaceShader.GetInputs() )
	{
		FString InputName = UsdToUnreal::ConvertToken( ShadeInput.GetBaseName() );

		pxr::UsdShadeInput ConnectInput = ShadeInput;
		if ( ShadeInput.HasConnectedSource() )
		{
			pxr::UsdShadeConnectableAPI Source;
			pxr::TfToken SourceName;
			pxr::UsdShadeAttributeType SourceType;
			pxr::UsdShadeConnectableAPI::GetConnectedSource( ShadeInput.GetAttr(), &Source, &SourceName, &SourceType );

			ConnectInput = Source.GetInput(SourceName);
		}

		FString DisplayName = UsdToUnreal::ConvertString( ConnectInput.GetAttr().GetDisplayName() );

		if ( DisplayName.IsEmpty() )
		{
			DisplayName = InputName;
		}

		UsdShadeConversionImpl::FParameterValue ParameterValue;

		const bool bForUsdPreviewSurface = false;
		const bool bIsNormalMap = DisplayName.Contains( TEXT("normal") );

		if ( ShadeInput.GetTypeName() == pxr::SdfValueTypeNames->Bool )
		{
			if ( UsdShadeConversionImpl::GetBoolParameterValue( Connectable, ShadeInput.GetBaseName(), false, ParameterValue, &MaterialInstance, TexturesCache) )
			{
				UsdShadeConversionImpl::SetParameterValue( MaterialInstance, *DisplayName, ParameterValue, bForUsdPreviewSurface );
				bHasMaterialInfo = true;
			}
		}
		else if ( ShadeInput.GetTypeName() == pxr::SdfValueTypeNames->Float ||
			ShadeInput.GetTypeName() == pxr::SdfValueTypeNames->Double ||
			ShadeInput.GetTypeName() == pxr::SdfValueTypeNames->Half )
		{
			if ( UsdShadeConversionImpl::GetFloatParameterValue( Connectable, ShadeInput.GetBaseName(), 1.f, ParameterValue, &MaterialInstance, TexturesCache ) )
			{
				UsdShadeConversionImpl::SetParameterValue( MaterialInstance, *DisplayName, ParameterValue, bForUsdPreviewSurface );
				bHasMaterialInfo = true;
			}
		}
		else if ( UsdShadeConversionImpl::GetVec3ParameterValue( Connectable, ShadeInput.GetBaseName(), FLinearColor( 0.f, 0.f, 0.f ), ParameterValue, bIsNormalMap, &MaterialInstance, TexturesCache ) )
		{
			UsdShadeConversionImpl::SetParameterValue( MaterialInstance, *DisplayName, ParameterValue, bForUsdPreviewSurface );
			bHasMaterialInfo = true;
		}
	}

	return true;
}

#if WITH_EDITOR
bool UnrealToUsd::ConvertMaterialToBakedSurface( const UMaterialInterface& InMaterial, const TArray<FPropertyEntry>& InMaterialProperties, const FIntPoint& InDefaultTextureSize, const FDirectoryPath& InTexturesDir, pxr::UsdPrim& OutUsdShadeMaterialPrim )
{
	pxr::UsdShadeMaterial OutUsdShadeMaterial{ OutUsdShadeMaterialPrim };
	if ( !OutUsdShadeMaterial )
	{
		return false;
	}

	FBakeOutput BakedData;
	if ( !UsdShadeConversionImpl::BakeMaterial( InMaterial, InMaterialProperties, InDefaultTextureSize, BakedData ) )
	{
		return false;
	}

	UsdShadeConversionImpl::FBakedMaterialView View{ BakedData };
	TMap<EMaterialProperty, FString> WrittenTextures = UsdShadeConversionImpl::WriteTextures( View, InMaterial.GetPathName(), InTexturesDir );

	// Manually add user supplied constant values. Can't place these in InMaterial as they're floats, and baked data is just quantized FColors
	TMap<EMaterialProperty, float> UserConstantValues;
	for ( const FPropertyEntry& Entry : InMaterialProperties )
	{
		if ( Entry.bUseConstantValue )
		{
			UserConstantValues.Add( Entry.Property, Entry.ConstantValue );
		}
	}

	return UsdShadeConversionImpl::ConfigureShadePrim( View, WrittenTextures, UserConstantValues, OutUsdShadeMaterial );
}

bool UnrealToUsd::ConvertFlattenMaterial( const FString& InMaterialName, FFlattenMaterial& InMaterial, const TArray<FPropertyEntry>& InMaterialProperties, const FDirectoryPath& InTexturesDir, UE::FUsdPrim& OutUsdShadeMaterialPrim )
{
	pxr::UsdShadeMaterial OutUsdShadeMaterial{ pxr::UsdPrim{ OutUsdShadeMaterialPrim } };
	if ( !OutUsdShadeMaterial )
	{
		return false;
	}

	UsdShadeConversionImpl::FBakedMaterialView View{ InMaterial };
	TMap<EMaterialProperty, FString> WrittenTextures = UsdShadeConversionImpl::WriteTextures( View, InMaterialName, InTexturesDir );

	// Manually add user supplied constant values. Can't place these in InMaterial as they're floats, and baked data is just quantized FColors
	TMap<EMaterialProperty, float> UserConstantValues;
	for ( const FPropertyEntry& Entry : InMaterialProperties )
	{
		if ( Entry.bUseConstantValue )
		{
			UserConstantValues.Add( Entry.Property, Entry.ConstantValue );
		}
	}

	return UsdShadeConversionImpl::ConfigureShadePrim( View, WrittenTextures, UserConstantValues, OutUsdShadeMaterial );
}

#endif // WITH_EDITOR

FString UsdUtils::GetResolvedTexturePath( const pxr::UsdAttribute& TextureAssetPathAttr )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::SdfAssetPath TextureAssetPath;
	TextureAssetPathAttr.Get< pxr::SdfAssetPath >( &TextureAssetPath );

	pxr::ArResolver& Resolver = pxr::ArGetResolver();

	std::string AssetIdentifier = TextureAssetPath.GetResolvedPath();
	// Don't normalize an empty path as the result will be "."
	if ( AssetIdentifier.size() > 0 )
	{
		AssetIdentifier = Resolver.CreateIdentifier( AssetIdentifier );
	}

	FString ResolvedTexturePath = UsdToUnreal::ConvertString( AssetIdentifier );
	FPaths::NormalizeFilename( ResolvedTexturePath );

	if ( ResolvedTexturePath.IsEmpty() )
	{
		FString TexturePath = UsdToUnreal::ConvertString( TextureAssetPath.GetAssetPath() );
		FPaths::NormalizeFilename( TexturePath );

		if ( !TexturePath.IsEmpty() )
		{
			pxr::SdfLayerRefPtr TextureLayer = UsdUtils::FindLayerForAttribute( TextureAssetPathAttr, pxr::UsdTimeCode::EarliestTime().GetValue() );
			ResolvedTexturePath = UsdShadeConversionImpl::ResolveAssetPath( TextureLayer, TexturePath );
		}
	}

	if ( ResolvedTexturePath.IsEmpty() )
	{
		UE_LOG( LogUsd, Warning, TEXT( "Failed to resolve texture path on attribute '%s'" ), *UsdToUnreal::ConvertPath( TextureAssetPathAttr.GetPath() ) );
	}

	return ResolvedTexturePath;
}

UTexture* UsdUtils::CreateTexture( const pxr::UsdAttribute& TextureAssetPathAttr, const FString& PrimPath, TextureGroup LODGroup, UObject* Outer )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( UsdUtils::CreateTexture );

	// Standalone game does have WITH_EDITOR defined, but it can't use the texture factories, so we need to check this instead
	if ( GIsEditor )
	{
		return UsdShadeConversionImpl::CreateTextureWithEditor( TextureAssetPathAttr, PrimPath, LODGroup, Outer );
	}
	else
	{
		return UsdShadeConversionImpl::CreateTextureAtRuntime( TextureAssetPathAttr, PrimPath, LODGroup );
	}
}

void UsdUtils::NotifyIfVirtualTexturesNeeded( UTexture* Texture )
{
	if ( !Texture || !Texture->VirtualTextureStreaming )
	{
		return;
	}

	FString TexturePath = Texture->GetName();
	if ( UUsdAssetImportData* AssetImportData = Cast<UUsdAssetImportData>( Texture ) )
	{
		TexturePath = AssetImportData->PrimPath;
	}

	if ( !UseVirtualTexturing( GMaxRHIFeatureLevel ) )
	{
		FUsdLogManager::LogMessage(
			EMessageSeverity::Warning,
			FText::Format( LOCTEXT( "DisabledVirtualTexturing", "Texture '{0}' requires Virtual Textures, but the feature is disabled for this project" ),
				FText::FromString( TexturePath ) )
		);
	}
}

#if WITH_EDITOR
EFlattenMaterialProperties UsdUtils::MaterialPropertyToFlattenProperty( EMaterialProperty MaterialProperty )
{
	const static TMap<EMaterialProperty, EFlattenMaterialProperties> InvertedMap =
	[]()
	{
		TMap<EMaterialProperty, EFlattenMaterialProperties> Result;
		Result.Reserve( UsdShadeConversionImpl::FlattenToMaterialProperty.Num() );

		for ( const TPair< EFlattenMaterialProperties, EMaterialProperty >& EnumPair : UsdShadeConversionImpl::FlattenToMaterialProperty )
		{
			Result.Add(EnumPair.Value, EnumPair.Key);
		}

		return Result;
	}();

	if ( const EFlattenMaterialProperties* FoundConversion = InvertedMap.Find( MaterialProperty ) )
	{
		return *FoundConversion;
	}

	return EFlattenMaterialProperties::NumFlattenMaterialProperties;
}

EMaterialProperty UsdUtils::FlattenPropertyToMaterialProperty( EFlattenMaterialProperties FlattenProperty )
{
	if ( const EMaterialProperty* FoundConversion = UsdShadeConversionImpl::FlattenToMaterialProperty.Find( FlattenProperty ) )
	{
		return *FoundConversion;
	}

	return EMaterialProperty::MP_MAX;
}

void UsdUtils::CollapseConstantChannelsToSinglePixel( FFlattenMaterial& InMaterial )
{
	auto ConvertSamplesInPlace = []( TArray<FColor>& Samples ) -> bool
	{
		if ( Samples.Num() < 2 )
		{
			return false;
		}

		FColor ConstantValue = Samples[0];
		bool bResize = Algo::AllOf( Samples, [&ConstantValue]( const FColor& Sample ) { return Sample == ConstantValue; } );
		if ( bResize )
		{
			Samples.SetNum(1);
			return true;
		}

		return false;
	};

	for ( const TPair< EFlattenMaterialProperties, EMaterialProperty >& EnumPair : UsdShadeConversionImpl::FlattenToMaterialProperty )
	{
		const EFlattenMaterialProperties Property = EnumPair.Key;
		bool bResized = ConvertSamplesInPlace( InMaterial.GetPropertySamples( Property ) );
		if ( bResized )
		{
			InMaterial.SetPropertySize( Property, FIntPoint( 1, 1 ) );
		}
	}
}
#endif // WITH_EDITOR

bool UsdUtils::MarkMaterialPrimWithWorldSpaceNormals( const UE::FUsdPrim& MaterialPrim )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrim UsdPrim{MaterialPrim};
	if ( !UsdPrim )
	{
		return false;
	}

	const bool bCustom = true;
	pxr::UsdAttribute Attr = UsdPrim.CreateAttribute( UnrealIdentifiers::WorldSpaceNormals, pxr::SdfValueTypeNames->Bool, bCustom );
	if ( !Attr )
	{
		return false;
	}

	Attr.Set<bool>( true );
	UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	return true;
}

TOptional<FString> UsdUtils::GetUnrealSurfaceOutput( const pxr::UsdPrim& MaterialPrim )
{
	if ( !MaterialPrim )
	{
		return {};
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeMaterial ShadeMaterial{ MaterialPrim };
	if ( !ShadeMaterial )
	{
		return {};
	}

	pxr::UsdShadeShader SurfaceShader = ShadeMaterial.ComputeSurfaceSource( UnrealIdentifiers::Unreal );
	if ( !SurfaceShader )
	{
		return {};
	}

	pxr::SdfAssetPath AssetPath;
	if ( SurfaceShader.GetSourceAsset( &AssetPath, UnrealIdentifiers::Unreal ) )
	{
		return UsdToUnreal::ConvertString( AssetPath.GetAssetPath() );
	}

	return {};
}

bool UsdUtils::SetUnrealSurfaceOutput( pxr::UsdPrim& MaterialPrim, const FString& UnrealMaterialPathName )
{
	if ( !MaterialPrim )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeMaterial Material{ MaterialPrim };
	if ( !Material )
	{
		return false;
	}

	pxr::UsdStageRefPtr Stage = MaterialPrim.GetStage();
	pxr::SdfPath ShaderPath = MaterialPrim.GetPath().AppendChild( UnrealToUsd::ConvertToken( TEXT( "UnrealShader" ) ).Get() );

	pxr::UsdShadeShader UnrealShader = pxr::UsdShadeShader::Define( Stage, ShaderPath );
	if ( !UnrealShader )
	{
		return false;
	}

	UnrealShader.CreateImplementationSourceAttr( pxr::VtValue{ pxr::UsdShadeTokens->sourceAsset } );
	UnrealShader.SetSourceAsset(
		UnrealMaterialPathName.IsEmpty() ? pxr::SdfAssetPath{} : pxr::SdfAssetPath{ UnrealToUsd::ConvertString( *UnrealMaterialPathName ).Get() },
		UnrealIdentifiers::Unreal
	);
	pxr::UsdShadeOutput ShaderOutput = UnrealShader.CreateOutput( UnrealToUsd::ConvertToken( TEXT( "out" ) ).Get(), pxr::SdfValueTypeNames->Token );

	pxr::UsdShadeOutput MaterialOutput = Material.CreateSurfaceOutput( UnrealIdentifiers::Unreal );
	pxr::UsdShadeConnectableAPI::ConnectToSource( MaterialOutput, ShaderOutput );

	return true;
}

bool UsdUtils::RemoveUnrealSurfaceOutput( pxr::UsdPrim& MaterialPrim, const UE::FSdfLayer& LayerToAuthorIn )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeMaterial ShadeMaterial{ MaterialPrim };
	pxr::UsdShadeConnectableAPI Connectable{ MaterialPrim };
	if ( !ShadeMaterial || !Connectable )
	{
		return false;
	}

	if ( pxr::UsdShadeOutput MaterialOutput = ShadeMaterial.GetSurfaceOutput( UnrealIdentifiers::Unreal ) )
	{
		if ( pxr::UsdShadeShader SurfaceShader = ShadeMaterial.ComputeSurfaceSource( UnrealIdentifiers::Unreal ) )
		{
			// Fully remove the UnrealShader
			UsdUtils::RemoveAllLocalPrimSpecs( UE::FUsdPrim{ SurfaceShader.GetPrim() }, LayerToAuthorIn );
		}

		// Disconnect would author something like `token outputs:unreal:surface.connect = None`, which is not quite what we want:
		// That would be an opinion to have it connected to nothing, but instead we just want to remove any opinion whatsoever,
		// which is what ClearSource does. Note that these will still leave behind `token outputs:unreal:surface` lines,
		// but those don't actually count as opinions apparently
		pxr::UsdShadeConnectableAPI::ClearSource( MaterialOutput );
	}

	return true;
}

bool UsdUtils::IsMaterialTranslucent( const pxr::UsdShadeMaterial& UsdShadeMaterial )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource();
	if ( !SurfaceShader )
	{
		return false;
	}
	pxr::UsdShadeConnectableAPI Connectable{ SurfaceShader };

	UsdShadeConversionImpl::FParameterValue ParameterValue;
	bool bHasOpacityConnection = UsdShadeConversionImpl::GetFloatParameterValue( Connectable, UnrealIdentifiers::Opacity, 1.f, ParameterValue );

	// Don't check if the texture is nullptr here as we won't actually parse it yet. If the variant has this type we know it's meant to be bound to a texture
	const bool bHasBoundTexture = ParameterValue.IsType<UsdShadeConversionImpl::FTextureParameterValue>();
	const bool bIsTranslucentFloat = ( ParameterValue.IsType<float>() && !FMath::IsNearlyEqual( ParameterValue.Get<float>(), 1.0f ) );

	return bHasOpacityConnection && ( bIsTranslucentFloat || bHasBoundTexture );
}

bool UsdUtils::IsMaterialUsingUDIMs( const pxr::UsdShadeMaterial& UsdShadeMaterial )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource();
	if ( !SurfaceShader )
	{
		return false;
	}

	for ( const pxr::UsdShadeInput& ShadeInput : SurfaceShader.GetInputs() )
	{
		pxr::UsdShadeConnectableAPI Source;
		pxr::TfToken SourceName;
		pxr::UsdShadeAttributeType AttributeType;

		if ( pxr::UsdShadeConnectableAPI::GetConnectedSource( ShadeInput.GetAttr(), &Source, &SourceName, &AttributeType ) )
		{
			pxr::UsdShadeInput FileInput;

			// UsdUVTexture: Get its file input
			if ( AttributeType == pxr::UsdShadeAttributeType::Output )
			{
				FileInput = Source.GetInput( UnrealIdentifiers::File );
			}
			// Check if we are being directly passed an asset
			else
			{
				FileInput = Source.GetInput( SourceName );
			}

			if ( FileInput && FileInput.GetTypeName() == pxr::SdfValueTypeNames->Asset ) // Check that FileInput is of type Asset
			{
				pxr::SdfAssetPath TextureAssetPath;
				FileInput.GetAttr().Get< pxr::SdfAssetPath >( &TextureAssetPath );

				FString TexturePath = UsdToUnreal::ConvertString( TextureAssetPath.GetAssetPath() );

				if ( TexturePath.Contains( TEXT("<UDIM>") ) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

FSHAHash UsdUtils::HashShadeMaterial( const pxr::UsdShadeMaterial& UsdShadeMaterial, const pxr::TfToken& RenderContext )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource( { RenderContext } );
	if ( !SurfaceShader )
	{
		return {};
	}

	FSHA1 HashState;

	for ( const pxr::UsdShadeInput& ShadeInput : SurfaceShader.GetInputs() )
	{
		UsdShadeConversionImpl::HashShadeInput( ShadeInput, HashState );
	}

	bool bValue = false;
	if ( pxr::UsdAttribute Attr = UsdShadeMaterial.GetPrim().GetAttribute( UnrealIdentifiers::WorldSpaceNormals ) )
	{
		Attr.Get<bool>( &bValue );
	}
	HashState.Update( reinterpret_cast< uint8* >( &bValue ), sizeof( bValue ) );

	FSHAHash OutHash;
	HashState.Final();
	HashState.GetHash( &OutHash.Hash[0] );

	return OutHash;
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK
