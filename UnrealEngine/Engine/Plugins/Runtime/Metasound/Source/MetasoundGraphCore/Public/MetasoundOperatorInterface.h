// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReferenceCollection.h"
#include "MetasoundVertexData.h"

namespace Metasound
{
	/** IOperator
	 *
	 *  IOperator defines the interface for render time operations.  IOperators are created using an INodeOperatorFactory.
	 */
	class IOperator
	{
	public:
		/** Pointer to execute function for an operator.
		 *
		 * @param IOperator* - The operator associated with the function pointer.
		 */
		typedef void(*FExecuteFunction)(IOperator*);

		virtual ~IOperator() {}

		/** Return the input parameters associated with this IOperator.
		 *
		 * Implementations of IOperator should populate and return their input parameters
		 * if they want to enable callers to query and write to their parameters. Most IOperator
		 * implementations will return an empty FDataReferenceCollection because thier inputs are
		 * set during the IOperator's construction and do not need to be updated afterwards. The
		 * exceptions are input operators and graph operators which need to interface with external
		 * systems even after the operator is created.
		 */
		virtual FDataReferenceCollection GetInputs() const = 0;

		/** Return the output parameters associated with this IOperator.
		 *
		 * Implementations of IOperator should return a collection of their output parameters
		 * which other nodes can read.
		 */
		virtual FDataReferenceCollection GetOutputs() const = 0;

		/** Bind data references to the interface of this operator.
		 *
		 * Operators should bind all data references to the vertices of the provided
		 * InVertexData. This method is meant to replace GetInputs() and GetOutputs()
		 * because it provides better error checking against mismatched vertex names.
		 */
		virtual void Bind(FVertexInterfaceData& InVertexData) const
		{
			InVertexData.GetInputs().Bind(GetInputs());
			InVertexData.GetOutputs().Bind(GetOutputs());
		}

		/** Return the execution function to call during graph execution.
		 *
		 * The IOperator* argument to the FExecutionFunction will be the same IOperator instance
		 * which returned the execution function.
		 *
		 * nullptr return values are valid and signal an IOperator which does not need to be
		 * executed.
		 */
		virtual FExecuteFunction GetExecuteFunction() = 0;
	};
}
