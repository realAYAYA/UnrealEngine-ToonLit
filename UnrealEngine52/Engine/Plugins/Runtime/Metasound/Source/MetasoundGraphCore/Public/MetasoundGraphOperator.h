// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundExecutableOperator.h"

namespace Metasound
{
	class METASOUNDGRAPHCORE_API FGraphOperator : public IOperator
	{
		public:
			using FOperatorPtr = TUniquePtr<IOperator>;
			using FExecuteFunction = IOperator::FExecuteFunction;

			FGraphOperator() = default;

			virtual ~FGraphOperator();

			// Add an operator to the end of the executation stack.
			void AppendOperator(FOperatorPtr InOperator);

			UE_DEPRECATED(5.1, "Use FGraphOperator::SetVertexInterfaceData instead.")
			void SetInputs(const FDataReferenceCollection& InCollection);

			UE_DEPRECATED(5.1, "Use FGraphOperator::SetVertexInterfaceData instead.")
			void SetOutputs(const FDataReferenceCollection& InCollection);

			// Set the vertex interface data. This data will be copied to output 
			// during calls to Bind(InOutVertexData).
			void SetVertexInterfaceData(FVertexInterfaceData&& InVertexData);

			virtual FDataReferenceCollection GetInputs() const override;

			virtual FDataReferenceCollection GetOutputs() const override;

			// Bind the graph's interface data references to FVertexInterfaceData.
			virtual void Bind(FVertexInterfaceData& InOutVertexData) const override;

			virtual FExecuteFunction GetExecuteFunction() override;

			void Execute();

		private:
			// Delete copy operator because underlying types cannot be copied. 
			FGraphOperator& operator=(const FGraphOperator&) = delete;
			FGraphOperator(const FGraphOperator&) = delete;

			static void ExecuteFunction(IOperator* InOperator);

			TArray<FExecuter> OperatorStack;
			FVertexInterfaceData VertexData;
	};
}
