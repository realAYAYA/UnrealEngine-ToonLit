// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundThreadLocalDebug.h"

#if UE_METASOUND_DEBUG_ENABLED

#include "MetasoundNodeInterface.h"
#include "Containers/UnrealString.h"

namespace Metasound
{
	namespace ThreadLocalDebug
	{
		class FDebugInfo
		{
		public:
			// Set the active node for the debug info.
			void SetActiveNode(const INode* InNode);

			// Return the active node.
			const INode* GetActiveNode() const;

			// Returns the class name and version string for the active node in the current thread.
			const TCHAR* GetActiveNodeClassNameAndVersion() const;

		private:

			const INode* ActiveNode = nullptr;
			FString NodeClassNameAndVersion;
		};

		namespace ThreadLocalDebugPrivate
		{
			static thread_local FDebugInfo DebugInfoOnThisThread;
		}

		void FDebugInfo::SetActiveNode(const INode* InNode)
		{
			ActiveNode = InNode;
			if (ActiveNode)
			{
				const FNodeClassMetadata& Metadata = ActiveNode->GetMetadata();

				NodeClassNameAndVersion = FString::Format(TEXT("{0} v{1}.{2}"), {Metadata.ClassName.GetFullName().ToString(), Metadata.MajorVersion, Metadata.MinorVersion});
			}
			else
			{
				NodeClassNameAndVersion = TEXT("[No Active Debug Node Set]");
			}
		}

		const INode* FDebugInfo::GetActiveNode() const
		{
			return ActiveNode;
		}

		const TCHAR* FDebugInfo::GetActiveNodeClassNameAndVersion() const
		{
			if (ActiveNode)
			{
				return *NodeClassNameAndVersion;
			}
			return TEXT("");
		}

		const TCHAR* GetActiveNodeClassNameAndVersionOnThisThread()
		{
			return ThreadLocalDebugPrivate::DebugInfoOnThisThread.GetActiveNodeClassNameAndVersion();
		}

		FDebugInfo* GetDebugInfoOnThisThread()
		{
			return &ThreadLocalDebugPrivate::DebugInfoOnThisThread;
		}

		FScopeDebugActiveNode::FScopeDebugActiveNode(FDebugInfo* InDebugInfo, const INode* InNode)
		: DebugInfo(InDebugInfo)
		{
			if (DebugInfo)
			{
				PriorNode = DebugInfo->GetActiveNode();
				DebugInfo->SetActiveNode(InNode);
			}
		}

		FScopeDebugActiveNode::~FScopeDebugActiveNode()
		{
			if (DebugInfo)
			{
				DebugInfo->SetActiveNode(PriorNode);
			}
		}
	}
}

#endif
