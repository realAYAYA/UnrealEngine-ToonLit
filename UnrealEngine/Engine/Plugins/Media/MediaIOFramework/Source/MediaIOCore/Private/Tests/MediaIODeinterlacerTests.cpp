// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MediaIOCoreEncodeTime.h"
#include "MediaIOCoreDeinterlacer.h"

namespace UE::MediaIOTests
{
	constexpr uint8 FIRST_LINE_VALUE = 5;
	constexpr uint8 SECOND_LINE_VALUE = 10;

	TArray<uint8> CreateBuffer()
	{
		TArray<uint8> Buffer;
		Buffer.Reserve(1080 * 1920);
		
		for (uint32 YIndex = 0; YIndex < 1080; YIndex++)
		{
			for (uint32 XIndex = 0; XIndex < 1920; XIndex++)
			{
				uint8 Value = FIRST_LINE_VALUE;
				if ((YIndex % 2) != 0)
				{
					Value = SECOND_LINE_VALUE;
				}

				for (uint32 RGBAIndex = 0; RGBAIndex < 4; RGBAIndex++)
				{
					Buffer.Add(Value);
				}
			}
		}

		return Buffer;
	}
	
	void TestBobDeinterlace(FAutomationTestBase& Test)
	{
		// Setup test data
		TArray<uint8> Buffer = CreateBuffer();

		EMediaTextureSampleFormat VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;

		constexpr uint32 Width = 1920;
		constexpr uint32 Height = 1080;
		constexpr uint32 Stride = Width * 4;

		UE::MediaIOCore::FVideoFrame FrameInfo
		{
			Buffer.GetData(),
			(uint32)Buffer.Num(),
			Stride,
			Width,
			Height,
			EMediaTextureSampleFormat::CharBGRA,
			FTimespan::FromSeconds(0),
			FFrameRate(),
			FTimecode(),
			UE::MediaIOCore::FColorFormatArgs{UE::Color::EEncoding::Linear, UE::Color::EColorSpace::sRGB}
		};

		// Execute test
		UE::MediaIOCore::FBobDeinterlacer Deinterlacer(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread::CreateLambda([](){ return MakeShared<FMediaIOCoreTextureSampleBase>(); }), EMediaIOInterlaceFieldOrder::TopFieldFirst);
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> DeinterlacedSamples = Deinterlacer.Deinterlace(FrameInfo);

		// Validate result
		Test.TestEqual(TEXT("Samples were created"), DeinterlacedSamples.Num(), 2);
		uint8* Buffer1 = (uint8*)DeinterlacedSamples[0]->GetBuffer();
		uint8* Buffer2 = (uint8*)DeinterlacedSamples[1]->GetBuffer();

		Test.TestEqual(TEXT("First sample first line is correct"), *Buffer1, FIRST_LINE_VALUE);
		Test.TestEqual(TEXT("First sample Second line is correct"), *(Buffer1 + Stride), FIRST_LINE_VALUE);

		Test.TestEqual(TEXT("Second sample first line is correct"), *Buffer2, SECOND_LINE_VALUE);
		Test.TestEqual(TEXT("Second sample Second line is correct"), *(Buffer2 + Stride), SECOND_LINE_VALUE);
	}

	void TestBlendDeinterlace(FAutomationTestBase& Test)
	{
		// Setup test data
		TArray<uint8> Buffer = CreateBuffer();

		EMediaTextureSampleFormat VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;

		constexpr uint32 Width = 1920;
		constexpr uint32 Height = 1080;
		constexpr uint32 Stride = Width * 4;

		UE::MediaIOCore::FVideoFrame FrameInfo
		{
			Buffer.GetData(),
			(uint32)Buffer.Num(),
			Stride,
			Width,
			Height,
			EMediaTextureSampleFormat::CharBGRA,
			FTimespan::FromSeconds(0),
			FFrameRate(),
			FTimecode(),
			UE::MediaIOCore::FColorFormatArgs{UE::Color::EEncoding::Linear, UE::Color::EColorSpace::sRGB}
		};

		// Execute test
		UE::MediaIOCore::FBlendDeinterlacer Deinterlacer(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread::CreateLambda([]() { return MakeShared<FMediaIOCoreTextureSampleBase>(); }), EMediaIOInterlaceFieldOrder::TopFieldFirst);
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> DeinterlacedSamples = Deinterlacer.Deinterlace(FrameInfo);

		// Validate result
		Test.TestEqual(TEXT("One Sample was created"), DeinterlacedSamples.Num(), 1);
		uint8* Buffer1 = (uint8*)DeinterlacedSamples[0]->GetBuffer();

		Test.TestEqual(TEXT("Sample line is averaged"), *Buffer1, FMath::DivideAndRoundNearest(FIRST_LINE_VALUE + SECOND_LINE_VALUE, 2));
	}

	void TestDiscardDeinterlace(FAutomationTestBase& Test)
	{
		// Setup test data
		TArray<uint8> Buffer = CreateBuffer();

		EMediaTextureSampleFormat VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;

		constexpr uint32 Width = 1920;
		constexpr uint32 Height = 1080;
		constexpr uint32 Stride = Width * 4;

		UE::MediaIOCore::FVideoFrame FrameInfo
		{
			Buffer.GetData(),
			(uint32)Buffer.Num(),
			Stride,
			Width,
			Height,
			EMediaTextureSampleFormat::CharBGRA,
			FTimespan::FromSeconds(0),
			FFrameRate(),
			FTimecode(),
			UE::MediaIOCore::FColorFormatArgs{UE::Color::EEncoding::Linear, UE::Color::EColorSpace::sRGB}
		};

		// Execute test
		UE::MediaIOCore::FDiscardDeinterlacer Deinterlacer(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread::CreateLambda([]() { return MakeShared<FMediaIOCoreTextureSampleBase>(); }), EMediaIOInterlaceFieldOrder::TopFieldFirst);
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> DeinterlacedSamples = Deinterlacer.Deinterlace(FrameInfo);

		// Validate result
		Test.TestEqual(TEXT("One sample was created"), DeinterlacedSamples.Num(), 1);
		uint8* Buffer1 = (uint8*)DeinterlacedSamples[0]->GetBuffer();

		Test.TestEqual(TEXT("First sample first line is correct"), *Buffer1, FIRST_LINE_VALUE);
		Test.TestEqual(TEXT("First sample Second line is correct"), *(Buffer1 + Stride), FIRST_LINE_VALUE);
	}

	void TestDiscardDeinterlaceBottomFieldOrder(FAutomationTestBase& Test)
	{
		// Setup test data
		TArray<uint8> Buffer = CreateBuffer();

		EMediaTextureSampleFormat VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;

		constexpr uint32 Width = 1920;
		constexpr uint32 Height = 1080;
		constexpr uint32 Stride = Width * 4;

		UE::MediaIOCore::FVideoFrame FrameInfo
		{
			Buffer.GetData(),
			(uint32)Buffer.Num(),
			Stride,
			Width,
			Height,
			EMediaTextureSampleFormat::CharBGRA,
			FTimespan::FromSeconds(0),
			FFrameRate(),
			FTimecode(),
			UE::MediaIOCore::FColorFormatArgs{UE::Color::EEncoding::Linear, UE::Color::EColorSpace::sRGB}
		};

		// Execute test
		UE::MediaIOCore::FDiscardDeinterlacer Deinterlacer(UE::MediaIOCore::FDeinterlacer::FOnAcquireSample_AnyThread::CreateLambda([]() { return MakeShared<FMediaIOCoreTextureSampleBase>(); }), EMediaIOInterlaceFieldOrder::BottomFieldFirst);
		TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> DeinterlacedSamples = Deinterlacer.Deinterlace(FrameInfo);

		// Validate result
		Test.TestEqual(TEXT("One sample was created"), DeinterlacedSamples.Num(), 1);
		uint8* Buffer1 = (uint8*)DeinterlacedSamples[0]->GetBuffer();

		Test.TestEqual(TEXT("First sample first line is correct"), *Buffer1, SECOND_LINE_VALUE);
		Test.TestEqual(TEXT("First sample Second line is correct"), *(Buffer1 + Stride), SECOND_LINE_VALUE);
	}
}

DEFINE_SPEC(FMediaIODeinterlacerTests, "Plugins.MediaIO.Deinterlace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FMediaIODeinterlacerTests::Define()
{
	It("BobDeinterlace", [this]()
		{
			UE::MediaIOTests::TestBobDeinterlace(*this);
		});

	It("BlendDeinterlace", [this]()
		{
			UE::MediaIOTests::TestBlendDeinterlace(*this);
		});

	It("DiscardDeinterlace", [this]()
		{
			UE::MediaIOTests::TestDiscardDeinterlace(*this);
		});

	It("DiscardDeinterlaceBottomFieldOrder", [this]()
		{
			UE::MediaIOTests::TestDiscardDeinterlaceBottomFieldOrder(*this);
		});
}