// Copyright Epic Games, Inc. All Rights Reserved.

import { BranchSpec, ConflictedResolveNFile, RoboWorkspace } from '../common/perforce';
// TODO: Remove Circular Dependency on bot-interfaces
import { NodeBotInterface } from './bot-interfaces';
import { FailureKind } from './status-types';
import { BotConfig, BranchBase, ApprovalOptions, EdgeOptions, NodeOptions, IntegrationWindowPane } from './branchdefs';
import { BlockageNodeOpUrls } from './roboserver';

export type BranchArg = Branch | string
export function resolveBranchArg(branchArg: BranchArg, toUpperCase?: boolean): string {
	return toUpperCase ? 
		(branchArg as Branch).upperName || (branchArg as string).toUpperCase() :
		(branchArg as Branch).name || branchArg as string
}
export function branchesMatch(branchArg1: BranchArg | null | undefined, branchArg2: BranchArg | null | undefined) {
	return !branchArg1 || !branchArg2 || (typeof(branchArg1) === 'object' && typeof(branchArg2) === 'object') ?
		branchArg1 === branchArg2 : resolveBranchArg(branchArg1, true) === resolveBranchArg(branchArg2, true)
}

// Interface for returning from non-trivial node operations
export interface OperationResult { 
	success: boolean
	message: string // Goal: to provide meaningful error messaging to the end user
}

export interface StompedRevision {
	changelist: string
	author: string
	description: string
}

export interface StompVerificationFile {
	filetype: string // output from p4resolve for display purposes
	resolved: boolean // Determine if the file was successfully resolved and does not need to be stomped

	resolveResult?: ConflictedResolveNFile // output data from "p4 -ztag resolve -N"

	// Stomped Revisions can have a few returns
	// 1. stomped revisions were able to be calculated, and stompedRevisions is a populated array (can be empty)
	// 2. stomped revision calculations were skipped due to some criteria (such as task streams)
	// 3. we were not able to determine stomped revisions for some reason
	stompedRevisions?: StompedRevision[] | null // revisions being stomped from common ancestor between branches
	stompedRevisionsSkipped: boolean
	stompedRevisionsCalculationIssues: boolean
	
	targetFileName : string

	branchOrDeleteResolveRequired: boolean // Currently we don't have a good way to handle branch / delete merges. Fail those requests for now.
}

export interface StompVerification extends OperationResult {
	validRequest?: boolean
	nonBinaryFilesResolved?: boolean // Warning to the user that non-binary files are included in changelist, but passed the first resolve
	svFiles?: StompVerificationFile[] // Array of remaining files

	// Convinence booleans to alert user to problems in verification result
	branchOrDeleteResolveRequired?: boolean // Currently we don't have a good way to handle branch / delete merges. Fail those requests for now.
	remainingAllBinary?: boolean // Check to see if unresolved non-binary files are remaining -- we shouldn't stomp those!
}

export interface BranchGraphInterface {
	botname: string
	config: BotConfig

	branches: Branch[]

	getBranchesMonitoringSameStreamAs(branch: Branch): Branch[] | null

	// todo: make a better utility for this
	_computeReachableFrom(visited: Set<Branch>, flowKey: string, branch: Branch): Set<string>

	getBranch(name: string): Branch | undefined
	getBranchNames(): string[]
}

export type EditableBranch = BranchBase & {
	bot?: NodeBotInterface
	parent: BranchGraphInterface
	workspace: RoboWorkspace

	branchspec: Map<string, BranchSpec>
	upperName: string
	depot: string
	stream?: string
	config: NodeOptions
	reachable?: string[]
	forcedDownstream?: string[]
	enabled: boolean
	convertIntegratesToEdits: boolean
	visibility: string[] | string
	blockAssetTargets: Set<string>
	allowDeadend: boolean

	edgeProperties: Map<string, EdgeOptions>

	isMonitored: boolean // property
}

export type Branch = Readonly<EditableBranch>

export interface Target {
	branchName: string
	mergeMode: string
	otherBot?: string
}

export type MergeMode = 'safe' | 'normal' | 'null' | 'clobber' | 'skip'
export interface MergeAction {
	branch: Branch
	mergeMode: MergeMode
	furtherMerges: Target[]
	flags: Set<string>
	description?: string  // gets filled in for immediate targets in _mergeCl
}

export interface TargetInfo {
	errors?: string[]
	allDownstream?: Branch[]

	targets?: MergeAction[]

	owner?: string
	author: string

	targetWorkspaceForShelf?: string // Filled in during the reconsider in case of a createShelf nodeop request
	sendNoShelfEmail: boolean // Used for internal use shelves, such as stomp changes

	forceStompChanges: boolean
	additionalDescriptionText?: string
}

type UserRequest =
	'node-reconsider' | 'edge-reconsider'

export interface ChangeInfo extends TargetInfo {
	branch: Branch
	cl: number
	source_cl: number
	userRequest?: UserRequest
	authorTag?: string
	source: string
	description: string

	propagatingNullMerge: boolean
	forceCreateAShelf: boolean
	edgeServerToHostShelf?: {
		id: string
		address: string
	}
	targetWorkspaceOverride?: string
	overriddenCommand: string
	macros: string[]
}

export interface PendingChange {
	change: ChangeInfo
	action: MergeAction
	newCl: number
}

export interface Failure {
	kind: FailureKind		// short description of integration error or conflict
	description: string		// detailed description (can be very long - don't want to store this)
	summary?: string
}

export interface Blockage {
	action: MergeAction | null // If we have a syntax error, this can be null
	change: ChangeInfo
	failure: Failure
	owner: string
	ownerEmail: Promise<string | null>
	approval?: {
		settings: ApprovalOptions
		shelfCl: number
	}
	time: Date
}
export type NodeOpUrlGenerator = (blockage: Blockage | null) => BlockageNodeOpUrls | null


export interface ConflictingFile {
	name: string
	kind: ConflictKind
}
export type ConflictKind = 'merge' | 'branch' | 'delete' | 'unknown'

export interface AlreadyIntegrated {
	change: ChangeInfo
	action: MergeAction
}

export interface ForcedCl {
	nodeOrEdgeName: string
	forcedCl: number
	previousCl: number
	culprit: string
	reason: string
}

export type GateInfo = {
	cl: number
	link?: string
	timestamp?: number

	// optional overrides for integration window (takes precedence over any in config)
	integrationWindow?: IntegrationWindowPane[]
	invertIntegrationWindow?: boolean
}

export function gatesSame(lhs: GateInfo | null, rhs: GateInfo | null) {
	if (!lhs && !rhs) {
		return true
	}

	if (!lhs || !rhs) {
		return false
	}

	if (lhs.cl !== rhs.cl) {
		return false
	}

	// neither has windows? no need to check further
	if (!lhs.integrationWindow && !rhs.integrationWindow) {
		return true
	}

	if (!lhs.integrationWindow || !rhs.integrationWindow) {
		return false
	}

	if (lhs.integrationWindow.length !== rhs.integrationWindow.length) {
		return false
	}

	if (!lhs.invertIntegrationWindow !== !rhs.invertIntegrationWindow) {
		return false
	}

	for (let n = 0; n < lhs.integrationWindow.length; ++n) {
		const lhsWindow = lhs.integrationWindow[n]
		const rhsWindow = rhs.integrationWindow[n]
		const lhsDays = lhsWindow.daysOfTheWeek || []
		const rhsDays = rhsWindow.daysOfTheWeek || []
		if (lhsWindow.startHourUTC !== rhsWindow.startHourUTC ||
				lhsWindow.durationHours !== rhsWindow.durationHours ||
				lhsDays.length !== rhsDays.length ||
				!lhsDays.every((lhsDay, index) => lhsDay === rhsDays[index])) {
			return false
		}
	}

	return true
}

export type GateEventContext = {
	from: Branch
	to: Branch
	edgeLastCl: number
	pauseCIS: boolean
}

export type BeginIntegratingToGateEvent = {
	context: GateEventContext
	info: GateInfo

	// would like to provide this for logging, but I'm not storing it in a convenient place yet
	changesRemaining: number
}

export type EndIntegratingToGateEvent = {
	context: GateEventContext
	targetCl: number
}
