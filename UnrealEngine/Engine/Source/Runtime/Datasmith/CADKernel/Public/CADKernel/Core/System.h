// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/UI/Console.h"
#include "CADKernel/UI/Progress.h"
#include "CADKernel/UI/Visu.h"

class FArchive;

namespace UE::CADKernel
{
class FConsole;
class FKernelParameters;
class FVisu;

class FSystem
{
	friend class FDatabase;

protected:

	FString ProductName = FString(TEXT("CADKernel"));

	FVisu DefaultVisu;
	FVisu* Viewer;

	FConsole DefaultConsole;
	FConsole* Console;

	FProgressManager DefaultProgressManager;
	FProgressManager* ProgressManager;

	static TUniquePtr<FSystem> Instance;

	TSharedPtr<FArchive> LogFile;
	EVerboseLevel LogLevel;
	TSharedPtr<FArchive> SpyFile;

#if defined(CADKERNEL_DEV) || defined(CADKERNEL_STDA)
	TSharedRef<FKernelParameters> Parameters;
	TSharedPtr<FArchive> ReportFile;
	TSharedPtr<FArchive> ReportHeaderFile;
#endif

	EVerboseLevel VerboseLevel;

public:

	FSystem();

	void Initialize(bool bIsDll = true, const FString& LogFilePath = TEXT(""), const FString& SpyFilePath = TEXT(""));

	void Shutdown();

	void CloseLogFiles();

	static FString GetToolkitVersion();
	static FString GetCompilationDate();

	FVisu* GetVisu() const
	{
		return Viewer;
	}

	FConsole& GetConsole() const
	{
		ensureCADKernel(Console);
		return *Console;
	}

	void SetViewer(FVisu* NewViewer)
	{
		Viewer = NewViewer;
	}

	void SetConsole(FConsole* InConsole)
	{
		Console = InConsole;
	}

	FProgressManager& GetProgressManager()
	{
		ensureCADKernel(ProgressManager);
		return *ProgressManager;
	}

	void SetProgressManager(FProgressManager* InProgressManager)
	{
		ProgressManager = InProgressManager;
	}

	EVerboseLevel GetVerboseLevel() const
	{
		return VerboseLevel;
	}

	void SetVerboseLevel(EVerboseLevel Level)
	{
		VerboseLevel = Level;
	}

	void InitializeCADKernel();

	EVerboseLevel GetLogLevel() const
	{
		return LogLevel;
	}

	void DefineLogFile(const FString& LogFilePath, EVerboseLevel Level = Log);
	TSharedPtr<FArchive> GetLogFile() const
	{
		return LogFile;
	}

	void DefineSpyFile(const FString& SpyFilePath);
	TSharedPtr<FArchive> GetSpyFile() const
	{
		return SpyFile;
	}

#if defined(CADKERNEL_DEV) || defined(CADKERNEL_STDA)
	TSharedRef<FKernelParameters> GetParameters()
	{
		return Parameters;
	}

	void DefineReportFile(const FString& InLogFilePath);
	TSharedPtr<FArchive> GetReportFile() const
	{
		return ReportFile;
	}

	TSharedPtr<FArchive> GetReportHeaderFile() const
	{
		return ReportHeaderFile;
	}
#endif

	static FSystem& Get()
	{
		if (!Instance.IsValid())
		{
			Instance = MakeUnique<FSystem>();
		}
		return *Instance;
	}

protected:

	void PrintHeader();

};

} // namespace UE::CADKernel

