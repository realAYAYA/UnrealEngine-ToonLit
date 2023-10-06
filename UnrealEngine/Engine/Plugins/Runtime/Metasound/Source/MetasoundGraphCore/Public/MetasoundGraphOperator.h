// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertexData.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	// Forward declare
	namespace DirectedGraphAlgo
	{
		struct FGraphOperatorData;
	}

	class METASOUNDGRAPHCORE_API FGraphOperator : public TExecutableOperator<FGraphOperator>
	{
		public:
			using FOperatorPtr = TUniquePtr<IOperator>;
			using FExecuteFunction = IOperator::FExecuteFunction;
			using FResetFunction = IOperator::FResetFunction;
			using FResetParams = IOperator::FResetParams;

			FGraphOperator() = default;
			FGraphOperator(TUniquePtr<DirectedGraphAlgo::FGraphOperatorData>&& InOperatorData);

			virtual ~FGraphOperator() = default;

			// Add an operator to the end of the executation stack.
			void AppendOperator(FOperatorPtr InOperator);

			// Set the vertex interface data. This data will be copied to output 
			// during calls to Bind(InOutVertexData).
			void SetVertexInterfaceData(FVertexInterfaceData&& InVertexData);

			virtual FDataReferenceCollection GetInputs() const override;

			virtual FDataReferenceCollection GetOutputs() const override;

			// Bind the graph's interface data references to FVertexInterfaceData.
			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override;
			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override;

			virtual IOperator::FPostExecuteFunction GetPostExecuteFunction() override;

			void Execute();
			void PostExecute();
			void Reset(const FResetParams& InParams);

		private:
			// Delete copy operator because underlying types cannot be copied. 
			FGraphOperator& operator=(const FGraphOperator&) = delete;
			FGraphOperator(const FGraphOperator&) = delete;

			static void StaticPostExecute(IOperator* Operator);

			struct FExecuteEntry
			{
				FExecuteEntry(IOperator& InOperator, FExecuteFunction InFunc);
				void Execute();

				IOperator* Operator;
				FExecuteFunction Function;	
			};

			struct FPostExecuteEntry
			{
				FPostExecuteEntry(IOperator& InOperator, FPostExecuteFunction InFunc);
				void PostExecute();

				IOperator* Operator;
				FPostExecuteFunction Function;	
			};

			struct FResetEntry
			{
				FResetEntry(IOperator& InOperator, FResetFunction InFunc);
				void Reset(const FResetParams& InParams);

				IOperator* Operator;
				FResetFunction Function;	
			};

			TArray<FExecuteEntry> ExecuteStack;
			TArray<FPostExecuteEntry> PostExecuteStack;
			TArray<FResetEntry> ResetStack;
			TArray<TUniquePtr<IOperator>> ActiveOperators;
			FVertexInterfaceData VertexData;
	};
}
