// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/ScreenshotProviderPrivate.h"
#include "AnalysisServicePrivate.h"
#include "Common/FormatArgs.h"

namespace TraceServices
{

const FName FScreenshotProvider::ProviderName("ScreenshotProvider");

FScreenshotProvider::FScreenshotProvider(IAnalysisSession& InSession)
	: Session(InSession)
{

} 

TSharedPtr<FScreenshot> FScreenshotProvider::AddScreenshot(uint32 Id)
{
	TSharedPtr<FScreenshot> NewScreenshot = Screenshots.Add(Id, MakeShared<FScreenshot>());
	return NewScreenshot;
}

void FScreenshotProvider::AddScreenshotChunk(uint32 Id, uint32 ChunkNum, uint16 Size, const TArrayView<const uint8>& ChunkData)
{
	TSharedPtr<FScreenshot> *ScreenshotPtr = Screenshots.Find(Id);
	
	if (ScreenshotPtr == nullptr)
	{
		return;
	}

	TSharedPtr<FScreenshot> Screenshot = *ScreenshotPtr;

	constexpr uint32 ChunkSize = TNumericLimits<uint16>::Max();
	check(Screenshot->Data.Num() == ChunkNum * ChunkSize);

	Screenshot->Data.Append(ChunkData.GetData(), Size);
}

const TSharedPtr<const FScreenshot> FScreenshotProvider::GetScreenshot(uint32 Id) const
{
	const TSharedPtr<FScreenshot>* ScreenshotPtr = Screenshots.Find(Id);

	if (ScreenshotPtr == nullptr)
	{
		return nullptr;
	}

	return *ScreenshotPtr;
}

const IScreenshotProvider& ReadScreenshotProvider(const IAnalysisSession& Session)
{
	return *Session.ReadProvider<IScreenshotProvider>(FScreenshotProvider::ProviderName);
}

} // namespace TraceServices
