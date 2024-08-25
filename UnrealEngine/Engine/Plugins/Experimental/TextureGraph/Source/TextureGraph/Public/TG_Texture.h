// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/TiledBlob.h"
#include "Data/RawBuffer.h"
#include "Model/Mix/MixSettings.h"
#include "2D/TextureHelper.h"
#include "TG_Texture.generated.h"

USTRUCT()
struct TEXTUREGRAPH_API FTG_TextureDescriptor
{
	GENERATED_BODY()
		FTG_TextureDescriptor()
	{
	}

	FTG_TextureDescriptor(BufferDescriptor Desc)
	{
		Width = static_cast<EResolution>(Desc.Width);
		Height = static_cast<EResolution>(Desc.Height);
		TextureFormat = TextureHelper::GetTGTextureFormatFromChannelsAndFormat(Desc.ItemsPerPoint, Desc.Format);
	}

public:
	// Width of the texture in pixels. Auto means system will detect automatically based on other images
	UPROPERTY(EditAnywhere, Category = "TextureDescriptor")
		EResolution Width = EResolution::Auto;		

	// Height of the texture in pixels. Auto means system will detect automatically based on other images
	UPROPERTY(EditAnywhere, Category = "TextureDescriptor")
		EResolution Height = EResolution::Auto;	

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Texture Format instead"))
		ETSBufferChannels NumChannels_DEPRECATED = ETSBufferChannels::Auto; /// How many items of type BufferFormat per point

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Texture Format instead"))
		ETSBufferFormat Format_DEPRECATED = ETSBufferFormat::Auto; /// What is the type of each item in the buffer
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
	
	UPROPERTY(EditAnywhere, Category = "TextureDescriptor", DisplayName = "Texture Format")
		ETG_TextureFormat TextureFormat = ETG_TextureFormat::Auto;

	friend FArchive& operator<<(FArchive& Ar, FTG_TextureDescriptor& D)
	{
 		Ar << D.Width;
		Ar << D.Height;
		Ar << D.TextureFormat;
		return Ar;
	}

	FORCEINLINE BufferDescriptor ToBufferDescriptor() const
	{
		uint32 NumChannels = 0;
		BufferFormat Format = BufferFormat::Auto;
		TextureHelper::GetBufferFormatAndChannelsFromTGTextureFormat(TextureFormat, Format, NumChannels);
		return BufferDescriptor(
			static_cast<int32>(Width),
			static_cast<int32>(Height),
			NumChannels,
			Format
		);
	}

	FORCEINLINE operator BufferDescriptor() const { return ToBufferDescriptor(); }
};

USTRUCT()
struct TEXTUREGRAPH_API FTG_Texture
{
	GENERATED_BODY()
	
public:
	TiledBlobPtr RasterBlob = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "TextureDescriptor")
	FTG_TextureDescriptor Descriptor;

	FTG_Texture() = default;
	FTG_Texture(TiledBlobPtr RHS) : RasterBlob(RHS) {}
	
	operator TiledBlobPtr() { return RasterBlob; }
	operator TiledBlobRef() { return RasterBlob; }
	operator bool() const { return !!RasterBlob; }
	friend FArchive& operator<<(FArchive& Ar, FTG_Texture& T)
	{
		Ar << T.Descriptor;
		return Ar;
	}
	FTG_Texture& operator = (TiledBlobRef RHS) { RasterBlob = RHS; return *this; }
	FTG_Texture& operator = (TiledBlobPtr RHS) { RasterBlob = RHS; return *this; }
	FORCEINLINE TiledBlob* operator -> () const { return RasterBlob.get(); }
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static FTG_Texture GetBlack() { return { TextureHelper::GBlack }; } 
	static FTG_Texture GetWhite() { return { TextureHelper::GWhite }; } 
	static FTG_Texture GetGray() { return { TextureHelper::GGray }; } 
	static FTG_Texture GetRed() { return { TextureHelper::GRed}; } 
	static FTG_Texture GetGreen() { return { TextureHelper::GGreen}; } 
	static FTG_Texture GetBlue() { return { TextureHelper::GBlue}; } 
	static FTG_Texture GetYellow() { return { TextureHelper::GYellow}; } 
	static FTG_Texture GetMagenta() { return { TextureHelper::GMagenta}; } 
	static FTG_Texture GetWhiteMask() { return { TextureHelper::GWhiteMask}; } 
	static FTG_Texture GetBlackMask() { return { TextureHelper::GBlackMask}; } 
	

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE BufferDescriptor GetBufferDescriptor() const { return Descriptor.ToBufferDescriptor(); }
};