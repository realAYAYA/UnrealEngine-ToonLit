// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
	namespace Interchange
	{
		struct FTask;
	}
}
DECLARE_DELEGATE_OneParam(FInterchangeDispatcherTaskCompleted, int32 TaskIndex);

namespace UE
{
	namespace Interchange
	{
		enum class ETaskState
		{
			Unknown,
			Running,
			UnTreated,
			ProcessOk,
			ProcessFailed,
		};

		struct FTask
		{
			FTask() = delete;

			FTask(const FString& InJsonDescription)
			{
				JsonDescription = InJsonDescription;
				State = ETaskState::UnTreated;
			}

			FString JsonDescription;
			int32 Index = -1;
			ETaskState State = ETaskState::Unknown;
			FString JsonResult;
			TArray<FString> JsonMessages;
			FInterchangeDispatcherTaskCompleted OnTaskCompleted;
			double RunningStateStartTime = 0;
		};

		/**
		 * Json cmd helper to be able to read and write a FTask::JsonDescription
		 */
		class INTERCHANGEDISPATCHER_API IJsonCmdBase
		{
		public:
			virtual ~IJsonCmdBase() = default;

			virtual FString GetAction() const = 0;
			virtual FString GetTranslatorID() const = 0;
			virtual FString ToJson() const = 0;

			/**
			 * Return false if the JsonString do not match the command, true otherwise.
			 */
			virtual bool FromJson(const FString& JsonString) = 0;

			static FString GetCommandIDJsonKey()
			{
				static const FString Key = TEXT("CmdID");
				return Key;
			}
			static FString GetTranslatorIDJsonKey()
			{
				static const FString Key = TEXT("TranslatorID");
				return Key;
			}
			static FString GetCommandDataJsonKey()
			{
				static const FString Key = TEXT("CmdData");
				return Key;
			}

		protected:
			//Use this member to know if the data is initialize before using it
			bool bIsDataInitialize = false;
		};

		class INTERCHANGEDISPATCHER_API FJsonLoadSourceCmd : public IJsonCmdBase
		{
		public:
			FJsonLoadSourceCmd()
			{
				bIsDataInitialize = false;
			}

			FJsonLoadSourceCmd(const FString& InTranslatorID, const FString& InSourceFilename)
				: TranslatorID(InTranslatorID)
				, SourceFilename(InSourceFilename)
			{
				bIsDataInitialize = true;
			}

			virtual FString GetAction() const override
			{
				static const FString LoadString = TEXT("LoadSource");
				return LoadString;
			}

			virtual FString GetTranslatorID() const override
			{
				//Code should not do query data if the data was not set before
				ensure(bIsDataInitialize);
				return TranslatorID;
			}

			virtual FString ToJson() const;
			virtual bool FromJson(const FString& JsonString);

			FString GetSourceFilename() const
			{
				//Code should not do query data if the data was not set before
				ensure(bIsDataInitialize);
				return SourceFilename;
			}

			static FString GetSourceFilenameJsonKey()
			{
				static const FString Key = TEXT("SourceFile");
				return Key;
			}

			/**
			 * Use this class helper to create the cmd result json string and to read it
			 */
			class INTERCHANGEDISPATCHER_API JsonResultParser
			{
			public:
				FString GetResultFilename() const
				{
					return ResultFilename;
				}
				void SetResultFilename(const FString& InResultFilename)
				{
					ResultFilename = InResultFilename;
				}
				FString ToJson() const;
				bool FromJson(const FString& JsonString);

				static FString GetResultFilenameJsonKey()
				{
					const FString Key = TEXT("ResultFile");
					return Key;
				}
			private:
				FString ResultFilename = FString();
			};

		private:
			FString TranslatorID = FString();
			FString SourceFilename = FString();
		};

		class INTERCHANGEDISPATCHER_API FJsonFetchPayloadCmd : public IJsonCmdBase
		{
		public:
			FJsonFetchPayloadCmd()
			{
				bIsDataInitialize = false;
			}

			FJsonFetchPayloadCmd(const FString& InTranslatorID, const FString& InPayloadKey)
				: TranslatorID(InTranslatorID)
				, PayloadKey(InPayloadKey)
			{
				bIsDataInitialize = true;
			}

			virtual FString GetAction() const override
			{
				static const FString LoadString = TEXT("Payload");
				return LoadString;
			}

			virtual FString GetTranslatorID() const override
			{
				//Code should not do query data if the data was not set before
				ensure(bIsDataInitialize);
				return TranslatorID;
			}

			virtual FString ToJson() const;
			virtual bool FromJson(const FString& JsonString);

			FString GetPayloadKey() const
			{
				//Code should not do query data if the data was not set before
				ensure(bIsDataInitialize);
				return PayloadKey;
			}

			static FString GetPayloadKeyJsonKey()
			{
				static const FString Key = TEXT("PayloadKey");
				return Key;
			}

			/**
			 * Use this class helper to create the cmd result json string and to read it
			 */
			class INTERCHANGEDISPATCHER_API JsonResultParser
			{
			public:
				FString GetResultFilename() const
				{
					return ResultFilename;
				}
				void SetResultFilename(const FString& InResultFilename)
				{
					ResultFilename = InResultFilename;
				}
				FString ToJson() const;
				bool FromJson(const FString& JsonString);

				static FString GetResultFilenameJsonKey()
				{
					const FString Key = TEXT("ResultFile");
					return Key;
				}
			private:
				FString ResultFilename = FString();
			};

		protected:
			FString TranslatorID = FString();
			FString PayloadKey = FString();
		};

		//Animation transform payload require transform to be bake by the translator
		//The anim sequence API is not yet using curve to describe bone track animation
		class INTERCHANGEDISPATCHER_API FJsonFetchAnimationBakeTransformPayloadCmd : public FJsonFetchPayloadCmd
		{
		public:
			FJsonFetchAnimationBakeTransformPayloadCmd()
			{
				check(!bIsDataInitialize);
			}

			FJsonFetchAnimationBakeTransformPayloadCmd(const FString& InTranslatorID
				, const FString& InPayloadKey
				, double InBakeFrequency
				, double InRangeStartTime
				, double InRangeEndTime)
				: FJsonFetchPayloadCmd(InTranslatorID, InPayloadKey)
				, BakeFrequency(InBakeFrequency)
				, RangeStartTime(InRangeStartTime)
				, RangeEndTime(InRangeEndTime)
			{}

			virtual FString ToJson() const override;
			virtual bool FromJson(const FString& JsonString) override;

			double GetBakeFrequency() const
			{
				ensure(bIsDataInitialize);
				return BakeFrequency;
			}

			static FString GetBakeFrequencyJsonKey()
			{
				static const FString Key = TEXT("BakeFrequency");
				return Key;
			}

			double GetRangeStartTime() const
			{
				ensure(bIsDataInitialize);
				return RangeStartTime;
			}

			static FString GetRangeStartTimeJsonKey()
			{
				static const FString Key = TEXT("RangeStartTime");
				return Key;
			}

			double GetRangeEndTime() const
			{
				ensure(bIsDataInitialize);
				return RangeEndTime;
			}

			static FString GetRangeEndTimeJsonKey()
			{
				static const FString Key = TEXT("RangeEndTime");
				return Key;
			}
		protected:
			double BakeFrequency = 30.0;
			double RangeStartTime = 0.0;
			double RangeEndTime = 1.0/BakeFrequency;
		};
	} //ns Interchange
}//ns UE
