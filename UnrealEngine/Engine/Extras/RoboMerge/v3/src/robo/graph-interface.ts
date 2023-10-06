// Copyright Epic Games, Inc. All Rights Reserved.

import { BranchGraphInterface } from './branch-interfaces';
import { NodeBotInterface } from './bot-interfaces';
import { BotConfig, BranchGraphDefinition } from './branchdefs';

export interface GraphInterface {
	branchGraph: BranchGraphInterface
	filename: string

	ensureStopping(): boolean
	reinitFromBranchGraphsObject(config: BotConfig, branchGraphs: BranchGraphDefinition): Promise<void>
	handleRequestedIntegrationsForAllNodes(): Promise<[NodeBotInterface, Error] | null>
}
