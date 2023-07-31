// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAudioDecoderScheduler.h"
#include "IAudioCodecRegistry.h"
#include "IAudioCodec.h"
#include "Templates/Atomic.h"

namespace Audio
{
	struct FCmdQueue
	{
		using FCmdFunction = TFunction<void()>;
		using FCommandQ = TArray<FCmdFunction>;

		FCommandQ Q[2];
		TAtomic<uint32> WriteIndex;
		TAtomic<uint32> NumCmds;

		void Execute()
		{	
			// Consumer thread.

			// Swap the currently writing index.
			uint32 CurrentIndex = WriteIndex;
			while( !WriteIndex.CompareExchange(CurrentIndex,!WriteIndex) )
			{
				CurrentIndex = WriteIndex.Load(EMemoryOrder::SequentiallyConsistent);
			}
			
			FCommandQ& CurrentQ = Q[CurrentIndex];
			for (int32 i = 0; i < CurrentQ.Num(); ++i)
			{
				FCmdFunction Cmd(MoveTemp(CurrentQ[i]));
				Cmd();
				NumCmds--;
			}
			CurrentQ.Reset();
		}
		void QueueTask(
			FCmdFunction&& InFunction)
		{
			// Producer thread.
			Q[WriteIndex].Add(MoveTemp(InFunction));
			NumCmds++;
		}
	};

	struct FDecoderScheduler : public IDecoderScheduler
	{
		struct FActiveDecoder
		{
			enum EDecodeState
			{
				eStart,
				eCreatingDecoder,
				eParsingHeader,
				eDecodingPackets,
				eFinished
			};

			bool Update()
			{
				switch(State)
				{
					case eStart:
					{
						State = eCreatingDecoder;
					} 
					// fall-through.
					case eCreatingDecoder:
					{
						ICodec* pCodec = ICodecRegistry::Get().FindCodecByParsingInput(
							StartArgs.InputObject.Get());

						if( !pCodec )
						{
							return false;
						}
						Decoder = pCodec->CreateDecoder(
							StartArgs.InputObject.Get(),
							StartArgs.OutputObject.Get() );							

						State = eParsingHeader;
					}
					// fall-through.
					case eParsingHeader:
					{
						/*FDecodedFormatInfo Info;												
						if( !Decoder->ParseHeader(StartArgs.InputObject.Get(),Info))
						{
							return false;
						}*/
						State = eDecodingPackets;
					}
					// fall-through.
					case eDecodingPackets:
					{
						// Do the decode.
						IDecoder::EDecodeResult Result = Decoder->Decode();

						if (Result == IDecoder::EDecodeResult::Finished || 
							Result == IDecoder::EDecodeResult::Fail )
						{
							State = eFinished;
						}
						else
						{
							break;
						}
					}
					// fall-through.
					case eFinished:
					{
						break;
					}
					default: 
					{
						checkNoEntry();
					}
				}

				// WIP. State machine logic.
				return State != eFinished;
			}

			FStartDecodeArgs StartArgs;
			TUniquePtr<IDecoder> Decoder;
			EDecodeState State = eStart;
		};

		FCmdQueue CmdQueue;	
		using FActiveMap = TMap<uint32, FActiveDecoder>;
		TMap<uint32, FActiveDecoder> ActiveMap;

		FDecoderScheduler() = default;
		virtual ~FDecoderScheduler() = default;

		void Update()
		{
			// Execute any cmds we have queued.
			CmdQueue.Execute();

			// Update active decodes.
			for( auto i = ActiveMap.CreateIterator(); i; ++i )
			{
				if( !i->Value.Update() )
				{
					i.RemoveCurrent();
				}
			}
		}

		FActiveDecodeHandle StartDecode(
			FStartDecodeArgs&& InArgs ) override
		{
			static TAtomic<uint32> sCounter;
			uint32 Id = ++sCounter;

			CmdQueue.QueueTask([this, Id, Args = MoveTemp(InArgs)] 
			{
				check(ActiveMap.Find(Id) == nullptr);
				ActiveMap.Emplace(Id, FActiveDecoder { Args } );	
			});
			return FActiveDecodeHandle(Id);
		}
	};
	
	Audio::IDecoderScheduler& IDecoderScheduler::Get()
	{
		static FDecoderScheduler sInstance;
		return sInstance;
	}

} //namespace Audio