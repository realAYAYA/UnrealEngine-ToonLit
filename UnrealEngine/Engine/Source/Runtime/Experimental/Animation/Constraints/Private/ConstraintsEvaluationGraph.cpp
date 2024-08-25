// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConstraintsEvaluationGraph.h"

#include "ConstraintsManager.h"
#include "ConstraintSubsystem.h"

namespace ConstraintsEvaluationGraph
{

static bool	bUseEvaluationGraph = true;
static FAutoConsoleVariableRef CVarUseEvaluationGraph(
	TEXT("Constraints.UseEvaluationGraph"),
	bUseEvaluationGraph,
	TEXT("Use Evaluation Graph to update constraints when manipulating.") );
	
static bool	bDebugGraph = false;
static FAutoConsoleVariableRef CVarDebugEvaluationGraph(
	TEXT("Constraints.DebugEvaluationGraph"),
	bDebugGraph,
	TEXT("Print debug info about constraitns evaluation graph.") );
	
}

bool FConstraintsEvaluationGraph::UseEvaluationGraph()
{
	return ConstraintsEvaluationGraph::bUseEvaluationGraph;
}

FConstraintNode& FConstraintsEvaluationGraph::GetNode(const TWeakObjectPtr<UTickableConstraint>& InConstraint)
{
	const int32 Found = Nodes.IndexOfByPredicate([InConstraint](const FConstraintNode& Node)
	{
		return InConstraint.IsValid() && Node.ConstraintID == InConstraint->ConstraintID; 
	});

	if (Found != INDEX_NONE)
	{
		return Nodes[Found];
	}

	FConstraintNode Node;
	Node.ConstraintID = InConstraint->ConstraintID;
	Node.ConstraintTick = &InConstraint->GetTickFunction(ConstraintsInWorld.World.Get());
	return Nodes.Emplace_GetRef(MoveTemp(Node)); 
}

FConstraintNode* FConstraintsEvaluationGraph::FindNode(const TWeakObjectPtr<UTickableConstraint>& InConstraint)
{
	if (!InConstraint.IsValid())
	{
		return nullptr;
	}
	
	return Nodes.FindByPredicate([InConstraint](const FConstraintNode& Node)
	{
		return InConstraint.IsValid() && Node.ConstraintID == InConstraint->ConstraintID; 
	});
}

void FConstraintsEvaluationGraph::FlushPendingEvaluations()
{
	if (State == InvalidData || State == Flushing)
	{
		return;
	}
	
	if (Nodes.IsEmpty())
	{
		return;
	}
	
	if (ConstraintsEvaluationGraph::bDebugGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("Flush Constraints Evaluation Graph"));
	}

	State = Flushing;
	
	for (FConstraintNode& Node: Nodes)
	{
		if (Node.bMarkedForEvaluation)
		{
			Evaluate(&Node);
		}
	}

	const bool bHasNodesToEvaluate = Nodes.ContainsByPredicate([](const FConstraintNode& Node)
	{
		return Node.bMarkedForEvaluation;
	});
	ensure(!bHasNodesToEvaluate);

	State = ReadyForEvaluation;
}

void FConstraintsEvaluationGraph::Rebuild()
{
	Nodes.Empty();
	
	if (!ensure(ConstraintsInWorld.World.Get()))
	{
		return;
	}

	const TArray<TWeakObjectPtr<UTickableConstraint>>& Constraints = ConstraintsInWorld.Constraints;
	if (Constraints.IsEmpty())
	{
		return;
	}

	// build nodes
	for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ConstraintIndex++)
	{
		if (Constraints[ConstraintIndex].IsValid())
		{
			FConstraintNode& Node = GetNode(Constraints[ConstraintIndex]);
			Node.ConstraintIndex = ConstraintIndex;
		}
	}

	// sort nodes by tick dependencies
	auto EvaluationOrderPredicate = [](const FConstraintNode& LHS, const FConstraintNode& RHS)
	{
		const TArray<FTickPrerequisite>& RHSPrerex = RHS.ConstraintTick->GetPrerequisites();
		const FConstraintTickFunction* LHSTickFunction = LHS.ConstraintTick;
		const bool bIsLHSAPrerexOfRHS = RHSPrerex.ContainsByPredicate([LHSTickFunction](const FTickPrerequisite& Prerex)
		{
			return Prerex.PrerequisiteTickFunction == LHSTickFunction;
		});
		return bIsLHSAPrerexOfRHS;
	};
	Algo::Sort(Nodes, EvaluationOrderPredicate);

	// store node index after re-ordering
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		Nodes[NodeIndex].NodeIndex = NodeIndex;
	}

	// build edges	
	for (FConstraintNode& Node: Nodes)
	{
		const TArray<FTickPrerequisite>& Prerequisites = Node.ConstraintTick->GetPrerequisites();
		for (const FTickPrerequisite& Prerex: Prerequisites)
		{
			if (const FTickFunction* PrerexFunction = Prerex.Get())
			{
				FConstraintNode* PrerexNode = Nodes.FindByPredicate([PrerexFunction, Node](const FConstraintNode& OtherNode)
				{
					if (OtherNode.NodeIndex != Node.NodeIndex)
					{
						if (OtherNode.ConstraintTick == PrerexFunction)
						{
							return true;
						}
					}
					return false;
				});

				if (PrerexNode)
				{
					if (!PrerexNode->Parents.Contains(Node.NodeIndex))
					{
						Node.Parents.Add(PrerexNode->NodeIndex);
					}
					else
					{
						// we may have create a cycle
						ensure(false);
					}

					if (!Node.Children.Contains(PrerexNode->NodeIndex))
					{
						PrerexNode->Children.Add(Node.NodeIndex);
					}
					else
					{
						// we may have create a cycle
						ensure(false);
					}
				}
			}
		}
	}
	
	State = ReadyForEvaluation;

	Dump();
}

bool FConstraintsEvaluationGraph::IsPendingEvaluation() const
{
	return State == PendingEvaluation;
}

void FConstraintsEvaluationGraph::Evaluate(const TWeakObjectPtr<UTickableConstraint>& InConstraint)
{
	if (State == InvalidData)
	{
		Rebuild();
	}

	if (Nodes.IsEmpty())
	{
		return;
	}
	
	if (FConstraintNode* Node = FindNode(InConstraint))
	{
		Evaluate(Node);
	}
}

void FConstraintsEvaluationGraph::Evaluate(FConstraintNode* InNode)
{
	const TArray<TWeakObjectPtr<UTickableConstraint>>& Constraints = ConstraintsInWorld.Constraints;
	if (!Constraints.IsValidIndex(InNode->ConstraintIndex))
	{
		return;
	}

	const TWeakObjectPtr<UTickableConstraint> Constraint = Constraints[InNode->ConstraintIndex];
	if (!Constraint.IsValid())
	{
		return;
	}

	// evaluate
	if (Constraint->IsFullyActive() && InNode->ConstraintTick->IsTickFunctionRegistered() && InNode->ConstraintTick->IsTickFunctionEnabled())
	{
		Constraint->Evaluate();
	}
	InNode->bMarkedForEvaluation = false;

	// evaluate dependencies
	for (const uint32 ChildIndex: InNode->Children)
	{
		if (ensure(Nodes.IsValidIndex(ChildIndex)))
		{
			Evaluate(&Nodes[ChildIndex]);
		}
	}
}

void FConstraintsEvaluationGraph::InvalidateData()
{
	State = InvalidData;
	Nodes.Empty();
}

void FConstraintsEvaluationGraph::MarkForEvaluation(const TWeakObjectPtr<UTickableConstraint>& InConstraint)
{
	if (State == InvalidData)
	{
		Rebuild();
	}

	if (State == Flushing)
	{
		// do not mark this constraint for evaluation while flushing.
		// this can happen with UControlRig::OnControlModified being called while evaluating additive rigs
		return;
	}
	
	if (FConstraintNode* Node = FindNode(InConstraint))
	{
		auto GetConstraintLabel = [](const TWeakObjectPtr<UTickableConstraint>& InConstraint)
		{
#if WITH_EDITOR
			return InConstraint->GetFullLabel();
#else
			return InConstraint->GetName();
#endif		
		};

		if (ConstraintsEvaluationGraph::bDebugGraph)
		{
			UE_LOG(LogTemp, Warning, TEXT("Mark %s For Evaluation"), *GetConstraintLabel(InConstraint));
		}
		
		Node->bMarkedForEvaluation = true;
		
		if (State == ReadyForEvaluation)
		{
			State = PendingEvaluation;
		}
	}
}

void FConstraintsEvaluationGraph::Dump() const
{
	if (!ConstraintsEvaluationGraph::bDebugGraph)
	{
		return;
	}
	
	auto GetConstraintLabel = [](const TWeakObjectPtr<UTickableConstraint>& InConstraint)
	{
#if WITH_EDITOR
		return InConstraint->GetFullLabel();
#else
		return InConstraint->GetName();
#endif		
	};
	
	const TArray<TWeakObjectPtr<UTickableConstraint>>& Constraints = ConstraintsInWorld.Constraints;
	UE_LOG(LogTemp, Warning, TEXT("Nb Constraints = %d"), Constraints.Num());
	for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ConstraintIndex++)
	{
		if (Constraints[ConstraintIndex].IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("\tConstraint[%d] = %s"), ConstraintIndex, *GetConstraintLabel(Constraints[ConstraintIndex]));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Nb Nodes = %d"), Nodes.Num());
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		const FConstraintNode& Node = Nodes[NodeIndex];
		ensure(Constraints.IsValidIndex(Node.ConstraintIndex));
		ensure(Node.NodeIndex == NodeIndex);
		UE_LOG(LogTemp, Warning, TEXT("\tNode[%d] = %s [%d]"), NodeIndex, *GetConstraintLabel(Constraints[Node.ConstraintIndex]), Node.ConstraintIndex);
	}
}