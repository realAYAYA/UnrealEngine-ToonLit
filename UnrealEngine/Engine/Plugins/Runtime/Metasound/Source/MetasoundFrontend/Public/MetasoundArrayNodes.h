// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundEnvironment.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"

#include <type_traits>

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
	namespace MetasoundArrayNodesPrivate
	{
		// Convenience function for make FNodeClassMetadata of array nodes.
		METASOUNDFRONTEND_API FNodeClassMetadata CreateArrayNodeClassMetadata(const FName& InDataTypeName, const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface);

		// Retrieve the ElementType from an ArrayType
		template<typename ArrayType>
		struct TArrayElementType
		{
			// Default implementation has Type. 
		};

		// ElementType specialization for TArray types.
		template<typename ElementType>
		struct TArrayElementType<TArray<ElementType>>
		{
			using Type = ElementType;
		};
	}

	namespace ArrayNodeVertexNames
	{
		static const FLazyName InputInitialArrayName = TEXT("Array");
#if WITH_EDITOR
		static const FText InputInitialArrayTooltip = LOCTEXT("InitialArrayTooltip", "Initial Array");
		static const FText InputInitialArrayDisplayName = LOCTEXT("InitialArrayDisplayName", "Init Array"); 
#else
		static const FText InputInitialArrayTooltip = FText::GetEmpty();
		static const FText InputInitialArrayDisplayName = FText::GetEmpty();
#endif

		METASOUND_PARAM(InputArray, "Array", "Input Array.")
		METASOUND_PARAM(InputLeftArray, "Left Array", "Input Left Array.")
		METASOUND_PARAM(InputRightArray, "Right Array", "Input Right Array.")
		METASOUND_PARAM(InputTriggerGet, "Trigger", "Trigger to get value.")
		METASOUND_PARAM(InputTriggerSet, "Trigger", "Trigger to set value.")
		METASOUND_PARAM(InputIndex, "Index", "Index in Array.")
		METASOUND_PARAM(InputStartIndex, "Start Index", "First index to include.")
		METASOUND_PARAM(InputEndIndex, "End Index", "Last index to include.")
		METASOUND_PARAM(InputValue, "Value", "Value to set.")

		METASOUND_PARAM(OutputNum, "Num", "Number of elements in the array.")
		METASOUND_PARAM(OutputValue, "Element", "Value of element at array index.")
		METASOUND_PARAM(OutputArrayConcat, "Array", "Array after concatenation.")
		METASOUND_PARAM(OutputArraySet, "Array", "Array after setting.")
		METASOUND_PARAM(OutputArraySubset, "Array", "Subset of input array.")
	};

	/** TArrayNumOperator gets the number of elements in an Array. The operator
	 * uses the FNodeFacade and defines the vertex, metadata and vertex interface
	 * statically on the operator class. */
	template<typename ArrayType>
	class TArrayNumOperator : public TExecutableOperator<TArrayNumOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;

		// Declare the vertex interface
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray))
				),
				FOutputVertexInterface(
					TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNum))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				const FName OperatorName = TEXT("Num");
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpArrayNumDisplayNamePattern", "Num ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ArrayOpArrayNumDescription", "Number of elements in the array");
				const FVertexInterface NodeInterface = GetDefaultInterface();
			
				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};
			
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayNodeVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			// Get the input array or construct an empty one. 
			FArrayDataReadReference Array = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputArray), InParams.OperatorSettings);

			return MakeUnique<TArrayNumOperator>(Array);
		}

		TArrayNumOperator(FArrayDataReadReference InArray)
		: Array(InArray)
		, Num(TDataWriteReference<int32>::CreateNew())
		{
			// Initialize value for downstream nodes.
			*Num = Array->Num();
		}

		virtual ~TArrayNumOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), Array);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputNum), Num);
		}

		void Execute()
		{
			*Num = Array->Num();
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			Execute();
		}

	private:

		FArrayDataReadReference Array;
		TDataWriteReference<int32> Num;
	};

	template<typename ArrayType>
	class TArrayNumNode : public FNodeFacade
	{
	public:
		TArrayNumNode(const FNodeInitData& InInitData)
		: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayNumOperator<ArrayType>>())
		{
		}

		virtual ~TArrayNumNode() = default;
	};

	/** TArrayGetOperator copies a value from the array to the output when
	 * a trigger occurs. Initially, the output value is default constructed and
	 * will remain that way until until a trigger is encountered.
	 */
	template<typename ArrayType>
	class TArrayGetOperator : public TExecutableOperator<TArrayGetOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;
		using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeVertexNames;
			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerGet)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray)),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIndex))
				),
				FOutputVertexInterface(
					TOutputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputValue))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				const FName OperatorName = TEXT("Get"); 
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpArrayGetDisplayNamePattern", "Get ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ArrayOpArrayGetDescription", "Get element at index in array.");
				const FVertexInterface NodeInterface = GetDefaultInterface();
			
				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};
			
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		struct FInitParams
		{
			TDataReadReference<FTrigger> Trigger;
			FArrayDataReadReference Array;
			TDataReadReference<int32> Index;
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			FString GraphName;
#endif
		};

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayNodeVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			// Input Trigger
			TDataReadReference<FTrigger> Trigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerGet), InParams.OperatorSettings);
			
			// Input Array
			FArrayDataReadReference Array = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputArray), InParams.OperatorSettings);

			// Input Index
			TDataReadReference<int32> Index = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputIndex), InParams.OperatorSettings);
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			FString GraphName;
			if (InParams.Environment.Contains<FString>(Frontend::SourceInterface::Environment::GraphName))
			{
				GraphName = InParams.Environment.GetValue<FString>(Frontend::SourceInterface::Environment::GraphName);
			}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

			FInitParams OperatorInitParams
			{
				Trigger
				, Array
				, Index
#if WITH_METASOUND_DEBUG_ENVIRONMENT
				, GraphName
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
			};

			return MakeUnique<TArrayGetOperator>(InParams.OperatorSettings, MoveTemp(OperatorInitParams));
		}



		TArrayGetOperator(const FOperatorSettings& InSettings, FInitParams&& InParams)
		: Trigger(InParams.Trigger)
		, Array(InParams.Array)
		, Index(InParams.Index)
		, Value(TDataWriteReferenceFactory<ElementType>::CreateExplicitArgs(InSettings))
#if WITH_METASOUND_DEBUG_ENVIRONMENT
		, GraphName(InParams.GraphName)
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
		{
			const int32 IndexValue = *Index;
			const ArrayType& ArrayRef = *Array;

			if ((IndexValue >= 0) && (IndexValue < ArrayRef.Num()))
			{
				*Value = ArrayRef[IndexValue];
			}
		}

		virtual ~TArrayGetOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerGet), Trigger);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), Array);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputIndex), Index);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputValue), Value);
		}

		void Execute()
		{
			// Only perform get on trigger.
			if (*Trigger)
			{
				const int32 IndexValue = *Index;
				const ArrayType& ArrayRef = *Array;

				if ((IndexValue >= 0) && (IndexValue < ArrayRef.Num()))
				{
					*Value = ArrayRef[IndexValue];
				}
#if WITH_METASOUND_DEBUG_ENVIRONMENT
				else
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Attempt to get value at invalid index [ArraySize:%d, Index:%d] in MetaSound Graph \"%s\"."), ArrayRef.Num(), IndexValue, *GraphName);
				}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
			}
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			const int32 IndexValue = *Index;
			const ArrayType& ArrayRef = *Array;

			if ((IndexValue >= 0) && (IndexValue < ArrayRef.Num()))
			{
				*Value = ArrayRef[IndexValue];
			}
			else
			{
				*Value = TDataTypeFactory<ElementType>::CreateExplicitArgs(InParams.OperatorSettings);
			}
		}

	private:

		TDataReadReference<FTrigger> Trigger;
		FArrayDataReadReference Array;
		TDataReadReference<int32> Index;
		TDataWriteReference<ElementType> Value;

#if WITH_METASOUND_DEBUG_ENVIRONMENT
		FString GraphName;
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

	};

	template<typename ArrayType>
	class TArrayGetNode : public FNodeFacade
	{
	public:
		TArrayGetNode(const FNodeInitData& InInitData)
		: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayGetOperator<ArrayType>>())
		{
		}

		virtual ~TArrayGetNode() = default;
	};

	/** TArraySetOperator sets an element in an array to a specific value. */
	template<typename ArrayType>
	class TArraySetOperator : public TExecutableOperator<TArraySetOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;
		using FArrayDataWriteReference = TDataWriteReference<ArrayType>;
		using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeVertexNames;
			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSet)),
					TInputDataVertex<ArrayType>(InputInitialArrayName, FDataVertexMetadata { InputInitialArrayTooltip, InputInitialArrayDisplayName }),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIndex)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue))
				),
				FOutputVertexInterface(
					TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArraySet))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				const FName OperatorName = TEXT("Set"); 
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpArraySetDisplayNamePattern", "Set ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ArrayOpArraySetDescription", "Set element at index in array.");
				const FVertexInterface NodeInterface = GetDefaultInterface();
			
				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};
			
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		struct FInitParams
		{
			TDataReadReference<FTrigger> Trigger;
			FArrayDataReadReference InitArray;
			FArrayDataWriteReference Array;
			TDataReadReference<int32> Index;
			TDataReadReference<ElementType> Value;
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			FString GraphName;
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
		};

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayNodeVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;
			
			TDataReadReference<FTrigger> Trigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerSet), InParams.OperatorSettings);

			FArrayDataReadReference InitArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(InputInitialArrayName, InParams.OperatorSettings);
			FArrayDataWriteReference Array = TDataWriteReferenceFactory<ArrayType>::CreateExplicitArgs(InParams.OperatorSettings, *InitArray);

			TDataReadReference<int32> Index = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputIndex), InParams.OperatorSettings);

			TDataReadReference<ElementType> Value = InputData.GetOrCreateDefaultDataReadReference<ElementType>(METASOUND_GET_PARAM_NAME(InputValue), InParams.OperatorSettings);
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			FString GraphName;
			if (InParams.Environment.Contains<FString>(Frontend::SourceInterface::Environment::GraphName))
			{
				GraphName = InParams.Environment.GetValue<FString>(Frontend::SourceInterface::Environment::GraphName);
			}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

			FInitParams OperatorInitParams 
			{
				Trigger
				, InitArray
				, Array 
				, Index 
				, Value
#if WITH_METASOUND_DEBUG_ENVIRONMENT
				, GraphName
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
			};

			return MakeUnique<TArraySetOperator>(InParams.OperatorSettings, MoveTemp(OperatorInitParams));
		}

		TArraySetOperator(const FOperatorSettings& InSettings, FInitParams&& InParams)
		: OperatorSettings(InSettings)
		, Trigger(InParams.Trigger)
		, InitArray(InParams.InitArray)
		, Array(InParams.Array)
		, Index(InParams.Index)
		, Value(InParams.Value)
#if WITH_METASOUND_DEBUG_ENVIRONMENT
		, GraphName(InParams.GraphName)
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
		{
		}

		virtual ~TArraySetOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSet), Trigger);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InitArray);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputIndex), Index);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue), Value);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArraySet), Array);
		}

		void Execute()
		{
			if (*Trigger)
			{
				const int32 IndexValue = *Index;
				ArrayType& ArrayRef = *Array;

				if ((IndexValue >= 0) && (IndexValue < ArrayRef.Num()))
				{
					ArrayRef[IndexValue] = *Value;
				}
#if WITH_METASOUND_DEBUG_ENVIRONMENT
				else
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Attempt to set value at invalid index [ArraySize:%d, Index:%d] in MetaSound Graph \"%s\"."), ArrayRef.Num(), IndexValue, *GraphName);
				}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
			}
		}

		void Reset(const IOperator::FResetParams& Inparams)
		{
			*Array = *InitArray;
		}

	private:
		FOperatorSettings OperatorSettings;

		TDataReadReference<FTrigger> Trigger;
		FArrayDataReadReference InitArray;
		FArrayDataWriteReference Array;
		TDataReadReference<int32> Index;
		TDataReadReference<ElementType> Value;

#if WITH_METASOUND_DEBUG_ENVIRONMENT
		FString GraphName;
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

	};

	template<typename ArrayType>
	class TArraySetNode : public FNodeFacade
	{
	public:
		TArraySetNode(const FNodeInitData& InInitData)
		: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArraySetOperator<ArrayType>>())
		{
		}

		virtual ~TArraySetNode() = default;
	};

	/** TArrayConcatOperator concatenates two arrays on trigger. */
	template<typename ArrayType>
	class TArrayConcatOperator : public TExecutableOperator<TArrayConcatOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;
		using FArrayDataWriteReference = TDataWriteReference<ArrayType>;
		using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerGet)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftArray)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightArray))
				),
				FOutputVertexInterface(
					TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArrayConcat))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				const FName OperatorName = TEXT("Concat"); 
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpArrayConcatDisplayNamePattern", "Concatenate ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ArrayOpArrayConcatDescription", "Concatenates two arrays on trigger.");
				const FVertexInterface NodeInterface = GetDefaultInterface();
			
				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};
			
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayNodeVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;
			
			TDataReadReference<FTrigger> Trigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerGet), InParams.OperatorSettings);

			FArrayDataReadReference LeftArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputLeftArray), InParams.OperatorSettings);
			FArrayDataReadReference RightArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputRightArray), InParams.OperatorSettings);

			return MakeUnique<TArrayConcatOperator>(Trigger, LeftArray, RightArray);
		}


		TArrayConcatOperator(TDataReadReference<FTrigger> InTrigger, FArrayDataReadReference InLeftArray, FArrayDataReadReference InRightArray)
		: Trigger(InTrigger)
		, LeftArray(InLeftArray)
		, RightArray(InRightArray)
		, OutArray(TDataWriteReference<ArrayType>::CreateNew())
		{
		}

		virtual ~TArrayConcatOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerGet), Trigger);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLeftArray), LeftArray);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRightArray), RightArray);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArrayConcat), OutArray);
		}

		void Execute()
		{
			if (*Trigger)
			{
				*OutArray = *LeftArray;
				OutArray->Append(*RightArray);
			}
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			OutArray->Reset();
		}

	private:

		TDataReadReference<FTrigger> Trigger;
		FArrayDataReadReference LeftArray;
		FArrayDataReadReference RightArray;
		FArrayDataWriteReference OutArray;
	};

	template<typename ArrayType>
	class TArrayConcatNode : public FNodeFacade
	{
	public:
		TArrayConcatNode(const FNodeInitData& InInitData)
		: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayConcatOperator<ArrayType>>())
		{
		}

		virtual ~TArrayConcatNode() = default;
	};

	/** TArraySubsetOperator slices an array on trigger. */
	template<typename ArrayType>
	class TArraySubsetOperator : public TExecutableOperator<TArraySubsetOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;
		using FArrayDataWriteReference = TDataWriteReference<ArrayType>;
		using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerGet)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray)),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputStartIndex)),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEndIndex))

				),
				FOutputVertexInterface(
					TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArraySubset))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				const FName OperatorName = TEXT("Subset"); 
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpArraySubsetDisplayNamePattern", "Subset ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ArrayOpArraySubsetDescription", "Subset array on trigger.");
				const FVertexInterface NodeInterface = GetDefaultInterface();
			
				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};
			
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayNodeVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;
			
			TDataReadReference<FTrigger> Trigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerGet), InParams.OperatorSettings);

			FArrayDataReadReference InArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputArray), InParams.OperatorSettings);

			TDataReadReference<int32> StartIndex = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputStartIndex), InParams.OperatorSettings);
			TDataReadReference<int32> EndIndex = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputEndIndex), InParams.OperatorSettings);

			TDataWriteReference<ArrayType> OutputArray = TDataWriteReferenceFactory<ArrayType>::CreateExplicitArgs(InParams.OperatorSettings);

			return MakeUnique<TArraySubsetOperator>(Trigger, InArray, StartIndex, EndIndex, OutputArray);
		}


		TArraySubsetOperator(TDataReadReference<FTrigger> InTrigger, FArrayDataReadReference InInputArray, TDataReadReference<int32> InStartIndex, TDataReadReference<int32> InEndIndex, TDataWriteReference<ArrayType> InOutputArray)
		: Trigger(InTrigger)
		, InputArray(InInputArray)
		, StartIndex(InStartIndex)
		, EndIndex(InEndIndex)
		, OutputArray(InOutputArray)
		{
		}

		virtual ~TArraySubsetOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			checkNoEntry();
			return {};
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerGet), Trigger);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputStartIndex), StartIndex);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEndIndex), EndIndex);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			using namespace ArrayNodeVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArraySubset), OutputArray);
		}

		void Execute()
		{
			if (*Trigger)
			{
				OutputArray->Reset();

				const ArrayType& InputArrayRef = *InputArray;
				const int32 StartIndexValue = FMath::Max(0, *StartIndex);
				const int32 EndIndexValue = FMath::Min(InputArrayRef.Num(), *EndIndex + 1);

				if (StartIndexValue < EndIndexValue)
				{
					const int32 Num = EndIndexValue - StartIndexValue;
					OutputArray->Append(&InputArrayRef[StartIndexValue], Num);
				}
			}
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			OutputArray->Reset();
		}

	private:

		TDataReadReference<FTrigger> Trigger;
		FArrayDataReadReference InputArray;
		TDataReadReference<int32> StartIndex;
		TDataReadReference<int32> EndIndex;
		FArrayDataWriteReference OutputArray;
	};

	template<typename ArrayType>
	class TArraySubsetNode : public FNodeFacade
	{
	public:
		TArraySubsetNode(const FNodeInitData& InInitData)
		: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArraySubsetOperator<ArrayType>>())
		{
		}

		virtual ~TArraySubsetNode() = default;
	};
} // namespace Metasound
#undef LOCTEXT_NAMESPACE
