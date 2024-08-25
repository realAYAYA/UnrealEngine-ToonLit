// Copyright Epic Games, Inc. All Rights Reserved.

import * as Sentry from '@sentry/node';
import * as p4util from '../common/p4util';
import { _nextTick } from '../common/helper';
import { ContextualLogger } from '../common/logger';
import { Mailer, MailParams, Recipients } from '../common/mailer';
import { Change, ConflictedResolveNFile, PerforceContext, RoboWorkspace, coercePerforceWorkspace } from '../common/perforce';
import { IPCControls, NodeBotInterface, QueuedChange, ReconsiderArgs } from './bot-interfaces';
import { ApprovalOptions, BotConfig, EdgeOptions, NodeOptions } from './branchdefs'
import { BlockagePauseInfo, BranchStatus } from './status-types';
import { AlreadyIntegrated, Blockage, Branch, BranchArg, BranchGraphInterface, ChangeInfo, EndIntegratingToGateEvent, Failure } from './branch-interfaces';
import { MergeAction, OperationResult, PendingChange, resolveBranchArg, StompedRevision, StompVerification, StompVerificationFile, UnlockVerification }  from './branch-interfaces';
import { Conflicts } from './conflicts';
import { EdgeBot, EdgeMergeResults } from './edgebot';
import { BotEventTriggers } from './events';
import { makeClLink, SlackMessages } from './notifications';
import { PerforceStatefulBot } from './perforce-stateful-bot';
import { BlockageNodeOpUrls, OperationUrlHelper } from './roboserver';
import { Context } from './settings';
import { SlackMessage, SlackMessageField, SlackMessageStyles } from './slack';
import { PauseState } from './state-interfaces';
import { newTickJournal, TickJournal } from './tick-journal';
import { computeTargets, parseDescriptionLines, processOtherBotTargets, getIntegrationOwner, getNodeBotFullName, getNodeBotFullNameForLogging } from './targets';
import { GraphAPI } from '../new/graph';

/**********************************
 * Bot monitoring a single stream
 **********************************/

const DEFAULT_NAG_SCHEDULE = [10, 20, 30, 60]
const DEFAULT_ACKNOWLEDGED_NAG_SCHEDULE = [30, 30, 60]
const DEFAULT_ACKNOWLEDGED_NAG_LEEWAY = 15
const NAG_EMAIL_MIN_TIME_MINUTES = 60
const NAG_EMAIL_MIN_TIME_DESCRIPTION = 'an hour'
const SYNTAX_ERROR_PAUSE_TIMEOUT_SECONDS = 10 * 60

const ALLOWED_STOMPABLE_NONBINARY = [
	/\.collection$/
]

function isStompableNonBinary(filename: string) {
	for (const regex of ALLOWED_STOMPABLE_NONBINARY) {
		if (regex.test(filename)) {
			return true
		}
	}
	return false
}


type InfoResult = { info?: ChangeInfo, errors: any[] }


export class NodeBot extends PerforceStatefulBot implements NodeBotInterface {
	readonly graphBotName: string
	readonly branch: Branch
	readonly branchGraph: BranchGraphInterface
	private readonly edges: Map<string, EdgeBot>

	private nodeBotLogger : ContextualLogger

	tickJournal?: TickJournal
	
	private headCL = -1

	private readonly mailer: Mailer
	private readonly slackMessages?: SlackMessages
	protected readonly p4: PerforceContext

	private readonly conflicts: Conflicts
	private edgesQueuedToUnblock: Set<string>
	private queuedChanges: QueuedChange[]

	private readonly externalRobomergeUrl: string

	/**
		Every 10 ticks (initially 30 seconds) of no changes, skip a tick
	 */
	private ticksSinceLastNewP4Commit = 0
	private skipTickCounter = 0


	// required by Bot interface
	public isActive = false

	constructor(
		branchDef: Branch,
		private readonly previewMode: boolean,
		mailer: Mailer,
		slackMessages: SlackMessages | undefined,
		externalUrl: string, 
		private readonly eventTriggers: BotEventTriggers,
		settings: Context,
		private readonly ubergraph: GraphAPI,
		private postIntegrate: () => Promise<void>
	) {
		// Node initial CL used when creating new (sub-)graph: should be able to set initial CL of source node and have that apply to edges
		super(settings, reason => this.unblock(reason), branchDef.config.initialCL)
		
		this.branch = branchDef
		this.branchGraph = branchDef.parent
		this.graphBotName = this.branchGraph.botname
		this.nodeBotLogger = new ContextualLogger(this.fullNameForLogging)
		this.slackMessages = slackMessages

		this.externalRobomergeUrl = externalUrl

		this.p4 = new PerforceContext(this.nodeBotLogger)

		this.mailer = mailer

		const settingsQueuedCls = this.settings.get('queuedCls')
		this.queuedChanges = Array.isArray(settingsQueuedCls) ? settingsQueuedCls : []

		const settingsQueuedUnblocks = this.settings.get('edgesQueuedToUnblock')
		this.edgesQueuedToUnblock = new Set(Array.isArray(settingsQueuedUnblocks) ? settingsQueuedUnblocks : [])
		
		this.eventTriggers = eventTriggers
		this.conflicts = new Conflicts(branchDef.upperName, eventTriggers, this.settings, externalUrl, this.branchGraph.config.reportToBuildHealth, this.nodeBotLogger)

		this.eventTriggers.onChangeParsed((info: ChangeInfo) => this.onGlobalChange(info))
		this.eventTriggers.onBlockage((blockage: Blockage, isNew: boolean) => 
			{ 
				if (isNew) { this.emailBlockageCulprit(blockage) } 
			}
		)

		this.initTickJournal()

		if (!this.branch.workspace) {
			throw new Error(`Branch ${this.fullName} has no valid workspace specified`)
		}

		// not looking for min CL of edges any more

		// Finally after setup, create the edges. (Edges may rely on NodeBot data for setup, so always do this last.)
		this.edges = this.createEdges()
	}

	private createEdges(): Map<string, EdgeBot> {
		const targetEdgeMap = new Map<string, EdgeBot>()

		for (const branchName of this.branch.flowsTo) {
			const targetBranch = this.branchGraph.getBranch(branchName)

			if (!targetBranch) {
				const err = new Error(`Branch "${branchName}" does not seem to exist, cannot set up EdgeBot`)
				this.nodeBotLogger.printException(err)
				throw err
			}

			const key = branchName.toUpperCase()
			if (!targetEdgeMap.has(key)) {

				// Edge initial CL:
				// in decreasing order of precendence: stored last cl, configured initial cl, source node last cl
				// source cl covers case: pause node, create new stream, new edge should start from CL source node paused on
				const edgeBot = new EdgeBot(this, targetBranch, this.lastCl,
					this.branch.edgeProperties.get(targetBranch.upperName) || {},
					this.eventTriggers,
					(...args) => { return this.createEdgeBlockageInfo(targetBranch, ...args) },
					this.settings.getSubContext(['edges', branchName]),
					this.conflicts.findUnresolvedConflictByBranch(branchName)
				)

				targetEdgeMap.set(key, edgeBot)
			}
		}

		return targetEdgeMap
	}

	private printEdges() {
		let mapString = ""
		this.edges.forEach((_edgeBot, targetBranch) => {
			mapString += `${targetBranch},`
			
		});
		return mapString.slice(0, -1)
	}

	/**
	 * Gets an edge directly assigned to this NodeBot that matches a branch.
	 * @param branch Branch to match against Node's edges
	 * @throws {Error} If the branch argument does not have an immediate edge representing it for this NodeBot,
	 *  this will throw an error.
	 */
	getImmediateEdge(branch: BranchArg): EdgeBot | null {
		let branchName = resolveBranchArg(branch, true)
		return this.edges.get(branchName) || null
	}

	getEdgeIPCControls(branch: BranchArg): IPCControls | null {
		const edge = this.getImmediateEdge(branch)

		return edge ? edge.getIPCControls() : null  
	}

	hasEdge(branch: BranchArg): boolean {
		return !!this.getImmediateEdge(branch)
	}

	private async getChanges(startingCl: number) {
		const path = this.branch.rootPath
		this._log_action(`checking for changes in ${path}`, 'silly')

		try {
			if (startingCl <= 0) {
				// new bot with no initial CL set: start with latest change
				const change = await this.p4.latestChange(path)
				if (change) {
					return [change]
				}
			}
			else if (this.lastCl === startingCl) {
				const change = await this.p4.latestChange(path);
				if (change.change === startingCl) {
					// this.nodeBotLogger.info('all CLs match')
					return []
				}

				const result = await this.p4.changesBetween(path, startingCl + 1, change.change)
				return result
			}
			else {
				return await this.p4.changes(path, startingCl)
			}
		}
		catch (err) {
			this.nodeBotLogger.printException(err, `${this.fullName} Error while querying P4 for changes`)
		}

		return []
	}

	public initTickJournal() {
		this.tickJournal = newTickJournal()
	}

	async setBotToLatestCl(): Promise<void> {
		return PerforceStatefulBot.setBotToLatestClInBranch(this, this.branch)
	}

	/** @return true if full tick completed, only used for analytics */
	async tick() {
		if (this.previewMode) {
			return false
		}
		await Promise.all(Array.from(this.edges.values(), async edgeBot => edgeBot.tick()))

		// Pre-tick

		if (this.edgesQueuedToUnblock.size > 0) {
			for (const edgeName of this.edgesQueuedToUnblock) {
				const edge = this.getImmediateEdge(edgeName)
				if (edge) {
					edge.unblock('queued unblock')
				}
				else {
					// could have been removed
					this.nodeBotLogger.warn(`Couldn't find edge ${edgeName} to unblock`)
				}
			}
			this.edgesQueuedToUnblock.clear()
			this.persistQueuedChanges()
		}

		// Find the changelist # to start our p4 changes command at
		let minCl = this.lastCl
		// Gather the list of available edges for tick -- edges may change mid-tick but we will only allow this set to perform integrations
		let availableEdges: Map<string, EdgeBot> = new Map<string, EdgeBot>()

		this.conflicts.checkForResolvedConflicts(this.lastCl, this.edges)
		for (const edgeBot of this.edges.values()) {
			if (edgeBot.isAvailable) {
				availableEdges.set(edgeBot.targetBranch.upperName, edgeBot)
				if (edgeBot.lastCl > 0 && (minCl < 0 || edgeBot.lastCl < minCl)) {
					minCl = edgeBot.lastCl
				}
			}

			// reset integration timers - easier to do it here than on every integration code path
			edgeBot.resetIntegrationTimestamp()
		}
		// End Pre-tick

		// see if our flow is paused
		if (this.isManuallyPaused) {
			this.nodeBotLogger.silly('tick() - This bot is manually paused')

			await _nextTick()
			return false
		}

		if (this.conflicts.getConflicts().length > 0)
		{
			this.sendNagNotifications()
		}

		// see if our flow is blocked
		if (this.isBlocked) {
			this.nodeBotLogger.verbose('tick() - This bot is blocked')
			await _nextTick()
			return false
		}

		// temp extra debugging for large catch-ups
		if (this.lastCl - minCl > 10000) {
			for (const edgeBot of this.edges.values()) {
				this.nodeBotLogger.info(`${edgeBot.displayName} --- cl: ${edgeBot.lastCl}, available: ${edgeBot.isAvailable}`)
			}
		}

		if (this.skipTickCounter > 0) {
			--this.skipTickCounter
			return false
		}

		/**
		 * Values for stream that becomes idle:
		 *			after n more ticks	total minutes  skip ticks	minutes between ticks
		 *				4 			 		2			1 			1
		 *				2					3			2			1.5
		 *				3					4.5			3			2
		 *				4					6.5			4			2.5
		 *				5					9			5			3
		 *				6					12			6			3.5

	triangle: 4, 6, 9, 13, 18 ...  (((n+1)*n)/2+3)/2, e.g. 3:((4*3)/2+3)/2 = 4.5
	simplified: (n²/2 + n/2 + 3)/2 = (n² + n + 6)/4

			gets to 5 minutes between skips (skip 9) after 25 minutes
		 */
		// process the list of changes from p4 changes
		const changes = await this.getChanges(minCl)
		if (changes.length === 0) {
			if (++this.ticksSinceLastNewP4Commit > 3) {
				// max gap of 5 minutes
				this.skipTickCounter = Math.min(this.ticksSinceLastNewP4Commit - 3, 9)
			}
		}
		else {
			this.ticksSinceLastNewP4Commit = 0

			this.headCL = changes[0].change
			await this._processListOfChanges(availableEdges, changes)
		}
		return true
	}

	queueEdgeUnblock(targetUpperName: string) {
		this.edgesQueuedToUnblock.add(targetUpperName)
	}

	async processQueuedChange(fromQueue: QueuedChange) {
		if (this.previewMode) {
			this._log_action(`(PREVIEW MODE) - Skipping manually queued change ${fromQueue.cl} on ${this.fullName}, requested by ${fromQueue.who}`)
			return
		}
		let logMessage = `Processing manually queued change ${fromQueue.cl} on ${this.fullName}, requested by ${fromQueue.who}`
		if (fromQueue.workspace) {
			logMessage += `, in workspace ${fromQueue.workspace}`
		}
		if (fromQueue.timestamp) {
			logMessage += ` (delay: ${Math.round((Date.now() - fromQueue.timestamp) / 1000)})`
		}
	
		let specifiedTargetBranch : Branch | null = null
		if (fromQueue.targetBranchName) {
			specifiedTargetBranch = this._getBranch(fromQueue.targetBranchName)

			if (!specifiedTargetBranch) {
				if (this.slackMessages) {
					const dm: SlackMessage = {
						text: `Manually queued change ${fromQueue.cl} on ${this.fullName}, requested by ${fromQueue.who}, ` + 
								`specifies non-existant branch ${fromQueue.targetBranchName}. Aborting reconsider.`,
						style: SlackMessageStyles.DANGER,
						channel: '',
						mrkdwn: true
					}
					
					let emailAddress = await this.findEmail(fromQueue.who)
					this.slackMessages.postDM(emailAddress, fromQueue.cl, this.branch, dm)
				}
				else {
					// We somehow have a specified branch from a reconsider request, but it doesn't exist in the branch map. Report the error
					this.sendErrorEmail(
						new Recipients(fromQueue.who),
						`Invalid Reconsider Request -- Branch "${fromQueue.targetBranchName}" does not exist`,
						`Manually queued change ${fromQueue.cl} on ${this.fullName}, requested by ${fromQueue.who}, ` + 
							`specifies non-existant branch ${fromQueue.targetBranchName}. Aborting reconsider.`
					)
				}
				return
			}
			else if (specifiedTargetBranch === this.branch) {
				if (this.slackMessages) {
					const dm: SlackMessage = {
						text: `Manually queued change ${fromQueue.cl} on ${this.fullName}, requested by ${fromQueue.who}, ` +
								`specifies integration from branch ${fromQueue.targetBranchName} into itself. Aborting reconsider.`,
						style: SlackMessageStyles.DANGER,
						channel: '',
						mrkdwn: true
					}
					
					let emailAddress = await this.findEmail(fromQueue.who)
					this.slackMessages.postDM(emailAddress, fromQueue.cl, this.branch, dm)
				}
				else {
					// We somehow have targeted our own branch with a merge. Report the issue
					this.sendErrorEmail(
						new Recipients(fromQueue.who),
						`Invalid Reconsider Request -- Trying to merge "${fromQueue.targetBranchName}" into itself`,
						`Manually queued change ${fromQueue.cl} on ${this.fullName}, requested by ${fromQueue.who}, ` +
							`specifies integration from branch ${fromQueue.targetBranchName} into itself. Aborting reconsider.`
					)
				}
				// Persist the remaining queued changes and carry on
				return
			}

			logMessage += `, targeted specifically at ${specifiedTargetBranch.name}`
		}

		this._log_action(logMessage)

		// Get Change object from P4 for queued cl
		const changeResult = await this._getChange(fromQueue.cl)

		if (typeof changeResult === 'string') {

			if (this.slackMessages) {

				let reconsiderErrorMessage = `While processing manually queued change ${fromQueue.cl} on ${this.fullName}, `
				reconsiderErrorMessage += specifiedTargetBranch ? `targeted specifically at ${specifiedTargetBranch.name}` : ''
				reconsiderErrorMessage += `\n\nError while querying P4 for change ${fromQueue.cl}: ${changeResult}`

				const dm: SlackMessage = {
					text: reconsiderErrorMessage,
					style: SlackMessageStyles.DANGER,
					channel: '',
					mrkdwn: true
				}
				
				let emailAddress = await this.findEmail(fromQueue.who)
				this.slackMessages.postDM(emailAddress, fromQueue.cl, this.branch, dm)
			}

			// change is invalid

			// @todo distinguish between Perforce errors and change not existing in branch


			this.nodeBotLogger.error(`${this.fullName} - Error while querying P4 for change ${fromQueue.cl}: ${changeResult}`)
			// @todo email admin page user when users have to log in
			// any way to show error without blocking page?
			return
		}

		// prep the change
		const change = changeResult as Change
		change.isUserRequest = true

		const additionalFlags = fromQueue.additionalFlags || []
		// By default, we'll submit any changes as a result of processing the CL. In some cases, such as
		// the create a shelf feature, we actually want to hold off on submitting any changes.
		if (additionalFlags.indexOf('createashelf') !== -1) {
			change.forceCreateAShelf = true
		}
		if (additionalFlags.indexOf('sendnoshelfnotification') != -1) {
			change.sendNoShelfNotification = true
		}

		// Stomp Changes support
		if (additionalFlags.indexOf('stomp') !== -1) {
			change.forceStompChanges = true
		}
		change.additionalDescriptionText = fromQueue.description
		if (fromQueue.commandOverride) {
			change.commandOverride = fromQueue.commandOverride
		}
		// regular reconsiders, make sure edge target is included
		else if (specifiedTargetBranch && !change.forceCreateAShelf && !change.forceStompChanges) {
			// new behaviour: make target explicit
			const bg = this.branchGraph
			
			const botNameOrAlias = bg.config.aliases.length > 0 ? bg.config.aliases[0] : bg.botname
			change.commandOverride = `#robomerge[${botNameOrAlias}] ${specifiedTargetBranch.upperName}`
			change.accumulateCommandOverride = true
		}

		// process just this change
		await this._processAndMergeCl(new Map<string, EdgeBot>(this.edges), change, true, fromQueue.who, fromQueue.workspace, specifiedTargetBranch)
		return
	}

	get displayName() {
		return this.branch.name
	}
	get fullName() {
		return getNodeBotFullName(this.graphBotName, this.branch.name)
	}
	get fullNameForLogging() {
		return getNodeBotFullNameForLogging(this.graphBotName, this.branch.name)
	}
	protected get logger() {
		return this.nodeBotLogger
	}

	forceSetLastClWithContext(value: number, culprit: string, reason: string, _?: boolean) {
		const prevValue = this._forceSetLastCl_NoReset(value)

		if (prevValue !== value) {
			this.onForcedLastCl(this.displayName, value, prevValue, culprit, reason)
		}
		return prevValue
	}
	
	onForcedLastCl(nodeOrEdgeName: string, forcedCl: number, previousCl: number, culprit: string, reason: string) {
		this.conflicts.onForcedLastCl({nodeOrEdgeName, forcedCl, previousCl, culprit, reason})
	}

	persistQueuedChanges() {
		this.settings.set('queuedCls', this.queuedChanges);
		this.settings.set('edgesQueuedToUnblock', [...this.edgesQueuedToUnblock]);
	}

	reconsider(instigator: string, changenum: number, additionalArgs?: Partial<ReconsiderArgs>) {
		const targetBranchName = additionalArgs && additionalArgs.targetBranchName
		// manually queue
		let logMessage = `Queueing reconsider of CL ${changenum} on ${this.fullName}, requested by ${instigator}`
		if (targetBranchName) {
			logMessage += ', target: ' + targetBranchName
		}
		if (additionalArgs) {
			if (additionalArgs.additionalFlags) {
				logMessage += `, (${additionalArgs.additionalFlags.join(', ')})`
			}
			if (additionalArgs.commandOverride) {
				logMessage += ', with command ' + additionalArgs.commandOverride
			}
		}
		this.nodeBotLogger.info(logMessage);
		this.queuedChanges.push({cl: changenum, who: instigator, timestamp: Date.now(), ...(additionalArgs || {})});
		this.persistQueuedChanges();
	}

	popRequestedIntegration() {
		return this.queuedChanges.shift()
	}

	createShelf(owner: string, workspace: string, changeCl: number, targetBranchName: string): OperationResult {
		// Ensure this is a valid request
		// Parameter validation (should not be blank)
		if (!owner || !workspace || !changeCl || !targetBranchName) {
			return { success: false, message: "Missing parameters for createShelf node operation" }
		}

		const relevantEdge = this.getImmediateEdge(targetBranchName)

		if (!relevantEdge) {
			return { success: false, message: `${this.fullName}: Cannot find edge for branch "${targetBranchName}"` }
		}

		// Ensure we're paused on a non-manual pause blockage
		if (!relevantEdge.isBlocked) {
			return { success: false, message: "Edge needs to be blocked on a conflict to create a shelf" }
		}

		const blockageInfo = (relevantEdge.pauseState.blockagePauseInfo as BlockagePauseInfo)

		// We've ensured we're on a blockage now. Make sure the CL is still valid.
		if (blockageInfo.change !== changeCl) {
			return { success: false, message: `Requested change CL "${changeCl}" does not match blockage change CL "${blockageInfo.change}"` }
		}

		let conflict = this.conflicts.getConflictByChangeCl(changeCl, targetBranchName)

		if (!conflict) {
			return { success: false, message: `Unable to retrieve conflict for CL ${changeCl} merging to ${targetBranchName}. Unable to create a shelf, please contact Robomerge support.` }
		}
		if (conflict.kind === 'Exclusive check-out') {
			return { success: false, message: `Unable to stomp changes over exclusive checkout to files in ${targetBranchName}. ` +
				`Unable to create a shelf, please revert the locked files in ${targetBranchName} or contact Robomerge support.` }
		}
		
		// Redo the merge, this time specifying the workspace to place the shelf in
		this.reconsider(owner, changeCl, {additionalFlags: ['createashelf'], workspace, targetBranchName})

		return { success: true, message: `Requested CL ${changeCl} successfully sent to bot for reconsideration and shelf creation` }
	}

	/* 
	 * In order to verify a stomp request, we need to:
	 *  1. process the CL using the normal Robomerge process
	 *  2. Resolve any files that are safely resolvable
	 *  3. Report files that could not be safely resolved automatically
	 *  4. Return the report to the user.
	 *  5. If this is purely for verification (and not a precursor to performing the stomp), revert the leftover changelist
	 */
	async verifyStomp(changeCl: number, targetBranchName: string) : Promise<StompVerification> {
		this._log_action(`Verifying stomp request of CL ${changeCl} to ${targetBranchName}`, "verbose")
		if (!changeCl || !targetBranchName) {
			return { success: false, message: "Missing parameters for stomp node operation" }
		}

		const targetBranch = this._getBranch(targetBranchName)

		if (!targetBranch) {
			return { success: false, message: `Unable to retrieve branch infomation for "${targetBranchName}". Unable to stomp, please contact Robomerge support."` }
		}

		const edge = this.getImmediateEdge(targetBranch)

		if (!edge) {
			return { success: false, message: `This bot has no edge for specified branch "${targetBranch.name}" on ${this.fullName}`}
		}

		// Ensure we're paused on a non-manual pause blockage
		if (!edge.isBlocked) {
			return { success: false, message: "Branch needs to have conflict to stomp changes" }
		}
		
		// Get the pause info of the edge
		const blockageInfo = edge.pauseState.blockagePauseInfo!

		// We've ensured we're on a blockage now. Make sure the CL is still valid.
		if (blockageInfo.change !== changeCl) {
			return { success: false, message: `Requested change CL "${changeCl}" does not match blockage change CL "${blockageInfo.change}"` }
		}

		let conflict = this.conflicts.getConflictByChangeCl(changeCl, targetBranch)

		if (!conflict) {
			return { success: false, message: `Unable to retrieve conflict for CL ${changeCl} merging to ${targetBranch.name}. Unable to stomp, please contact Robomerge support.` }
		}
		if (conflict.kind === 'Exclusive check-out') {
			return { success: false, message: `Unable to stomp changes over exclusive checkout to files in ${targetBranch.name}. ` + 
				`Unable to stomp, please revert the locked files in ${targetBranch.name} or contact Robomerge support.` }
		}

		// Get Change object from P4 for blockage cl
		const blockageChangeRetrieval = await this._getChange(changeCl)

		if (typeof blockageChangeRetrieval === 'string') {
			return { 
				success: false, 
				message: `Unable to retrieve change infomation for given conflict. ${blockageChangeRetrieval}`
			}
		}

		const blockageChange = blockageChangeRetrieval as Change

		// Set a variety of flags on the change before processing
		blockageChange.isUserRequest = true
		blockageChange.forceCreateAShelf = true // We don't want to submit this
		blockageChange.sendNoShelfNotification = true // Don't send any shelf creation notifications for this

		// Process and attempt the integration
		const edgeMap = new Map<string, EdgeBot>([[edge.targetBranch.upperName, edge]])
		let integratedCl: Change | null = null // Set to possible undefines as we use this in the finally clause
		try {
			const processResult = await this._createChangeInfo(blockageChange, edgeMap, this.p4.username, targetBranch.workspace, targetBranch)
			if (!processResult.info) {
				return {
					success: false, 
					message: `Could not parse blockage change\nErrors: ${processResult.errors}`
				}
			}
			const mergeResult = await this._mergeClViaEdges(edgeMap, processResult.info, true)

			// If we don't have a change returned, we ran into an error processing the CL
			const changes = mergeResult.allChanges()
			if (changes.length !== 1) { // Encountered when we get errors back
				let message = `Got incorrect number of entries back from integrating CL ${changeCl} (length = ${changes.length}, should be 1)`
				if (mergeResult.errors.length > 0) {
					message = `${message}\nErrors: ${mergeResult.errors}`
				}
				return { 
					success: false, 
					message
				}
			}

			const changelist = changes[0]!
			if (changelist <= 0) {
				return { 
					success: false, 
					message: `Unable to get integration shelf for CL ${changeCl}`
				}
			}

			let p4ChangeResult
			try {
				p4ChangeResult = await this.p4.getChange( targetBranch.rootPath, changelist, 'shelved')
			}
			catch (err) {
				return { 
					success: false, 
					message: `Unable to get integration shelf for CL ${changeCl}: ${err}`
				}
			}

			integratedCl = p4ChangeResult as Change

			// Unshelve 
			if (!await this.p4.unshelve(targetBranch.workspace, integratedCl.change)) {
				return { 
					success: false, 
					message: `Unable to unshelve CL ${integratedCl.change}`
				}
			}

			// Get what is left to resolve out of the changelist
			const resolveResult = await this.p4.resolve(integratedCl.client, integratedCl.change, 'normal')

			if (!resolveResult.hasConflict()) {
				return { 
					success: false, 
					message: `No conflict found resolving CL ${changeCl}`
				}
			}
			if (!resolveResult.successfullyParsedConflicts) {
				return { 
					success: false, 
					message: `Encountered error enumerating conflicting files after integration attempt: ${resolveResult.getParsingError()}`
				}
			}


			/* 
			 * Now that we have the resolve result sitting in changelist, we need to analyze the results of the resolve.
			 * #1 -- Process each file in the changelist to determine if only binary files remain, and catalog which revisions on these files will be stomped
			 *       NOTE: Integrations to task streams do not show up in p4 filelog on source files, only target files. 
			 *             Because our calculateStompedRevisions() uses information from the "fromFile" field out of the resolve command, our code breaks when the target is a task stream.
			 *             Skip calculating stomped revisions in '//Tasks/' streams
			 * #2 -- Determine if any of the resolved files were non-binary to display warning to user that this will be a mixed merge,
			 * 			 which may have dangers if the resolve files were code and relied on binary file changes.
			 */

			const skipStompedRevisions = targetBranch.rootPath.startsWith('//Tasks/')
			
			// Start collecting data used by the requestor
			let svFiles : StompVerificationFile[] = []

			// Get information about the pending changelist as a whole
			const describeResult = await this.p4.describe(integratedCl.change)

			const remainingFiles = resolveResult.getConflicts()
			// We need the depot file for each conflict file to do comparisons
			for (const remainingFile of remainingFiles) {
				// Find target file 
				const targetFileInDepotZtag = await this.p4.where(integratedCl.client, remainingFile.clientFile)
				if (!targetFileInDepotZtag[0].depotFile) { 
					throw new Error(`Error retrieving depot path for the merge target of ${remainingFile.clientFile}`)
				}
				remainingFile.targetDepotFile = targetFileInDepotZtag[0].depotFile
			}

			let remainingAllBinary = true
			let nonBinaryFilesResolved = false

			const getRemainingFile = function (fromFile: string, remainingFiles: ConflictedResolveNFile[]) {
				for (const remainingFile of remainingFiles) {
					if (fromFile === remainingFile.targetDepotFile) {
						return remainingFile
					}
				}
				return null
			}
			
			// Process all the files in our pending changelist
			let stompedRevisionsCalculationIssues = false
			for (const file of describeResult.entries) {
				// Check if this file is still remaining in the changelist with a conflict
				const remainingFile = getRemainingFile(file.depotFile, remainingFiles)
				const resolved = !remainingFile
				const stompable = file.type.startsWith('binary') || isStompableNonBinary(file.depotFile)
				
				let stompedRevisions : StompedRevision[] | null = []
				let stompVerificationFile : StompVerificationFile = {
					filetype: file.type,
					stompedRevisionsSkipped: false,
					stompedRevisionsCalculationIssues: false,
					resolved,
					targetFileName: file.depotFile
				}

				if (!stompable) {
					if (resolved) {
						// If this is a non-binary file that has been resolved, warn user this is a mixed merge
						nonBinaryFilesResolved = true
					} else {
						// If this is a non-binary file that has been not been resolved, flag stomp request as non-valid
						remainingAllBinary = false
					}
				} 

				if (!resolved) {
					// Skip stomped revisions for task streams
					if (file.depotFile.startsWith('//Tasks/')) {
						stompVerificationFile.stompedRevisionsSkipped = true
						stompedRevisions = null
					}
					else if (!skipStompedRevisions) {
						const setDebugScope = (debugScope : Sentry.Scope) => {
							debugScope.setExtra('changeCl', changeCl)
							debugScope.setExtra('targetfile', file.depotFile)
							debugScope.setExtra('fromFile', remainingFile!.fromFile)
							debugScope.setExtra('startFromRev', remainingFile!.startFromRev.toString())
							debugScope.setExtra('endFromRev', remainingFile!.endFromRev.toString())
							debugScope.setExtra('remainingFileJson', JSON.stringify(remainingFile!))
							debugScope.setExtra('resolveResult', resolveResult!.getConflictsText())
							debugScope.setExtra('resolveResultParsingError?', resolveResult!.getParsingError())
						}

						// Attempt to find cause of NaN errors
						if (remainingFile!.startFromRev.toString().indexOf("NaN") !== -1 || remainingFile!.endFromRev.toString().indexOf("NaN") !== -1) {
							Sentry.withScope((scope) => {
								setDebugScope(scope)
								Sentry.captureMessage("Remaining file has NaN instead of a revision")
							})
						}

						// Add information to stomped files for return
						const stompedRevisionsResult = await this.calculateStompedRevisions(remainingFile!, file.depotFile) 

						// If we got a non-null result, populate the StompVerificationFile's data
						if (stompedRevisionsResult) {
							stompedRevisions = stompedRevisionsResult
						} 
						// Otherwise there was an issue calculating the revisions
						else {
							Sentry.withScope((scope) => {
								setDebugScope(scope)
								Sentry.captureMessage("Stompable file encountered stomped revision calculation issues")
							})	

							stompVerificationFile.stompedRevisionsCalculationIssues = true
							stompedRevisionsCalculationIssues = true
							stompedRevisions = null
						}
					}
					
					stompVerificationFile.resolveResult = remainingFile!
				}

				stompVerificationFile.stompedRevisions = stompedRevisions
				svFiles.push(stompVerificationFile)
			}

			let returnResult : StompVerification = {
				success: true,
				message: "TBD",
				nonBinaryFilesResolved,
				remainingAllBinary,
				svFiles
			}

			if (!remainingAllBinary) {
				returnResult.message= "Invalid Request: Not all remaining files are binary"
				returnResult.validRequest = false
			}
			else if (stompedRevisionsCalculationIssues) {
				returnResult.message= "Valid Request, but encountered issues calculating stomped revisions on some files"
				returnResult.validRequest = true
				
				if (this.tickJournal) {
					// Alert our analytics that we ran into this issue
					++this.tickJournal.stompedRevisionsCalculationIssues
				}
			}
			else {
				returnResult.message= "Valid Request"
				returnResult.validRequest = true
			}

			// Finally return the results
			return returnResult

			
		} catch (error) {
			return { 
				success: false, 
				message: `Encountered error processing ${changeCl}: ${error}`
			}
		} 
		finally {
			// Delete the CL, including any remaining shelved files
			this.nodeBotLogger.info(`${this.fullName} - Cleaning up after verifyStompRequest()`)
			if (integratedCl) {
				await edge.revertPendingCLWithShelf(integratedCl.client, integratedCl.change, 'Stomp Verify')
			}
		}
	}

	// TODO: Support branch/delete merges
	private async calculateStompedRevisions(resolveNResult: ConflictedResolveNFile, targetDepotFilePath: string) : Promise<StompedRevision[] | null> {
		// Get the file log of the start revision
		const sourceStartZtag = await this.p4.filelog(null, resolveNResult.fromFile, resolveNResult.startFromRev.toString(), resolveNResult.startFromRev.toString()) 

		// Get filelog from start revision to find integration information between target->source branch
		let targetEndRev : string | null = null

		// First find a direct integration history
		for (const key in sourceStartZtag[0]) {
			// We're looking for a "file#,#" entry that matches the target name
			if (key.startsWith('file') && sourceStartZtag[0][key] === targetDepotFilePath) {
				const filelogLabel = key.slice(4) // remove 'file' from key name, leaving us with the '#,#'
				targetEndRev = sourceStartZtag[0][`erev${filelogLabel}`]
				break
			}
		}

		// Ensure we got the common ancestor end revision
		if (!targetEndRev) {
			this.nodeBotLogger.warn(`CalculateStompedRevisions: Could not find integration revision from ${targetDepotFilePath} -> ${resolveNResult.fromFile}`)
			return null
		}

		// targetEndRev is a number prefaced with a pound sign (#)
		const targetEndRevNum = parseInt(targetEndRev.slice(1))

		// When we better handle ztag multiline output, we can use long output for descriptions
		// #have would be better but can fail if workspace doesn't have the file
		const targetHistory = await this.p4.filelog(null, targetDepotFilePath, (targetEndRevNum + 1).toString(), "#head", true)

		let stompedRevisions : StompedRevision[] = []
		// Go through the history, with a max of 100 revisions
		for (let count = 0; count < 100 && count < targetHistory.length; count++) {
			// Ensure our current count is a valid value (eventually our count will exceed the number of revisions to be stomped)
			if (!targetHistory[count][`rev${count}`]) {
				break
			}

			// If we have a revision with the current count, create a stomped revision record
			try {
				stompedRevisions.push({
					changelist: targetHistory[count][`change${count}`],
					author: targetHistory[count][`user${count}`],
					description: targetHistory[count][`desc${count}`],
				})
			}
			catch (err) {
				throw new Error(`Error enumerating stomped revisions in ${targetDepotFilePath}#${targetEndRevNum + 1},#head: ${err.toString()}`)
			}
		}

		return stompedRevisions
	}
	// First, re-verify the stomp request
	// If verified, we should have an integrated CL that we can now stomp.
	async stompChanges(owner: string, changeCl: number, targetBranchName: string): Promise<OperationResult> {
		// Ensure this is a valid request
		// Parameter validation (should not be blank)
		if (!owner || !changeCl || !targetBranchName) {
			return { success: false, message: "Missing parameters for stompChanges node operation" }
		}

		// Reverify, but don't revert on a successful verification!
		const verifyResult = await this.verifyStomp(changeCl, targetBranchName)

		if (!verifyResult.success) {
			return { success: false, message: `stompChanges Request Failed Due to Bad Verification: ${verifyResult.message}` }
		}

		if (!verifyResult.validRequest) {
			return { success: false, message: `stompChanges Request Cannot Be Completed: ${verifyResult.message}` }
		}

		// One last check to ensure this is an all binary stomp
		if (!verifyResult.remainingAllBinary) {
			return { success: false, message: `stompChanges Failed -- Verification had remaining text files (${verifyResult.message})` }
		}

		// Before we submit, add an #fyi for all the stomped revision authors in the changelist
		let fyiSet = new Set<string>([owner.toLowerCase()])
		for (const file of verifyResult.svFiles!) {
			if (file.stompedRevisions) {
				for (const rev of file.stompedRevisions) {
					fyiSet.add(rev.author.toLowerCase())
				}
			}
		}

		this.nodeBotLogger.info(`Stomp Request for cl ${changeCl} by ${owner} validated, submitting change through reconsider system`)
		this.reconsider(owner, changeCl, {
			additionalFlags: ['stomp'], targetBranchName, 
			description: `\nStomp Operation may have overwritten commits by these authors:\n#fyi ${Array.from(fyiSet).join(', ')}`
		})

		if (this.tickJournal) {
			// Add number of successful stomps
			++this.tickJournal.stompQueued
		}

		return { success: true, message: 'Queued stomp request' }

	}

	async verifyUnlock(changeCl: number, targetBranchName: string) : Promise<UnlockVerification> {
		this._log_action(`Verifying unlock request of CL ${changeCl} to ${targetBranchName}`, "verbose")
		if (!changeCl || !targetBranchName) {
			return { success: false, message: "Missing parameters for unlock operation" }
		}

		const targetBranch = this._getBranch(targetBranchName)

		if (!targetBranch) {
			return { success: false, message: `Unable to retrieve branch infomation for "${targetBranchName}". Unable to unlock, please contact Robomerge support."` }
		}

		const edge = this.getImmediateEdge(targetBranch)

		if (!edge) {
			return { success: false, message: `This bot has no edge for specified branch "${targetBranch.name}" on ${this.fullName}`}
		}

		// Ensure we're paused on a non-manual pause blockage
		if (!edge.isBlocked) {
			return { success: false, message: "Branch needs to have conflict to unlock files." }
		}

		// Get the pause info of the edge
		const blockageInfo = edge.pauseState.blockagePauseInfo!

		// We've ensured we're on a blockage now. Make sure the CL is still valid.
		if (blockageInfo.change !== changeCl) {
			return { success: false, message: `Requested change CL "${changeCl}" does not match blockage change CL "${blockageInfo.change}"` }
		}

		let conflict = this.conflicts.getConflictByChangeCl(changeCl, targetBranch)

		if (!conflict) {
			return { success: false, message: `Unable to retrieve conflict for CL ${changeCl} merging to ${targetBranch.name}. Unable to unlock, please contact Robomerge support.` }
		}
		if (conflict.kind !== 'Exclusive check-out') {
			return { success: false, message: `Unlock only usable when edge is blocked by exclusive check-out ${targetBranch.name}. ` + 
				`If you thinks this is an error, contact Robomerge support.` }
		}

		return { success: true, validRequest: true, message: "Unlock verified", lockedFiles: blockageInfo.additionalInfo.exclusiveFiles}
	}

	// First, re-verify the unlock request
	// If verified, we should have a list of files to unlock
	async unlockChanges(owner: string, changeCl: number, targetBranchName: string): Promise<OperationResult> {
		// Ensure this is a valid request
		// Parameter validation (should not be blank)
		if (!owner || !changeCl || !targetBranchName) {
			return { success: false, message: "Missing parameters for unlockChanges node operation" }
		}

		const verifyResult = await this.verifyUnlock(changeCl, targetBranchName)

		if (!verifyResult.success) {
			return { success: false, message: `unlockChanges Request Failed Due to Bad Verification: ${verifyResult.message}` }
		}

		if (!verifyResult.validRequest) {
			return { success: false, message: `unlockChanges Request Cannot Be Completed: ${verifyResult.message}` }
		}

		this.nodeBotLogger.info(`Unlock Request for cl ${changeCl} by ${owner} validated, unlocking files`)

		let filesByClient = new Map<string,string[]>()
		let filesByUser = new Map<string,string[]>()
		for (const lockedFile of verifyResult.lockedFiles!) {
			filesByClient.set(lockedFile.client, [...(filesByClient.get(lockedFile.client) || []), lockedFile.depotPath])
			filesByUser.set(lockedFile.user, [...(filesByUser.get(lockedFile.user) || []), lockedFile.depotPath])
		}

		filesByClient.forEach((files,client) => this.p4.revertFiles(files,client))

		if (this.slackMessages) {
			
			filesByUser.forEach(async (files, author) => {

				let userToNotify = `@${author}`
				const emailAddress = await this.findEmail(author)
				if (emailAddress)
				{
					const user = await this.slackMessages!.getSlackUser(emailAddress)
					if (user) {
						userToNotify = `<@${user}>`
					}
				}
				const unlockMessage: SlackMessage = {
					text: `${userToNotify} - ${owner} has unlocked the following files:\n${files.join('\n')}`,
					style: SlackMessageStyles.DANGER,
					channel: this.branchGraph.config.slackChannel,
					mrkdwn: true
				}

				this.slackMessages!.postReply(changeCl, targetBranchName, unlockMessage)
			})
		}

		if (this.tickJournal) {
			// Add number of successful unlocks
			++this.tickJournal.unlockQueued
		}

		const targetBranch = this._getBranch(targetBranchName)

		if (!targetBranch) {
			return { success: false, message: `Unable to retrieve branch infomation for "${targetBranchName}". Unable to unlock, please contact Robomerge support."` }
		}

		const edge = this.getImmediateEdge(targetBranch)

		if (!edge) {
			return { success: false, message: `This bot has no edge for specified branch "${targetBranch.name}" on ${this.fullName}`}
		}

		edge.unblock("files unlocked")

		return { success: true, message: 'Queued unlock request' }
	}

	createEdgeBlockageInfo(edgeBranch: Branch, failure: Failure, pending: PendingChange): BlockagePauseInfo {
		// Check source node if it is currently tracking this conflict
		let existingConflict = this.getConflictByBranchAndSourceCl(edgeBranch, pending.change.source_cl)
		let pauseInfo = NodeBot._makeBlockageInfo(
			failure.kind,
			failure.description,
			<PendingChange> pending,
			existingConflict ? existingConflict.time : new Date,
			edgeBranch,
			failure.additionalInfo
		)

		// Check to see if this conflict has been acknowledged
		if (existingConflict && existingConflict.acknowledger) {
			pauseInfo.acknowledger = existingConflict.acknowledger
			pauseInfo.acknowledgedAt = existingConflict.acknowledgedAt
		}

		return pauseInfo
	}

	private static _makeBlockageInfo(errstr: string, errlong: string, arg: ChangeInfo | PendingChange, startedAt: Date, targetBranch?: Branch, additionalInfo?: any): BlockagePauseInfo {
		const MAX_LEN = 700
		if (errlong.length > MAX_LEN) {
			errlong = errlong.substring(0,MAX_LEN) + "..."
		}

		const maybePendingChange = arg as PendingChange
		const info = maybePendingChange.change || (arg as ChangeInfo)
		const owner = (maybePendingChange.action ? getIntegrationOwner(maybePendingChange) : info.owner) || info.author

		const pauseInfo: BlockagePauseInfo = {
			startedAt, owner, type: 'branch-stop',
			change: info.cl,
			sourceCl: info.source_cl,
			source: info.source,
			message: `${errstr}:\n${errlong}`,
			additionalInfo
		}

		if (maybePendingChange.change) {
			pauseInfo.targetBranchName = maybePendingChange.action.branch.name

			if (maybePendingChange.action.branch.stream) {
				pauseInfo.targetStream = maybePendingChange.action.branch.stream
			}
		}
		else if (targetBranch) {
			pauseInfo.targetBranchName = targetBranch.name

			if (targetBranch.stream) {
				pauseInfo.targetStream = targetBranch.stream
			}
		}

		if (info.author) {
			pauseInfo.author = info.author;
		}
		return pauseInfo;
	}

	acknowledgeConflict(acknowledger: string, changeCl: number, pauseState: PauseState, blockageInfo: BlockagePauseInfo) : OperationResult {
		// Attempt to acknowledge conflict requested
		if (this.conflicts.acknowledgeConflict(acknowledger, changeCl)) {
			const message = `${acknowledger} acknowledges blockage specified by CL ${changeCl} on ${this.fullName}`
			this.nodeBotLogger.info(message)

			// If our current pause state corresponds to this conflict, update the pause info for the website
			if (blockageInfo.change === changeCl) {
				pauseState.acknowledge(acknowledger)
			}

			return { success: true, message }
		} 
		else {
			const message = `Cannot acknowledge for ${acknowledger} -- No conflict found on ${this.fullName} with CL ${changeCl}.`
			this.nodeBotLogger.error(message)
			return { success: false, message }
		}
	}
	unacknowledgeConflict(changeCl: number, pauseState: PauseState, blockageInfo: BlockagePauseInfo) : OperationResult {
		if (this.conflicts.unacknowledgeConflict(changeCl)) {
			const message = `The blockage specified by CL ${changeCl} in ${this.fullName} has been unacknowledged.`
			this.nodeBotLogger.info(message)

			// If our current pause state corresponds to this conflict, update the pause info for the website
			if (blockageInfo.change === changeCl) {
				pauseState.unacknowledge()
			}

			return { success: true, message }
		} 
		else {
			const message = `Cannot unacknowledge -- No conflict found on ${this.fullName} with CL ${changeCl}.`
			this.nodeBotLogger.error(message)
			return { success: false, message }
		}
	}

	/* Used by robo.ts */
	pauseAllEdges(reason: string, user: string): number {
		for (const [, edge] of this.edges) {
			edge.pause(reason, user)
		}
		return this.edges.size
	}

	applyStatus(status: BranchStatus) {
		status.display_name = this.displayName
		status.last_cl = this.lastCl

		status.is_active = this.isActive
		status.is_available = this.isAvailable
		status.is_blocked = this.isBlocked
		status.is_paused = this.isManuallyPaused
		
		status.queue = this.queuedChanges
		if (this.headCL !== -1) {
			status.headCL = this.headCL
		}

		if (status.is_active)
		{
			status.status_msg = this.lastAction
			status.status_since = this.actionStart.toISOString()
		}

		if (!status.is_available) {
			this.pauseState.applyStatus(status)
		}

		if (this.lastBlockage > 0) {
			status.lastBlockage = this.lastBlockage
		}

		status.conflicts = [] as any[]
		this.conflicts.applyStatus(status.conflicts)

		status.tick_count = this.tickCount

		const edgeObj: { [key: string]: any } = {}
		for (const [branchName, edgeBot] of this.edges) {
			const edgeStatus = edgeBot.createStatus()
			if (this.headCL !== -1) {
				edgeStatus.headCL = this.headCL
			}
			edgeObj[branchName] = edgeStatus
		}
		status.edges = edgeObj
	}

	getNumConflicts() {
		return this.conflicts.getUnresolvedConflicts().length
	}

	getConflictByBranchAndSourceCl(branch: Branch, sourceCl: number) {
		return this.conflicts.find(branch, sourceCl)
	}

	getBotUrl() {
		return `${this.externalRobomergeUrl}#${this.graphBotName}`
	}

	disallowSkipForBlockage(blockage: Blockage): boolean {
		if (blockage.action && this.hasEdge(blockage.action.branch)) {
			const targetEdge = this.getImmediateEdge(blockage.action.branch)

			if (targetEdge) {
				return targetEdge.disallowSkip
			}
		}

		// Throw Error? We shouldn't get here from getBlockageUrls...
		return false
	}

	getBlockageUrls(blockage: Blockage, skipReason?: string) {
		const externalUrl = this.externalRobomergeUrl
		const botname = this.graphBotName
		const branchname = this.branch.name
		const cl = String(blockage.change.cl)
		const disallowSkip = this.disallowSkipForBlockage(blockage)

		// Call static method so we can centralize the logic (see Notifications.sendTestMessage())
		return NodeBot.getBlockageUrls(blockage, externalUrl, botname, branchname, cl, disallowSkip, skipReason)
	}

	static getBlockageUrls(blockage: Blockage, externalUrl: string, botname: string, branchname: string, cl: string, disallowSkip: boolean, skipReason?: string) {
		let urls : BlockageNodeOpUrls = {
			acknowledgeUrl: OperationUrlHelper.createAcknowledgeUrl(
				externalUrl,
				botname,
				branchname,
				cl,
				blockage.action ? blockage.action.branch.name : undefined
			)
		}
		
		if (blockage.action) {
			// Do not generate create shelf or stomp urls for exclusive checkouts
			if (blockage.failure.kind === 'Merge conflict') {
				urls.createShelfUrl = OperationUrlHelper.createCreateShelfUrl(
					externalUrl,
					botname,
					branchname,
					cl,
					blockage.action.branch.name,
					blockage.action.branch.stream
				)

				urls.stompUrl = OperationUrlHelper.createStompUrl(
					externalUrl,
					botname,
					branchname,
					cl,
					blockage.action.branch.name
				)
			}

			if (blockage.failure.kind === 'Exclusive check-out') {
				urls.unlockUrl = OperationUrlHelper.createUnlockUrl(
					externalUrl,
					botname,
					branchname,
					cl,
					blockage.action.branch.name
				)
			}

			// Do not generate skip urls if this branch is configure to disallow it
			if (!disallowSkip) {
				urls.skipUrl = OperationUrlHelper.createSkipUrl(
					externalUrl,
					botname,
					branchname,
					cl,
					blockage.action.branch.name,
					skipReason
				)
			}
		}

		return urls
	}

	private onGlobalChange(info: ChangeInfo) {
		if (info.branch === this.branch) {
			return; // ignore our own changes
		}

		this.conflicts.onGlobalChange(info)

		// check for auto-unpause after a resolve
		if (this.isBlocked) {
			const blockageInfo = this.pauseState.blockagePauseInfo!;

			const branchName = blockageInfo.targetBranchName

			// see if this change matches our stop CL
			if (branchName && branchName.toUpperCase() === info.branch.upperName && blockageInfo.sourceCl === info.source_cl)
			{
				// Unblock
				this.unblock(`finding CL ${info.source_cl} merged to ${info.branch.name} in CL ${info.cl}`);
			}
		}
	}

	onAlreadyIntegrated(event: AlreadyIntegrated) {
		this.conflicts.onAlreadyIntegrated(event)

		if (event.change.userRequest && this.slackMessages) {
			const message = `Change ${event.change.cl} was not necessary in ${event.action.branch.name}`
			let dm: SlackMessage = {
				text: message,
				style: SlackMessageStyles.WARNING,
				channel: '',
				mrkdwn: true
			}
			
			this.slackMessages.postDM(this.findEmail(event.change.owner!), event.change.cl, this.branch, dm)			
		}
	}

	onEndIntegratingToGate(event: EndIntegratingToGateEvent) {
		if (event.context.from === this.branch) {
			this.nodeBotLogger.info(`Gate closed info:
	to: ${event.context.to.upperName}
	edgeLastCl: ${event.context.edgeLastCl}
	targetCl: ${event.targetCl}
`)
		}
	}

	findEmail = (user: string) => {
		this._log_action(`Getting email for user ${user}`);
		return this.p4.getEmail(user);
	}

	// Listener method to email a blockage owner when the branch encounters said blockage,
	// depending on NodeBot configuration
	private emailBlockageCulprit(blockage: Blockage) {
		// Get branch referenced by blockage object
		const conflictBranch : Branch = blockage.change.branch

		// Only email for our branch, if the branch is configured to receive emails
		if (this.branch.name !== conflictBranch.name) {
			return
		}
		if (!conflictBranch.emailOnBlockage) {
			this.nodeBotLogger.verbose(`Skipping sending email for ${blockage.failure.kind} blockage in "${this.fullName}": CL "${blockage.change.cl}" due to branch configuration.`)
			return
		}
		
		this.nodeBotLogger.info(`Sending email for ${blockage.failure.kind} blockage in "${this.fullName}": CL "${blockage.change.cl}" to ${blockage.owner}.`)

		this._sendBlockageEmail(blockage)
	}

	async notifyShelfRequester(pendingChange: PendingChange) {
		// Get branch referenced by blockage object
		const shelfBranch : Branch = pendingChange.change.branch

		// Only notify for our branch, if the branch is configured to receive notifications
		if (this.branch.name !== shelfBranch.name) {
			return
		}

		const owner = pendingChange.change.owner || pendingChange.change.author
		// Should never happen
		if (!owner) {
			this.nodeBotLogger.warn(`Unable to send shelf creation notification for source CL ${pendingChange.change.source_cl} in branch ${shelfBranch.name} because there was no owner.`)
			return
		}

		this.nodeBotLogger.info(`Sending reconsider shelf creation (shelf CL ${pendingChange.newCl}) notification to ${pendingChange.change.owner} for source CL ${pendingChange.change.source_cl}`)

		if (this.slackMessages) {
			let dm: SlackMessage = {
				text: `Robomerge has created shelf CL ${pendingChange.newCl} in workspace ${pendingChange.change.targetWorkspaceForShelf} ` +
				`for merging source CL ${pendingChange.change.source_cl} (${this.fullName}).\n` +
				`\`\`\`${pendingChange.change.description}\`\`\``,
				channel: '',
				mrkdwn: true
			}
			
			let emailAddress = await this.findEmail(owner)
			this.slackMessages.postDM(emailAddress, pendingChange.newCl, this.branch, dm)
		}
		else
	 	{
			this._sendGenericEmail(
				new Recipients(owner),
				`Robomerge created Shelf CL ${pendingChange.newCl} in ${pendingChange.change.targetWorkspaceForShelf}`,
				`Robomerge has created shelf CL ${pendingChange.newCl} in workspace ${pendingChange.change.targetWorkspaceForShelf} ` +
					`for merging source CL ${pendingChange.change.source_cl} (${this.fullName}).`,
				`${pendingChange.change.source_cl}:\n${pendingChange.change.description}`)
		}
	}

	private async _processListOfChanges(availableEdges: Map<string, EdgeBot>, allChanges: Change[]) {
		// list of changes except reconsiders
		const changes: Change[] = []

		// process manual changes even if paused or above the lastGoodCL
		for (const change of allChanges) {
			if (change.isUserRequest) {
				this.nodeBotLogger.debug(`Processing manual change CL ${change.change}`)
				await this._processAndMergeCl(availableEdges, change, true)
			}
			else {
				changes.push(change)
			}
		}

		if (!this.isAvailable) {
			this.nodeBotLogger.verbose(`Not processing changes due to unavailability.`)
			this.nodeBotLogger.debug(`Blocked? ${this.pauseState.isBlocked()}: ${this.pauseState.blockagePauseInfo}`)
			this.nodeBotLogger.debug(`Paused? ${this.pauseState.isManuallyPaused()}: ${this.pauseState.manualPauseInfo}`)
			this.nodeBotLogger.debug(`Availablity? ${this.pauseState.isAvailable()}: ${this.pauseState.availableInfo}`)
			return
		}

		// make sure the list is sorted in ascending order
		changes.sort((a, b) => a.change - b.change)

		const startTime = Date.now()

		for (let changeIndex = 0; changeIndex < changes.length; ++changeIndex) {
			const change = changes[changeIndex]

			// If the user of the CL is robomerge, lookup the actual author from the tag
			if (change.user === "robomerge") {
				let bAuthorFound = false
				for (const line of change.desc.split('\n')) {
					const authorMatch = line.match("#ROBOMERGE-AUTHOR: (.*)")
					if (authorMatch) {
						change.user = authorMatch[1]
						bAuthorFound = true
						break
					}
				}
				if (!bAuthorFound) {
					this.nodeBotLogger.warn(`Processing change ${change.change} by user robomerge. Now: ${Date.now()} Change time: ${change.time}`)
				}
			}
			// If this is a change submitted via swarm on behalf of someone don't process the
			// change for a few minutes to give the change owner command time to be processed
			else if (change.client.startsWith("swarm-")) {
				// TODO: Use the Swarm API to lookup the actual author
				if ((Date.now() - ((change.time || 0) * 1000)) < 120000) {
					this.nodeBotLogger.info(`Delaying processing of ${change.change} to give swarm time to change owner.  Now: ${Date.now()} Change time: ${change.time}`)
					return
				}
			}

			const changeResult = await this._processAndMergeCl(availableEdges, change, false)

			// Exit immediately on syntax errors
			if (this.isBlocked) {
				return
			}

			if (change.change > this.lastCl) {
				this.lastCl = change.change
			}

			// Every edge that was available at the start of the tick that is still available had the chance to receive this change, even if they weren't a target
			// Count this as processed to keep them up-to-date
			for (const edgeBot of availableEdges.values()) {
				edgeBot.onNodeProcessedChange(changes, changeIndex, changeResult)
			}

			const duration = Math.round((Date.now() - startTime) / 1000);
			if (this.branchGraph.config.checkIntervalSecs < duration) {
				this.nodeBotLogger.info(`${this.fullName} yielding after ${duration}s`)
				return
			}

			// If we've been paused (or blocked?) while the previous change was being processed, stop now
			// seems either very rare on impossible in practice
			if (!this.isAvailable) {
				this.nodeBotLogger.info(`Node is no longer available to process further changes. Revisions processed: ${changeIndex}`)
				return
			}
		}
	}

	/**
	 * Emulates previous versions of "_processCl()" which would automatically create the ChangeInfo and merge.
	 */
	private async _processAndMergeCl(
		availableEdges: Map<string, EdgeBot>, change: Change, ignoreEdgePauseState: boolean,
		optOwnerOverride?: string, optWorkspaceOverride?: RoboWorkspace, optTargetBranch?: Branch | null
	) : Promise<EdgeMergeResults> {
		const result = await this._createChangeInfo(change, availableEdges, optOwnerOverride, optWorkspaceOverride, optTargetBranch)
		if (!result.info) {
			const emResult = new EdgeMergeResults()
			emResult.errors = result.errors
			return emResult
		}

		// this will deal with #manual changes with a single target and will also manage to work for multiple
		// targets as long as all the target workspaces are on the same edge, but it will still crash if you
		// have multiple targets with workspaces on different edgeservers
		const numTargets = (result.info.targets || []).length;
		let isManualChange = numTargets > 0 ? result.info.targets![0].flags.has('manual') : false;
		if (change.forceCreateAShelf || (change.isUserRequest && !change.forceStompChanges) || isManualChange) {
			if (!optWorkspaceOverride && numTargets == 1) {
				result.info.targetWorkspaceForShelf = await p4util.chooseBestWorkspaceForUser(this.p4, result.info.owner||result.info.author, result.info.targets![0].branch.stream)
				optWorkspaceOverride = result.info.targetWorkspaceForShelf
				this.nodeBotLogger.info(`Chose workspace ${optWorkspaceOverride}`)
			}
			if (optWorkspaceOverride) {
				// see if we need to talk to an edge server to create the shelf
				const edgeServer = await this.p4.getWorkspaceEdgeServer(
					coercePerforceWorkspace(optWorkspaceOverride)!.name)
				if (edgeServer) {
					result.info.edgeServerToHostShelf = edgeServer
				}
			}
		}

		return await this._mergeClViaEdges(availableEdges, result.info, ignoreEdgePauseState)
	}

	private async _createChangeInfo(
		change: Change,
		availableEdges: Map<string, EdgeBot>,
		optOwnerOverride?: string,
		optWorkspaceOverride?: RoboWorkspace,
		optTargetBranch?: Branch | null) : Promise<InfoResult> {
		this._log_action(`Analyzing CL ${change.change}`, "verbose")

		// parse the change
		const info : ChangeInfo = this.parseChange(change, optTargetBranch)

		if (optOwnerOverride) {
			info.owner = optOwnerOverride
		}

		if (optWorkspaceOverride) {
			info.targetWorkspaceForShelf = typeof optWorkspaceOverride === "string" ? optWorkspaceOverride : optWorkspaceOverride.name
		}

		if (info.targets && availableEdges.size > 0) {
			// check if we need to block a change containing assets
			const blockAssetEdges: [EdgeBot, MergeAction][] = []
			for (const action of info.targets) {
				if (action.mergeMode === 'normal' && !action.flags.has('disregardassetblock')) {

					for (const blockTargetName of this.branch.blockAssetTargets) {
						if (this._getBranch(blockTargetName) === action.branch) {

							const edge = this.getImmediateEdge(action.branch)
							if (!edge) {
								throw new Error('Missing edge! ' + action.branch.name)
							}

							if (info.userRequest || info.cl > edge.lastCl) {
								blockAssetEdges.push([edge, action])
							}
							break
						}
					}
				}
			}

			if (blockAssetEdges.length > 0) {
				const describeResult = await this.p4.describe(change.change)

				let changeContainsAssets = false
				for (const entry of describeResult!.entries) {
					const fileExtIndex = entry.depotFile.lastIndexOf('.')
					if (fileExtIndex !== -1) {
						const fileExt = entry.depotFile.substring(fileExtIndex + 1)
						if (fileExt === 'uasset' || fileExt === 'umap') {
							changeContainsAssets = true
							break
						}
					}
				}

				if (changeContainsAssets) {
					for (const [edge, action] of blockAssetEdges) {
						const failure: Failure = {kind: 'Disallowed files', description: 'Changelist contains assets which are not allowed on this route'}
						const pending: PendingChange = {change: info, action, newCl: -1}

						if (!change.isUserRequest) {
							await edge.block(this.createEdgeBlockageInfo(action.branch, failure, pending))
						}

						// ... then let node do facilitate notification handling
						await this.handleMergeFailure(failure, pending)

						// remove action from targets
						info.targets.splice(info.targets.indexOf(action), 1)
					}
				}
			}
		}

		// stop on errors and e-mail interested parties
		if (info.errors) {
			this.nodeBotLogger.debug(`Errors during createChangeInfo():\n${info.errors.join('\n')}`)
			this.handleSyntaxError(change.desc!, info)
			return { errors: info.errors }
		}

		// Only notify each change once, even though we reparse changes due to edges handling the same change at different times
		if (info.cl > this.lastCl) {
			this.conflicts.onChangeParsed(info, optTargetBranch)
		}
		
		// If there are any targets, start a merge to them
		if (!info.targets) {
			const nothingToDoMesasage = `Nothing to do for ${info.cl}`
			this.nodeBotLogger.silly(nothingToDoMesasage)
			return { errors:  [ nothingToDoMesasage ] }
		}

		return { info, errors: [] }
	}

	private createEmailLinkButton(buttonText: string, buttonLink: string, buttonDescription: string, textColor: string, buttonColor: string): string {
		// Keeping it simple for now
		const borderColor = textColor

		let buttonHtml = '<div>'

		buttonHtml += `<table width="100%" cellspacing="0" cellpadding="0">
<tr>
	<td>
		<table cellspacing="0" cellpadding="0">
			<tr>
				<td style="border-radius: 5px" bgcolor="${buttonColor}" >
					<a href="${buttonLink}" target="_blank" style="padding: 8px 10px; border: 1px solid ${borderColor}; ` +
						`border-radius: 5px;font-family: Helvetica, Arial, sans-serif;font-size: 14px; color: ${textColor}; ` +
						`text-decoration: none;font-weight:bold;display: inline-block; text-align: center; width: 150px">
						${buttonText}
					</a>
				</td>
				<td style="text-align: left; vertical-align: middle; padding: 5px;"> - ${buttonDescription}</td>
			</tr>
		</table>
	</td>
</tr>
</table>`

		buttonHtml += `</div><br />`

		return buttonHtml
	}
	private _sendGenericEmail(recipients: Recipients, subject: string, intro: string, message: string) {
		const params : MailParams = { intro, message, botname: this.graphBotName, 
			robomergeRootUrl: { value: this.externalRobomergeUrl, noEscape: true } }
		this.mailer.sendEmail('generic-email-template', recipients, subject, params, this.findEmail)
	}
	private _sendBlockageEmail(blockage: Blockage) {
		const recipients = new Recipients(blockage.owner)
		const subject = `Change ${blockage.change.cl} caused blockage in ${this.fullName}`
		const intro = `Robomerge encountered an error merging change ${blockage.change.cl} from branch ${blockage.change.branch.name}.`
		const urls = this.getBlockageUrls(blockage)

		// Begin contructing email message
		let emailPreformattedText = `${blockage.failure.kind}!\n\nCL #${blockage.change.cl}: ${blockage.change.description}`
		if (blockage.failure.description) {
			emailPreformattedText += `\n\n${blockage.failure.description}`
		}

		const params : MailParams = { intro, message: emailPreformattedText, botname: this.graphBotName, 
			robomergeRootUrl: { value: this.externalRobomergeUrl, noEscape: true } }

		const conflictCLUrl = `<a href="https://p4-swarm.companyname.net/changes/${blockage.change.cl}">${blockage.failure.kind} CL #${blockage.change.cl}</a>`
		// Should never actually see 'the target branch' string
		const targetBranch = blockage.action ? blockage.action.branch.name : "the target branch"

		// Create list of action links
		let resolutionOptionsStr = ""

		const acknowledgeText = blockage.action ? `merge ${conflictCLUrl} to ${targetBranch}` : `resolve ${conflictCLUrl}`
		resolutionOptionsStr += this.createEmailLinkButton("Acknowledge Conflict", urls.acknowledgeUrl, 
			`"I will ${acknowledgeText} myself."`, '#228B22', '#ffffff') // 'Forest Green' on White
		if (urls.createShelfUrl) {
			resolutionOptionsStr += this.createEmailLinkButton("Create Shelf", urls.createShelfUrl, 
				`"I want Robomerge to create a shelf for me containing the conflict merging ${conflictCLUrl} to ${targetBranch}."`, '#228B22', '#ffffff') // 'Forest Green' on White
		}
		/*if (urls.skipUrl) {
			resolutionOptionsStr += this.createEmailLinkButton("Skip Changes", urls.skipUrl, 
				`"Robomerge should ignore ${conflictCLUrl} and not attempt to merge it to ${targetBranch}."`, '#FF8C00', '#ffffff') // Dark Orange on White
		}*/
		if (urls.stompUrl) {
			resolutionOptionsStr += this.createEmailLinkButton("Stomp Changes", urls.stompUrl, 
				`"Robomerge should attempt to stomp changes in ${targetBranch} with the content in ${conflictCLUrl}."`, '#B22222', '#ffffff') // 'Firebrick' on White)
		}

		
		// Add list to email parameters
		params.resolutionOptions = { value: resolutionOptionsStr, noEscape: true }

		this.mailer.sendEmail('new-blockage', recipients, subject, params, this.findEmail)
	}

	sendErrorEmail(recipients: Recipients, subject: string, message: string, errorPreface='RoboMerge needs your help:'): void {
		// log to STDERR
		this.nodeBotLogger.error(`${subject}\n${message}\n\n`);
		this._sendGenericEmail(recipients, subject, errorPreface, message);
	}

	private parseChange(change: Change, optTargetBranch?: Branch | null): ChangeInfo {

		// parse the description
		if (change.desc && typeof(change.desc.split) !== 'function') {
			this.nodeBotLogger.warn(`Unrecognised description type: ${typeof(change.desc)}, CL#${change.change}`)
			change.desc = '<description not available>'
		}

		let combinedMacros: { [name: string]: string[] } = {}
		for (let macroName in this.branch.macros)
		{
			combinedMacros[macroName.toLowerCase()] = this.branch.macros[macroName]
		}
		for (let macroName in this.branchGraph.config.macros)
		{
			const macroNameLower = macroName.toLowerCase()
			if (combinedMacros[macroNameLower] === undefined)
			{
				combinedMacros[macroNameLower] = this.branchGraph.config.macros[macroName]
			}
		}

		const parse = (lines: string[]) => parseDescriptionLines({
			lines,
			isDefaultBot: this.branch.isDefaultBot,
			graphBotName: this.graphBotName,
			cl: change.change,
			aliasesUpper: this.branchGraph.config.aliases.map(s => s.toUpperCase()),
			macros: combinedMacros,
			logger: this.nodeBotLogger
		})

		const parsedLines = parse(change.desc.split('\n'))

		const commandOverride = (change.commandOverride || '').trim()
		if (commandOverride) {

			const parsedOverride = parse(commandOverride.split('|').map(s => s.trim()))
			if (change.accumulateCommandOverride) {
				parsedLines.append(parsedOverride)
			}
			else {
				parsedLines.override(parsedOverride)
			}
		}

		// Author is always CL user
		const author = (change.user || 'robomerge').toLowerCase()

		let source_cl = change.change

		// make the end description (without any robomerge tags or 'at' references)
		const description = parsedLines.descFinal.join('\n').replace(/@/g, '[at]').trim()

		// default source to current
		if (!parsedLines.source) {
			parsedLines.source = `CL ${change.change} in ${this.branch.rootPath}`
		}
		else {
			let m = parsedLines.source.match(/^CL (\d+)/)
			let num = m ? (parseInt(m[1]) || 0) : 0
			source_cl = num > 0 ? num : change.change
		}

		// By default, we should perform any submit that comes from a change.
		// If the change is setup to be forcibly shelved, honor it
		let forceCreateAShelf = !!change.forceCreateAShelf
		let sendNoShelfNotification = !!change.sendNoShelfNotification

		let forceStompChanges = !!change.forceStompChanges
		let additionalDescriptionText = change.additionalDescriptionText

		const info: ChangeInfo = {
			branch: this.branch,
			cl: change.change,
			source_cl: source_cl,
			author,
			source: parsedLines.source,
			description,
			propagatingNullMerge: parsedLines.propagatingNullMerge,
			forceCreateAShelf, sendNoShelfNotification,

			forceStompChanges, additionalDescriptionText,
			overriddenCommand: commandOverride,
			macros: parsedLines.expandedMacros
		}

		if (change.isUserRequest) {
			info.userRequest = optTargetBranch ? 'edge-reconsider' : 'node-reconsider'
		}

		if (parsedLines.authorTag) {
			info.authorTag = parsedLines.authorTag
		}

		if (parsedLines.owner) {
			info.owner = parsedLines.owner
		}

		if (parsedLines.errors.length > 0) {
			info.errors = parsedLines.errors
			return info
		}

		// compute targets if list is not empty
		const defaultTargets = commandOverride ? [] :
								parsedLines.useDefaultFlow ? this.branch.defaultFlow :
								this.branch.forceFlowTo

		const args = parsedLines.arguments
		if (optTargetBranch || (args.length + defaultTargets.length) > 0) {
			const computeResult = computeTargets(this.branch, this.ubergraph, change.change, info, args, defaultTargets, this.nodeBotLogger, optTargetBranch)
			if (computeResult) {
				const [computedMergeActions, flags] = computeResult

				if (computedMergeActions.length > 0) {
					if (info.userRequest) {
						info.targets = computedMergeActions
					}
					else {
						const mergeActions: MergeAction[] = []

						for (const action of computedMergeActions) {
							const edgeProperties = this.branch.edgeProperties.get(action.branch.upperName)

							// let ignore and disregard cancel each other out here, but don't honour ignore here so we can validate it at the bottom of the function
							const authorIsExcluded = edgeProperties && edgeProperties.excludeAuthors && edgeProperties.excludeAuthors.indexOf(info.author) >= 0

							if (!authorIsExcluded || (flags.has('disregardexcludedauthors') && !flags.has('ignore'))) {
								mergeActions.push(action)
							}
							else {
								// skip this one
								const skipMessage = `Skipping CL ${info.cl} due to excluded author '${info.author}'`
								this.nodeBotLogger.info(skipMessage)

							}
						}

						if (mergeActions.length > 0) {
							info.targets = mergeActions
						}
					}

					if (info.targets && flags.has('ignore')) {
						if (this.branch.config.disallowDeadend) {
							info.errors = ['deadend (ignore) disabled for this stream']
						}
						else {
							delete info.targets
						}
					}
				}
			}

		}

// try doing all of disallowDeadend, exludedAuthor and disregaredExlcuded here
//	disallowDeadend is by source node, others
//	only complain about disallowDeadend if any targets remaining 
// recreate target list, excluding 

	// if (ri.flags.has('ignore')) {
	// 	return { computeResult: null, errors: allowIgnore ? [] : ['deadend (ignore) disabled for this stream'] }
	// }



		if (!info.errors) {
			const errors: string[] = []
			const actions = info.targets || []
			processOtherBotTargets(parsedLines, this.branch, this.ubergraph, actions, errors)
			if (actions.length > 0) {
				info.targets = actions
			}
			if (errors.length > 0) {
				info.errors = errors
			}
		}
		return info
	}

	private _getBranch(name: string) {
		return this.branchGraph.getBranch(name) || null;
	}

	private async _mergeClViaEdges(availableEdges: Map<string, EdgeBot>, info: ChangeInfo, ignoreEdgePauseState: boolean) : Promise<EdgeMergeResults> {
		const mergedChanges = new EdgeMergeResults()

		for (const target of info.targets!) {
			if (!this.hasEdge(target.branch)) {
				const err = new Error(`No edge exists for target branch "${target.branch.name}"\nCurrent Edges: ${this.printEdges()}`)
				this.nodeBotLogger.printException(err)
				throw err
			}

			const targetEdge = availableEdges.get(target.branch.upperName)

			// If the edge requested is currently not available this tick, simply log that we skipped it and move on
			if (!targetEdge) {
				this.nodeBotLogger.verbose(`No available edges can serve CL ${info.cl} -> ${target.branch.name} this tick.`)
				continue
			}

			// Gate problem: say gate is 15, last integrated on edge was 12, incoming is 17. Edge is available because 12 < 15
			// Note: good thing about using availability (in theory) - will not fetch loads of revisions repeatedly

			// assuming ignoreEdgePauseState is false only in the regular flow, so we're good updating edge last CLs
			if (!ignoreEdgePauseState) {

				// give edge a change to take its gate into account
				targetEdge.preIntegrate(info.cl)

				// If this edge is paused, skip it but we'll have to report it so the node doesn't advance its lastCl
				if (!targetEdge.isAvailable) {
					mergedChanges.markSkipped(target.branch)
					availableEdges.delete(targetEdge.targetBranch.upperName)
					continue
				}
			}

			// If this is not a manual change, ensure we don't tell an edge that has moved past this point to redo work
			if (!info.userRequest && info.cl <= targetEdge.lastCl) {
				this.nodeBotLogger.debug(`${info.cl} is behind ${targetEdge.displayName} lastCL: ${targetEdge.lastCl}`)
				continue
			}

			if (this.tickJournal) {
				++this.tickJournal.merges
			}

			const additionalDescription = 
				targetEdge.implicitCommands
					.map(s => s + '\n')
					.join('')

			const integrationResult = await targetEdge.performMerge(info, target, this.branch.convertIntegratesToEdits, additionalDescription)

			// wake up target nodebot, so look for further merges or conflict resolutions
			const targetNodebot = target.branch.bot as NodeBot
			if (targetNodebot) {
				targetNodebot.ticksSinceLastNewP4Commit = 0
				targetNodebot.skipTickCounter = 0
			}

			// If the target edge is now blocked after the merge attempt, remove it from future merges this tick
			if (targetEdge.isBlocked) {
				this.nodeBotLogger.info(`${targetEdge.displayName} is blocked after attempting to merge ${info.cl}, removing edge from merges this tick.`)
				availableEdges.delete(targetEdge.targetBranch.upperName)
			}

			mergedChanges.addIntegrationResult(target, integrationResult)

			// if integration did something, enough time has probably passed to make worth checking to see if any manual requests came in
			if (integrationResult.result !== 'nothing to do') {
				await this.postIntegrate()
			}
		}

		return mergedChanges
	}

	handleRoboShelveError(failure: Failure, toShelve: PendingChange) {
		const owner = getIntegrationOwner(toShelve) || toShelve.change.author
		const ownerEmail = this.p4.getEmail(owner)
		const blockage: Blockage = {change: toShelve.change, failure, owner, ownerEmail,
										time: new Date, action: toShelve.action}
		this.eventTriggers.reportBlockage(blockage, true)
	}

	private async handleSyntaxError(originalCommitMessage: string, change: ChangeInfo) {
		const shortMessage = `Syntax error in CL ${change.cl}`
		let message = `CL ${change.cl} in ${this.branch.rootPath} contained one or more RoboMerge syntax errors (see below).\n\n`
		for (const err of change.errors!) {
			message += `* ${err}\n`
		}

		const owner: string = change.owner || change.author

		if (change.userRequest) {
			this.nodeBotLogger.info(shortMessage + ` (reconsider triggered by ${change.owner}):\n${message}.`)
			if (this.slackMessages) {

				let dm: SlackMessage = {
					text: message,
					style: SlackMessageStyles.DANGER,
					channel: '',
					mrkdwn: true
				}
				
				let emailAddress = await this.findEmail(owner)
				this.slackMessages.postDM(emailAddress, change.cl, this.branch, dm)				
			}
			else {
				this.sendErrorEmail(new Recipients(owner), shortMessage, message + '\nFull CL description:\n\n' + originalCommitMessage)
			}
		}
		else {
			// see if we've already sent a notification for this one
			const existingBlockage = this.conflicts.find(null, change.source_cl)
			const blockageOccurred = existingBlockage ? existingBlockage.time : new Date

			if (!existingBlockage) {
				const ownerEmailJustForSlackAtTheMoment = this.p4.getEmail(owner)
				this.conflicts.onBlockage({action: null, change, failure: {kind: 'Syntax error', description: message}, owner,
					ownerEmail: ownerEmailJustForSlackAtTheMoment, time: blockageOccurred})
			}
	
			// stop the branch
			const pauseInfo = NodeBot._makeBlockageInfo(shortMessage, message, change, blockageOccurred)
			this.block(pauseInfo, SYNTAX_ERROR_PAUSE_TIMEOUT_SECONDS)
		}
	}

	/** return true if this is a new flow blockage (excludes reconsiders) */
	async handleMergeFailure(failure: Failure, pending: PendingChange, postIntegrate?: boolean) {
		const targetBranch = pending.action.branch
		const shortMessage = `${failure.kind} while merging CL ${pending.change.cl} to ${targetBranch.name}`

		const owner = getIntegrationOwner(pending) || pending.change.author
		if (pending.change.userRequest) {
			this.nodeBotLogger.info(shortMessage + ` (reconsider triggered by ${pending.change.owner}):\n${failure.description}.`)

			let message = `While reconsidering CL#${pending.change.cl}`

			if (failure.summary) {
				message += '\n' + failure.summary 
			}
			else
			{
				message += '\n' + failure.description
			}

			if (postIntegrate && pending.change.targetWorkspaceForShelf) {
				message += `\n\nShelved ${pending.newCl} in workspace ${pending.change.targetWorkspaceForShelf}`
			}

			if (this.slackMessages) {

				let dm: SlackMessage = {
					text: message,
					style: SlackMessageStyles.WARNING,
					channel: '',
					mrkdwn: true
				}
				
				let emailAddress = await this.findEmail(owner)
				this.slackMessages.postDM(emailAddress, pending.newCl, this.branch, dm)
			}
			else {
				this.sendErrorEmail(new Recipients(owner), shortMessage, message)
			}
			return false
		}

		return this.findOrCreateBlockage(failure, pending, shortMessage)
	}

	findOrCreateBlockage(failure: Failure, pending: PendingChange, shortMessage: string, approval?: {settings: ApprovalOptions, shelfCl: number}) {
		const owner = getIntegrationOwner(pending) || pending.change.author

		// see if we've already created a notification for this one
		const existingBlockage = this.conflicts.find(pending.action.branch, pending.change.source_cl)
		const blockageOccurred = existingBlockage ? existingBlockage.time : new Date

		const ownerEmail = this.p4.getEmail(owner)
		const blockage: Blockage = {change: pending.change, action: pending.action, failure, owner, ownerEmail, time: blockageOccurred, approval}

		if (existingBlockage) {
			this.nodeBotLogger.info(shortMessage + ' (already notified)')

			this.conflicts.updateBlockage(blockage)
			return false
		}
		if (this.tickJournal && (failure.kind === 'Merge conflict' || failure.kind === 'Exclusive check-out')) {
			++this.tickJournal.conflicts;
		}

		this.conflicts.onBlockage(blockage)
		return true
	}

	async reportApprovalRequired(approval: ApprovalOptions, pending: PendingChange) {
		if (this.slackMessages) {
			const userEmail = await this.p4.getEmail(pending.change.author)
			const slackUser = userEmail ? await this.slackMessages.getSlackUser(userEmail) : null
			const channelPing = slackUser ? `<@${slackUser}>` : `@${pending.change.author}`

			const fields: SlackMessageField[] = [
				{title: 'Change', short: true, value: makeClLink(pending.newCl)}
			]

			const opts: SlackMessage = { 
				title:'', 
				text: `${channelPing}'s change in ${pending.action.branch.name} needs to be approved.\n\n` +
						approval.description, 
				style: SlackMessageStyles.DANGER, 
				fields,
				mrkdwn: true,
				channel: approval.channelId
			}

			this.slackMessages.postNonConflictMessage(opts)
		}
	}

	private async sendNagNotifications() {
		let naggedSomeone = false
		const now = Date.now()
		for (const conflict of this.conflicts.getConflicts()) {

			const getNagProperty = (property:string) => {
						
				const configKey = property as keyof BotConfig
				const edgeKey   = property as keyof EdgeOptions
				const nodeKey   = property as keyof NodeOptions

				let prop: any | null = this.branchGraph.config[configKey]
				if (conflict.targetBranchName) {
					const targetBranch = this.branchGraph.getBranch(conflict.targetBranchName)
					if (targetBranch && targetBranch.config[nodeKey] !== undefined)
					{
						prop = targetBranch.config[nodeKey]
					}

					const edgeProperties = this.branch.edgeProperties.get(conflict.targetBranchName)
					if (edgeProperties && edgeProperties[edgeKey] !== undefined) {
						prop = edgeProperties[edgeKey]
					}
				}
				else if (this.branch.config[nodeKey] !== undefined)
				{
					prop = this.branch.config[nodeKey]
				}
				return prop
			}
			
			const nagWhenBlocked = getNagProperty("nagWhenBlocked")
			
			if (nagWhenBlocked) {
				const ageMinutes = ((now - conflict.time.getTime()) / 1000) / 60

				if (this.slackMessages) {

					const nagSchedule = (conflict.acknowledger && (getNagProperty("nagAcknowledgedSchedule") || DEFAULT_ACKNOWLEDGED_NAG_SCHEDULE))
											|| getNagProperty("nagSchedule") || DEFAULT_NAG_SCHEDULE

					const nagDelay = conflict.lastNagTime ? ((now - conflict.lastNagTime.getTime()) / 1000) / 60 : ageMinutes
					const nagWait = nagSchedule[Math.min(conflict.nagCount, nagSchedule.length - 1)]
					const nagLeeway = getNagProperty("nagAcknowledgedLeeway") || DEFAULT_ACKNOWLEDGED_NAG_LEEWAY
					const giveLeeway = conflict.acknowledgedAt && (nagLeeway > ((now - conflict.acknowledgedAt.getTime()) / 1000) / 60)

					if (nagDelay > nagWait && !giveLeeway) {
						conflict.nagCount++ 
						conflict.lastNagTime = new Date(now)
						naggedSomeone = true

						let timeDesc
						if (ageMinutes < 60) {
							timeDesc = `${Math.floor(ageMinutes)} minutes`
						}
						else {
							const hours = Math.floor(ageMinutes / 60)
							if (hours == 1) {
								timeDesc = "1 hour"
							}
							else {
								timeDesc = `${hours} hours`
							}
						}
				
						let triager = getNagProperty("triager")

						if (triager && !triager.startsWith('@'))
						{
							const emailAddress = await this.findEmail(triager)
							if (emailAddress)
							{
								const user = await this.slackMessages.getSlackUser(emailAddress)
								if (user) {
									triager = `<@${user}>`
								}
								else {
									triager = `@${triager}`
									this.nodeBotLogger.error(`Unable to look up triager from ${triager} (${emailAddress})`)
								}
							}
							else {
								triager = `@${triager}`
								this.nodeBotLogger.error(`Unable to look up email address for triager ${triager}`)
							}
						}

						const nagMessage: SlackMessage = {
							text: `${triager ? triager + ' ' : ''}RoboMerge blocked for more than ${timeDesc}`,
							style: SlackMessageStyles.DANGER,
							channel: this.branchGraph.config.slackChannel,
							mrkdwn: true
						}

						this.nodeBotLogger.info(`Sending nag notification to ${conflict.owner} after ${Math.floor(ageMinutes)} minutes`)
						const branchName =  (conflict.kind === "Syntax error" ? conflict.kind : conflict.targetBranchName || conflict.blockedBranchName)
						this.slackMessages.postReply(conflict.cl, branchName, nagMessage)
					}
				}
				else if (conflict.nagCount == 0) // for now we'll maintain the email only once behavior
				{
					if (ageMinutes > NAG_EMAIL_MIN_TIME_MINUTES) {

						conflict.nagCount = 1
						naggedSomeone = true

						this.sendErrorEmail(new Recipients(conflict.owner),
							`${this.graphBotName} RoboMerge blocked for more than ${NAG_EMAIL_MIN_TIME_DESCRIPTION}`,
							`Please check robomerge page for more details`
						)
					}
				}
			}
		}

		if (naggedSomeone) {
			this.conflicts.persist()
		}
	}

	
}
