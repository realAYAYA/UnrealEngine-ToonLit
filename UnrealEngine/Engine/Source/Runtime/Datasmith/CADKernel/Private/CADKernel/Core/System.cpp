// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Core/System.h"

#include "CADKernel/Core/Database.h"
#include "CADKernel/Core/Entity.h"
#include "CADKernel/UI/Visu.h"
#include "CADKernel/Utils/Util.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Core/KernelParameters.h"
#include "CADKernel/Core/Version.h"

#include <stdlib.h>
#include <signal.h>
#endif 

namespace UE::CADKernel
{

TUniquePtr<FSystem> FSystem::Instance = nullptr;

FSystem::FSystem()
	: DefaultVisu()
	, Viewer(&DefaultVisu)
	, Console(&DefaultConsole)
	, ProgressManager(&DefaultProgressManager)
#ifdef CADKERNEL_DEV
	, Parameters(MakeShared<FKernelParameters>())
#endif
{
	LogLevel = Log;
	VerboseLevel = Log;
}

void FSystem::Initialize(bool bIsDll, const FString& LogFilePath, const FString& SpyFilePath)
{
	SetVerboseLevel(Log);

	if (LogFilePath.Len() > 0)
	{
		DefineLogFile(LogFilePath);
	}
	if (SpyFilePath.Len() > 0)
	{
		DefineSpyFile(SpyFilePath);
	}

	PrintHeader();

	fflush(stdout);

	if(bIsDll) 
	{
		SetVerboseLevel(EVerboseLevel::NoVerbose);
	} 
	else 
	{
		SetVerboseLevel(EVerboseLevel::Log);
	}
}

void FSystem::CloseLogFiles()
{
	if (LogFile)
	{
		LogFile->Close();
		LogFile.Reset();
	}
	if (SpyFile)
	{
		SpyFile->Close();
		SpyFile.Reset();
	}
}

void FSystem::Shutdown()
{
	CloseLogFiles();
	Instance.Reset();
}

void FSystem::DefineLogFile(const FString& InLogFilePath, EVerboseLevel InLevel)
{
	if(LogFile) 
	{
		LogFile->Close();
		LogFile.Reset();
	}

	LogFile = MakeShareable<FArchive>(IFileManager::Get().CreateFileWriter(*InLogFilePath, IO_WRITE));
	LogLevel = InLevel;
}

void FSystem::DefineSpyFile(const FString& InSpyFilePath)
{
	if(SpyFile) 
	{
		SpyFile->Close();
		SpyFile.Reset();
	}
	SpyFile = MakeShareable<FArchive>(IFileManager::Get().CreateFileWriter(*InSpyFilePath, IO_WRITE));
}


#if defined(CADKERNEL_DEV) || defined(CADKERNEL_STDA)
void FSystem::DefineReportFile(const FString& InReportFilePath)
{
	if (ReportFile.IsValid())
	{
		ReportFile->Close();
		ReportFile.Reset();

		if (ReportHeaderFile.IsValid())
		{
			ReportHeaderFile->Close();
			ReportHeaderFile.Reset();
		}
	}

	FString ReportHeaderPath = FPaths::GetPath(InReportFilePath);
	if (!FPaths::DirectoryExists(ReportHeaderPath))
	{
		IFileManager::Get().MakeDirectory(*ReportHeaderPath, true);
	}

	ReportFile = MakeShareable<FArchive>(IFileManager::Get().CreateFileWriter(*InReportFilePath, IO_WRITE));

	ReportHeaderPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ReportHeaderPath, TEXT("ReportHeader.csv")));
	if(!FPaths::FileExists(ReportHeaderPath))
	{
		ReportHeaderFile = MakeShareable<FArchive>(IFileManager::Get().CreateFileWriter(*ReportHeaderPath, IO_WRITE));
	}
}
#endif

void FSystem::InitializeCADKernel()
{
	FSystem::Get().Initialize();
	FSystem::Get().SetVerboseLevel(EVerboseLevel::Log);
}

FString FSystem::GetToolkitVersion()
{
#ifdef CADKERNEL_DEV
	return UTF8_TO_TCHAR(TOOLKIT_VERSION_ASCII);
#endif
	return FString();
}

FString FSystem::GetCompilationDate()
{
	return UTF8_TO_TCHAR(__DATE__);
}

void FSystem::PrintHeader()
{
	FMessage::Printf(Log, TEXT("_______________________________________________________________________________\n"));
	FMessage::Printf(Log, TEXT("\n"));
#ifdef CADKERNEL_DEV
	FMessage::Printf(Log, TEXT("\tDatasmith CAD Kernel Toolkit release %s (%s)\n"), UTF8_TO_TCHAR(TOOLKIT_VERSION_ASCII), UTF8_TO_TCHAR(__DATE__));
#endif
	FMessage::Printf(Log, TEXT("\tCopyright Epic Games, Inc. All Rights Reserved.\n"));
	FMessage::Printf(Log, TEXT("\n"));
	FMessage::Printf(Log, TEXT("_______________________________________________________________________________\n"));
	FMessage::Printf(Log, TEXT("\n"));
}


}