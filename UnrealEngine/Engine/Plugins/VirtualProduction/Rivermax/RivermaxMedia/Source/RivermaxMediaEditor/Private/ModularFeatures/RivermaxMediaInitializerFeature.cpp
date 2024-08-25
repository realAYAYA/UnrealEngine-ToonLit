// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularFeatures/RivermaxMediaInitializerFeature.h"

#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"

#include "RivermaxMediaSource.h"
#include "RivermaxMediaOutput.h"


bool FRivermaxMediaInitializerFeature::IsMediaSubjectSupported(const UObject* MediaSubject)
{
	if (MediaSubject)
	{
		return MediaSubject->IsA<URivermaxMediaSource>() || MediaSubject->IsA<URivermaxMediaOutput>();
	}

	return false;
}

void FRivermaxMediaInitializerFeature::InitializeMediaSubjectForTile(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo, const FIntPoint& TilePos)
{
	if (URivermaxMediaSource* RivermaxMediaSource = Cast<URivermaxMediaSource>(MediaSubject))
	{
		RivermaxMediaSource->PlayerMode          = ERivermaxPlayerMode::Framelock;
		RivermaxMediaSource->bUseZeroLatency     = true;
		RivermaxMediaSource->bOverrideResolution = false;
		//RivermaxMediaSource->Resolution          = default value
		RivermaxMediaSource->FrameRate           = { 60,1 };
		RivermaxMediaSource->PixelFormat         = ERivermaxMediaSourcePixelFormat::RGB_10bit;
		RivermaxMediaSource->InterfaceAddress    = GetRivermaxInterfaceAddress();
		RivermaxMediaSource->StreamAddress       = GenerateStreamAddress(OnwerInfo.OwnerUniqueIdx, TilePos);
		RivermaxMediaSource->Port                = 50000;
		RivermaxMediaSource->bIsSRGBInput        = false;
		RivermaxMediaSource->bUseGPUDirect       = true;
	}
	else if (URivermaxMediaOutput* RivermaxMediaOutput = Cast<URivermaxMediaOutput>(MediaSubject))
	{
		RivermaxMediaOutput->AlignmentMode       = ERivermaxMediaAlignmentMode::FrameCreation;
		RivermaxMediaOutput->bDoContinuousOutput = false;
		RivermaxMediaOutput->FrameLockingMode    = ERivermaxFrameLockingMode::BlockOnReservation;
		RivermaxMediaOutput->PresentationQueueSize = 2;
		RivermaxMediaOutput->bDoFrameCounterTimestamping = true;
		RivermaxMediaOutput->bOverrideResolution = false;
		//RivermaxMediaOutput->Resolution          = default value
		RivermaxMediaOutput->FrameRate           = { 60,1 };
		RivermaxMediaOutput->PixelFormat         = ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;
		RivermaxMediaOutput->InterfaceAddress    = GetRivermaxInterfaceAddress();
		RivermaxMediaOutput->StreamAddress       = GenerateStreamAddress(OnwerInfo.OwnerUniqueIdx, TilePos);
		RivermaxMediaOutput->Port                = 50000;
		RivermaxMediaOutput->bUseGPUDirect       = true;
	}
}

void FRivermaxMediaInitializerFeature::InitializeMediaSubjectForFullFrame(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo)
{
	if (URivermaxMediaSource* RivermaxMediaSource = Cast<URivermaxMediaSource>(MediaSubject))
	{
		RivermaxMediaSource->PlayerMode          = ERivermaxPlayerMode::Framelock;
		RivermaxMediaSource->bUseZeroLatency     = true;
		RivermaxMediaSource->bOverrideResolution = false;
		//RivermaxMediaSource->Resolution          = default value
		RivermaxMediaSource->FrameRate           = { 60,1 };
		RivermaxMediaSource->PixelFormat         = ERivermaxMediaSourcePixelFormat::RGB_10bit;
		RivermaxMediaSource->InterfaceAddress    = GetRivermaxInterfaceAddress();
		RivermaxMediaSource->StreamAddress       = GenerateStreamAddress(OnwerInfo.ClusterNodeUniqueIdx.Get(0), OnwerInfo.OwnerUniqueIdx, OnwerInfo.OwnerType);
		RivermaxMediaSource->Port                = 50000;
		RivermaxMediaSource->bIsSRGBInput        = false;
		RivermaxMediaSource->bUseGPUDirect       = true;
	}
	else if (URivermaxMediaOutput* RivermaxMediaOutput = Cast<URivermaxMediaOutput>(MediaSubject))
	{
		RivermaxMediaOutput->FrameLockingMode      = ERivermaxFrameLockingMode::BlockOnReservation;
		RivermaxMediaOutput->PresentationQueueSize = 2;
		RivermaxMediaOutput->bOverrideResolution   = false;
		//RivermaxMediaOutput->Resolution            = default value
		RivermaxMediaOutput->PixelFormat           = ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;
		RivermaxMediaOutput->InterfaceAddress      = GetRivermaxInterfaceAddress();
		RivermaxMediaOutput->StreamAddress         = GenerateStreamAddress(OnwerInfo.ClusterNodeUniqueIdx.Get(0), OnwerInfo.OwnerUniqueIdx, OnwerInfo.OwnerType);
		RivermaxMediaOutput->Port                  = 50000;

		switch (OnwerInfo.OwnerType)
		{
		case FMediaSubjectOwnerInfo::EMediaSubjectOwnerType::ICVFXCamera:
		case FMediaSubjectOwnerInfo::EMediaSubjectOwnerType::Viewport:
			RivermaxMediaOutput->AlignmentMode       = ERivermaxMediaAlignmentMode::FrameCreation;
			RivermaxMediaOutput->bDoContinuousOutput = false;
			RivermaxMediaOutput->bDoFrameCounterTimestamping = true;
			RivermaxMediaOutput->FrameRate           = { 60,1 };
			RivermaxMediaOutput->bUseGPUDirect       = true;
			break;

		case FMediaSubjectOwnerInfo::EMediaSubjectOwnerType::Backbuffer:
			RivermaxMediaOutput->AlignmentMode       = ERivermaxMediaAlignmentMode::AlignmentPoint;
			RivermaxMediaOutput->bDoContinuousOutput = true;
			RivermaxMediaOutput->bDoFrameCounterTimestamping = false;
			RivermaxMediaOutput->FrameRate           = { 24,1 };
			RivermaxMediaOutput->bUseGPUDirect       = false;
			break;

		default:
			checkNoEntry();
		}
	}
}

FString FRivermaxMediaInitializerFeature::GetRivermaxInterfaceAddress() const
{
	FString ResultAddress{ TEXT("*.*.*.*") };

	// Now let's see if we have any interfaces available
	IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
	const TConstArrayView<UE::RivermaxCore::FRivermaxDeviceInfo> Devices = RivermaxModule.GetRivermaxManager()->GetDevices();
	if (Devices.Num() > 0)
	{
		// Split address into octets
		TArray<FString> Octets;
		Devices[0].InterfaceAddress.ParseIntoArray(Octets, TEXT("."));

		// IPv4 always has 4 octets
		if (Octets.Num() == 4)
		{
			ResultAddress = FString::Printf(TEXT("%s.%s.%s.*"), *Octets[0], *Octets[1], *Octets[2]);
		}
	}

	return ResultAddress;
}

FString FRivermaxMediaInitializerFeature::GenerateStreamAddress(uint8 OwnerUniqueIdx, const FIntPoint& TilePos) const
{
	static const constexpr uint8 MaxVal = TNumericLimits<uint8>::Max();
	checkSlow(TilePos.X < MaxVal && TilePos.Y < MaxVal);

	static constexpr uint8 AddressOffsetForTiles = 200;
	checkSlow((AddressOffsetForTiles + OwnerUniqueIdx) < MaxVal);

	// 228.200.*.* - 228.255.*.* - range for tiled media (max 56 objects)
	return FString::Printf(TEXT("228.%u.%u.%u"), AddressOffsetForTiles + OwnerUniqueIdx, TilePos.X, TilePos.Y);
}

FString FRivermaxMediaInitializerFeature::GenerateStreamAddress(uint8 ClusterNodeUniqueIdx, uint8 OwnerUniqueIdx, const FMediaSubjectOwnerInfo::EMediaSubjectOwnerType OwnerType) const
{
	// 228.0.*.* - 228.199.*.* - range for full-frame media (max 200 objects). But could be extended up to the limit if no tiles used.
	return FString::Printf(TEXT("228.%u.%u.%u"), ClusterNodeUniqueIdx, static_cast<uint8>(OwnerType), OwnerUniqueIdx);
}
