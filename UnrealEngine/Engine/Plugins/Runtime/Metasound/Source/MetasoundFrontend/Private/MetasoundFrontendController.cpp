// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendController.h"

#include "MetasoundFrontendInvalidController.h"
#include "MetasoundFrontendDocumentController.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"


namespace Metasound
{
	namespace Frontend
	{
		FOutputHandle IOutputController::GetInvalidHandle()
		{
			static FOutputHandle Invalid = MakeShared<FInvalidOutputController>();
			return Invalid;
		}

		FInputHandle IInputController::GetInvalidHandle()
		{
			static FInputHandle Invalid = MakeShared<FInvalidInputController>();
			return Invalid;
		}

		FVariableHandle IVariableController::GetInvalidHandle()
		{
			static FVariableHandle Invalid = MakeShared<FInvalidVariableController>();
			return Invalid;
		}

		FNodeHandle INodeController::GetInvalidHandle()
		{
			static FNodeHandle Invalid = MakeShared<FInvalidNodeController>();
			return Invalid;
		}

		FGraphHandle IGraphController::GetInvalidHandle()
		{
			static FGraphHandle Invalid = MakeShared<FInvalidGraphController>();
			return Invalid;
		}

		FDocumentHandle IDocumentController::GetInvalidHandle()
		{
			static FDocumentHandle Invalid = MakeShared<FInvalidDocumentController>();
			return Invalid;
		}
			
		FDocumentHandle IDocumentController::CreateDocumentHandle(FDocumentAccessPtr InDocument)
		{
			// Create using standard document controller.
			return FDocumentController::CreateDocumentHandle(InDocument);
		}

		FDocumentHandle IDocumentController::CreateDocumentHandle(FMetasoundFrontendDocument& InDocument)
		{
			return CreateDocumentHandle(MakeAccessPtr<FDocumentAccessPtr>(InDocument.AccessPoint, InDocument));
		}

		FConstDocumentHandle IDocumentController::CreateDocumentHandle(FConstDocumentAccessPtr InDocument)
		{
			// Create using standard document controller. 
			return FDocumentController::CreateDocumentHandle(ConstCastAccessPtr<FDocumentAccessPtr>(InDocument));
		}

		FConstDocumentHandle IDocumentController::CreateDocumentHandle(const FMetasoundFrontendDocument& InDocument)
		{
			return CreateDocumentHandle(MakeAccessPtr<FConstDocumentAccessPtr>(InDocument.AccessPoint, InDocument));
		}

		FDocumentAccess IDocumentAccessor::GetSharedAccess(IDocumentAccessor& InDocumentAccessor)
		{
			return InDocumentAccessor.ShareAccess();
		}

		FConstDocumentAccess IDocumentAccessor::GetSharedAccess(const IDocumentAccessor& InDocumentAccessor)
		{
			return InDocumentAccessor.ShareAccess();
		}

		FConstOutputHandle FindReroutedOutput(FConstOutputHandle InOutputHandle)
		{
			using namespace Frontend;

			if (InOutputHandle->IsValid())
			{
				FConstNodeHandle NodeHandle = InOutputHandle->GetOwningNode();
				if (NodeHandle->IsValid())
				{
					if (NodeHandle->GetClassMetadata().GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						TArray<FConstInputHandle> Inputs = NodeHandle->GetConstInputs();
						if (!Inputs.IsEmpty())
						{
							FConstInputHandle RerouteInputHandle = Inputs.Last();
							if (RerouteInputHandle->IsValid())
							{
								FConstOutputHandle ConnectedOutputHandle = RerouteInputHandle->GetConnectedOutput();
								return FindReroutedOutput(ConnectedOutputHandle);
							}
						}
					}
				}
			}

			return InOutputHandle;
		}

		void FindReroutedInputs(FConstInputHandle InHandleToCheck, TArray<FConstInputHandle>& InOutInputHandles)
		{
			using namespace Frontend;

			if (InHandleToCheck->IsValid())
			{
				FConstNodeHandle NodeHandle = InHandleToCheck->GetOwningNode();
				if (NodeHandle->IsValid())
				{
					if (NodeHandle->GetClassMetadata().GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						TArray<FConstOutputHandle> Outputs = NodeHandle->GetConstOutputs();
						for (FConstOutputHandle& OutputHandle : Outputs)
						{
							TArray<FConstInputHandle> LinkedInputs = OutputHandle->GetConstConnectedInputs();
							for (FConstInputHandle LinkedInput : LinkedInputs)
							{
								FindReroutedInputs(LinkedInput, InOutInputHandles);
							}
						}

						return;
					}

					InOutInputHandles.Add(InHandleToCheck);
				}

			}
		}

		void IterateReroutedInputs(FConstInputHandle InHandleToCheck, TFunctionRef<void(FConstInputHandle)> Func)
		{
			using namespace Frontend;

			if (InHandleToCheck->IsValid())
			{
				FConstNodeHandle NodeHandle = InHandleToCheck->GetOwningNode();
				if (NodeHandle->IsValid())
				{
					if (NodeHandle->GetClassMetadata().GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						TArray<FConstOutputHandle> Outputs = NodeHandle->GetConstOutputs();
						for (FConstOutputHandle& OutputHandle : Outputs)
						{
							TArray<FConstInputHandle> LinkedInputs = OutputHandle->GetConstConnectedInputs();
							for (FConstInputHandle LinkedInput : LinkedInputs)
							{
								IterateReroutedInputs(LinkedInput, Func);
							}
						}

						return;
					}

					Func(InHandleToCheck);
				}

			}
		}
	} // namespace Frontend
} // namespace Metasound
