// Copyright Epic Games, Inc. All Rights Reserved.

#include "DumpGPU.h"
#include "RenderGraph.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/WildcardString.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CoreDelegates.h"
#include "Runtime/Launch/Resources/Version.h"
#include "BuildSettings.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "RHIValidation.h"
#include "RenderGraphPrivate.h"

IDumpGPUUploadServiceProvider* IDumpGPUUploadServiceProvider::GProvider = nullptr;

FString IDumpGPUUploadServiceProvider::FDumpParameters::DumpServiceParametersFileContent() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("Type"), *Type);
	JsonObject->SetStringField(TEXT("Time"), *Time);
	JsonObject->SetStringField(TEXT("CompressionName"), *CompressionName.ToString());
	JsonObject->SetStringField(TEXT("CompressionFiles"), *CompressionFiles);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

bool IDumpGPUUploadServiceProvider::FDumpParameters::DumpServiceParametersFile() const
{
	FString FileName = LocalPath / kServiceFileName;
	FString OutputString = DumpServiceParametersFileContent();

	return FFileHelper::SaveStringToFile(OutputString, *FileName);
}

#if RDG_DUMP_RESOURCES

namespace
{

static TAutoConsoleVariable<FString> GDumpGPURootCVar(
	TEXT("r.DumpGPU.Root"),
	TEXT("*"),
	TEXT("Allows to filter the tree when using r.DumpGPU command, the pattern match is case sensitive."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpTextureCVar(
	TEXT("r.DumpGPU.Texture"), 2,
	TEXT("Whether to dump textures.\n")
	TEXT(" 0: Ignores all textures\n")
	TEXT(" 1: Dump only textures' descriptors\n")
	TEXT(" 2: Dump textures' descriptors and binaries (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> GDumpBufferCVar(
	TEXT("r.DumpGPU.Buffer"), 2,
	TEXT("Whether to dump buffer.\n")
	TEXT(" 0: Ignores all buffers\n")
	TEXT(" 1: Dump only buffers' descriptors\n")
	TEXT(" 2: Dump buffers' descriptors and binaries (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> GDumpMaxStagingSize(
	TEXT("r.DumpGPU.MaxStagingSize"), 64,
	TEXT("Maximum size of stating resource in MB (default=64)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> GDumpGPUPassParameters(
	TEXT("r.DumpGPU.PassParameters"), 1,
	TEXT("Whether to dump the pass parameters."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> GDumpGPUDraws(
	TEXT("r.DumpGPU.Draws"), 0,
	TEXT("Whether to dump resource after each individual draw call (disabled by default)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpGPUMask(
	TEXT("r.DumpGPU.Mask"), 1,
	TEXT("Whether to include GPU mask in the name of each Pass (has no effect unless system has multiple GPUs)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpExploreCVar(
	TEXT("r.DumpGPU.Explore"), 1,
	TEXT("Whether to open file explorer to where the GPU dump on completion (enabled by default)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpRenderingConsoleVariablesCVar(
	TEXT("r.DumpGPU.ConsoleVariables"), 1,
	TEXT("Whether to dump rendering console variables (enabled by default)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpTestEnableDiskWrite(
	TEXT("r.DumpGPU.Test.EnableDiskWrite"), 1,
	TEXT("Main switch whether any files should be written to disk, used for r.DumpGPU automation tests to not fill up workers' hard drive."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpTestPrettifyResourceFileNames(
	TEXT("r.DumpGPU.Test.PrettifyResourceFileNames"), 0,
	TEXT("Whether the resource file names should include resource name. May increase the likelyness of running into Windows' filepath limit."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<FString> GDumpGPUDirectoryCVar(
	TEXT("r.DumpGPU.Directory"), TEXT(""),
	TEXT("Directory to dump to."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpGPUUploadCVar(
	TEXT("r.DumpGPU.Upload"), 1,
	TEXT("Allows to upload the GPU dump automatically if set-up."),
	ECVF_Default);

static TAutoConsoleVariable<int32> GDumpGPUUploadCompressResources(
	TEXT("r.DumpGPU.Upload.CompressResources"), 1,
	TEXT("Whether to compress resource binary.\n")
	TEXT(" 0: Disabled (default)\n")
	TEXT(" 1: Zlib\n")
	TEXT(" 2: GZip"),
	ECVF_Default);

// Although this cvar does not seams used in the C++ code base, it is dumped by DumpRenderingCVarsToCSV() and used by GPUDumpViewer.html.
static TAutoConsoleVariable<FString> GDumpGPUVisualizeResource(
	TEXT("r.DumpGPU.Viewer.Visualize"), TEXT(""),
	TEXT("Name of RDG output resource to automatically open in the dump viewer."),
	ECVF_Default);

} // namespace

#endif // RDG_DUMP_RESOURCES

namespace
{

class FDumpTextureCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDumpTextureCS);
	SHADER_USE_PARAMETER_STRUCT(FDumpTextureCS, FGlobalShader);

	enum class ETextureType
	{
		Texture2DFloatNoMSAA,
		Texture2DUintNoMSAA,
		Texture2DDepthStencilNoMSAA,
		MAX
	};
	class FTextureTypeDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_TEXTURE_TYPE", ETextureType);
	using FPermutationDomain = TShaderPermutationDomain<FTextureTypeDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(Texture2D, Texture)
		SHADER_PARAMETER_UAV(RWTexture2D, StagingOutput)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDumpTextureCS, "/Engine/Private/Tools/DumpTexture.usf", "MainCS", SF_Compute);

} // namespace

#if RDG_DUMP_RESOURCES

namespace
{

BEGIN_SHADER_PARAMETER_STRUCT(FDumpTexturePass, )
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Texture)
	RDG_TEXTURE_ACCESS_DYNAMIC(TextureAccess)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FDumpBufferPass, )
	RDG_BUFFER_ACCESS(Buffer, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

}

class FRDGResourceDumpContext
{
public:
	static constexpr const TCHAR* kBaseDir = TEXT("Base/");
	static constexpr const TCHAR* kPassesDir = TEXT("Passes/");
	static constexpr const TCHAR* kResourcesDir = TEXT("Resources/");
	static constexpr const TCHAR* kStructuresDir = TEXT("Structures/");
	static constexpr const TCHAR* kStructuresMetadataDir = TEXT("StructuresMetadata/");

	bool bEnableDiskWrite = false;
	bool bUpload = false;
	FName UploadResourceCompressionName;
	FString DumpingDirectoryPath;
	FDateTime Time;
	FGenericPlatformMemoryConstants MemoryConstants;
	FGenericPlatformMemoryStats MemoryStats;
	int32 ResourcesDumpPasses = 0;
	int32 ResourcesDumpExecutedPasses = 0;
	int32 PassesCount = 0;
	TMap<const FRDGResource*, const FRDGPass*> LastResourceVersion;
	TSet<const void*> IsDumpedToDisk;

	// Pass being dumping individual draws
	const FRDGPass* DrawDumpingPass = nullptr;
	int32 DrawDumpCount = 0;

	int32 MetadataFilesOpened = 0;
	int64 MetadataFilesWriteBytes = 0;
	int32 ResourceBinaryFilesOpened = 0;
	int64 ResourceBinaryWriteBytes = 0;

	enum class ETimingBucket : uint8
	{
		RHIReadbackCommands,
		RHIReleaseResources,
		GPUWait,
		CPUPostProcessing,
		MetadataFileWrite,
		ResourceBinaryFileWrite,
		MAX
	};
	mutable double TimingBucket[int32(ETimingBucket::MAX)];

	class FTimeBucketMeasure
	{
	public:
		FTimeBucketMeasure(FRDGResourceDumpContext* InDumpCtx, ETimingBucket InBucket)
			: DumpCtx(InDumpCtx)
			, Start(FPlatformTime::Cycles64())
			, Bucket(InBucket)
		{ }

		~FTimeBucketMeasure()
		{
			uint64 End = FPlatformTime::Cycles64();
			DumpCtx->TimingBucket[int32(Bucket)] += FPlatformTime::ToSeconds64(FMath::Max(End - Start, uint64(0)));
		}

	protected:
		FRDGResourceDumpContext* const DumpCtx;

	private:
		const uint64 Start;
		const ETimingBucket Bucket;
	};

	class FFileWriteCtx : public FTimeBucketMeasure
	{
	public:
		FFileWriteCtx(FRDGResourceDumpContext* InDumpCtx, ETimingBucket InBucket, int64 WriteSizeBytes, int32 FilesOpened = 1)
			: FTimeBucketMeasure(InDumpCtx, InBucket)
		{
			if (InBucket == ETimingBucket::MetadataFileWrite)
			{
				DumpCtx->MetadataFilesOpened += FilesOpened;
				DumpCtx->MetadataFilesWriteBytes += WriteSizeBytes;
			}
			else if (InBucket == ETimingBucket::ResourceBinaryFileWrite)
			{
				DumpCtx->ResourceBinaryFilesOpened += FilesOpened;
				DumpCtx->ResourceBinaryWriteBytes += WriteSizeBytes;
			}
			else
			{
				unimplemented();
			}
		}
	};

	bool bShowInExplore = false;

	FRDGResourceDumpContext()
	{
		for (int32 i = 0; i < int32(ETimingBucket::MAX); i++)
		{
			TimingBucket[i] = 0.0f;
		}
	}

	IDumpGPUUploadServiceProvider::FDumpParameters GetDumpParameters() const
	{
		IDumpGPUUploadServiceProvider::FDumpParameters DumpServiceParameters;
		DumpServiceParameters.Type = TEXT("DumpGPU");
		DumpServiceParameters.LocalPath = DumpingDirectoryPath;
		DumpServiceParameters.Time = Time.ToString();
		return DumpServiceParameters;
	}

	FString GetDumpFullPath(const FString& DumpRelativeFileName) const
	{
		check(bEnableDiskWrite);
		return DumpingDirectoryPath / DumpRelativeFileName;
	}

	bool DumpStringToFile(FStringView OutputString, const FString& FileName, uint32 WriteFlags = FILEWRITE_None)
	{
		// Make it has if the write happened and was successful.
		if (!bEnableDiskWrite)
		{
			return true;
		}

		FString FullPath = GetDumpFullPath(FileName);

		FFileWriteCtx WriteCtx(this, ETimingBucket::MetadataFileWrite, OutputString.Len());
		return FFileHelper::SaveStringToFile(
			OutputString, *FullPath,
			FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), WriteFlags);
	}

	bool DumpStatusToFile(FStringView StatusString)
	{
		return DumpStringToFile(StatusString, FString(FRDGResourceDumpContext::kBaseDir) / TEXT("Status.txt"));
	}

	bool DumpJsonToFile(const TSharedPtr<FJsonObject>& JsonObject, const FString& FileName, uint32 WriteFlags = FILEWRITE_None)
	{
		FString OutputString;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

		return DumpStringToFile(OutputString, FileName, WriteFlags);
	}

	bool DumpBinaryToFile(TArrayView<const uint8> ArrayView, const FString& FileName)
	{
		// Make it has if the write happened and was successful.
		if (!bEnableDiskWrite)
		{
			return true;
		}

		FString FullPath = GetDumpFullPath(FileName);
		FFileWriteCtx WriteCtx(this, ETimingBucket::ResourceBinaryFileWrite, ArrayView.Num());
		return FFileHelper::SaveArrayToFile(ArrayView, *FullPath);
	}

	bool DumpBinaryToFile(const uint8* Data, int64 DataByteSize, const FString& FileName)
	{
		// Make it has if the write happened and was successful.
		if (!bEnableDiskWrite)
		{
			return true;
		}

		FString FullPath = GetDumpFullPath(FileName);
		FFileWriteCtx WriteCtx(this, ETimingBucket::ResourceBinaryFileWrite, DataByteSize);

		TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*FullPath, /* WriteFlags = */ 0));
		if (!Ar)
		{
			return false;
		}
		Ar->Serialize((void*)Data, DataByteSize);

		// Always explicitly close to catch errors from flush/close
		Ar->Close();

		return !Ar->IsError() && !Ar->IsCriticalError();
	}

	bool DumpResourceBinaryToFile(const uint8* UncompressedData, int64 UncompressedSize, const FString& FileName)
	{
		return DumpBinaryToFile(UncompressedData, UncompressedSize, FileName);
	}

	bool IsUnsafeToDumpResource(SIZE_T ResourceByteSize, float DumpMemoryMultiplier) const
	{
		const uint64 AproximatedStagingMemoryRequired = uint64(double(ResourceByteSize) * DumpMemoryMultiplier);

		// Skip AvailableVirtual if it's not supported (reported as exactly 0)
		const uint64 MaxMemoryAvailable = (MemoryStats.AvailableVirtual > 0) ? FMath::Min(MemoryStats.AvailablePhysical, MemoryStats.AvailableVirtual) : MemoryStats.AvailablePhysical;

		return AproximatedStagingMemoryRequired > MaxMemoryAvailable;
	}

	template<typename T>
	static uint64 PtrToUint(const T* Ptr)
	{
		return static_cast<uint64>(reinterpret_cast<size_t>(Ptr));
	}

	template<typename T>
	static FString PtrToString(const T* Ptr)
	{
		return FString::Printf(TEXT("%016x"), PtrToUint(Ptr));
	}

	static FString GetUniqueResourceName(const FRDGResource* Resource)
	{
		if (GDumpTestPrettifyResourceFileNames.GetValueOnRenderThread())
		{
			FString UniqueResourceName = FString::Printf(TEXT("%s.%016x"), Resource->Name, PtrToUint(Resource));
			UniqueResourceName.ReplaceInline(TEXT("/"), TEXT(""));
			UniqueResourceName.ReplaceInline(TEXT("\\"), TEXT(""));
			return UniqueResourceName;
		}
		return PtrToString(Resource);
	}

	static FString GetUniqueSubResourceName(const FRDGTextureSRVDesc& SubResourceDesc)
	{
		check(SubResourceDesc.NumMipLevels == 1);
		check(!SubResourceDesc.Texture->Desc.IsTextureArray() || SubResourceDesc.NumArraySlices == 1);

		FString UniqueResourceName = GetUniqueResourceName(SubResourceDesc.Texture);

		if (SubResourceDesc.Texture->Desc.IsTextureArray())
		{
			UniqueResourceName += FString::Printf(TEXT(".[%d]"), SubResourceDesc.FirstArraySlice);
		}

		if (SubResourceDesc.Format == PF_X24_G8)
		{
			return FString::Printf(TEXT("%s.stencil"), *UniqueResourceName);
		}

		return FString::Printf(TEXT("%s.mip%d"), *UniqueResourceName, SubResourceDesc.MipLevel);
	}

	void ReleaseRHIResources(FRHICommandListImmediate& RHICmdList)
	{
		FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::RHIReleaseResources);

		// Flush the RHI resource memory so the readback memory can be fully reused in the next resource dump.
		{
			RHICmdList.SubmitCommandsAndFlushGPU();
			RHICmdList.BlockUntilGPUIdle();
			FRHIResource::FlushPendingDeletes(RHICmdList);
			RHICmdList.FlushResources();
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
		}
	}

	void UpdatePassProgress()
	{
		ResourcesDumpExecutedPasses++;

		if (ResourcesDumpExecutedPasses % 10 == 0)
		{
			UE_LOG(LogRendererCore, Display, TEXT("Dumped %d / %d resources"), ResourcesDumpExecutedPasses, ResourcesDumpPasses);
		}
	}

	void DumpRenderingCVarsToCSV() const
	{
		FString FileName = GetDumpFullPath(FString(FRDGResourceDumpContext::kBaseDir) / TEXT("ConsoleVariables.csv"));

		TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*FileName));
		auto OnConsoleVariable = [&Ar](const TCHAR* CVarName, IConsoleObject* ConsoleObj)
		{
			if (ConsoleObj->TestFlags(ECVF_Unregistered))
			{
				return;
			}

			const IConsoleVariable* CVar = ConsoleObj->AsVariable();
			if (!CVar)
			{
				return;
			}

			EConsoleVariableFlags CVarFlags = CVar->GetFlags();

			const TCHAR* Type = nullptr;
			if (CVar->IsVariableBool())
			{
				Type = TEXT("bool");
			}
			else if (CVar->IsVariableInt())
			{
				Type = TEXT("int32");
			}
			else if (CVar->IsVariableFloat())
			{
				Type = TEXT("float");
			}
			else if (CVar->IsVariableString())
			{
				Type = TEXT("FString");
			}
			else
			{
				return;
			}

			const TCHAR* SetByName = TEXT("");
			switch (CVarFlags & ECVF_SetByMask)
			{
			case ECVF_SetByConstructor:
			{
				SetByName = TEXT("Constructor");
				break;
			}
			case ECVF_SetByScalability:
			{
				SetByName = TEXT("Scalability");
				break;
			}
			case ECVF_SetByGameSetting:
			{
				SetByName = TEXT("GameSetting");
				break;
			}
			case ECVF_SetByProjectSetting:
			{
				SetByName = TEXT("ProjectSetting");
				break;
			}
			case ECVF_SetBySystemSettingsIni:
			{
				SetByName = TEXT("SystemSettingsIni");
				break;
			}
			case ECVF_SetByDeviceProfile:
			{
				SetByName = TEXT("DeviceProfile");
				break;
			}
			case ECVF_SetByConsoleVariablesIni:
			{
				SetByName = TEXT("ConsoleVariablesIni");
				break;
			}
			case ECVF_SetByCommandline:
			{
				SetByName = TEXT("Commandline");
				break;
			}
			case ECVF_SetByCode:
			{
				SetByName = TEXT("Code");
				break;
			}
			case ECVF_SetByConsole:
			{
				SetByName = TEXT("Console");
				break;
			}
			default:
				unimplemented();
			}

			FString Value = CVar->GetString();

			FString CSVLine = FString::Printf(TEXT("%s,%s,%s,%s\n"), CVarName, Type, SetByName, *Value);
			FStringView CSVLineView(CSVLine);

			auto Src = StringCast<ANSICHAR>(CSVLineView.GetData(), CSVLineView.Len());
			Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
		};

		bool bSuccess = false;
		if (Ar)
		{
			{
				FString CSVLine = TEXT("CVar,Type,SetBy,Value\n");
				FStringView CSVLineView(CSVLine);

				auto Src = StringCast<ANSICHAR>(CSVLineView.GetData(), CSVLineView.Len());
				Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
			}

			if (GDumpRenderingConsoleVariablesCVar.GetValueOnGameThread() != 0)
			{
				IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda(OnConsoleVariable), TEXT(""));
			}
			else
			{
				IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda(OnConsoleVariable), TEXT("r.DumpGPU."));
			}
			// Always explicitly close to catch errors from flush/close
			Ar->Close();

			bSuccess = !Ar->IsError() && !Ar->IsCriticalError();
		}

		if (bSuccess)
		{
			UE_LOG(LogRendererCore, Display, TEXT("DumpGPU dumped rendering cvars to %s."), *FileName);
		}
		else
		{
			UE_LOG(LogRendererCore, Error, TEXT("DumpGPU had a file error when dumping rendering cvars to %s."), *FileName);
		}
	}

	template<typename T>
	bool IsDumped(const T* Ptr) const
	{
		return IsDumpedToDisk.Contains(static_cast<const void*>(Ptr));
	}

	template<typename T>
	void SetDumped(const T* Ptr)
	{
		check(!IsDumped(Ptr));
		if (IsDumpedToDisk.Num() % 1024 == 0)
		{
			IsDumpedToDisk.Reserve(IsDumpedToDisk.Num() + 1024);
		}
		IsDumpedToDisk.Add(static_cast<const void*>(Ptr));
	}

	bool HasResourceEverBeenDumped(const FRDGResource* Resource) const
	{
		return LastResourceVersion.Contains(Resource);
	}

	void RegisterResourceAsDumped(
		const FRDGResource* Resource,
		const FRDGPass* LastModifyingPass)
	{
		check(!LastResourceVersion.Contains(Resource));
		if (LastResourceVersion.Num() % 1024 == 0)
		{
			LastResourceVersion.Reserve(LastResourceVersion.Num() + 1024);
		}
		LastResourceVersion.Add(Resource, LastModifyingPass);
	}

	void UpdateResourceLastModifyingPass(
		const FRDGResource* Resource,
		const FRDGPass* LastModifyingPass)
	{
		check(LastResourceVersion.Contains(Resource));
		check(LastModifyingPass);
		LastResourceVersion[Resource] = LastModifyingPass;
	}

	FString ToJson(EPixelFormat Format)
	{
		FString PixelFormat = GPixelFormats[Format].Name;
		if (!PixelFormat.StartsWith(TEXT("PF_")))
		{
			PixelFormat = FString::Printf(TEXT("PF_%s"), GPixelFormats[Format].Name);
		}
		return PixelFormat;
	}

	TSharedPtr<FJsonObject> ToJson(const FShaderParametersMetadata::FMember& Member)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("Name"), Member.GetName());
		JsonObject->SetStringField(TEXT("ShaderType"), Member.GetShaderType());
		JsonObject->SetNumberField(TEXT("FileLine"), Member.GetFileLine());
		JsonObject->SetNumberField(TEXT("Offset"), Member.GetOffset());
		JsonObject->SetStringField(TEXT("BaseType"), GetUniformBufferBaseTypeString(Member.GetBaseType()));
		JsonObject->SetNumberField(TEXT("Precision"), Member.GetPrecision());
		JsonObject->SetNumberField(TEXT("NumRows"), Member.GetNumRows());
		JsonObject->SetNumberField(TEXT("NumColumns"), Member.GetNumColumns());
		JsonObject->SetNumberField(TEXT("NumElements"), Member.GetNumElements());
		JsonObject->SetStringField(TEXT("StructMetadata"), *PtrToString(Member.GetStructMetadata()));
		return JsonObject;
	}

	TSharedPtr<FJsonObject> ToJson(const FShaderParametersMetadata* Metadata)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("StructTypeName"), Metadata->GetStructTypeName());
		JsonObject->SetStringField(TEXT("ShaderVariableName"), Metadata->GetShaderVariableName());
		JsonObject->SetStringField(TEXT("FileName"), Metadata->GetFileName());
		JsonObject->SetNumberField(TEXT("FileLine"), Metadata->GetFileLine());
		JsonObject->SetNumberField(TEXT("Size"), Metadata->GetSize());
		//JsonObject->SetNumberField(TEXT("UseCase"), Metadata->GetUseCase());

		{
			TArray<TSharedPtr<FJsonValue>> Members;
			for (const FShaderParametersMetadata::FMember& Member : Metadata->GetMembers())
			{
				Members.Add(MakeShared<FJsonValueObject>(ToJson(Member)));
			}
			JsonObject->SetArrayField(TEXT("Members"), Members);
		}

		return JsonObject;
	}

	TSharedPtr<FJsonObject> ToJson(const FString& UniqueResourceName, const TCHAR* Name, const FRDGTextureDesc& Desc)
	{
		FString PixelFormat = ToJson(Desc.Format);

		int32 ResourceByteSize = Desc.Extent.X * Desc.Extent.Y * Desc.Depth * Desc.ArraySize * Desc.NumSamples * GPixelFormats[Desc.Format].BlockBytes;

		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("Name"), Name);
		JsonObject->SetStringField(TEXT("UniqueResourceName"), UniqueResourceName);
		JsonObject->SetNumberField(TEXT("ByteSize"), ResourceByteSize);
		JsonObject->SetStringField(TEXT("Desc"), TEXT("FRDGTextureDesc"));
		JsonObject->SetStringField(TEXT("Type"), GetTextureDimensionString(Desc.Dimension));
		JsonObject->SetStringField(TEXT("Format"), *PixelFormat);
		JsonObject->SetNumberField(TEXT("ExtentX"), Desc.Extent.X);
		JsonObject->SetNumberField(TEXT("ExtentY"), Desc.Extent.Y);
		JsonObject->SetNumberField(TEXT("Depth"), Desc.Depth);
		JsonObject->SetNumberField(TEXT("ArraySize"), Desc.ArraySize);
		JsonObject->SetNumberField(TEXT("NumMips"), Desc.NumMips);
		JsonObject->SetNumberField(TEXT("NumSamples"), Desc.NumSamples);

		{
			TArray<TSharedPtr<FJsonValue>> FlagsNames;
			for (uint64 BitId = 0; BitId < uint64(8 * sizeof(ETextureCreateFlags)); BitId++)
			{
				ETextureCreateFlags Flag = ETextureCreateFlags(uint64(1) << BitId);
				if (EnumHasAnyFlags(Desc.Flags, Flag))
				{
					FlagsNames.Add(MakeShareable(new FJsonValueString(GetTextureCreateFlagString(Flag))));
				}
			}
			JsonObject->SetArrayField(TEXT("Flags"), FlagsNames);
		}

		return JsonObject;
	}

	TSharedPtr<FJsonObject> ToJson(const FString& UniqueResourceName, const TCHAR* Name, const FRDGBufferDesc& Desc, int32 ResourceByteSize)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("Name"), Name);
		JsonObject->SetStringField(TEXT("UniqueResourceName"), UniqueResourceName);
		JsonObject->SetNumberField(TEXT("ByteSize"), ResourceByteSize);
		JsonObject->SetStringField(TEXT("Desc"), TEXT("FRDGBufferDesc"));
		JsonObject->SetNumberField(TEXT("BytesPerElement"), Desc.BytesPerElement);
		JsonObject->SetNumberField(TEXT("NumElements"), Desc.NumElements);
		JsonObject->SetStringField(TEXT("Metadata"), *PtrToString(Desc.Metadata));

		{
			TArray<TSharedPtr<FJsonValue>> UsageNames;
			for (uint64 BitId = 0; BitId < uint64(8 * sizeof(EBufferUsageFlags)); BitId++)
			{
				EBufferUsageFlags Flag = EBufferUsageFlags(uint64(1) << BitId);
				if (EnumHasAnyFlags(Desc.Usage, Flag))
				{
					UsageNames.Add(MakeShareable(new FJsonValueString(GetBufferUsageFlagString(Flag))));
				}
			}
			JsonObject->SetArrayField(TEXT("Usage"), UsageNames);
		}
		return JsonObject;
	}

	void Dump(const FShaderParametersMetadata* Metadata)
	{
		if (IsDumped(Metadata))
		{
			return;
		}

		TSharedPtr<FJsonObject> JsonObject = ToJson(Metadata);
		FString JsonPath = kStructuresMetadataDir / PtrToString(Metadata) + TEXT(".json");
		DumpJsonToFile(JsonObject, JsonPath);
		SetDumped(Metadata);

		// Dump dependencies
		Metadata->IterateStructureMetadataDependencies(
		[&](const FShaderParametersMetadata* Struct)
		{
			if (Struct)
			{
				Dump(Struct);
			}
		});
	}

	struct FTextureSubresourceDumpDesc
	{
		FIntPoint SubResourceExtent = FIntPoint(0, 0);
		SIZE_T ByteSize = 0;
		bool bPreprocessForStaging = false;
		FDumpTextureCS::ETextureType DumpTextureType = FDumpTextureCS::ETextureType::MAX;
		EPixelFormat PreprocessedPixelFormat;

		bool IsDumpSupported() const
		{
			return ByteSize != SIZE_T(0);
		}
	};
	
	FTextureSubresourceDumpDesc TranslateSubresourceDumpDesc(FRDGTextureSRVDesc SubresourceDesc)
	{
		check(SubresourceDesc.NumMipLevels == 1);
		check(!SubresourceDesc.Texture->Desc.IsTextureArray() || SubresourceDesc.NumArraySlices == 1);

		const FRDGTextureDesc& Desc = SubresourceDesc.Texture->Desc;

		FTextureSubresourceDumpDesc SubresourceDumpDesc;
		SubresourceDumpDesc.PreprocessedPixelFormat = Desc.Format;

		bool bIsUnsupported = false;

		// We support uncompressing certain BC formats, as the DumpGPU viewer doesn't support compressed formats.  Compression
		// is used by Lumen, so it's extremely useful to preview.  Additional code below selects the uncompressed format (BC4 is
		// scalar, so we use PF_R32_FLOAT, BC5 is dual channel, so it uses PF_G16R16F, etc).  If you add a format here, you must
		// also update Engine\Extras\GPUDumpViewer\GPUDumpViewer.html (see "translate_subresource_desc").
		if ((GPixelFormats[Desc.Format].BlockSizeX != 1 ||
			 GPixelFormats[Desc.Format].BlockSizeY != 1 ||
			 GPixelFormats[Desc.Format].BlockSizeZ != 1) &&
			!(Desc.Format == PF_BC4 ||
			  Desc.Format == PF_BC5 ||
			  Desc.Format == PF_BC6H ||
			  Desc.Format == PF_BC7))
		{
			bIsUnsupported = true;
		}

		if (!bIsUnsupported && Desc.IsTexture2D() && !Desc.IsMultisample())
		{
			SubresourceDumpDesc.SubResourceExtent.X = Desc.Extent.X >> SubresourceDesc.MipLevel;
			SubresourceDumpDesc.SubResourceExtent.Y = Desc.Extent.Y >> SubresourceDesc.MipLevel;

			if (IsUintFormat(Desc.Format) || IsSintFormat(Desc.Format))
			{
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DUintNoMSAA;
			}
			else
			{
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}

			if (SubresourceDesc.Format == PF_X24_G8)
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_R8_UINT;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DDepthStencilNoMSAA;
			}
			else if (Desc.Format == PF_DepthStencil)
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_R32_FLOAT;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}
			else if (Desc.Format == PF_ShadowDepth)
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_R32_FLOAT;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}
			else if (Desc.Format == PF_D24)
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_R32_FLOAT;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}
			else if (Desc.Format == PF_BC4)
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_R32_FLOAT;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}
			else if (Desc.Format == PF_BC5)
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_G16R16F;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}
			else if ((Desc.Format == PF_BC6H) || (Desc.Format == PF_BC7))
			{
				SubresourceDumpDesc.PreprocessedPixelFormat = PF_FloatRGBA;
				SubresourceDumpDesc.DumpTextureType = FDumpTextureCS::ETextureType::Texture2DFloatNoMSAA;
			}
		}

		SubresourceDumpDesc.ByteSize = SIZE_T(SubresourceDumpDesc.SubResourceExtent.X) * SIZE_T(SubresourceDumpDesc.SubResourceExtent.Y) * SIZE_T(GPixelFormats[SubresourceDumpDesc.PreprocessedPixelFormat].BlockBytes);

		// Whether the subresource need preprocessing pass before copy into staging.
		{
			// If need a pixel format conversion, use a pixel shader to do it.
			SubresourceDumpDesc.bPreprocessForStaging |= SubresourceDumpDesc.PreprocessedPixelFormat != Desc.Format;

			// If the texture has a mip chain, use pixel shader to correctly copy the right mip given RHI doesn't support copy from mip levels. Also on Mip 0 to avoid bugs on D3D11
			SubresourceDumpDesc.bPreprocessForStaging |= SubresourceDesc.Texture->Desc.NumMips > 1;

			// If the texture is an array, use pixel shader to correctly copy the right slice given RHI doesn't support copy from slices
			SubresourceDumpDesc.bPreprocessForStaging |= SubresourceDesc.Texture->Desc.IsTextureArray();
		}

		return SubresourceDumpDesc;
	}

	void DumpTextureSubResource(
		FRHICommandListImmediate& RHICmdList,
		const TCHAR* TextureDebugName,
		FRHITexture* Texture,
		FRHIShaderResourceView* SubResourceSRV,
		const FTextureSubresourceDumpDesc& SubresourceDumpDesc,
		const FString& DumpFilePath)
	{
		check(IsInRenderingThread());

		// Preprocess
		FTextureRHIRef StagingSrcTexture;
		EPixelFormat PreprocessedPixelFormat = SubresourceDumpDesc.PreprocessedPixelFormat;
		SIZE_T SubresourceByteSize = SubresourceDumpDesc.ByteSize;
		FTextureRHIRef StagingTexture;

		{
			FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::RHIReadbackCommands);
			if (SubresourceDumpDesc.bPreprocessForStaging)
			{
				// Some RHIs (GL) only support 32Bit single channel images as CS output
				if (IsOpenGLPlatform(GMaxRHIShaderPlatform) &&
					GPixelFormats[PreprocessedPixelFormat].NumComponents == 1 && 
					GPixelFormats[PreprocessedPixelFormat].BlockBytes < 4)
				{
					SubresourceByteSize*= (4 / GPixelFormats[PreprocessedPixelFormat].BlockBytes);
					PreprocessedPixelFormat = PF_R32_UINT;
				}

				{
					const FRHITextureCreateDesc Desc =
						FRHITextureCreateDesc::Create2D(TEXT("DumpGPU.PreprocessTexture"), SubresourceDumpDesc.SubResourceExtent, PreprocessedPixelFormat)
						.SetFlags(ETextureCreateFlags::UAV | ETextureCreateFlags::ShaderResource | ETextureCreateFlags::HideInVisualizeTexture);

					StagingSrcTexture = RHICreateTexture(Desc);
				}

				FUnorderedAccessViewRHIRef StagingOutput = RHICreateUnorderedAccessView(StagingSrcTexture, /* MipLevel = */ 0);

				RHICmdList.Transition(FRHITransitionInfo(StagingSrcTexture, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

				FDumpTextureCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FDumpTextureCS::FTextureTypeDim>(SubresourceDumpDesc.DumpTextureType);
				TShaderMapRef<FDumpTextureCS> ComputeShader(GetGlobalShaderMap(GMaxRHIShaderPlatform), PermutationVector);

				FDumpTextureCS::FParameters ShaderParameters;
				ShaderParameters.Texture = SubResourceSRV;
				ShaderParameters.StagingOutput = StagingOutput;
				FComputeShaderUtils::Dispatch(
					RHICmdList,
					ComputeShader,
					ShaderParameters,
					FComputeShaderUtils::GetGroupCount(SubresourceDumpDesc.SubResourceExtent, 8));

				RHICmdList.Transition(FRHITransitionInfo(StagingSrcTexture, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));
			}
			else
			{
				StagingSrcTexture = Texture;
			}

			// Copy the texture for CPU readback
			{
				const FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create2D(TEXT("DumpGPU.StagingTexture"), SubresourceDumpDesc.SubResourceExtent, PreprocessedPixelFormat)
					.SetFlags(ETextureCreateFlags::CPUReadback | ETextureCreateFlags::HideInVisualizeTexture);

				StagingTexture = RHICreateTexture(Desc);

				RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));

				// Transfer memory GPU -> CPU
				RHICmdList.CopyTexture(StagingSrcTexture, StagingTexture, {});

				RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopyDest, ERHIAccess::CPURead));
			}
		}

		// Submit to GPU and wait for completion.
		static const FName FenceName(TEXT("DumpGPU.TextureFence"));
		FGPUFenceRHIRef Fence = RHICreateGPUFence(FenceName);
		{
			FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::GPUWait);

			Fence->Clear();
			RHICmdList.WriteGPUFence(Fence);
			RHICmdList.SubmitCommandsAndFlushGPU();
			RHICmdList.BlockUntilGPUIdle();
		}

		void* Content = nullptr;
		int32 RowPitchInPixels = 0;
		int32 ColumnPitchInPixels = 0;

		// jhoerner_todo 12/9/2021:  pick arbitrary GPU out of mask to avoid assert.  Eventually want to dump results for all GPUs, but
		// I need to understand how to modify the dumping logic, and this works for now (usually when debugging, the bugs happen on
		// secondary GPUs, so I figure the last index is most useful if we need to pick one).  I also would like the dump to include
		// information about the GPUMask for each pass, and perhaps have the dump include the final state of all external resources
		// modified by the graph (especially useful for MGPU, where we are concerned about cross-view or cross-frame state).
		uint32 GPUIndex = RHICmdList.GetGPUMask().GetLastIndex();

		RHICmdList.MapStagingSurface(StagingTexture, Fence.GetReference(), Content, RowPitchInPixels, ColumnPitchInPixels, GPUIndex);

		if (Content)
		{
			TArray64<uint8> Array;
			{
				FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::CPUPostProcessing);

				Array.SetNumUninitialized(SubresourceByteSize);

				SIZE_T BytePerPixel = SIZE_T(GPixelFormats[PreprocessedPixelFormat].BlockBytes);

				const uint8* SrcData = static_cast<const uint8*>(Content);

				for (int32 y = 0; y < SubresourceDumpDesc.SubResourceExtent.Y; y++)
				{
					// Flip the data to be bottom left corner for the WebGL viewer.
					const uint8* SrcPos = SrcData + SIZE_T(SubresourceDumpDesc.SubResourceExtent.Y - 1 - y) * SIZE_T(RowPitchInPixels) * BytePerPixel;
					uint8* DstPos = (&Array[0]) + SIZE_T(y) * SIZE_T(SubresourceDumpDesc.SubResourceExtent.X) * BytePerPixel;

					FPlatformMemory::Memmove(DstPos, SrcPos, SIZE_T(SubresourceDumpDesc.SubResourceExtent.X) * BytePerPixel);
				}

				RHICmdList.UnmapStagingSurface(StagingTexture, GPUIndex);

				if (PreprocessedPixelFormat != SubresourceDumpDesc.PreprocessedPixelFormat)
				{
					// Convert 32Bit values back to 16 or 8bit
					const int32 DstPixelNumBytes = GPixelFormats[SubresourceDumpDesc.PreprocessedPixelFormat].BlockBytes;
					const uint32* SrcData32 = (const uint32*)Array.GetData();
					uint8* DstData8 = Array.GetData();
					uint16* DstData16 = (uint16*)Array.GetData();
											
					for (int32 Index = 0; Index < Array.Num()/4; Index++)
					{
						uint32 Value32 = SrcData32[Index];
						if (DstPixelNumBytes == 2)
						{
							DstData16[Index] = (uint16)Value32;
						}
						else
						{
							DstData8[Index] = (uint8)Value32;
						}
					}

					Array.SetNum(Array.Num() / (4 / DstPixelNumBytes));
				}
			}

			DumpResourceBinaryToFile(Array.GetData(), Array.Num(), DumpFilePath);
		}
		else
		{
			UE_LOG(LogRendererCore, Warning, TEXT("RHICmdList.MapStagingSurface() to dump texture %s failed."), TextureDebugName);
		}
	}

	void DumpDrawTextureSubResource(
		FRHICommandList& RHICmdList,
		FRDGTextureSRVDesc SubresourceDesc,
		ERHIAccess RHIAccessState)
	{
		check(IsInRenderingThread());

		FRHICommandListImmediate& RHICmdListImmediate = FRHICommandListExecutor::GetImmediateCommandList();
		check(&RHICmdListImmediate == &RHICmdList);

		const FString UniqueResourceSubResourceName = GetUniqueSubResourceName(SubresourceDesc);
		const FTextureSubresourceDumpDesc SubresourceDumpDesc = TranslateSubresourceDumpDesc(SubresourceDesc);

		if (!SubresourceDumpDesc.IsDumpSupported())
		{
			return;
		}

		FRHITexture* RHITexture = SubresourceDesc.Texture->GetRHI();

		FShaderResourceViewRHIRef SubResourceSRV;
		if (SubresourceDumpDesc.bPreprocessForStaging)
		{
			SubResourceSRV = RHICreateShaderResourceView(RHITexture, FRHITextureSRVCreateInfo(SubresourceDesc));
			RHICmdListImmediate.Transition(FRHITransitionInfo(RHITexture, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		}
		else
		{
			RHICmdListImmediate.Transition(FRHITransitionInfo(RHITexture, RHIAccessState, ERHIAccess::CopySrc));
		}

		FString DumpFilePath = kResourcesDir / FString::Printf(
			TEXT("%s.v%016x.d%d.bin"),
			*UniqueResourceSubResourceName,
			PtrToUint(DrawDumpingPass),
			DrawDumpCount);

		DumpTextureSubResource(
			RHICmdListImmediate,
			SubresourceDesc.Texture->Name,
			RHITexture,
			SubResourceSRV,
			SubresourceDumpDesc,
			DumpFilePath);

		if (SubresourceDumpDesc.bPreprocessForStaging)
		{
			RHICmdListImmediate.Transition(FRHITransitionInfo(RHITexture, ERHIAccess::SRVCompute, RHIAccessState));
		}
		else
		{
			RHICmdListImmediate.Transition(FRHITransitionInfo(RHITexture, ERHIAccess::CopySrc, RHIAccessState));
		}

		SubResourceSRV = nullptr;
		ReleaseRHIResources(RHICmdListImmediate);
	}

	void AddDumpTextureSubResourcePass(
		FRDGBuilder& GraphBuilder,
		TArray<TSharedPtr<FJsonValue>>& InputResourceNames,
		TArray<TSharedPtr<FJsonValue>>& OutputResourceNames,
		const FRDGPass* Pass,
		FRDGTextureSRVDesc SubresourceDesc,
		bool bRegisterSubResourceToPass,
		bool bIsOutputResource,
		bool bAllowDumpBinary)
	{
		int32 DumpTextureMode = GDumpTextureCVar.GetValueOnRenderThread();

		if (DumpTextureMode == 0)
		{
			return;
		}

		const FRDGTextureDesc& Desc = SubresourceDesc.Texture->Desc;
		const FString UniqueResourceSubResourceName = GetUniqueSubResourceName(SubresourceDesc);
		const FTextureSubresourceDumpDesc SubresourceDumpDesc = TranslateSubresourceDumpDesc(SubresourceDesc);

		if (bRegisterSubResourceToPass)
		{
			if (bIsOutputResource)
			{
				OutputResourceNames.AddUnique(MakeShareable(new FJsonValueString(UniqueResourceSubResourceName)));
			}
			else
			{
				InputResourceNames.AddUnique(MakeShareable(new FJsonValueString(UniqueResourceSubResourceName)));
			}
		}

		// Early return if this resource shouldn't be dumped.
		if (!bAllowDumpBinary || DumpTextureMode != 2)
		{
			return;
		}

		if (!SubresourceDumpDesc.IsDumpSupported())
		{
			return;
		}

		// Verify there is enough available memory to dump the resource.
		if (IsUnsafeToDumpResource(SubresourceDumpDesc.ByteSize, 2.2f + (SubresourceDumpDesc.bPreprocessForStaging ? 1.0f : 0.0f)))
		{
			UE_LOG(LogRendererCore, Warning, TEXT("Not dumping %s because of insuficient memory available for staging texture."), SubresourceDesc.Texture->Name);
			return;
		}

		// Verify the texture is able to do resource transitions.
		if (EnumHasAnyFlags(SubresourceDesc.Texture->Flags, ERDGTextureFlags::SkipTracking))
		{
			UE_LOG(LogRendererCore, Warning, TEXT("Not dumping %s because has ERDGTextureFlags::SkipTracking."), SubresourceDesc.Texture->Name);
			return;
		}

		const FRDGViewableResource::FAccessModeState AccessModeState = SubresourceDesc.Texture->AccessModeState;

		if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::External)
		{
			GraphBuilder.UseInternalAccessMode(SubresourceDesc.Texture);
		}

		// Dump the resource's binary to a .bin file.
		{
			FString DumpFilePath = kResourcesDir / FString::Printf(TEXT("%s.v%016x.bin"), *UniqueResourceSubResourceName, PtrToUint(bIsOutputResource ? Pass : nullptr));

			FDumpTexturePass* PassParameters = GraphBuilder.AllocParameters<FDumpTexturePass>();
			if (SubresourceDumpDesc.bPreprocessForStaging)
			{
				if (!(SubresourceDesc.Texture->Desc.Flags & TexCreate_ShaderResource))
				{
					UE_LOG(LogRendererCore, Warning, TEXT("Not dumping %s because requires copy to staging texture using compute, but is missing TexCreate_ShaderResource."), *UniqueResourceSubResourceName);
					return;
				}

				PassParameters->Texture = GraphBuilder.CreateSRV(SubresourceDesc);
			}
			else
			{
				PassParameters->TextureAccess = FRDGTextureAccess(SubresourceDesc.Texture, ERHIAccess::CopySrc);
			}

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RDG DumpTexture(%s -> %s) %dx%d",
					SubresourceDesc.Texture->Name, *DumpFilePath,
					SubresourceDumpDesc.SubResourceExtent.X, SubresourceDumpDesc.SubResourceExtent.Y),
				PassParameters,
				(SubresourceDumpDesc.bPreprocessForStaging ? ERDGPassFlags::Compute : ERDGPassFlags::Copy) | ERDGPassFlags::NeverCull,
				[
					PassParameters, this, DumpFilePath,
					SubresourceDesc, SubresourceDumpDesc]
				(FRHICommandListImmediate& RHICmdList)
			{

				this->DumpTextureSubResource(
					RHICmdList,
					SubresourceDesc.Texture->Name,
					SubresourceDumpDesc.bPreprocessForStaging ? nullptr : SubresourceDesc.Texture->GetRHI(),
					SubresourceDumpDesc.bPreprocessForStaging ? PassParameters->Texture->GetRHI() : nullptr,
					SubresourceDumpDesc,
					DumpFilePath);
				this->ReleaseRHIResources(RHICmdList);
				this->UpdatePassProgress();
			});

			ResourcesDumpPasses++;
		}

		if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::External)
		{
			GraphBuilder.UseExternalAccessMode(SubresourceDesc.Texture, AccessModeState.Access, AccessModeState.Pipelines);
		}
	}

	void AddDumpTextureAllSubResourcesPasses(
		FRDGBuilder& GraphBuilder,
		TArray<TSharedPtr<FJsonValue>>& InputResourceNames,
		TArray<TSharedPtr<FJsonValue>>& OutputResourceNames,
		const FRDGPass* Pass,
		const FRDGTextureSRVDesc& SubresourceRangeDesc,
		bool bRegisterSubResourceToPass,
		bool bIsOutputResource,
		bool bAllowDumpBinary)
	{
		if (SubresourceRangeDesc.Texture->Desc.IsTextureArray() && SubresourceRangeDesc.NumArraySlices != 1)
		{
			// If this is an array, recursively dump all subresource of each individual array slice.
			uint32 ArraySliceStart = SubresourceRangeDesc.FirstArraySlice;
			uint32 ArraySliceCount = (SubresourceRangeDesc.NumArraySlices > 0) ? SubresourceRangeDesc.NumArraySlices : (SubresourceRangeDesc.Texture->Desc.ArraySize - SubresourceRangeDesc.FirstArraySlice);

			for (uint32 ArraySlice = ArraySliceStart; ArraySlice < (ArraySliceStart + ArraySliceCount); ArraySlice++)
			{
				FRDGTextureSRVDesc SubresourceDesc = SubresourceRangeDesc;
				SubresourceDesc.FirstArraySlice = ArraySlice;
				SubresourceDesc.NumArraySlices = 1;

				AddDumpTextureAllSubResourcesPasses(
					GraphBuilder,
					InputResourceNames,
					OutputResourceNames,
					Pass,
					SubresourceDesc,
					bRegisterSubResourceToPass,
					bIsOutputResource,
					bAllowDumpBinary);
			}
		}
		else if (SubresourceRangeDesc.Format == PF_X24_G8)
		{
			// Dump the stencil.
			AddDumpTextureSubResourcePass(
				GraphBuilder,
				InputResourceNames,
				OutputResourceNames,
				Pass,
				SubresourceRangeDesc,
				bRegisterSubResourceToPass,
				bIsOutputResource,
				bAllowDumpBinary);
		}
		else
		{
			for (int32 MipLevel = SubresourceRangeDesc.MipLevel; MipLevel < (SubresourceRangeDesc.MipLevel + SubresourceRangeDesc.NumMipLevels); MipLevel++)
			{
				FRDGTextureSRVDesc SubresourceDesc = SubresourceRangeDesc;
				SubresourceDesc.MipLevel = MipLevel;
				SubresourceDesc.NumMipLevels = 1;

				AddDumpTextureSubResourcePass(
					GraphBuilder,
					InputResourceNames,
					OutputResourceNames,
					Pass,
					SubresourceDesc,
					bRegisterSubResourceToPass,
					bIsOutputResource,
					bAllowDumpBinary);
			}
		}
	}

	void AddDumpTexturePasses(
		FRDGBuilder& GraphBuilder,
		TArray<TSharedPtr<FJsonValue>>& InputResourceNames,
		TArray<TSharedPtr<FJsonValue>>& OutputResourceNames,
		const FRDGPass* Pass,
		const FRDGTextureSRVDesc& SubresourceRangeDesc,
		bool bIsOutputResource)
	{
		bool bAllowDumpBinary = true;
		if (!HasResourceEverBeenDumped(SubresourceRangeDesc.Texture))
		{
			RegisterResourceAsDumped(SubresourceRangeDesc.Texture, /* LastModifyingPass = */ bIsOutputResource ? Pass : nullptr);

			// Dump the descriptor of the resource
			{
				const FString UniqueResourceName = GetUniqueResourceName(SubresourceRangeDesc.Texture);
				TSharedPtr<FJsonObject> JsonObject = ToJson(UniqueResourceName, SubresourceRangeDesc.Texture->Name, SubresourceRangeDesc.Texture->Desc);
				DumpJsonToFile(JsonObject, FString(FRDGResourceDumpContext::kBaseDir) / TEXT("ResourceDescs.json"), FILEWRITE_Append);
			}

			// If the resource is input but has not been dumped ever before, make sure to dump all of its sub resources instead of just the sub resource described by SubresourceRangeDesc
			if (!bIsOutputResource)
			{
				// Dump the stencil if there is one
				if (IsStencilFormat(SubresourceRangeDesc.Texture->Desc.Format))
				{
					AddDumpTextureAllSubResourcesPasses(
						GraphBuilder,
						InputResourceNames,
						OutputResourceNames,
						Pass,
						FRDGTextureSRVDesc::CreateWithPixelFormat(SubresourceRangeDesc.Texture, PF_X24_G8),
						/* bRegisterSubResourceToPass = */ false,
						bIsOutputResource,
						/* bAllowDumpBinary = */ true);
				}

				// Dump all the remaining sub resource (mip levels, array slices...)
				AddDumpTextureAllSubResourcesPasses(
					GraphBuilder,
					InputResourceNames,
					OutputResourceNames,
					Pass,
					FRDGTextureSRVDesc::Create(SubresourceRangeDesc.Texture),
					/* bRegisterSubResourceToPass = */ false,
					bIsOutputResource,
					/* bAllowDumpBinary = */ true);

				// Still register the input subresources used without dump any binary.
				return AddDumpTextureAllSubResourcesPasses(
					GraphBuilder,
					InputResourceNames,
					OutputResourceNames,
					Pass,
					SubresourceRangeDesc,
					/* bRegisterSubResourceToPass = */ true,
					bIsOutputResource,
					/* bAllowDumpBinary = */ false);
			}
		}
		else if (bIsOutputResource)
		{
			UpdateResourceLastModifyingPass(SubresourceRangeDesc.Texture, Pass);
		}
		else
		{
			// The texture has already been dumped, so can early return given it is not being modified by this Pass.
			bAllowDumpBinary = false;
		}

		return AddDumpTextureAllSubResourcesPasses(
			GraphBuilder,
			InputResourceNames,
			OutputResourceNames,
			Pass,
			SubresourceRangeDesc,
			/* bRegisterSubResourceToPass = */ true,
			bIsOutputResource,
			bAllowDumpBinary);
	}

	void AddDumpBufferPass(
		FRDGBuilder& GraphBuilder,
		TArray<TSharedPtr<FJsonValue>>& InputResourceNames,
		TArray<TSharedPtr<FJsonValue>>& OutputResourceNames,
		const FRDGPass* Pass,
		FRDGBuffer* Buffer,
		bool bIsOutputResource)
	{
		int32 DumpBufferMode = GDumpTextureCVar.GetValueOnRenderThread();

		if (DumpBufferMode == 0)
		{
			return;
		}

		FString UniqueResourceName = GetUniqueResourceName(Buffer);

		if (bIsOutputResource)
		{
			OutputResourceNames.AddUnique(MakeShareable(new FJsonValueString(UniqueResourceName)));
		}
		else
		{
			InputResourceNames.AddUnique(MakeShareable(new FJsonValueString(UniqueResourceName)));
		}

		const FRDGBufferDesc& Desc = Buffer->Desc;
		const int32 ByteSize = Desc.GetSize();

		bool bDumpResourceInfos = false;
		bool bDumpResourceBinary = bIsOutputResource;
		{
			if (!HasResourceEverBeenDumped(Buffer))
			{
				// First time we ever see this buffer, so dump it's info to disk
				bDumpResourceInfos = true;

				// If not an output, it might be a resource undumped by r.DumpGPU.Root or external texture so still dump it as v0.
				if (!bIsOutputResource)
				{
					bDumpResourceBinary = true;
				}

				RegisterResourceAsDumped(Buffer, bIsOutputResource ? Pass : nullptr);
			}
			else if (bIsOutputResource)
			{
				UpdateResourceLastModifyingPass(Buffer, Pass);
			}
		}

		// Dump the information of the buffer to json file.
		if (bDumpResourceInfos)
		{
			TSharedPtr<FJsonObject> JsonObject = ToJson(UniqueResourceName, Buffer->Name, Desc, ByteSize);
			DumpJsonToFile(JsonObject, FString(FRDGResourceDumpContext::kBaseDir) / TEXT("ResourceDescs.json"), FILEWRITE_Append);

			if (Desc.Metadata && DumpBufferMode == 2)
			{
				Dump(Desc.Metadata);
			}
		}

		// Dump the resource's binary to a .bin file.
		if (bDumpResourceBinary && DumpBufferMode == 2)
		{
			int32 StagingResourceByteSize = FMath::Min(ByteSize, GDumpMaxStagingSize.GetValueOnRenderThread() * 1024 * 1024);

			FString DumpFilePath = kResourcesDir / FString::Printf(TEXT("%s.v%016x.bin"), *UniqueResourceName, PtrToUint(bIsOutputResource ? Pass : nullptr));

			if (IsUnsafeToDumpResource(StagingResourceByteSize, 1.2f))
			{
				UE_LOG(LogRendererCore, Warning, TEXT("Not dumping %s because of insuficient memory available for staging buffer."), *DumpFilePath);
				return;
			}

			// Verify the texture is able to do resource transitions.
			if (EnumHasAnyFlags(Buffer->Flags, ERDGBufferFlags::SkipTracking))
			{
				UE_LOG(LogRendererCore, Warning, TEXT("Not dumping %s because has ERDGBufferFlags::SkipTracking."), Buffer->Name);
				return;
			}

			const FRDGViewableResource::FAccessModeState AccessModeState = Buffer->AccessModeState;

			if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::External)
			{
				GraphBuilder.UseInternalAccessMode(Buffer);
			}

			FDumpBufferPass* PassParameters = GraphBuilder.AllocParameters<FDumpBufferPass>();
			PassParameters->Buffer = Buffer;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RDG DumpBuffer(%s -> %s)", Buffer->Name, *DumpFilePath),
				PassParameters,
				ERDGPassFlags::Readback,
				[this, DumpFilePath, Buffer, ByteSize, StagingResourceByteSize](FRHICommandListImmediate& RHICmdList)
			{
				check(IsInRenderingThread());

				FStagingBufferRHIRef StagingBuffer;
				FGPUFenceRHIRef Fence;
				{
					FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::RHIReadbackCommands);
					StagingBuffer = RHICreateStagingBuffer();

					static const FName FenceName(TEXT("DumpGPU.BufferFence"));
					Fence = RHICreateGPUFence(FenceName);
				}
				
				TUniquePtr<FArchive> Ar;
				if (bEnableDiskWrite)
				{
					FString FullPath = GetDumpFullPath(DumpFilePath);
					FFileWriteCtx WriteCtx(this, ETimingBucket::ResourceBinaryFileWrite, /* ByteSize = */ 0);

					Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*FullPath, /* WriteFlags = */ 0));
				}

				for (int32 Offset = 0; Offset < ByteSize; Offset += StagingResourceByteSize)
				{
					int32 CopyByteSize = FMath::Min(StagingResourceByteSize, ByteSize - Offset);

					// Transfer memory GPU -> CPU
					{
						FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::RHIReadbackCommands);
						RHICmdList.CopyToStagingBuffer(Buffer->GetRHI(), StagingBuffer, Offset, CopyByteSize);
					}

					// Submit to GPU and wait for completion.
					{
						FTimeBucketMeasure TimeBucketMeasure(this, ETimingBucket::GPUWait);
						Fence->Clear();
						RHICmdList.WriteGPUFence(Fence);
						RHICmdList.SubmitCommandsAndFlushGPU();
						RHICmdList.BlockUntilGPUIdle();
					}

					void* Content = RHICmdList.LockStagingBuffer(StagingBuffer, Fence.GetReference(), 0, CopyByteSize);
					if (Content)
					{
						if (Ar)
						{
							FFileWriteCtx WriteCtx(this, ETimingBucket::ResourceBinaryFileWrite, CopyByteSize, /* OpenedFiles = */ 0);
							Ar->Serialize((void*)Content, CopyByteSize);
							Ar->Flush();
						}
						RHICmdList.UnlockStagingBuffer(StagingBuffer);
					}
					else
					{
						UE_LOG(LogRendererCore, Warning, TEXT("RHICmdList.LockStagingBuffer() to dump buffer %s failed."), Buffer->Name);
						break;
					}
				}

				if (Ar)
				{
					FFileWriteCtx WriteCtx(this, ETimingBucket::ResourceBinaryFileWrite, /* ByteSize = */ 0, /* OpenedFiles = */ 0);
					// Always explicitly close to catch errors from flush/close
					Ar->Close();
					Ar = nullptr;
				}

				StagingBuffer = nullptr;
				Fence = nullptr;
				this->ReleaseRHIResources(RHICmdList);
				this->UpdatePassProgress();
			});

			if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::External)
			{
				GraphBuilder.UseExternalAccessMode(Buffer, AccessModeState.Access, AccessModeState.Pipelines);
			}

			ResourcesDumpPasses++;
		}
	}

	// Look whether the pass matches matches r.DumpGPU.Root
	bool IsDumpingPass(const FRDGPass* Pass)
	{
		FString RootWildcardString = GDumpGPURootCVar.GetValueOnRenderThread();
		FWildcardString WildcardFilter(RootWildcardString);

		bool bDumpPass = (RootWildcardString == TEXT("*"));

		if (!bDumpPass)
		{
			bDumpPass = WildcardFilter.IsMatch(Pass->GetEventName().GetTCHAR());
		}

		#if RDG_GPU_DEBUG_SCOPES
		if (!bDumpPass)
		{
			const FRDGEventScope* ParentScope = Pass->GetGPUScopes().Event;

			while (ParentScope)
			{
				bDumpPass = bDumpPass || WildcardFilter.IsMatch(ParentScope->Name.GetTCHAR());
				ParentScope = ParentScope->ParentScope;
			}
		}
		#endif

		return bDumpPass;
	}

	void OnCrash()
	{
		if (GLog)
		{
			GLog->Panic();
		}
		DumpStatusToFile(TEXT("crash"));
		FGenericCrashContext::DumpLog(DumpingDirectoryPath / FRDGResourceDumpContext::kBaseDir);
	}
};

// 0 = not dumping, MAX_uint64 dump request for next frame, otherwise dump frame counter
static uint64 DumpingFrameCounter_GameThread = 0;
static FRDGResourceDumpContext* GRDGResourceDumpContext = nullptr;

bool IsDumpingRDGResources()
{
	return GRDGResourceDumpContext != nullptr;
}

void FRDGBuilder::InitResourceDump()
{
	if(DumpingFrameCounter_GameThread == MAX_uint64)
	{
		DumpingFrameCounter_GameThread = GFrameCounter;
	}
}

FString FRDGBuilder::BeginResourceDump(const TCHAR* Cmd)
{
	check(IsInGameThread());

	if (DumpingFrameCounter_GameThread != 0)
	{
		return FString();
	}

	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	{
		const TCHAR* LocalCmd = Cmd;

		FString NextToken;
		while (FParse::Token(LocalCmd, NextToken, false))
		{
			if (**NextToken == TCHAR('-'))
			{
				new(Switches) FString(NextToken.Mid(1));
			}
			else
			{
				new(Tokens) FString(NextToken);
			}
		}

		for (int32 SwitchIdx = Switches.Num() - 1; SwitchIdx >= 0; --SwitchIdx)
		{
			FString& Switch = Switches[SwitchIdx];
			TArray<FString> SplitSwitch;

			// Remove Bar=1 from the switch list and put it in params as {Bar,1}.
			// Note: Handle nested equality such as Bar="Key=Value"
			int32 AssignmentIndex = 0;
			if (Switch.FindChar(TEXT('='), AssignmentIndex))
			{
				Parameters.Add(Switch.Left(AssignmentIndex), Switch.RightChop(AssignmentIndex + 1).TrimQuotes());
				Switches.RemoveAt(SwitchIdx);
			}
		}
	}

	FRDGResourceDumpContext* NewResourceDumpContext = new FRDGResourceDumpContext;

	NewResourceDumpContext->Time = FDateTime::Now();
	{
		FString CVarDirectoryPath = GDumpGPUDirectoryCVar.GetValueOnGameThread();
		FString EnvDirectoryPath = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-DumpGPUPath"));

		FString DirectoryPath;
		if (!CVarDirectoryPath.IsEmpty())
		{
			DirectoryPath = CVarDirectoryPath;
		}
		else if (!EnvDirectoryPath.IsEmpty())
		{
			DirectoryPath = EnvDirectoryPath;
		}
		else
		{
			DirectoryPath = FPaths::ProjectSavedDir() / TEXT("GPUDumps/");
		}
		NewResourceDumpContext->DumpingDirectoryPath = DirectoryPath / FApp::GetProjectName() + TEXT("-") + FPlatformProperties::PlatformName() + TEXT("-") + NewResourceDumpContext->Time.ToString() + TEXT("/");
	}
	NewResourceDumpContext->bEnableDiskWrite = GDumpTestEnableDiskWrite.GetValueOnGameThread() != 0;

	if (Switches.Contains(TEXT("upload")))
	{
		if (!IDumpGPUUploadServiceProvider::GProvider || !NewResourceDumpContext->bEnableDiskWrite)
		{
			UE_LOG(LogRendererCore, Warning, TEXT("DumpGPU upload services are not set up."));
		}
		else if (GDumpGPUUploadCVar.GetValueOnGameThread() == 0)
		{
			UE_LOG(LogRendererCore, Warning, TEXT("DumpGPU upload services are not available because r.DumpGPU.Upload=0."));
		}
		else
		{
			NewResourceDumpContext->bUpload = true;
		}
	}

	if (NewResourceDumpContext->bUpload)
	{
		if (GDumpGPUUploadCompressResources.GetValueOnGameThread() == 1)
		{
			NewResourceDumpContext->UploadResourceCompressionName = NAME_Zlib;
		}
		else if (GDumpGPUUploadCompressResources.GetValueOnGameThread() == 2)
		{
			NewResourceDumpContext->UploadResourceCompressionName = NAME_Gzip;
		}
	}

	NewResourceDumpContext->bShowInExplore = NewResourceDumpContext->bEnableDiskWrite && GDumpExploreCVar.GetValueOnGameThread() != 0 && !NewResourceDumpContext->bUpload;
	NewResourceDumpContext->MemoryConstants = FPlatformMemory::GetConstants();
	NewResourceDumpContext->MemoryStats = FPlatformMemory::GetStats();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (NewResourceDumpContext->bEnableDiskWrite)
	{
		if (!PlatformFile.DirectoryExists(*NewResourceDumpContext->DumpingDirectoryPath))
		{
			PlatformFile.CreateDirectoryTree(*NewResourceDumpContext->DumpingDirectoryPath);
		}
		PlatformFile.CreateDirectoryTree(*(NewResourceDumpContext->DumpingDirectoryPath / FRDGResourceDumpContext::kBaseDir));
		PlatformFile.CreateDirectoryTree(*(NewResourceDumpContext->DumpingDirectoryPath / FRDGResourceDumpContext::kResourcesDir));

		NewResourceDumpContext->DumpStringToFile(TEXT(""), FString(FRDGResourceDumpContext::kBaseDir) / TEXT("Passes.json"));
		NewResourceDumpContext->DumpStringToFile(TEXT(""), FString(FRDGResourceDumpContext::kBaseDir) / TEXT("ResourceDescs.json"));
		NewResourceDumpContext->DumpStringToFile(TEXT(""), FString(FRDGResourceDumpContext::kBaseDir) / TEXT("PassDrawCounts.json"));
	}

	// Dump status file and register NewResourceDumpContext to listen for OnShutdownAfterError to get a log of what happened with callstack.
	{
		NewResourceDumpContext->DumpStatusToFile(TEXT("dumping"));
		FCoreDelegates::OnShutdownAfterError.AddRaw(NewResourceDumpContext, &FRDGResourceDumpContext::OnCrash);
	}

	// Dump service parameters so GPUDumpViewer.html remain compatible when not using upload provider.
	if (NewResourceDumpContext->bEnableDiskWrite)
	{
		NewResourceDumpContext->GetDumpParameters().DumpServiceParametersFile();
	}

	// Output informations
	{
		const TCHAR* BranchName = BuildSettings::GetBranchName();
		const TCHAR* BuildDate = BuildSettings::GetBuildDate();
		const TCHAR* BuildVersion = BuildSettings::GetBuildVersion();

		FString BuildConfiguration = LexToString(FApp::GetBuildConfiguration());
		FString BuildTarget = LexToString(FApp::GetBuildTargetType());

		FString OSLabel, OSVersion;
		FPlatformMisc::GetOSVersions(OSLabel, OSVersion);
		if (!OSVersion.IsEmpty())
		{
			OSLabel = FString::Printf(TEXT("%s %s"), *OSLabel, *OSVersion);
		}

		FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);

		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("Project"), FApp::GetProjectName());
		JsonObject->SetNumberField(TEXT("EngineMajorVersion"), ENGINE_MAJOR_VERSION);
		JsonObject->SetNumberField(TEXT("EngineMinorVersion"), ENGINE_MINOR_VERSION);
		JsonObject->SetNumberField(TEXT("EnginePatchVersion"), ENGINE_PATCH_VERSION);
		JsonObject->SetStringField(TEXT("BuildBranch"), BranchName ? BranchName : TEXT(""));
		JsonObject->SetStringField(TEXT("BuildDate"), BuildDate ? BuildDate : TEXT(""));
		JsonObject->SetStringField(TEXT("BuildVersion"), BuildVersion ? BuildVersion : TEXT(""));
		JsonObject->SetStringField(TEXT("BuildTarget"), BuildTarget);
		JsonObject->SetStringField(TEXT("BuildConfiguration"), BuildConfiguration);
		JsonObject->SetNumberField(TEXT("Build64Bits"), (PLATFORM_64BITS ? 1 : 0));
		JsonObject->SetStringField(TEXT("Platform"), FPlatformProperties::IniPlatformName());
		JsonObject->SetStringField(TEXT("OS"), OSLabel);
		JsonObject->SetStringField(TEXT("DeviceName"), FPlatformProcess::ComputerName());
		JsonObject->SetStringField(TEXT("CPUVendor"), FPlatformMisc::GetCPUVendor());
		JsonObject->SetStringField(TEXT("CPUBrand"), FPlatformMisc::GetCPUBrand());
		JsonObject->SetNumberField(TEXT("CPUNumberOfCores"), FPlatformMisc::NumberOfCores());
		JsonObject->SetNumberField(TEXT("CPUNumberOfCoresIncludingHyperthreads"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
		JsonObject->SetStringField(TEXT("GPUVendor"), RHIVendorIdToString());
		JsonObject->SetStringField(TEXT("GPUDeviceDescription"), GPUDriverInfo.DeviceDescription);
		JsonObject->SetStringField(TEXT("GPUDriverUserVersion"), GPUDriverInfo.UserDriverVersion);
		JsonObject->SetStringField(TEXT("GPUDriverInternalVersion"), GPUDriverInfo.GetUnifiedDriverVersion());
		JsonObject->SetStringField(TEXT("GPUDriverDate"), GPUDriverInfo.DriverDate);
		JsonObject->SetNumberField(TEXT("MemoryTotalPhysical"), NewResourceDumpContext->MemoryConstants.TotalPhysical);
		JsonObject->SetNumberField(TEXT("MemoryPageSize"), NewResourceDumpContext->MemoryConstants.PageSize);
		JsonObject->SetStringField(TEXT("RHI"), GDynamicRHI->GetName());
		JsonObject->SetStringField(TEXT("RHIMaxFeatureLevel"), LexToString(GMaxRHIFeatureLevel));
		JsonObject->SetStringField(TEXT("DumpTime"), NewResourceDumpContext->Time.ToString());

		NewResourceDumpContext->DumpJsonToFile(JsonObject, FString(FRDGResourceDumpContext::kBaseDir) / TEXT("Infos.json"));
	}

	// Dump the rendering cvars
	if (NewResourceDumpContext->bEnableDiskWrite)
	{
		NewResourceDumpContext->DumpRenderingCVarsToCSV();
	}

	// Copy the viewer
	if (NewResourceDumpContext->bEnableDiskWrite)
	{
		const TCHAR* OpenGPUDumpViewerBatName = TEXT("OpenGPUDumpViewer.bat");
		const TCHAR* OpenGPUDumpViewerShName = TEXT("OpenGPUDumpViewer.sh");

		const TCHAR* ViewerHTML = TEXT("GPUDumpViewer.html");
		FString DumpGPUViewerSourcePath = FPaths::EngineDir() + FString(TEXT("Extras")) / TEXT("GPUDumpViewer");

		PlatformFile.CopyFile(*(NewResourceDumpContext->DumpingDirectoryPath / ViewerHTML), *(DumpGPUViewerSourcePath / ViewerHTML));
		PlatformFile.CopyFile(*(NewResourceDumpContext->DumpingDirectoryPath / OpenGPUDumpViewerBatName), *(DumpGPUViewerSourcePath / OpenGPUDumpViewerBatName));
		PlatformFile.CopyFile(*(NewResourceDumpContext->DumpingDirectoryPath / OpenGPUDumpViewerShName), *(DumpGPUViewerSourcePath / OpenGPUDumpViewerShName));
	}

	ENQUEUE_RENDER_COMMAND(FStartGPUDump)(
		[NewResourceDumpContext](FRHICommandListImmediate& ImmediateRHICmdList)
	{
		check(IsInRenderingThread());
		GRDGResourceDumpContext = NewResourceDumpContext;

		ImmediateRHICmdList.SubmitCommandsAndFlushGPU();

		// Disable the validation for BUF_SourceCopy so that all buffers can be copied into staging buffer for CPU readback.
		#if ENABLE_RHI_VALIDATION
			GRHIValidateBufferSourceCopy = false;
		#endif
	});

	// Mark ready for dump on next available frame
	DumpingFrameCounter_GameThread = MAX_uint64;

	if (NewResourceDumpContext->bEnableDiskWrite)
	{
		return NewResourceDumpContext->DumpingDirectoryPath;
	}
	return FString();
}

void FRDGBuilder::EndResourceDump()
{
	check(IsInGameThread());

	 // make sure at least one frame has passed since we start a resource dump and we are not waiting on the dump to begin
	if (DumpingFrameCounter_GameThread == 0 ||
		DumpingFrameCounter_GameThread == MAX_uint64 ||
		DumpingFrameCounter_GameThread >= GFrameCounter)
	{
		return;
	}

	// Wait all rendering commands are completed to finish with GRDGResourceDumpContext.
	{
		UE_LOG(LogRendererCore, Display, TEXT("Stalling game thread until render thread finishes to dump resources"));

		ENQUEUE_RENDER_COMMAND(FEndGPUDump)(
			[](FRHICommandListImmediate& ImmediateRHICmdList)
		{
			ImmediateRHICmdList.SubmitCommandsAndFlushGPU();
			#if ENABLE_RHI_VALIDATION
				GRHIValidateBufferSourceCopy = true;
			#endif
		});

		FlushRenderingCommands();
	}

	// Log information about the dump.
	FString AbsDumpingDirectoryPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*GRDGResourceDumpContext->DumpingDirectoryPath);
	{
		FDateTime Now = FDateTime::Now();
		double TotalDumpSeconds = (Now - GRDGResourceDumpContext->Time).GetTotalSeconds();
		double RHIReadbackCommandsSeconds = GRDGResourceDumpContext->TimingBucket[int32(FRDGResourceDumpContext::ETimingBucket::RHIReadbackCommands)];
		double GPUWaitSeconds = GRDGResourceDumpContext->TimingBucket[int32(FRDGResourceDumpContext::ETimingBucket::GPUWait)];
		double CPUPostProcessingSeconds = GRDGResourceDumpContext->TimingBucket[int32(FRDGResourceDumpContext::ETimingBucket::CPUPostProcessing)];
		double MetadataFileWriteSeconds = GRDGResourceDumpContext->TimingBucket[int32(FRDGResourceDumpContext::ETimingBucket::MetadataFileWrite)];
		double ResourceBinaryFileWriteSeconds = GRDGResourceDumpContext->TimingBucket[int32(FRDGResourceDumpContext::ETimingBucket::ResourceBinaryFileWrite)];
		double RHIReleaseResourcesTimeSeconds = GRDGResourceDumpContext->TimingBucket[int32(FRDGResourceDumpContext::ETimingBucket::RHIReleaseResources)];

		UE_LOG(LogRendererCore, Display, TEXT("Dumped %d resources in %.3f s to %s"), GRDGResourceDumpContext->ResourcesDumpPasses, float(TotalDumpSeconds), *AbsDumpingDirectoryPath);
		UE_LOG(LogRendererCore, Display, TEXT("Dumped GPU readback commands: %.3f s"), float(RHIReadbackCommandsSeconds));
		UE_LOG(LogRendererCore, Display, TEXT("Dumped GPU wait: %.3f s"), float(GPUWaitSeconds));
		UE_LOG(LogRendererCore, Display, TEXT("Dumped CPU resource binary post processing: %.3f s"), float(CPUPostProcessingSeconds));
		UE_LOG(LogRendererCore, Display, TEXT("Dumped metadata: %.3f MB in %d files under %.3f s at %.3f MB/s"),
			float(GRDGResourceDumpContext->MetadataFilesWriteBytes) / float(1024 * 1024),
			GRDGResourceDumpContext->MetadataFilesOpened,
			float(MetadataFileWriteSeconds),
			float(GRDGResourceDumpContext->MetadataFilesWriteBytes) / (float(1024 * 1024) * float(MetadataFileWriteSeconds)));
		UE_LOG(LogRendererCore, Display, TEXT("Dumped resource binary: %.3f MB in %d files under %.3f s at %.3f MB/s"),
			float(GRDGResourceDumpContext->ResourceBinaryWriteBytes) / float(1024 * 1024),
			GRDGResourceDumpContext->ResourceBinaryFilesOpened,
			float(ResourceBinaryFileWriteSeconds),
			float(GRDGResourceDumpContext->ResourceBinaryWriteBytes) / (float(1024 * 1024) * float(ResourceBinaryFileWriteSeconds)));
		UE_LOG(LogRendererCore, Display, TEXT("Dumped GPU readback resource release: %.3f s"), float(RHIReleaseResourcesTimeSeconds));
	}

	// Dump the log into the dump directory.
	if (GRDGResourceDumpContext->bEnableDiskWrite)
	{
		if (GLog)
		{
			GLog->FlushThreadedLogs();
			GLog->Flush();
		}
		FGenericCrashContext::DumpLog(GRDGResourceDumpContext->DumpingDirectoryPath / FRDGResourceDumpContext::kBaseDir);
	}

	// Update the dump status to OK and removes subscription to crashes
	{
		GRDGResourceDumpContext->DumpStatusToFile(TEXT("ok"));
		FCoreDelegates::OnShutdownAfterError.RemoveAll(GRDGResourceDumpContext);
	}

	if (GRDGResourceDumpContext->bUpload && IDumpGPUUploadServiceProvider::GProvider)
	{
		IDumpGPUUploadServiceProvider::FDumpParameters DumpCompletedParameters = GRDGResourceDumpContext->GetDumpParameters();

		// Compress the resource binary in background before uploading.
		if (!GRDGResourceDumpContext->UploadResourceCompressionName.IsNone())
		{
			DumpCompletedParameters.CompressionName = GRDGResourceDumpContext->UploadResourceCompressionName;
			DumpCompletedParameters.CompressionFiles = FWildcardString(TEXT("*.bin"));
		}

		IDumpGPUUploadServiceProvider::GProvider->UploadDump(DumpCompletedParameters);
	}

	#if PLATFORM_DESKTOP
	if (GRDGResourceDumpContext->bShowInExplore)
	{
		FPlatformProcess::ExploreFolder(*AbsDumpingDirectoryPath);
	}
	#endif

	delete GRDGResourceDumpContext;
	GRDGResourceDumpContext = nullptr;
	DumpingFrameCounter_GameThread = 0;
}

static const TCHAR* GetPassEventNameWithGPUMask(const FRDGPass* Pass, FString& OutNameStorage)
{
#if WITH_MGPU
	if ((GNumExplicitGPUsForRendering > 1) && GDumpGPUMask.GetValueOnRenderThread())
	{
		// Prepend GPU mask on the event name of each pass, so you can see which GPUs the pass ran on.  Putting the mask at the
		// front rather than the back makes all the masks line up, and easier to read (or ignore if you don't care about them).
		// Also, it's easy to globally search for passes with a particular GPU mask using name search in the dump browser.
		OutNameStorage = FString::Printf(TEXT("[%x] %s"), Pass->GetGPUMask().GetNative(), Pass->GetEventName().GetTCHAR());
		return *OutNameStorage;
	}
	else
#endif  // WITH_MGPU
	{
		return Pass->GetEventName().GetTCHAR();
	}
}

void FRDGBuilder::DumpResourcePassOutputs(const FRDGPass* Pass)
{
	if (!AuxiliaryPasses.IsDumpAllowed())
	{
		return;
	}

	if (!GRDGResourceDumpContext)
	{
		return;
	}

	check(IsInRenderingThread());
	if (!GRDGResourceDumpContext->IsDumpingPass(Pass))
	{
		return;
	}

	RDG_RECURSION_COUNTER_SCOPE(AuxiliaryPasses.Dump);

	TArray<TSharedPtr<FJsonValue>> InputResourceNames;
	TArray<TSharedPtr<FJsonValue>> OutputResourceNames;
	Pass->GetParameters().Enumerate([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RDG_TEXTURE:
		{
			if (FRDGTextureRef Texture = Parameter.GetAsTexture())
			{
				FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::Create(Texture);
				GRDGResourceDumpContext->AddDumpTexturePasses(*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource, /* bIsOutputResource = */ false);
			}
		}
		break;
		case UBMT_RDG_TEXTURE_SRV:
		{
			if (FRDGTextureSRVRef SRV = Parameter.GetAsTextureSRV())
			{
				if (SRV->Desc.MetaData == ERHITextureMetaDataAccess::None)
				{
					GRDGResourceDumpContext->AddDumpTexturePasses(*this, InputResourceNames, OutputResourceNames, Pass, SRV->Desc, /* bIsOutputResource = */ false);
				}
				else
				{
					UE_LOG(LogRendererCore, Warning, TEXT("Dumping texture %s's meta data unsupported"), SRV->Desc.Texture->Name);
				}
			}
		}
		break;
		case UBMT_RDG_TEXTURE_UAV:
		{
			if (FRDGTextureUAVRef UAV = Parameter.GetAsTextureUAV())
			{
				if (UAV->Desc.MetaData == ERHITextureMetaDataAccess::None)
				{
					FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::Create(UAV->Desc.Texture);
					TextureSubResource.MipLevel = UAV->Desc.MipLevel;
					TextureSubResource.NumMipLevels = 1;
					if (UAV->Desc.Texture->Desc.IsTextureArray())
					{
						TextureSubResource.FirstArraySlice = UAV->Desc.FirstArraySlice;
						TextureSubResource.NumArraySlices = UAV->Desc.NumArraySlices;
					}
					GRDGResourceDumpContext->AddDumpTexturePasses(*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource, /* bIsOutputResource = */ true);
				}
				else
				{
					UE_LOG(LogRendererCore, Warning, TEXT("Dumping texture %s's meta data unsupported"), UAV->Desc.Texture->Name);
				}
			}
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS:
		{
			if (FRDGTextureAccess TextureAccess = Parameter.GetAsTextureAccess())
			{
				bool bIsOutputResource = (
					TextureAccess.GetAccess() == ERHIAccess::UAVCompute ||
					TextureAccess.GetAccess() == ERHIAccess::UAVGraphics ||
					TextureAccess.GetAccess() == ERHIAccess::RTV);

				FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::Create(TextureAccess);
				GRDGResourceDumpContext->AddDumpTexturePasses(*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource, bIsOutputResource);
			}
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
		{
			const FRDGTextureAccessArray& TextureAccessArray = Parameter.GetAsTextureAccessArray();

			for (FRDGTextureAccess TextureAccess : TextureAccessArray)
			{
				bool bIsOutputResource = (
					TextureAccess.GetAccess() == ERHIAccess::UAVCompute ||
					TextureAccess.GetAccess() == ERHIAccess::UAVGraphics ||
					TextureAccess.GetAccess() == ERHIAccess::RTV);

				FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::Create(TextureAccess);
				GRDGResourceDumpContext->AddDumpTexturePasses(*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource, bIsOutputResource);
			}
		}
		break;

		case UBMT_RDG_BUFFER_SRV:
		{
			if (FRDGBufferSRVRef SRV = Parameter.GetAsBufferSRV())
			{
				FRDGBufferRef Buffer = SRV->Desc.Buffer;
				GRDGResourceDumpContext->AddDumpBufferPass(*this, InputResourceNames, OutputResourceNames, Pass, Buffer, /* bIsOutputResource = */ false);
			}
		}
		break;
		case UBMT_RDG_BUFFER_UAV:
		{
			if (FRDGBufferUAVRef UAV = Parameter.GetAsBufferUAV())
			{
				FRDGBufferRef Buffer = UAV->Desc.Buffer;
				GRDGResourceDumpContext->AddDumpBufferPass(*this, InputResourceNames, OutputResourceNames, Pass, Buffer, /* bIsOutputResource = */ true);
			}
		}
		break;
		case UBMT_RDG_BUFFER_ACCESS:
		{
			if (FRDGBufferAccess BufferAccess = Parameter.GetAsBufferAccess())
			{
				bool bIsOutputResource = (
					BufferAccess.GetAccess() == ERHIAccess::UAVCompute ||
					BufferAccess.GetAccess() == ERHIAccess::UAVGraphics);

				GRDGResourceDumpContext->AddDumpBufferPass(*this, InputResourceNames, OutputResourceNames, Pass, BufferAccess, bIsOutputResource);
			}
		}
		break;
		case UBMT_RDG_BUFFER_ACCESS_ARRAY:
		{
			const FRDGBufferAccessArray& BufferAccessArray = Parameter.GetAsBufferAccessArray();

			for (FRDGBufferAccess BufferAccess : BufferAccessArray)
			{
				bool bIsOutputResource = (
					BufferAccess.GetAccess() == ERHIAccess::UAVCompute ||
					BufferAccess.GetAccess() == ERHIAccess::UAVGraphics);

				GRDGResourceDumpContext->AddDumpBufferPass(*this, InputResourceNames, OutputResourceNames, Pass, BufferAccess, bIsOutputResource);
			}
		}
		break;

		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			const FRenderTargetBindingSlots& RenderTargets = Parameter.GetAsRenderTargetBindingSlots();

			RenderTargets.Enumerate([&](FRenderTargetBinding RenderTarget)
			{
				FRDGTextureRef Texture = RenderTarget.GetTexture();
				FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::CreateForMipLevel(Texture, RenderTarget.GetMipIndex());

				if (Texture->Desc.IsTextureArray())
				{
					TextureSubResource.FirstArraySlice = RenderTarget.GetArraySlice();
					TextureSubResource.NumArraySlices = 1;
				}

				GRDGResourceDumpContext->AddDumpTexturePasses(*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource, /* bIsOutputResource = */ true);
			});

			const FDepthStencilBinding& DepthStencil = RenderTargets.DepthStencil;

			if (FRDGTextureRef Texture = DepthStencil.GetTexture())
			{
				FExclusiveDepthStencil DepthStencilAccess = DepthStencil.GetDepthStencilAccess();

				if (DepthStencilAccess.IsUsingDepth())
				{
					FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::CreateForMipLevel(Texture, 0);
					if (Texture->Desc.IsTextureArray())
					{
						TextureSubResource.FirstArraySlice = 0;
						TextureSubResource.NumArraySlices = 1;
					}

					GRDGResourceDumpContext->AddDumpTexturePasses(
						*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource,
						/* bIsOutputResource = */ DepthStencilAccess.IsDepthWrite());
				}

				if (DepthStencilAccess.IsUsingStencil())
				{
					FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::CreateWithPixelFormat(Texture, PF_X24_G8);
					if (Texture->Desc.IsTextureArray())
					{
						TextureSubResource.FirstArraySlice = 0;
						TextureSubResource.NumArraySlices = 1;
					}

					GRDGResourceDumpContext->AddDumpTexturePasses(
						*this, InputResourceNames, OutputResourceNames, Pass, TextureSubResource,
						/* bIsOutputResource = */ DepthStencilAccess.IsStencilWrite());
				}
			}
		}
		break;
		}
	});

	// Dump the pass informations
	{
		TArray<TSharedPtr<FJsonValue>> ParentEventScopeNames;
		#if RDG_GPU_DEBUG_SCOPES
		{
			const FRDGEventScope* ParentScope = Pass->GetGPUScopes().Event;

			while (ParentScope)
			{
				ParentEventScopeNames.Add(MakeShareable(new FJsonValueString(ParentScope->Name.GetTCHAR())));
				ParentScope = ParentScope->ParentScope;
			}
		}
		#endif
		{
			ParentEventScopeNames.Add(MakeShareable(new FJsonValueString(FString::Printf(TEXT("Frame %llu"), GFrameCounterRenderThread))));
		}

		FString EventNameStorage;

		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("EventName"), GetPassEventNameWithGPUMask(Pass, EventNameStorage));
		JsonObject->SetStringField(TEXT("ParametersName"), Pass->GetParameters().GetLayout().GetDebugName());
		JsonObject->SetStringField(TEXT("Parameters"), FRDGResourceDumpContext::PtrToString(Pass->GetParameters().GetContents()));
		JsonObject->SetStringField(TEXT("ParametersMetadata"), FRDGResourceDumpContext::PtrToString(Pass->GetParameters().GetMetadata()));
		JsonObject->SetStringField(TEXT("Pointer"), FString::Printf(TEXT("%016x"), FRDGResourceDumpContext::PtrToUint(Pass)));
		JsonObject->SetNumberField(TEXT("Id"), GRDGResourceDumpContext->PassesCount);
		JsonObject->SetArrayField(TEXT("ParentEventScopes"), ParentEventScopeNames);
		JsonObject->SetArrayField(TEXT("InputResources"), InputResourceNames);
		JsonObject->SetArrayField(TEXT("OutputResources"), OutputResourceNames);

		GRDGResourceDumpContext->DumpJsonToFile(JsonObject, FString(FRDGResourceDumpContext::kBaseDir) / TEXT("Passes.json"), FILEWRITE_Append);
	}

	// Dump the pass' parameters
	if (GDumpGPUPassParameters.GetValueOnRenderThread() != 0)
	{
		int32 PassParametersByteSize = 0;
		{
			const FShaderParametersMetadata* Metadata = Pass->GetParameters().GetMetadata();
			if (Metadata)
			{
				GRDGResourceDumpContext->Dump(Metadata);
				PassParametersByteSize = Metadata->GetSize();
			}
		}

		if (PassParametersByteSize == 0 && Pass->GetParameters().GetLayoutPtr())
		{
			PassParametersByteSize = Pass->GetParameters().GetLayout().ConstantBufferSize;
		}

		const uint8* PassParametersContent = Pass->GetParameters().GetContents();
		if (PassParametersContent && !GRDGResourceDumpContext->IsDumped(PassParametersContent))
		{
			TArrayView<const uint8> ArrayView(PassParametersContent, PassParametersByteSize);
			FString DumpFilePath = FRDGResourceDumpContext::kStructuresDir / FRDGResourceDumpContext::PtrToString(PassParametersContent) + TEXT(".bin");
			GRDGResourceDumpContext->DumpBinaryToFile(ArrayView, DumpFilePath);
			GRDGResourceDumpContext->SetDumped(PassParametersContent);
		}
	}

	GRDGResourceDumpContext->PassesCount++;
}

#if RDG_DUMP_RESOURCES_AT_EACH_DRAW

void FRDGBuilder::BeginPassDump(const FRDGPass* Pass)
{
	if (!GRDGResourceDumpContext)
	{
		return;
	}

	if (!GDumpGPUDraws.GetValueOnRenderThread())
	{
		return;
	}

	if (!EnumHasAnyFlags(Pass->GetFlags(), ERDGPassFlags::Raster))
	{
		return;
	}

	if (!IsInRenderingThread())
	{
		UE_LOG(LogRendererCore, Warning, TEXT("Couldn't start dumping draw's resources for pass %s because not in the rendering thread"), Pass->GetEventName().GetTCHAR());
		return;
	}

	check(GRDGResourceDumpContext->DrawDumpingPass == nullptr);

	if (GRDGResourceDumpContext->IsDumpingPass(Pass))
	{
		GRDGResourceDumpContext->DrawDumpingPass = Pass;
		GRDGResourceDumpContext->DrawDumpCount = 0;
	}
}

// static
void FRDGBuilder::DumpDraw(const FRDGEventName& DrawEventName)
{
	if (!GRDGResourceDumpContext)
	{
		return;
	}

	if (!IsInRenderingThread())
	{
		UE_LOG(LogRendererCore, Warning, TEXT("Couldn't dump draw because not in the rendering thread"));
		return;
	}

	if (!GRDGResourceDumpContext->DrawDumpingPass)
	{
		return;
	}

	const FRDGPass* Pass = GRDGResourceDumpContext->DrawDumpingPass;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	if (EnumHasAnyFlags(Pass->GetFlags(), ERDGPassFlags::Raster))
	{
		RHICmdList.EndRenderPass();
	}

	Pass->GetParameters().Enumerate([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			const FRenderTargetBindingSlots& RenderTargets = Parameter.GetAsRenderTargetBindingSlots();

			RenderTargets.Enumerate([&](FRenderTargetBinding RenderTarget)
			{
				FRDGTextureRef Texture = RenderTarget.GetTexture();
				FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::CreateForMipLevel(Texture, RenderTarget.GetMipIndex());
				GRDGResourceDumpContext->DumpDrawTextureSubResource(
					RHICmdList,
					TextureSubResource,
					ERHIAccess::RTV);
			});

			const FDepthStencilBinding& DepthStencil = RenderTargets.DepthStencil;

			if (FRDGTextureRef Texture = DepthStencil.GetTexture())
			{
				FExclusiveDepthStencil DepthStencilAccess = DepthStencil.GetDepthStencilAccess();

				if (DepthStencilAccess.IsDepthWrite())
				{
					FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::CreateForMipLevel(Texture, 0);
					GRDGResourceDumpContext->DumpDrawTextureSubResource(
						RHICmdList,
						TextureSubResource,
						ERHIAccess::RTV);
				}

				if (DepthStencilAccess.IsStencilWrite())
				{
					FRDGTextureSRVDesc TextureSubResource = FRDGTextureSRVDesc::CreateWithPixelFormat(Texture, PF_X24_G8);
					GRDGResourceDumpContext->DumpDrawTextureSubResource(
						RHICmdList,
						TextureSubResource,
						ERHIAccess::RTV);
				}
			}
		}
		break;
		}
	});

	if (EnumHasAnyFlags(Pass->GetFlags(), ERDGPassFlags::Raster))
	{
		RHICmdList.BeginRenderPass(Pass->GetParameters().GetRenderPassInfo(), Pass->GetName());
	}

	// Dump the draw even name
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("DrawName"), DrawEventName.GetTCHAR());

		FString DumpFilePath = FRDGResourceDumpContext::kPassesDir / FString::Printf(TEXT("Pass.%016x.Draws.json"), FRDGResourceDumpContext::PtrToUint(Pass));
		GRDGResourceDumpContext->DumpJsonToFile(JsonObject, DumpFilePath, FILEWRITE_Append);
	}

	GRDGResourceDumpContext->DrawDumpCount++;

	if (GRDGResourceDumpContext->DrawDumpCount % 10 == 0)
	{
		UE_LOG(LogRendererCore, Display, TEXT("Dumped %d draws' resources"), GRDGResourceDumpContext->DrawDumpCount);
		return;
	}
}

void FRDGBuilder::EndPassDump(const FRDGPass* Pass)
{
	if (!GRDGResourceDumpContext)
	{
		return;
	}

	if (!IsInRenderingThread())
	{
		return;
	}

	if (!GRDGResourceDumpContext->DrawDumpingPass)
	{
		return;
	}

	check(Pass == GRDGResourceDumpContext->DrawDumpingPass);

	// Output how many draw has been dump for this pass.
	if (GRDGResourceDumpContext->DrawDumpCount > 0)
	{
		FString EventNameStorage;

		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("EventName"), GetPassEventNameWithGPUMask(Pass, EventNameStorage));
		JsonObject->SetStringField(TEXT("Pointer"), FString::Printf(TEXT("%016x"), FRDGResourceDumpContext::PtrToUint(Pass)));
		JsonObject->SetNumberField(TEXT("DrawCount"), GRDGResourceDumpContext->DrawDumpCount);

		GRDGResourceDumpContext->DumpJsonToFile(JsonObject, FString(FRDGResourceDumpContext::kBaseDir) / TEXT("PassDrawCounts.json"), FILEWRITE_Append);

		UE_LOG(LogRendererCore, Display, TEXT("Completed dump of %d draws for pass: %s"), GRDGResourceDumpContext->DrawDumpCount, Pass->GetEventName().GetTCHAR());
	}

	GRDGResourceDumpContext->DrawDumpingPass = nullptr;
	GRDGResourceDumpContext->DrawDumpCount = 0;
}

// static
bool FRDGBuilder::IsDumpingFrame()
{
	return GRDGResourceDumpContext != nullptr;
}

bool FRDGBuilder::IsDumpingDraws()
{
	if (!GRDGResourceDumpContext)
	{
		return false;
	}

	return GDumpGPUDraws.GetValueOnRenderThread() != 0;
}

#endif // RDG_DUMP_RESOURCES_AT_EACH_DRAW

#else //! RDG_DUMP_RESOURCES

bool IsDumpingRDGResources()
{
	return false;
}

#endif //! RDG_DUMP_RESOURCES
