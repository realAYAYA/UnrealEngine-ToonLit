// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNode.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertex.h"


namespace Metasound
{
	class METASOUNDSTANDARDNODES_API FOscilatorNodeBase : public FNode
	{
	protected:
		FOscilatorNodeBase(const FVertexName& InInstanceName, const FGuid& InInstanceID, const FNodeClassMetadata& InInfo, const TSharedRef<IOperatorFactory, ESPMode::ThreadSafe>& InFactory, float InDefaultFrequency, float InDefaultGlideTime, bool bInDefaultEnablement);

		float GetDefaultPhaseOffset() const
		{
			return DefaultPhaseOffset; 
		}

		float GetDefaultFrequency() const
		{
			return DefaultFrequency;
		}

		float GetDefaultGlideFactor() const
		{
			return DefaultGlideFactor;
		}

		bool GetDefaultEnablement() const
		{
			return bDefaultEnablement;
		}

		FOperatorFactorySharedRef GetDefaultOperatorFactory() const override
		{
			return Factory;
		}

		const FVertexInterface& GetVertexInterface() const override
		{
			return VertexInterface;
		}

		bool SetVertexInterface(const FVertexInterface& InInterface) override
		{
			if (IsVertexInterfaceSupported(InInterface))
			{
				VertexInterface = InInterface;
				return true;
			}
			return false;
		}

		bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override
		{
			return VertexInterface == InInterface;
		}

		TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> Factory;
		FVertexInterface VertexInterface;

		float DefaultPhaseOffset = 0.f;
		float DefaultFrequency = 440.f;
		float DefaultGlideFactor = 0.0f;
		bool bDefaultEnablement = true;
	};

	class METASOUNDSTANDARDNODES_API FSineOscilatorNode : public FOscilatorNodeBase
	{
	public:
		class FFactory;
		FSineOscilatorNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, float InDefaultFrequency, float InDefautlGlideTime, bool bInDefaultEnablement);
		FSineOscilatorNode(const FNodeInitData& InInitData);
	};
		
	class METASOUNDSTANDARDNODES_API FSawOscilatorNode : public FOscilatorNodeBase
	{
	public:
		class FFactory;
		FSawOscilatorNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, float InDefaultFrequency, float InDefautlGlideTime, bool bInDefaultEnablement);
		FSawOscilatorNode(const FNodeInitData& InInitData);
	};

	class METASOUNDSTANDARDNODES_API FTriangleOscilatorNode : public FOscilatorNodeBase
	{
	public:
		class FFactory;
		FTriangleOscilatorNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, float InDefaultFrequency, float InDefautlGlideTime, bool bInDefaultEnablement);
		FTriangleOscilatorNode(const FNodeInitData& InInitData);
	};

	class METASOUNDSTANDARDNODES_API FSquareOscilatorNode : public FOscilatorNodeBase
	{
	public:
		class FFactory;
		FSquareOscilatorNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, float InDefaultFrequency, float InDefautlGlideTime, bool bInDefaultEnablement);
		FSquareOscilatorNode(const FNodeInitData& InInitData);
	};

	class METASOUNDSTANDARDNODES_API FLfoNode : public FNodeFacade
	{
	public:		
		FLfoNode(const FNodeInitData& InInitData);
		virtual ~FLfoNode() = default;
	};
}
