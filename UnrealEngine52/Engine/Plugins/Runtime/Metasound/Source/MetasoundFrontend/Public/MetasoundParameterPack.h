// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/AlignmentTemplates.h"

#include "MetasoundFrontendLiteral.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundRouter.h"
#include "MetasoundFrontendDocument.h"

#include "MetasoundParameterPack.generated.h"

class UMetaSoundSource;

// A structure that encapsulates a "parameter tag" in the raw 'bag-o-bytes'
// that will be the parameter storage. It lets us walk through memory to 
// find a parameter with a specific name and specific type. 
struct FMetasoundParameterPackItemBase
{
	FMetasoundParameterPackItemBase(const FName& InName, const FName& InTypeName, uint16 InNextOffset, uint16 InValueOffset)
		: Name(InName)
		, TypeName(InTypeName)
		, NextHeaderOffset(InNextOffset)
		, ValueOffset(InValueOffset)
	{}
	FMetasoundParameterPackItemBase(const FMetasoundParameterPackItemBase& Other) = default;
	void* GetPayload()
	{
		uint8* ptr = reinterpret_cast<uint8*>(this);
		return ptr + ValueOffset;
	}
	FName  Name;
	FName  TypeName;
	uint16 NextHeaderOffset;
	uint16 ValueOffset;
};

// A template that lets us stick an arbitrary data type into the raw 
// 'bag-o-bytes' that is the parameter storage. Uses the base class 
// above.
template <typename T>
struct FMetasoundParameterPackItem : public FMetasoundParameterPackItemBase
{
	FMetasoundParameterPackItem(const FName& InName, const FName& InTypeName, const T& InValue)
		: FMetasoundParameterPackItemBase(InName, InTypeName, AlignArbitrary<uint16>((uint16)sizeof(FMetasoundParameterPackItem<T>), alignof(std::max_align_t)), offsetof(FMetasoundParameterPackItem<T>,Value))
		, Value(InValue)
	{}
	FMetasoundParameterPackItem(const FMetasoundParameterPackItem& Other) = default;
	T      Value;
};

// This next class owns the 'bag-o-bytes' that holds all of the parameters. 
struct FMetasoundParameterPackStorage
{
	TArray<uint8> Storage;	
	int32         StorageBytesInUse = 0;

	FMetasoundParameterPackStorage() = default;
	FMetasoundParameterPackStorage(const FMetasoundParameterPackStorage& Other)
		: Storage(Other.Storage)
		, StorageBytesInUse(Other.StorageBytesInUse)
	{}
	FMetasoundParameterPackStorage(FMetasoundParameterPackStorage&& Other)
		: Storage(MoveTemp(Other.Storage))
		, StorageBytesInUse(Other.StorageBytesInUse)
	{
		Other.StorageBytesInUse = 0;
	}

	struct ParameterIterator
	{
		uint8* CurrentNode;
		uint8* EndAddress;

		ParameterIterator(uint8* Start, uint8* End)
			: CurrentNode(Start)
			, EndAddress(End)
		{}

		ParameterIterator& operator++()
		{
			if (!CurrentNode)
			{
				return *this; 
			}
			FMetasoundParameterPackItemBase* ParamAddress = reinterpret_cast<FMetasoundParameterPackItemBase*>(CurrentNode);
			CurrentNode += ParamAddress->NextHeaderOffset;
			if (CurrentNode >= EndAddress)
			{
				CurrentNode = nullptr;
			}
			return *this;
		}

		ParameterIterator operator++(int)
		{
			ParameterIterator ReturnValue = *this;
			++*this;
			return ReturnValue;
		}

		bool operator==(const ParameterIterator& Other) const
		{
			return CurrentNode == Other.CurrentNode;
		}

		bool operator!=(const ParameterIterator& Other) const
		{
			return CurrentNode != Other.CurrentNode;
		}

		FMetasoundParameterPackItemBase* operator->()
		{
			return reinterpret_cast<FMetasoundParameterPackItemBase*>(CurrentNode);
		}

		FMetasoundParameterPackItemBase* GetParameterBase()
		{
			return reinterpret_cast<FMetasoundParameterPackItemBase*>(CurrentNode);
		}

	};

	ParameterIterator begin()
	{
		if (Storage.IsEmpty())
		{
			return ParameterIterator(nullptr, nullptr);
		}
		return ParameterIterator(&Storage[0], &Storage[0] + StorageBytesInUse);
	}

	ParameterIterator end()
	{
		return ParameterIterator(nullptr, nullptr);
	}

	template<typename T>
	FMetasoundParameterPackItem<T>* FindParameter(const FName& ParamName, const FName& InTypeName)
	{
		if (Storage.IsEmpty())
		{
			return nullptr;
		}
		FMetasoundParameterPackItemBase* ParamWalker = reinterpret_cast<FMetasoundParameterPackItemBase*>(&Storage[0]);
		int32 StorageOffset = 0;
		while (ParamWalker)
		{
			if (ParamWalker->Name == ParamName)
			{
				if (ParamWalker->TypeName == InTypeName)
				{
					return static_cast<FMetasoundParameterPackItem<T>*>(ParamWalker);
				}
				return nullptr;
			}
			StorageOffset += ParamWalker->NextHeaderOffset;
			if (StorageOffset < StorageBytesInUse)
			{
				ParamWalker = reinterpret_cast<FMetasoundParameterPackItemBase*>(&Storage[StorageOffset]);
			}
			else
			{
				break;
			}
		}
		return nullptr;
	}

	template<typename T>
	std::enable_if_t<!std::is_class_v<T>,T*> AddParameter(FMetasoundParameterPackItem<T>& NewParam)
	{
		int32 NextParamLocation = AlignArbitrary<int32>(StorageBytesInUse, alignof(std::max_align_t));
		int32 TotalStorageNeeded = NextParamLocation + NewParam.NextHeaderOffset;
		if (TotalStorageNeeded > Storage.Num())
		{
			Storage.AddUninitialized(TotalStorageNeeded - Storage.Num());
		}
		FMetasoundParameterPackItem<T>* Destination = reinterpret_cast<FMetasoundParameterPackItem<T>*>(&Storage[NextParamLocation]);
		*Destination = NewParam;
		StorageBytesInUse = NextParamLocation + NewParam.NextHeaderOffset;
		return &Destination->Value;
	}

	template<typename T>
	std::enable_if_t<std::is_class_v<T>, T*> AddParameter(FMetasoundParameterPackItem<T>& NewParam)
	{
		int32 NextParamLocation = AlignArbitrary<int32>(StorageBytesInUse, alignof(std::max_align_t));
		int32 TotalStorageNeeded = NextParamLocation + NewParam.NextHeaderOffset;
		if (TotalStorageNeeded > Storage.Num())
		{
			Storage.AddUninitialized(TotalStorageNeeded - Storage.Num());
		}
		FMetasoundParameterPackItem<T>* Destination = new (reinterpret_cast<FMetasoundParameterPackItem<T>*>(&Storage[NextParamLocation])) FMetasoundParameterPackItem<T>(NewParam);
		StorageBytesInUse = NextParamLocation + NewParam.NextHeaderOffset;
		return &Destination->Value;
	}
};

using FSharedMetasoundParameterStoragePtr = TSharedPtr<FMetasoundParameterPackStorage, ESPMode::ThreadSafe>;

UENUM(BlueprintType)
enum class ESetParamResult : uint8
{
	Succeeded,
	Failed
};

// Here is the UObject BlueprintType that can be used in c++ and blueprint code. It holds a FMetasoundParamStorage 
// instance and can pass it along to the audio system's SetObjectParameter function via an AudioProxy.
UCLASS(BlueprintType,meta = (DisplayName = "MetaSoundParameterPack"))
class METASOUNDFRONTEND_API UMetasoundParameterPack : public UObject, public IAudioProxyDataFactory
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack")
	static UMetasoundParameterPack* MakeMetasoundParameterPack();

	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	ESetParamResult SetBool(FName ParameterName, bool InValue, bool OnlyIfExists = true);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	ESetParamResult SetInt(FName ParameterName, int32 InValue, bool OnlyIfExists = true);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	ESetParamResult SetFloat(FName ParameterName, float InValue, bool OnlyIfExists = true);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	ESetParamResult SetString(FName ParameterName, const FString& InValue, bool OnlyIfExists = true);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	ESetParamResult SetTrigger(FName ParameterName, bool OnlyIfExists = true);

	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = "Result"))
	bool GetBool(FName ParameterName, ESetParamResult& Result);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = "Result"))
	int32 GetInt(FName ParameterName, ESetParamResult& Result);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = "Result"))
	float GetFloat(FName ParameterName, ESetParamResult& Result);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = "Result"))
	FString GetString(FName ParameterName, ESetParamResult& Result);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = "Result"))
	bool GetTrigger(FName ParameterName, ESetParamResult& Result);

	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	bool HasBool(FName ParameterName);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	bool HasInt(FName ParameterName);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	bool HasFloat(FName ParameterName);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	bool HasString(FName ParameterName);
	UFUNCTION(BlueprintCallable, Category = "MetaSoundParameterPack", meta = (ExpandEnumAsExecs = ReturnValue))
	bool HasTrigger(FName ParameterName);

	void AddBoolParameter(FName Name, bool InValue);
	void AddIntParameter(FName Name, int32 InValue);
	void AddFloatParameter(FName Name, float InValue);
	void AddStringParameter(FName Name, const FString& InValue);
	void AddTriggerParameter(FName Name, bool InValue = true);

	// YIKES! BEWARE: If the returned pointers are only valid until another parameter is added!
	bool*    GetBoolParameterPtr(FName Name);
	int32*   GetIntParameterPtr(FName Name);
	float*   GetFloatParameterPtr(FName Name);
	FString* GetStringParameterPtr(FName Name);
	bool*    GetTriggerParameterPtr(FName Name);

	template<typename T>
	ESetParamResult SetParameter(const FName& Name, const FName& InTypeName, const T& InValue, bool OnlyIfExists = true)
	{
		T* TheParam = GetParameterPtr<T>(Name, InTypeName);
		if (TheParam)
		{
			*TheParam = InValue;
			return ESetParamResult::Succeeded;
		}
		else if (!OnlyIfExists)
		{
			// TODO Add test to see if we are allowed to create it!
			AddParameter<T>(Name, InTypeName, InValue);
			return ESetParamResult::Succeeded;
		}
		return ESetParamResult::Failed;
	}

	template<typename T>
	bool HasParameter(const FName& Name, const FName& InTypeName)
	{
		if (!ParameterStorage.IsValid())
		{
			return false;
		}
		return ParameterStorage->FindParameter<T>(Name, InTypeName) != nullptr;
	}

	template<typename T>
	void AddParameter(const FName& Name, const FName& InTypeName, const T& InValue)
	{
		if (!ParameterStorage.IsValid())
		{
			ParameterStorage = MakeShared<FMetasoundParameterPackStorage>();
		}
		FMetasoundParameterPackItem<T> NewParameter(Name, InTypeName, InValue);
		ParameterStorage->AddParameter(NewParameter);
	}

	template<typename T>
	T* GetParameterPtr(const FName& Name, const FName& InTypeName)
	{
		if (!ParameterStorage.IsValid())
		{
			return nullptr;
		}
		auto TheParameter = ParameterStorage->FindParameter<T>(Name, InTypeName);
		if (!TheParameter)
		{
			return nullptr;
		}
		return &TheParameter->Value;
	}

	template<typename T>
	T GetParameter(const FName& Name, const FName& InTypeName, ESetParamResult& Result)
	{
		T* TheParameter = GetParameterPtr<T>(Name, InTypeName);
		if (!TheParameter)
		{
			Result = ESetParamResult::Failed;
			return T();
		}
		Result = ESetParamResult::Succeeded;
		return *TheParameter;
	}

	TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	
	// A couple of utilities for use by MetasoundAssetBase and MetasoundGenerator to set
	// up the routing for parameter packs
	static Metasound::FSendAddress CreateSendAddressFromEnvironment(const Metasound::FMetasoundEnvironment& InEnvironment);
	static FMetasoundFrontendClassInput GetClassInput();

	FSharedMetasoundParameterStoragePtr GetParameterStorage() const;
	FSharedMetasoundParameterStoragePtr GetCopyOfParameterStorage() const;

private:
	TSharedPtr<FMetasoundParameterPackStorage> ParameterStorage;
};

// Here is the proxy that UMetasoundParameterPack creates when asked for a proxy by the audio system
class METASOUNDFRONTEND_API FMetasoundParameterPackProxy : public Audio::TProxyData<FMetasoundParameterPackProxy>
{
public:
	IMPL_AUDIOPROXY_CLASS(FMetasoundParameterPackProxy);

	explicit FMetasoundParameterPackProxy(FSharedMetasoundParameterStoragePtr& Data)
		: ParameterStoragePtr(Data)
	{}

	FMetasoundParameterPackProxy(const FMetasoundParameterPackProxy& Other) = default;

	FSharedMetasoundParameterStoragePtr GetParamStorage()
	{
		return ParameterStoragePtr;
	}

private:
	FSharedMetasoundParameterStoragePtr ParameterStoragePtr;
};

// And finally... A type we can register with Metasound that holds a TSharedPtr to an FMetasoundParamStorage instance.
// It can be created by various systems in Metasound given a proxy.
class METASOUNDFRONTEND_API FMetasoundParameterStorageWrapper
{
	FSharedMetasoundParameterStoragePtr ParameterStoragePtr;

public:

	FMetasoundParameterStorageWrapper() = default;
	FMetasoundParameterStorageWrapper(const FMetasoundParameterStorageWrapper&) = default;
	FMetasoundParameterStorageWrapper& operator=(const FMetasoundParameterStorageWrapper& Other) = default;
	FMetasoundParameterStorageWrapper(const TSharedPtr<Audio::IProxyData>& InInitData)
	{
		if (InInitData.IsValid())
		{
			if (InInitData->CheckTypeCast<FMetasoundParameterPackProxy>())
			{
				// should we be getting handed a SharedPtr here?
				FMetasoundParameterPackProxy& AsParameterPack = InInitData->GetAs<FMetasoundParameterPackProxy>();
				ParameterStoragePtr = AsParameterPack.GetParamStorage();
			}
		}
	}

	FSharedMetasoundParameterStoragePtr Get() const
	{
		return ParameterStoragePtr;
	}

	bool IsPackValid() const
	{
		return ParameterStoragePtr.IsValid();
	}

	const FSharedMetasoundParameterStoragePtr& GetPackProxy() const
	{
		return ParameterStoragePtr;
	}

	const FMetasoundParameterPackStorage* operator->() const
	{
		return ParameterStoragePtr.Get();
	}

	FMetasoundParameterPackStorage* operator->()
	{
		return ParameterStoragePtr.Get();
	}
};

DECLARE_METASOUND_DATA_REFERENCE_TYPES(FMetasoundParameterStorageWrapper, METASOUNDFRONTEND_API, FMetasoundParameterStorageWrapperTypeInfo, FMetasoundParameterStorageWrapperReadRef, FMetasoundParameterStorageWrapperWriteRef)

