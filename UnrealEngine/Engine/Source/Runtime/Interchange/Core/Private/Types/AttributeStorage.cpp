// Copyright Epic Games, Inc. All Rights Reserved.
#include "Types/AttributeStorage.h"
#include "InterchangeLogPrivate.h"

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"

//Interchange namespace
namespace UE
{
	namespace Interchange
	{
		FString AttributeTypeToString(EAttributeTypes AttributeType)
		{
			FString AttributeTypeString;
			switch (AttributeType)
			{
				case EAttributeTypes::None: { AttributeTypeString = TEXT("None"); } break;
				case EAttributeTypes::Bool: { AttributeTypeString = TEXT("Bool"); } break;
				case EAttributeTypes::ByteArray: { AttributeTypeString = TEXT("ByteArray"); } break;
				case EAttributeTypes::ByteArray64: { AttributeTypeString = TEXT("ByteArray64"); } break;
				case EAttributeTypes::Color: { AttributeTypeString = TEXT("Color"); } break;
				case EAttributeTypes::DateTime: { AttributeTypeString = TEXT("DateTime"); } break;
				case EAttributeTypes::Double: { AttributeTypeString = TEXT("Double"); } break;
				case EAttributeTypes::Enum: { AttributeTypeString = TEXT("Enum"); } break;
				case EAttributeTypes::Float: { AttributeTypeString = TEXT("Float"); } break;
				case EAttributeTypes::Guid: { AttributeTypeString = TEXT("Guid"); } break;
				case EAttributeTypes::Int8: { AttributeTypeString = TEXT("Int8"); } break;
				case EAttributeTypes::Int16: { AttributeTypeString = TEXT("Int16"); } break;
				case EAttributeTypes::Int32: { AttributeTypeString = TEXT("Int32"); } break;
				case EAttributeTypes::Int64: { AttributeTypeString = TEXT("Int64"); } break;
				case EAttributeTypes::IntRect: { AttributeTypeString = TEXT("IntRect"); } break;
				case EAttributeTypes::LinearColor: { AttributeTypeString = TEXT("LinearColor"); } break;
				case EAttributeTypes::Name: { AttributeTypeString = TEXT("Name"); } break;
				case EAttributeTypes::RandomStream: { AttributeTypeString = TEXT("RandomStream"); } break;
				case EAttributeTypes::String: { AttributeTypeString = TEXT("String"); } break;
				case EAttributeTypes::Timespan: { AttributeTypeString = TEXT("Timespan"); } break;
				case EAttributeTypes::TwoVectors: { AttributeTypeString = TEXT("TwoVectors"); } break;
				case EAttributeTypes::UInt8: { AttributeTypeString = TEXT("UInt8"); } break;
				case EAttributeTypes::UInt16: { AttributeTypeString = TEXT("UInt16"); } break;
				case EAttributeTypes::UInt32: { AttributeTypeString = TEXT("UInt32"); } break;
				case EAttributeTypes::UInt64: { AttributeTypeString = TEXT("UInt64"); } break;
				case EAttributeTypes::Vector2d: { AttributeTypeString = TEXT("Vector2d"); } break;
				case EAttributeTypes::IntPoint: { AttributeTypeString = TEXT("IntPoint"); } break;
				case EAttributeTypes::IntVector: { AttributeTypeString = TEXT("IntVector"); } break;
				case EAttributeTypes::Vector2DHalf: { AttributeTypeString = TEXT("Vector2DHalf"); } break;
				case EAttributeTypes::Float16: { AttributeTypeString = TEXT("Float16"); } break;
				case EAttributeTypes::OrientedBox: { AttributeTypeString = TEXT("OrientedBox"); } break;
				case EAttributeTypes::FrameNumber: { AttributeTypeString = TEXT("FrameNumber"); } break;
				case EAttributeTypes::FrameRate: { AttributeTypeString = TEXT("FrameRate"); } break;
				case EAttributeTypes::FrameTime: { AttributeTypeString = TEXT("FrameTime"); } break;
				case EAttributeTypes::SoftObjectPath: { AttributeTypeString = TEXT("SoftObjectPath"); } break;
				case EAttributeTypes::Matrix44f: { AttributeTypeString = TEXT("Matrix44f"); } break;
				case EAttributeTypes::Matrix44d: { AttributeTypeString = TEXT("Matrix44d"); } break;
				case EAttributeTypes::Plane4f: { AttributeTypeString = TEXT("Plane4f"); } break;
				case EAttributeTypes::Plane4d: { AttributeTypeString = TEXT("Plane4d"); } break;
				case EAttributeTypes::Quat4f: { AttributeTypeString = TEXT("Quat4f"); } break;
				case EAttributeTypes::Quat4d: { AttributeTypeString = TEXT("Quat4d"); } break;
				case EAttributeTypes::Rotator3f: { AttributeTypeString = TEXT("Rotator3f"); } break;
				case EAttributeTypes::Rotator3d: { AttributeTypeString = TEXT("Rotator3d"); } break;
				case EAttributeTypes::Transform3f: { AttributeTypeString = TEXT("Transform3f"); } break;
				case EAttributeTypes::Transform3d: { AttributeTypeString = TEXT("Transform3d"); } break;
				case EAttributeTypes::Vector3f: { AttributeTypeString = TEXT("Vector3f"); } break;
				case EAttributeTypes::Vector3d: { AttributeTypeString = TEXT("Vector3d"); } break;
				case EAttributeTypes::Vector2f: { AttributeTypeString = TEXT("Vector2f"); } break;
				case EAttributeTypes::Vector4f: { AttributeTypeString = TEXT("Vector4f"); } break;
				case EAttributeTypes::Vector4d: { AttributeTypeString = TEXT("Vector4d"); } break;
				case EAttributeTypes::Box2f: { AttributeTypeString = TEXT("Box2f"); } break;
				case EAttributeTypes::Box2D: { AttributeTypeString = TEXT("Box2D"); } break;
				case EAttributeTypes::Box3f: { AttributeTypeString = TEXT("Box3f"); } break;
				case EAttributeTypes::Box3d: { AttributeTypeString = TEXT("Box3d"); } break;
				case EAttributeTypes::BoxSphereBounds3f: { AttributeTypeString = TEXT("BoxSphereBounds3f"); } break;
				case EAttributeTypes::BoxSphereBounds3d: { AttributeTypeString = TEXT("BoxSphereBounds3d"); } break;
				case EAttributeTypes::Sphere3f: { AttributeTypeString = TEXT("Sphere3f"); } break;
				case EAttributeTypes::Sphere3d: { AttributeTypeString = TEXT("Sphere3d"); } break;
				default:
				{
					//Ensure if we ask an unknown type
					const bool bUnknownType = true;
					ensure(!bUnknownType);
				}
				break;
			}
			return AttributeTypeString;
		}

		EAttributeTypes StringToAttributeType(const FString& AttributeTypeString)
		{
			const int32 MaxAttributetypes = static_cast<int32>(EAttributeTypes::Max);
			for (int32 AttributeTypeEnumIndex = 0; AttributeTypeEnumIndex < MaxAttributetypes; ++AttributeTypeEnumIndex)
			{
				const EAttributeTypes Attributetype = static_cast<EAttributeTypes>(AttributeTypeEnumIndex);
				if (AttributeTypeString.Equals(AttributeTypeToString(Attributetype)))
				{
					return Attributetype;
				}
			}
			return EAttributeTypes::None;
		}

		void LogAttributeStorageErrors(const EAttributeStorageResult Result, const FString OperationName, const FAttributeKey AttributeKey)
		{
			//////////////////////////////////////////////////////////////////////////
			//Errors
			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_AttributeAllocationCorrupted))
			{
				UE_LOG(LogInterchangeCore, Error, TEXT("Attribute storage operation [%s] Key[%s]: Storage is corrupted."), *OperationName, *(AttributeKey.ToString()));
			}

			//////////////////////////////////////////////////////////////////////////
			//Warning
			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_CannotFoundKey))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: Cannot find attribute key."), *OperationName, *(AttributeKey.ToString()));
			}

			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_CannotOverrideAttribute))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: Cannot override attribute."), *OperationName, *(AttributeKey.ToString()));
			}

			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_CannotRemoveAttribute))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: Cannot remove an attribute."), *OperationName, *(AttributeKey.ToString()));
			}

			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_WrongSize))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: Stored attribute value size does not match parameter value size."), *OperationName, *(AttributeKey.ToString()));
			}

			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_WrongType))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: Stored attribute value type does not match parameter value type."), *OperationName, *(AttributeKey.ToString()));
			}

			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_InvalidStorage))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: The storage is invalid (NULL)."), *OperationName, *(AttributeKey.ToString()));
			}

			if (HasAttributeStorageResult(Result, EAttributeStorageResult::Operation_Error_InvalidMultiSizeValueData))
			{
				UE_LOG(LogInterchangeCore, Warning, TEXT("Attribute storage operation [%s] Key[%s]: Cannot retrieve the multisize value data pointer."), *OperationName, *(AttributeKey.ToString()));
			}
		}

		FAttributeStorage::FAttributeStorage(const FAttributeStorage& Other)
		{
			//Lock both Storage mutex
			FScopeLock ScopeLock(&StorageMutex);
			FScopeLock ScopeLockOther(&Other.StorageMutex);

			//Copy all value
			AttributeAllocationTable = Other.AttributeAllocationTable;
			AttributeStorage = Other.AttributeStorage;
			FragmentedMemoryCost = Other.FragmentedMemoryCost;
			DefragRatio = Other.DefragRatio;
		}

		FAttributeStorage& FAttributeStorage::operator=(const FAttributeStorage& Other)
		{
			//Lock both Storage mutex
			FScopeLock ScopeLock(&StorageMutex);
			FScopeLock ScopeLockOther(&Other.StorageMutex);

			//Copy all value
			AttributeAllocationTable = Other.AttributeAllocationTable;
			AttributeStorage = Other.AttributeStorage;
			FragmentedMemoryCost = Other.FragmentedMemoryCost;
			DefragRatio = Other.DefragRatio;

			return *this;
		}

		EAttributeStorageResult FAttributeStorage::UnregisterAttribute(const FAttributeKey& ElementAttributeKey)
		{
			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			FAttributeAllocationInfo* AttributeAllocationInfo = AttributeAllocationTable.Find(ElementAttributeKey);
			if (!AttributeAllocationInfo)
			{
				return EAttributeStorageResult::Operation_Error_CannotFoundKey;
			}

			FragmentedMemoryCost += AttributeAllocationInfo->Size;
			if (AttributeAllocationTable.Remove(ElementAttributeKey) == 0)
			{
				return EAttributeStorageResult::Operation_Error_CannotRemoveAttribute;
			}

			DefragInternal();

			return EAttributeStorageResult::Operation_Success;
		}

		EAttributeTypes FAttributeStorage::GetAttributeType(const FAttributeKey& ElementAttributeKey) const
		{
			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);
			const FAttributeAllocationInfo* AttributeAllocationInfo = AttributeAllocationTable.Find(ElementAttributeKey);
			if (AttributeAllocationInfo)
			{
				return AttributeAllocationInfo->Type;
			}
			return EAttributeTypes::None;
		}

		bool FAttributeStorage::ContainAttribute(const FAttributeKey& ElementAttributeKey) const
		{
			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			return AttributeAllocationTable.Contains(ElementAttributeKey);
		}

		void FAttributeStorage::GetAttributeKeys(TArray<FAttributeKey>& AttributeKeys) const
		{
			FScopeLock ScopeLock(&StorageMutex);
			AttributeAllocationTable.GetKeys(AttributeKeys);
		}

		FGuid FAttributeStorage::GetAttributeHash(const FAttributeKey& ElementAttributeKey) const
		{
			FGuid AttributeHash;
			GetAttributeHash(ElementAttributeKey, AttributeHash);
			return AttributeHash;
		}

		bool FAttributeStorage::GetAttributeHash(const FAttributeKey& ElementAttributeKey, FGuid& OutGuid) const
		{
			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			const FAttributeAllocationInfo* AttributeAllocationInfo = AttributeAllocationTable.Find(ElementAttributeKey);
			if (AttributeAllocationInfo)
			{
				OutGuid = AttributeAllocationInfo->Hash;
				return true;
			}
			return false;
		}

		FGuid FAttributeStorage::GetStorageHash() const
		{
			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			TArray<FAttributeKey> OrderedKeys;
			AttributeAllocationTable.GetKeys(OrderedKeys);
			OrderedKeys.Sort();
			FSHA1 Sha;
			for (FAttributeKey Key : OrderedKeys)
			{
				const FAttributeAllocationInfo& AttributeAllocationInfo = AttributeAllocationTable.FindChecked(Key);
				//Skip Attribute that has the no hash flag
				if (HasAttributeProperty(AttributeAllocationInfo.Property, EAttributeProperty::NoHash))
				{
					continue;
				}
				uint32 GuidData[4];
				GuidData[0] = AttributeAllocationInfo.Hash.A;
				GuidData[1] = AttributeAllocationInfo.Hash.B;
				GuidData[2] = AttributeAllocationInfo.Hash.C;
				GuidData[3] = AttributeAllocationInfo.Hash.D;
				Sha.Update(reinterpret_cast<uint8*>(&GuidData[0]), 16);
			}
			Sha.Final();
			// Retrieve the hash and use it to construct a pseudo-GUID. 
			uint32 Hash[5];
			Sha.GetHash(reinterpret_cast<uint8*>(Hash));
			return FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
		}

		void FAttributeStorage::CompareStorage(const FAttributeStorage& BaseStorage, const FAttributeStorage& VersionStorage, TArray<FAttributeKey>& RemovedAttributes, TArray<FAttributeKey>& AddedAttributes, TArray<FAttributeKey>& ModifiedAttributes)
		{
			//Lock the storage
			FScopeLock ScopeLockVersion(&VersionStorage.StorageMutex);
			//Lock the storage
			FScopeLock ScopeLockBase(&BaseStorage.StorageMutex);

			for (const auto& KvpBase : BaseStorage.AttributeAllocationTable)
			{
				FAttributeKey KeyBase = KvpBase.Key;
				const FAttributeAllocationInfo* AttributeAllocationInfoVersion = VersionStorage.AttributeAllocationTable.Find(KeyBase);
				if (!AttributeAllocationInfoVersion)
				{
					//Add the attribute to RemovedAttributes
					RemovedAttributes.Add(KeyBase);
				}
				else if (KvpBase.Value.Hash != AttributeAllocationInfoVersion->Hash)
				{
					//Add the attribute to ModifiedAttributes
					ModifiedAttributes.Add(KeyBase);
				}
			}

			for (const auto& KvpVersion : VersionStorage.AttributeAllocationTable)
			{
				FAttributeKey KeyVersion = KvpVersion.Key;
				const FAttributeAllocationInfo* AttributeAllocationInfoBase = BaseStorage.AttributeAllocationTable.Find(KeyVersion);
				if (!AttributeAllocationInfoBase)
				{
					//Add the attribute to RemovedAttributes
					AddedAttributes.Add(KeyVersion);
				}
			}
		}

		void FAttributeStorage::CopyStorageAttributes(const FAttributeStorage& SourceStorage, FAttributeStorage& DestinationStorage, const TArray<FAttributeKey>& AttributeKeys)
		{
			//Lock both storages
			FScopeLock SourceScopeLock(&SourceStorage.StorageMutex);
			FScopeLock DestinationScopeLock(&DestinationStorage.StorageMutex);

			for (const FAttributeKey& AttributeKey : AttributeKeys)
			{
				if (const FAttributeAllocationInfo* SourceAttributeInfo = SourceStorage.AttributeAllocationTable.Find(AttributeKey))
				{
					FAttributeAllocationInfo* DestinationAttributeInfo = DestinationStorage.AttributeAllocationTable.Find(AttributeKey);
					
					if (!SourceStorage.AttributeStorage.IsValidIndex(SourceAttributeInfo->Offset))
					{
						//EAttributeStorageResult::Operation_Error_AttributeAllocationCorrupted;
						continue;
					}

 					const EAttributeTypes SourceValueType = SourceAttributeInfo->Type;
 					const uint64 SourceValueSize = SourceAttributeInfo->Size;
					if (DestinationAttributeInfo)
					{
						if (DestinationAttributeInfo->Type != SourceValueType)
						{
							//EAttributeStorageResult::Operation_Error_WrongType;
							continue;
						}
						if (DestinationAttributeInfo->Size != SourceValueSize)
						{
							//EAttributeStorageResult::Operation_Error_WrongSize;
							continue;
						}
						if (!DestinationStorage.AttributeStorage.IsValidIndex(DestinationAttributeInfo->Offset))
						{
							//EAttributeStorageResult::Operation_Error_AttributeAllocationCorrupted;
							continue;
						}
					}
					else
					{
						DestinationAttributeInfo = &DestinationStorage.AttributeAllocationTable.Add(AttributeKey);
						DestinationAttributeInfo->Type = SourceValueType;
						DestinationAttributeInfo->Size = SourceValueSize;
						DestinationAttributeInfo->Offset = DestinationStorage.AttributeStorage.AddZeroed(SourceValueSize);
					}

					//Force the specified attribute property
					DestinationAttributeInfo->Property = SourceAttributeInfo->Property;
					//Save the hash no need to compute it
					DestinationAttributeInfo->Hash = SourceAttributeInfo->Hash;

					//Memcpy source to destination storage
					const uint8* SourceStorageData = SourceStorage.AttributeStorage.GetData();
					uint8* DestinationStorageData = DestinationStorage.AttributeStorage.GetData();
					FMemory::Memcpy(&DestinationStorageData[DestinationAttributeInfo->Offset], &SourceStorageData[SourceAttributeInfo->Offset], SourceValueSize);
				}
			}
		}

		void FAttributeStorage::SetDefragRatio(const float InDefragRatio)
		{
			FScopeLock ScopeLock(&StorageMutex);
			DefragRatio = InDefragRatio;
			DefragInternal();
		}

		void FAttributeStorage::Reserve(int64 NewAttributeCount, int64 NewStorageSize)
		{
			if (NewAttributeCount > 0)
			{
				const int64 ReserveCount = AttributeAllocationTable.Num() + NewAttributeCount;
				AttributeAllocationTable.Reserve(ReserveCount);
			}
			if (NewStorageSize > 0)
			{
				const int64 ReserveCount = AttributeStorage.Num() + NewStorageSize;
				AttributeStorage.Reserve(ReserveCount);
			}
		}

		void FAttributeStorage::ExtractFStringAttributeFromStorage(const uint8* StorageData, const FAttributeAllocationInfo* AttributeAllocationInfo, FString& OutValue) const
		{
			//Allocate the ByteArray
			const uint64 NumberOfChar = AttributeAllocationInfo->Size / sizeof(TCHAR);
			check(NumberOfChar > 0);

			//Since the null character is always there, a empty String is only one TCHAR
			if (NumberOfChar <= 1)
			{
				OutValue.Empty();
				return;
			}

			//Empty and pre allocate the FString buffer (TArray<TCHAR>) and add the null terminator
			OutValue.Reset(NumberOfChar - 1);
			//SLOW! We have to add the characters 1 x 1, there is no AddZeroed or similar stuff. We do not have a string to use the operator= which do what we need here
			//We go up to NumberOfChar -1 because there is always a null terminating character add at the end of the FString
			for (int32 CharIndex = 0; CharIndex < NumberOfChar - 1; ++CharIndex)
			{
				//Adding spaces, we cannot add null character because the null character are not added
				OutValue.AppendChar(' ');
			}

			//Copy into the buffer
			FMemory::Memcpy((uint8*)(*OutValue), &StorageData[AttributeAllocationInfo->Offset], AttributeAllocationInfo->Size);
		}

		EAttributeStorageResult FAttributeStorage::GetAttribute(const FAttributeKey& ElementAttributeKey, FString& OutValue, TSpecializeType<FString >) const
		{
			static_assert(TAttributeTypeTraits<FString>::GetType() != EAttributeTypes::None, "Not a supported type for the attributes. Check EAttributeTypes for the supported types.");

			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			const EAttributeTypes ValueType = TAttributeTypeTraits<FString>::GetType();
			const FAttributeAllocationInfo* AttributeAllocationInfo = AttributeAllocationTable.Find(ElementAttributeKey);
			if (AttributeAllocationInfo)
			{
				if (AttributeAllocationInfo->Type != ValueType)
				{
					return EAttributeStorageResult::Operation_Error_WrongType;
				}
				if (!AttributeStorage.IsValidIndex(AttributeAllocationInfo->Offset))
				{
					return EAttributeStorageResult::Operation_Error_AttributeAllocationCorrupted;
				}
			}
			else
			{
				//The key do not exist
				return EAttributeStorageResult::Operation_Error_CannotFoundKey;
			}

			if (AttributeAllocationInfo->Size <= sizeof(TCHAR)) // Account for the null-terminator
			{
				OutValue = TEXT("");
				return EAttributeStorageResult::Operation_Success;
			}

			ExtractFStringAttributeFromStorage(AttributeStorage.GetData(), AttributeAllocationInfo, OutValue);

			return EAttributeStorageResult::Operation_Success;
		}

		EAttributeStorageResult FAttributeStorage::GetAttribute(const FAttributeKey& ElementAttributeKey, FName& OutValue, TSpecializeType<FName >) const
		{
			static_assert(TAttributeTypeTraits<FName>::GetType() != EAttributeTypes::None, "Not a supported type for the attributes. Check EAttributeTypes for supported types.");

			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			const EAttributeTypes ValueType = TAttributeTypeTraits<FName>::GetType();


			const FAttributeAllocationInfo* AttributeAllocationInfo = AttributeAllocationTable.Find(ElementAttributeKey);
			if (AttributeAllocationInfo)
			{
				if (AttributeAllocationInfo->Type != ValueType)
				{
					return EAttributeStorageResult::Operation_Error_WrongType;
				}
				if (!AttributeStorage.IsValidIndex(AttributeAllocationInfo->Offset))
				{
					return EAttributeStorageResult::Operation_Error_AttributeAllocationCorrupted;
				}
			}
			else
			{
				//The key do not exist
				return EAttributeStorageResult::Operation_Error_CannotFoundKey;
			}


			if (AttributeAllocationInfo->Size == 0)
			{
				OutValue = NAME_None;
				return EAttributeStorageResult::Operation_Success;
			}

			FString ValueStr;

			//Share the code with FString from here
			ExtractFStringAttributeFromStorage(AttributeStorage.GetData(), AttributeAllocationInfo, ValueStr);

			//Create the FName and copy it to OutValue
			OutValue = FName(*ValueStr);

			return EAttributeStorageResult::Operation_Success;
		}

		EAttributeStorageResult FAttributeStorage::GetAttribute(const FAttributeKey& ElementAttributeKey, FSoftObjectPath& OutValue, TSpecializeType<FSoftObjectPath >) const
		{
			static_assert(TAttributeTypeTraits<FSoftObjectPath>::GetType() != EAttributeTypes::None, "Not a supported type for the attributes. Check EAttributeTypes for supported types.");

			//Lock the storage
			FScopeLock ScopeLock(&StorageMutex);

			const EAttributeTypes ValueType = TAttributeTypeTraits<FSoftObjectPath>::GetType();


			const FAttributeAllocationInfo* AttributeAllocationInfo = AttributeAllocationTable.Find(ElementAttributeKey);
			if (AttributeAllocationInfo)
			{
				if (AttributeAllocationInfo->Type != ValueType)
				{
					return EAttributeStorageResult::Operation_Error_WrongType;
				}
				if (!AttributeStorage.IsValidIndex(AttributeAllocationInfo->Offset))
				{
					return EAttributeStorageResult::Operation_Error_AttributeAllocationCorrupted;
				}
			}
			else
			{
				//The key do not exist
				return EAttributeStorageResult::Operation_Error_CannotFoundKey;
			}


			if (AttributeAllocationInfo->Size == 0)
			{
				OutValue = TEXT("");
				return EAttributeStorageResult::Operation_Success;
			}

			FString ValueStr;

			//Share the code with FString from here
			ExtractFStringAttributeFromStorage(AttributeStorage.GetData(), AttributeAllocationInfo, ValueStr);

			//Create the FSoftObjectPath and copy it to OutValue
			OutValue = FSoftObjectPath(ValueStr);

			return EAttributeStorageResult::Operation_Success;
		}

		void FAttributeStorage::DefragInternal()
		{
			//Defrag if the fragmented memory cost is bigger then the DefragRatio.
			if (FragmentedMemoryCost < 11 || FragmentedMemoryCost < AttributeStorage.Num() * DefragRatio)
			{
				return;
			}

			//Sorts the allocation table per offset since we want to defrag using memory block
			//The ValueSort is also calling CompactStable which remove all holes in the TMap
			struct FAttributeAllocationInfoSmaller
			{
				FORCEINLINE bool operator()(const FAttributeAllocationInfo& A, const FAttributeAllocationInfo& B) const
				{
					return A.Offset < B.Offset;
				}
			};
			AttributeAllocationTable.ValueSort(FAttributeAllocationInfoSmaller());

			const uint64 AttributeNumber = AttributeAllocationTable.Num();
			uint8* StorageData = AttributeStorage.GetData();
			//Number of attribute we have done
			uint64 AttributeCount = 0;
			//Current storage offset we want to defrag
			uint64 CurrentOffset = 0;
			//Are we build a defrag block
			bool bBuildBlock = false;
			//Defrag block start
			uint64 MemmoveBlockStart = 0;
			//Replace CurrentOffset when we are moving a block
			uint64 MemmoveBlockCurrentOffset = 0;
			for (auto& Kvp : AttributeAllocationTable)
			{
				AttributeCount++;
				const bool bLast = AttributeCount == AttributeNumber;

				FAttributeAllocationInfo& AttributeAllocationInfo = Kvp.Value;
				if (AttributeAllocationInfo.Offset == CurrentOffset)
				{
					CurrentOffset += AttributeAllocationInfo.Size;
					continue;
				}
				check(CurrentOffset < AttributeAllocationInfo.Offset);
				if (!bBuildBlock)
				{
					bBuildBlock = true;
					MemmoveBlockStart = AttributeAllocationInfo.Offset;
					AttributeAllocationInfo.Offset = CurrentOffset;
					MemmoveBlockCurrentOffset = MemmoveBlockStart + AttributeAllocationInfo.Size;
				}
				else
				{
					if (AttributeAllocationInfo.Offset == MemmoveBlockCurrentOffset)
					{
						AttributeAllocationInfo.Offset -= MemmoveBlockStart - CurrentOffset;
						MemmoveBlockCurrentOffset += AttributeAllocationInfo.Size;
					}
					else
					{
						const uint64 BlockSize = MemmoveBlockCurrentOffset - MemmoveBlockStart;
						//Memmove support overlap
						FMemory::Memmove(&StorageData[CurrentOffset], &StorageData[MemmoveBlockStart], BlockSize);
						CurrentOffset += BlockSize;

						MemmoveBlockStart = AttributeAllocationInfo.Offset;
						AttributeAllocationInfo.Offset = CurrentOffset;
						MemmoveBlockCurrentOffset = MemmoveBlockStart + AttributeAllocationInfo.Size;
					}
				}

				//We have to move the block if we are the last attribute
				if (bLast)
				{
					const uint64 BlockSize = MemmoveBlockCurrentOffset - MemmoveBlockStart;
					//Memmove support overlap
					FMemory::Memmove(&StorageData[CurrentOffset], &StorageData[MemmoveBlockStart], BlockSize);
					const uint64 RemoveStartIndex = CurrentOffset + BlockSize;
					const uint64 RemoveCount = AttributeStorage.Num() - RemoveStartIndex;
					//Remove the Last items, allow shrinking so we are compact
					AttributeStorage.RemoveAt(RemoveStartIndex, RemoveCount);
				}
			}
			//Reset the fragmented cost
			FragmentedMemoryCost = 0;
		}

		FGuid FAttributeStorage::GetValueHash(const uint8* Value, uint64 ValueSize)
		{
			FSHA1 Sha;
			Sha.Update(Value, ValueSize);
			Sha.Final();
			// Retrieve the hash and use it to construct a pseudo-GUID. 
			uint32 Hash[5];
			Sha.GetHash(reinterpret_cast<uint8*>(Hash));
			return FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
		}

		uint64 FAttributeStorage::GetValueSize(const FString& Value, TSpecializeType<FString >)
		{
			//We must add the null character '/0' terminate string
			return (Value.Len() + 1) * sizeof(TCHAR);
		}

		uint64 FAttributeStorage::GetValueSize(const TArray<uint8>& Value, TSpecializeType<TArray<uint8> >)
		{
			return Value.Num();
		}

		uint64 FAttributeStorage::GetValueSize(const TArray64<uint8>& Value, TSpecializeType<TArray64<uint8> >)
		{
			return Value.Num();
		}

	} //ns Interchange
} //ns UE