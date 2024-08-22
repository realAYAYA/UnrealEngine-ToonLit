#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "IDirectoryWatcher.h"
#include "Misc/SecureHash.h"

class CUSTOMEDITOR_API FCustomFileWatcher
{
	
public:
	
	FCustomFileWatcher();

	~FCustomFileWatcher();

	void WatchDir(const FString& InPath);

	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);

	void GenProtocolTsDeclaration();
	
private:
	
	TMap<FString, FDelegateHandle> WatchedDirs;
	TMap<FString, TMap<FString, FMD5Hash>> WatchedFiles;
	FCriticalSection SourceFileWatcherCritical;
};

#endif