// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceRenderTargetCommon.h"

#include "NiagaraCommon.h"
#include "NiagaraSettings.h"

namespace NiagaraDataInterfaceRenderTargetCommon
{
	int32 GReleaseResourceOnRemove = false;
	static FAutoConsoleVariableRef CVarNiagaraReleaseResourceOnRemove(
		TEXT("fx.Niagara.RenderTarget.ReleaseResourceOnRemove"),
		GReleaseResourceOnRemove,
		TEXT("Releases the render target resource once it is removed from the manager list rather than waiting for a GC."),
		ECVF_Default
	);

	//-TEMP: Until we prune data interface on cook this will avoid consuming memory
	int32 GIgnoreCookedOut = true;
	static FAutoConsoleVariableRef CVarNiagaraRenderTargetIgnoreCookedOut(
		TEXT("fx.Niagara.RenderTarget.IgnoreCookedOut"),
		GIgnoreCookedOut,
		TEXT("Ignores create render targets for cooked out emitter, i.e. ones that are not used by any GPU emitter."),
		ECVF_Default
	);

	float GResolutionMultiplier = 1.0f;
	static FAutoConsoleVariableRef CVarNiagaraRenderTargetResolutionMultiplier(
		TEXT("fx.Niagara.RenderTarget.ResolutionMultiplier"),
		GResolutionMultiplier,
		TEXT("Optional global modifier to Niagara render target resolution."),
		ECVF_Default
	);

	int GAllowReads = 0;
	static FAutoConsoleVariableRef CVarNiagaraRenderTargetAllowReads(
		TEXT("fx.Niagara.RenderTarget.AllowReads"),
		GAllowReads,
		TEXT("Enables read operations to be visible in the UI, very experimental."),
		ECVF_Default
	);

	static bool GNiagaraRenderTargetOverrideFormatEnabled = false;
	static ETextureRenderTargetFormat GNiagaraRenderTargetOverrideFormat = ETextureRenderTargetFormat::RTF_RGBA32f;

	static FAutoConsoleCommandWithWorldAndArgs GCommandNiagaraRenderTargetOverrideFormat(
		TEXT("fx.Niagara.RenderTarget.OverrideFormat"),
		TEXT("Optional global format override for all Niagara render targets"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld*)
			{
				static UEnum* TextureRenderTargetFormatEnum = StaticEnum<ETextureRenderTargetFormat>();
				if ( ensure(TextureRenderTargetFormatEnum) )
				{
					if (Args.Num() == 1)
					{
						const int32 EnumIndex = TextureRenderTargetFormatEnum->GetIndexByNameString(Args[0]);
						if (EnumIndex != INDEX_NONE)
						{
							GNiagaraRenderTargetOverrideFormatEnabled = true;
							GNiagaraRenderTargetOverrideFormat = ETextureRenderTargetFormat(EnumIndex);
						}
						else
						{
							GNiagaraRenderTargetOverrideFormatEnabled = false;
						}
					}
					UE_LOG(LogNiagara, Log, TEXT("Niagara RenderTarget Override is '%s' with format '%s'."), GNiagaraRenderTargetOverrideFormatEnabled ? TEXT("Enabled") : TEXT("Disabled"), *TextureRenderTargetFormatEnum->GetNameStringByIndex(int32(GNiagaraRenderTargetOverrideFormat)));
				}
			}
		)
	);

	bool GetRenderTargetFormat(bool bOverrideFormat, ETextureRenderTargetFormat OverrideFormat, ETextureRenderTargetFormat& OutRenderTargetFormat)
	{
		OutRenderTargetFormat = bOverrideFormat ? OverrideFormat : GetDefault<UNiagaraSettings>()->DefaultRenderTargetFormat.GetValue();
		EPixelFormat PixelFormat = GetPixelFormatFromRenderTargetFormat(OutRenderTargetFormat);
		if (GNiagaraRenderTargetOverrideFormatEnabled)
		{
			OutRenderTargetFormat = GNiagaraRenderTargetOverrideFormat;
		}

		// If the format does not support typed store we need to find one that will
		while (RHIIsTypedUAVStoreSupported(GetPixelFormatFromRenderTargetFormat(OutRenderTargetFormat)) == false)
		{
			static const TPair<ETextureRenderTargetFormat, ETextureRenderTargetFormat> FormatRemapTable[] =
			{
				TPairInitializer(RTF_R8,			RTF_R16f),
				TPairInitializer(RTF_RG8,			RTF_RG16f),
				TPairInitializer(RTF_RGBA8,			RTF_RGBA16f),
				TPairInitializer(RTF_RGBA8_SRGB,	RTF_RGBA16f),
				TPairInitializer(RTF_R16f,			RTF_R32f),
				TPairInitializer(RTF_RG16f,			RTF_RG32f),
				TPairInitializer(RTF_RGBA16f,		RTF_RGBA32f),
				TPairInitializer(RTF_R32f,			RTF_RGBA32f),
				TPairInitializer(RTF_RG32f,			RTF_RGBA32f),
				TPairInitializer(RTF_RGBA32f,		RTF_RGBA32f),
				TPairInitializer(RTF_RGB10A2,		RTF_RGBA32f),
			};

			const ETextureRenderTargetFormat PreviousFormat = OutRenderTargetFormat;
			for (const auto& FormatRemap : FormatRemapTable)
			{
				if (FormatRemap.Key == OutRenderTargetFormat)
				{
					OutRenderTargetFormat = FormatRemap.Value;
					break;
				}
			}
			if (PreviousFormat == OutRenderTargetFormat)
			{
				// This is fatal as we failed to find any format that supports typed UAV stores
				UE_LOG(LogNiagara, Warning, TEXT("Failed to find a render target format that supports UAV store"));
				return false;
			}
		}

		return true;
	}
}
