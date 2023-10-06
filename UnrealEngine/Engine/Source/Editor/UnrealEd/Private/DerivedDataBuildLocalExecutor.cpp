// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildLocalExecutor.h"

#include "Compression/CompressedBuffer.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "DerivedDataBuild.h"
#include "DerivedDataBuildAction.h"
#include "DerivedDataBuildInputs.h"
#include "DerivedDataBuildOutput.h"
#include "DerivedDataBuildTypes.h"
#include "DerivedDataBuildWorker.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataRequestTypes.h"
#include "DerivedDataValue.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IO/IoHash.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Memory/CompositeBuffer.h"
#include "Memory/SharedBuffer.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "Serialization/Archive.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/Function.h"
#include "Templates/Tuple.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"

#include <atomic>

namespace UE::DerivedData
{

DEFINE_LOG_CATEGORY_STATIC(LogDerivedDataBuildLocalExecutor, Log, All);

/**
 * This implements a simple local DDC2 executor which spawns all jobs
 * as local processes. This is intentionally as simple as possible and
 * everything is synchronous. This is not meant to be used in production,
 * and is perhaps primarily useful as a debugging aid.
 */
class FLocalBuildWorkerExecutor final : public IBuildWorkerExecutor
{
	FString SandboxRootDir = FPaths::EngineSavedDir() / TEXT("LocalExec");

public:
	FLocalBuildWorkerExecutor()
	{
		// Clean out any leftovers from a previous run

		UE_LOG(LogDerivedDataBuildLocalExecutor, Warning, TEXT("Deleting existing local execution state from '%s'"), *SandboxRootDir);

		constexpr bool RequireExists = false;
		constexpr bool Tree = true;
		IFileManager::Get().DeleteDirectory(*SandboxRootDir, RequireExists, Tree);

		IModularFeatures::Get().RegisterModularFeature(IBuildWorkerExecutor::FeatureName, this);
	}

	~FLocalBuildWorkerExecutor() final = default;

	void Build(
		const FBuildAction& Action,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		const FBuildWorker& Worker,
		IBuild& BuildSystem,
		IRequestOwner& Owner,
		FOnBuildWorkerActionComplete&& OnComplete) final
	{
		// Review build action inputs to determine if they need to be materialized/propagated 
		// (right now, they always will be)

		TArray<FUtf8StringView> MissingInputs;

		Action.IterateInputs([&](FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
		{
			if (Inputs.IsNull() || Inputs.Get().FindInput(Key).IsNull())
			{
				MissingInputs.Emplace(Key);
			}
		});

		if (!MissingInputs.IsEmpty())
		{
			// Report missing inputs
			return OnComplete({Action.GetKey(), {}, MissingInputs, EStatus::Ok});
		}

		// This path will execute the build action synchronously in a scratch directory
		//
		// At this stage, all inputs are available in process
		//
		// Currently no cleanup whatsoever is performed, so inputs/outputs can be inspected
		// this could be problematic for large runs so we should probably add support for
		// configurable cleanup policies

		static std::atomic<int32> SerialNo = 0;

		TStringBuilder<256> SandboxRoot;
		FPathViews::Append(SandboxRoot, SandboxRootDir, TEXT("Scratch"));
		SandboxRoot.Appendf(TEXT("%06d"), ++SerialNo);

		// Manifest worker in scratch area

		{
			TArray<FIoHash> MissingWorkerData;

			TArray<FIoHash> WorkerFileHashes;
			TArray<TTuple<FStringView, bool>> WorkerFileMeta;
			Worker.IterateExecutables([&WorkerFileHashes, &WorkerFileMeta](FStringView Path, const FIoHash& RawHash, uint64 RawSize)
			{
				WorkerFileHashes.Emplace(RawHash);
				WorkerFileMeta.Emplace(Path, true);
			});

			Worker.IterateFiles([&WorkerFileHashes, &WorkerFileMeta](FStringView Path, const FIoHash& RawHash, uint64 RawSize)
			{
				WorkerFileHashes.Emplace(RawHash);
				WorkerFileMeta.Emplace(Path, false);
			});

			FRequestOwner BlockingOwner(EPriority::Blocking);
			Worker.FindFileData(WorkerFileHashes, BlockingOwner, [&SandboxRoot, &WorkerFileMeta](FBuildWorkerFileDataCompleteParams&& Params)
			{
				uint32 MetaIndex = 0;
				for (const FCompressedBuffer& Buffer : Params.Files)
				{
					const TTuple<FStringView, bool>& Meta = WorkerFileMeta[MetaIndex];

					TStringBuilder<256> Path;
					FPathViews::Append(Path, SandboxRoot, Meta.Key);
					if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Silent)})
					{
						FCompositeBuffer DecompressedComposite = Buffer.DecompressToComposite();

						for (auto& Segment : DecompressedComposite.GetSegments())
						{
							Ar->Serialize((void*)Segment.GetData(), Segment.GetSize());
						}
					}

					++MetaIndex;
				}
			});
			BlockingOwner.Wait();

			// This directory must exist in order for the builder to run correctly
			TStringBuilder<256> BinPath;
			FPathViews::Append(BinPath, SandboxRoot, TEXT("Engine/Binaries/Win64"));
			IFileManager::Get().MakeDirectory(*BinPath);
		}

		// Manifest inputs in scratch area

		if (!Inputs.IsNull())
		{
			Inputs.Get().IterateInputs([&SandboxRoot](FUtf8StringView Key, const FCompressedBuffer& Buffer)
			{
				TStringBuilder<256> Path;
				FPathViews::Append(Path, SandboxRoot, TEXT("Inputs"), FIoHash(Buffer.GetRawHash()));
				if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Silent)})
				{
					// Workaround for FArchive::operator<< not accepting const objects
					FCompressedBuffer Copy = Buffer;
					*Ar << Copy;
				}
			});
		}

		// Serialize action specification

		{
			TStringBuilder<256> Path;
			FPathViews::Append(Path, SandboxRoot, TEXT("Build.action"));
			if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Silent)})
			{
				FCbWriter BuildActionWriter;
				Action.Save(BuildActionWriter);
				BuildActionWriter.Save(*Ar);
			}
		}

		const bool bLaunchDetached = false;
		const bool bLaunchHidden = false;
		const bool bLaunchReallyHidden = false;
		uint32 ProcessID = 0;
		const int PriorityModifier = 0;
		const TCHAR* WorkingDirectory = *SandboxRoot;
		TStringBuilder<256> WorkerPath;
		FPathViews::Append(WorkerPath, SandboxRoot, Worker.GetPath());

		FProcHandle ProcHandle = FPlatformProcess::CreateProc(
			*WorkerPath,
			TEXT("-Build=Build.action"),
			bLaunchDetached,
			bLaunchHidden,
			bLaunchReallyHidden,
			&ProcessID,
			PriorityModifier,
			WorkingDirectory,
			nullptr,
			nullptr);

		FPlatformProcess::WaitForProc(ProcHandle);

		int32 ExitCode = -1;
		if (!FPlatformProcess::GetProcReturnCode(ProcHandle, &ExitCode) || ExitCode != 0)
		{
			UE_LOG(LogDerivedDataBuildLocalExecutor, Warning, TEXT("Worker process exit code = %d!"), ExitCode);
			return OnComplete({Action.GetKey(), {}, {}, EStatus::Error});
		}

		// Gather results

		FOptionalBuildOutput RemoteBuildOutput;

		{
			TStringBuilder<256> OutputPath;
			FPathViews::Append(OutputPath, SandboxRoot, TEXT("Build.output"));
			if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileReader(*OutputPath, FILEREAD_Silent)})
			{
				FCbObject BuildOutput = LoadCompactBinary(*Ar).AsObject();

				if (Ar->IsError())
				{
					UE_LOG(LogDerivedDataBuildLocalExecutor, Warning, TEXT("Worker error: build output structure not valid!"));
					return OnComplete({Action.GetKey(), {}, {}, EStatus::Error});
				}

				RemoteBuildOutput = FBuildOutput::Load(Action.GetName(), Action.GetFunction(), BuildOutput);
			}
		}

		if (RemoteBuildOutput.IsNull())
		{
			UE_LOG(LogDerivedDataBuildLocalExecutor, Warning, TEXT("Remote execution system error: build output blob missing!"));
			return OnComplete({Action.GetKey(), {}, {}, EStatus::Error});
		}

		FBuildOutputBuilder OutputBuilder = BuildSystem.CreateOutput(Action.GetName(), Action.GetFunction());
		for (const FValueWithId& Value : RemoteBuildOutput.Get().GetValues())
		{
			if (EnumHasAnyFlags(Policy.GetValuePolicy(Value.GetId()), EBuildPolicy::SkipData))
			{
				OutputBuilder.AddValue(Value.GetId(), Value);
			}
			else
			{
				FCompressedBuffer BufferForValue;

				TStringBuilder<128> Path;
				FPathViews::Append(Path, SandboxRoot, TEXT("Outputs"), FIoHash(Value.GetRawHash()));
				if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileReader(*Path, FILEREAD_Silent)})
				{
					BufferForValue = FCompressedBuffer::Load(*Ar);
				}

				if (BufferForValue.IsNull())
				{
					UE_LOG(LogDerivedDataBuildLocalExecutor, Warning, TEXT("Remote execution system error: payload blob missing!"));
					return OnComplete({Action.GetKey(), {}, {}, EStatus::Error});
				}

				OutputBuilder.AddValue(Value.GetId(), FValue(MoveTemp(BufferForValue)));
			}
		}

		FBuildOutput BuildOutput = OutputBuilder.Build();
		return OnComplete({Action.GetKey(), BuildOutput, {}, EStatus::Ok});
	}

	TConstArrayView<FStringView> GetHostPlatforms() const final
	{
		static constexpr FStringView HostPlatforms[]{TEXTVIEW("Win64")};
		return HostPlatforms;
	}

	void DumpStats()
	{
	}
};

}  // namespace UE::DerivedData

TOptional<UE::DerivedData::FLocalBuildWorkerExecutor> GLocalBuildWorkerExecutor;

void InitDerivedDataBuildLocalExecutor()
{
	if (!GLocalBuildWorkerExecutor.IsSet())
	{
		GLocalBuildWorkerExecutor.Emplace();
	}
}

void DumpDerivedDataBuildLocalExecutorStats()
{
	static bool bHasRun = false;
	if (GLocalBuildWorkerExecutor.IsSet() && !bHasRun)
	{
		bHasRun = true;
		GLocalBuildWorkerExecutor->DumpStats();
	}
}
