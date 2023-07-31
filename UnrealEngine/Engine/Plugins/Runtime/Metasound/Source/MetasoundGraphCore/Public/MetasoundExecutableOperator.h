// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetasoundOperatorInterface.h"


namespace Metasound
{
	// As a general rule, ExecutableDataTypes should be avoided whenever possible
	// as they incur an undesired cost and are generally not typically necessary.
	// This is primarily for the special case of trigger types, where state management
	// cannot be avoided (or rather an avoidable design has yet to be formulated).
	template<class DataType>
	struct TExecutableDataType
	{
		static constexpr bool bIsExecutable = false;

		static void Execute(const DataType& InData, const DataType& OutData)
		{
			// No-Op for base case as most DataTypes (ex POD) are not executable.
		}

		static void ExecuteInline(DataType& InData, bool bInUpdated)
		{
			// No-Op for base case as most DataTypes (ex POD) are not executable.
		}
	};

	/** Convenience class for supporting the IOperator interface's GetExecuteFunction virtual member function.
	 *
	 * Derived classes should inherit from this template class as well as implement a void Execute() member
	 * function.
	 *
	 * 	class MyOperator : public TExecutableOperator<MyOperator>
	 * 	{
	 * 	  public:
	 * 	  ...
	 * 	  void Execute()
	 * 	  {
	 *		  ...
	 * 	  }
	 * 	};
	 */

	template<class DerivedOperatorType>
	class TExecutableOperator : public IOperator
	{
		public:

			virtual ~TExecutableOperator() {}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				return &TExecutableOperator<DerivedOperatorType>::ExecuteFunction;
			}

		private:

			static void ExecuteFunction(IOperator* InOperator)
			{
				DerivedOperatorType* DerivedOperator = static_cast<DerivedOperatorType*>(InOperator);

				check(nullptr != DerivedOperator);

				DerivedOperator->Execute();
			}
	};

	class FNoOpOperator : public IOperator
	{
		public:
			virtual ~FNoOpOperator() {}

			FExecuteFunction GetExecuteFunction() override
			{
				return nullptr;
			}

			FDataReferenceCollection GetInputs() const override
			{
				return FDataReferenceCollection{};
			}

			FDataReferenceCollection GetOutputs() const override
			{
				return FDataReferenceCollection{};
			}
	};

	/** FExecuter
	 *
	 * Wraps an IOperator and provides an Execute() member function.
	 */
	class FExecuter : public IOperator
	{
		public:
			using FOperatorPtr = TUniquePtr<IOperator>;
			using FExecuteFunction = IOperator::FExecuteFunction;

			FExecuter()
			:	ExecuteFunction(&FExecuter::NoOp)
			{
			}

			FExecuter(FOperatorPtr InOperator)
			:	ExecuteFunction(&FExecuter::NoOp)
			{
				SetOperator(MoveTemp(InOperator));
			}

			void SetOperator(FOperatorPtr InOperator)
			{
				Operator = MoveTemp(InOperator);

				if (Operator.IsValid())
				{
					FExecuteFunction Func = Operator->GetExecuteFunction();

					if (nullptr != Func)
					{
						ExecuteFunction = Func;
					}
				}
				else
				{
					ExecuteFunction = &FExecuter::NoOp;
				}
			}

			void Execute()
			{
				ExecuteFunction(Operator.Get());
			}

			bool IsNoOp()
			{
				return (ExecuteFunction == &FExecuter::NoOp);
			}

			bool IsValid() const
			{
				return Operator.IsValid();
			}

			virtual FDataReferenceCollection GetInputs() const override
			{
				static FDataReferenceCollection EmptyCollection;

				if (Operator.IsValid())
				{
					return Operator->GetInputs();
				}

				return EmptyCollection;
			}

			virtual FDataReferenceCollection GetOutputs() const override
			{
				static FDataReferenceCollection EmptyCollection;

				if (Operator.IsValid())
				{
					return Operator->GetOutputs();
				}

				return EmptyCollection;
			}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				return ExecuteFunction;
			}

		private:
			FOperatorPtr Operator;

			FExecuteFunction ExecuteFunction;

			static void NoOp(IOperator*)
			{
			}
	};
}
