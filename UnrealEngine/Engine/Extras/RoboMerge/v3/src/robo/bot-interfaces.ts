// Copyright Epic Games, Inc. All Rights Reserved.

import { Branch, BranchArg, BranchGraphInterface, OperationResult, StompVerification } from "./branch-interfaces";
import { BlockagePauseInfoMinimal, BranchStatus } from "./status-types"
import { TickJournal } from "./tick-journal";


export interface Bot {
	start(): Promise<void>;
	tick(): Promise<boolean>;

	fullName: string;
	fullNameForLogging: string

	isRunning: boolean;
	isActive: boolean;

	lastCl: number

	tickCount: number

	tickJournal?: TickJournal;
}

export interface NodeBotIPC extends BotIPC {
	reconsider(instigator: string, changeCl: number, additionalArgs?: Partial<ReconsiderArgs>): void

	createShelf(owner: string, workspace: string, changeCl: number, targetBranchName: string): OperationResult

	verifyStomp(changeCl: number, targetBranchName: string): Promise<StompVerification>
	stompChanges(owner: string, changeCl: number, targetBranchName: string): Promise<OperationResult>
}



export interface BotIPC extends Bot, IPCControls {
}

export interface IPCControls {
	block(info: BlockagePauseInfoMinimal): void
	unblock(reason: string): void

	pause(message: string, owner: string): void
	unpause(requester: string): void

	reconsider(instigator: string, changeCl: number, additionalArgs?: Partial<ReconsiderArgs>): void

	acknowledge(acknowledger: string, changeCl: number) : OperationResult
	unacknowledge(changeCl: number) : OperationResult
	
	forceSetLastClWithContext(value: number, culprit: string, reason: string, unblock?: boolean): number
}

export interface EdgeBotInterface {
	get ipcControls(): IPCControls

	bypassGateWindow(sense: boolean): void
}

export interface NodeBotInterface extends NodeBotIPC {
	branch: Branch
	lastCl: number

	readonly branchGraph: BranchGraphInterface
	//readonly edges: Map<string, EdgeBot>
	hasEdge(branchName: BranchArg): boolean
	getEdgeIPCControls(branchName: BranchArg): IPCControls | null
	getImmediateEdge(branchName: BranchArg): EdgeBotInterface | null

	getNumConflicts(): number
	applyStatus(status: BranchStatus): void

}

// queued change vs reconsider:

//  reconsider is the name of user facing 'queue change' operation, but also the top level call to add to the queue,
//  used by stomp etc.

// slightly sneaky that since NodeBot.reconsider's additional args are optional, it fulfils both of these, i.e.
// implements the base bot reconsider function, and it also the 'queue change' function.

// ReconsiderArgs are the additional args used by stomp etc. I'm adding commandOverride to them, even thoguh that's a
// slightly different pattern (unless it breaks something)

export type ReconsiderArgs = {
	additionalFlags: string[]
	workspace: string
	targetBranchName: string
	description: string
	commandOverride: string
}

export type QueuedChange = Partial<ReconsiderArgs> & {
	cl: number
	who: string
	timestamp: number 
}
