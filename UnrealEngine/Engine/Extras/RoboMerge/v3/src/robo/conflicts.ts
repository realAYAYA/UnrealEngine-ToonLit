// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from 'common/logger';
import { AlreadyIntegrated, Blockage, Branch, BranchArg, branchesMatch } from './branch-interfaces';
import { ChangeInfo, ForcedCl, resolveBranchArg } from './branch-interfaces';
import { ConflictStatusFields } from './status-types';
import { PersistentConflict, PersistentConflictToString, Resolution } from './conflict-interfaces';
import { EdgeBot } from './edgebot';
import { BotEventHandler, BotEventTriggers } from './events';
import { Context } from './settings';
import { UGS } from './ugs';

function conflictMatches(conflict: PersistentConflict, sourceCl: number, branchArg?: BranchArg | null) {
	const targetBranchName = branchArg && resolveBranchArg(branchArg, true)
	return conflict.sourceCl === sourceCl && (
		targetBranchName ? conflict.targetBranchName === targetBranchName : !conflict.targetBranchName
	);
}

function conflictMatchesByChangeCl(conflict: PersistentConflict, changeCl: number, branchArg?: BranchArg | null) {
	const targetBranchName = branchArg && resolveBranchArg(branchArg, true)
	return conflict.cl === changeCl && (
		targetBranchName ? conflict.targetBranchName === targetBranchName.toUpperCase() : !conflict.targetBranchName
	);
}


function makePersistentConflict(blockage: Blockage): PersistentConflict {
	const sourceChange = blockage.change
	const result: PersistentConflict = {
		blockedBranchName: sourceChange.branch.upperName,
		cl: sourceChange.cl,
		sourceCl: sourceChange.source_cl,
		author: sourceChange.author,
		owner: blockage.owner,
		kind: blockage.failure.kind,
		time: blockage.time,
		nagged: false,
		ugsIssue: -1
	}

	if (blockage.action) {
		result.targetBranchName = blockage.action.branch.upperName

		if (blockage.action.branch.stream) {
			result.targetStream = blockage.action.branch.stream
		}
	}

	return result
}

function setResolvedTimeAsNow(conflict: PersistentConflict) {
	conflict.timeTakenToResolveSeconds = (Date.now() - conflict.time.getTime()) / 1000
}

class BuildHealthReporter implements BotEventHandler {
	constructor(private conflicts: Conflicts) {
	}

	onBlockage(blockage: Blockage, isNew: boolean) {
		if (isNew && blockage.change.branch.upperName === this.conflicts.nodeNameUpper) {
			UGS.reportBlockage(blockage, this.conflicts.externalUrl).then((issueNumber: number) => {
				const conflict = this.conflicts.find(blockage.action ? blockage.action.branch : null, blockage.change.source_cl)
				if (conflict) {
					if (conflict.resolution) {
						UGS.reportResolved(issueNumber)
					}
					else {
						conflict.ugsIssue = issueNumber
						this.conflicts.persist()
					}
				}
			}, () => {}) // swallow failure, already logged
		}
	}

	onBlockageAcknowledged(conflict: PersistentConflict) {
		if (conflict.ugsIssue !== -1 && conflict.blockedBranchName === this.conflicts.nodeNameUpper) {
			UGS.acknowledge(conflict.ugsIssue, conflict.acknowledger || '')
		}
	}

	onBranchUnblocked(conflict: PersistentConflict) {
		if (conflict.ugsIssue !== -1 && conflict.blockedBranchName === this.conflicts.nodeNameUpper) {
			UGS.reportResolved(conflict.ugsIssue)
		}
	}
}

/** Per node-bot record of conflicts waiting for resolution */
export class Conflicts {

	constructor(
		public nodeNameUpper: string,
		private eventTriggers: BotEventTriggers,
		private persistence: Context,
		public externalUrl: string,
		reportToBuildHealth: boolean, 
		private conflictLogger: ContextualLogger
	) {
		// need backward compat, or just switch over when no conflicts?
		const conflictsToLoad = persistence.get('conflicts') as PersistentConflict[]
		if (conflictsToLoad) {
			for (const conflict of conflictsToLoad) {
				conflict.time = new Date(conflict.time)
				this.conflicts.push(conflict)
			}
		}

		if (reportToBuildHealth) {
			this.eventTriggers.registerHandler(new BuildHealthReporter(this))
		}
	}

	getConflicts() {
		return this.conflicts
	}

	getUnresolvedConflicts() {
		return this.conflicts.filter(conflict => !conflict.resolution)
	}

	getConflictByChangeCl(changeCl: number, targetBranch: BranchArg) {
		for (const conflict of this.conflicts) {
			if (conflictMatchesByChangeCl(conflict, changeCl, targetBranch)) { 
				return conflict
			}
		}
		return null
	}

	findUnresolvedConflictByBranch(targetBranch: BranchArg) {
		const targetBranchName = resolveBranchArg(targetBranch, true)
		return this.conflicts.find(conflict => conflict.targetBranchName === targetBranchName && !conflict.resolution)
	}

	acknowledgeConflict(acknowledger : string, changeCl : number) : boolean {
		// Search list of conflicts for those matching the sourceCl, ack'ing every conflict that matches
		let conflictsFound = false
		for (const conflict of this.conflicts) {
			if (conflict.cl === changeCl) {
				conflict.acknowledger = acknowledger
				conflict.acknowledgedAt = new Date

				this.eventTriggers.reportBlockageAcknowledged(conflict)

				this.persist()
				conflictsFound = true
			}
		}

		return conflictsFound
	}

	unacknowledgeConflict(changeCl : number) : boolean {
		// Search list of conflicts for those matching the sourceCl
		let conflictsFound = false
		for (const conflict of this.conflicts) {
			if (conflict.cl === changeCl) {
				conflict.acknowledger = undefined
				conflict.acknowledgedAt = undefined

				this.eventTriggers.reportBlockageAcknowledged(conflict)

				this.persist()
				conflictsFound = true
			}
		}

		return conflictsFound
	}

	updateBlockage(blockage: Blockage) {
		const replacement = makePersistentConflict(blockage)

		for (let index = 0; index != this.conflicts.length; ++index) {
			const conflict = this.conflicts[index]
			if (conflictMatches(conflict, replacement.sourceCl, replacement.targetBranchName)) {

				// maintain certain fields case-by-case
				const acknowledger = conflict.acknowledger
				if (acknowledger) {
					replacement.acknowledger = acknowledger
					replacement.acknowledgedAt = conflict.acknowledgedAt
				}
				replacement.nagged = conflict.nagged
				replacement.ugsIssue = conflict.ugsIssue

				this.conflicts[index] = replacement
				this.persist()

				this.eventTriggers.reportBlockage(blockage, false)
				return
			}
		}

		// fail hard here - calling code has to guarantee that blockage to update will be found
		throw new Error('failed to update blockage')
	}

	private reportUnblocked(conflict: PersistentConflict) {
		this.conflictLogger.debug(`Unblocking ${PersistentConflictToString(conflict)}`)
		this.eventTriggers.reportBranchUnblocked(conflict)

	}

	onBlockage(blockage: Blockage) {
		//this.conflictLogger.debug(`Creating conflict for blockage from cl#${blockage.change.cl}:\nAction:\n${JSON.stringify(blockage.action)}`)

		const conflict = makePersistentConflict(blockage)
		//this.conflictLogger.debug(`Conflict:\n${JSON.stringify(conflict)}`)
		this.conflicts.push(conflict)
		this.persist()

		this.eventTriggers.reportBlockage(blockage, true)
		this.conflictLogger.debug(`Conflict added. Conflicts = ${JSON.stringify(this.conflicts)}`)
	}

	/**
	 * Check if any EdgeBots have moved past our NodeBot's conflict CLs
	 *
	 * This happens before a tick, so that all other bots will have had a chance
	 * to update, so that resolving changelists will have been seen
	 */
	checkForResolvedConflicts(edgeBot: EdgeBot) {
		if (this.conflicts.length === 0) {
			return
		}

		const targetBranchStr = resolveBranchArg(edgeBot.targetBranch, true)

		// Check all the conflicts from our branch for resolved blockages
		const remainingConflicts: PersistentConflict[] = []
		const resolvedConflicts: PersistentConflict[] = []

		for (const conflict of this.conflicts) {
			// If our bot CL is below the conflict CL, we can assume we haven't resolved this conflict
			if (!conflict.targetBranchName || conflict.targetBranchName.toUpperCase() !== targetBranchStr || edgeBot.lastCl < conflict.cl) {
				remainingConflicts.push(conflict)
				continue
			}

			// If the conflict didn't fill out the resolution, try to determine one now
			resolvedConflicts.push(conflict)
			if (!conflict.resolution) {
				if (conflict.targetBranchName) {
					// didn't work out what this was: put an innocuous message
					conflict.resolution = Resolution.DUNNO
				}
				else {
					// at time of writing, must have been a syntax error
					conflict.resolution = Resolution.RESOLVED
					conflict.resolvingAuthor = conflict.author
				}
			}

			if (!conflict.timeTakenToResolveSeconds) {
				// fall back to time of this event being fired - should be at most 30 seconds too long
				setResolvedTimeAsNow(conflict)
			}

			this.conflictLogger.info(`Conflict for branch ${targetBranchStr} cl ${conflict.cl} seems to be resolved: ${conflict.resolution} by ${conflict.resolvingAuthor}`)
		}


		this.setConflicts(remainingConflicts)
		
		// fire events after updating the conflicts state
		for (const conflict of resolvedConflicts) {
			this.reportUnblocked(conflict)
		}
	}

	/** Peek at every changelist that comes by, to see if it's a change intended to resolve existing conflicts */
	onChangeParsed(change: ChangeInfo, targetedBranch?: Branch | null) {
		this.eventTriggers.reportChangeParsed(change)

		for (const conflict of this.conflicts) {
			// Ensure conflict's changelist is the same as this parsed change
			if (conflict.cl !== change.cl) {
				continue
			}

			// If this change is targeted at a specific branch, ensure this is a conflict for said branch
			if (targetedBranch && !branchesMatch(conflict.targetBranchName, targetedBranch)) {
				continue
			}

			// This parsed change is relevant to this conflict. Does the parsed change still target this conflicts branch?
			let parsedChangeTargetsConflictedBranch = false
			if (change.targets) {
				for (const action of change.targets) {
					if (action.branch.upperName === conflict.targetBranchName) {
						parsedChangeTargetsConflictedBranch = true
						break
					}
				}
			}

			// If the parsed change no longer targets this branch, cancel the conflict -- that target no longer will recieve this change, so no need to block
			if (!parsedChangeTargetsConflictedBranch) {
				this.conflictLogger.debug(`Cancelling conflict ${PersistentConflictToString(conflict)}`)
				conflict.resolution = Resolution.CANCELLED
				setResolvedTimeAsNow(conflict)
				this.setConflicts(this.conflicts.filter(el => el !== conflict))
this.conflictLogger.info('Reporting unblocked (cancelled) ' + conflict.cl)
				this.reportUnblocked(conflict)
			}
		}
	}

	/** This is where we actually confirm that a conflict has been resolved */
	onAlreadyIntegrated(change: AlreadyIntegrated) {
		this.eventTriggers.reportAlreadyIntegrated(change)

		const conflict = this.find(change.action.branch, change.change.source_cl)
		if (conflict) {
			conflict.resolution = Resolution.RESOLVED
		}
	}

	// If a forced CL is the same as a conflict CL, the conflict as skipped.
	// Otherwise we'll notify that the branch has been forced to a CL
	onForcedLastCl(forced: ForcedCl) {
		this.eventTriggers.reportForcedLastCl(forced)
		if (!this.notifyConflictSkipped(forced)) {
			this.eventTriggers.reportNonSkipLastClChange(forced)
		}
	}

	// called for changes relating to all *other* bots
	onGlobalChange(change: ChangeInfo) {
		const existing = this.find(change.branch, change.source_cl)
		if (existing) {
			existing.resolvingCl = change.cl
			existing.resolvingAuthor = change.author
			setResolvedTimeAsNow(existing)
		}
	}

	private notifyConflictSkipped(forced: ForcedCl) {
		if (forced.forcedCl <= forced.previousCl) {
			return false
		}

		const remainingConflicts: PersistentConflict[] = []
		const unblocksToReport: PersistentConflict[] = []

		for (const conflict of this.conflicts) {
			// If the forced CL is the same as our conflict CL, count this conflict as skipped
			if (conflict.cl === forced.forcedCl) {
				conflict.resolvingAuthor = forced.culprit
				conflict.resolution = Resolution.SKIPPED
				conflict.resolvingReason = forced.reason
				setResolvedTimeAsNow(conflict)
				unblocksToReport.push(conflict)
			}
			else {
				remainingConflicts.push(conflict)
			}
		}

		if (unblocksToReport.length === 0) {
			return false
		}

		this.conflictLogger.debug(`notifyConflictSkipped -- ${forced.nodeOrEdgeName} @ CL# ${forced.forcedCl}`)
		this.setConflicts(remainingConflicts)

		for (const unblock of unblocksToReport) {
			this.conflictLogger.info('Reporting unblocked (skipped) ' + unblock.cl)
			this.reportUnblocked(unblock)
		}
		return true
	}

	persist() {
		this.persistence.set('conflicts', this.conflicts)
	}

	find(targetBranch: Branch | null, sourceCl: number): PersistentConflict | null {
		for (const conflict of this.conflicts) {
			if (conflictMatches(conflict, sourceCl, targetBranch)) {
				return conflict
			}
		}

		return null
	}

	applyStatus(conflictsForStatus: Partial<ConflictStatusFields>[]) {
		for (const conflict of this.conflicts) {
			const conflictObj: Partial<ConflictStatusFields> = {}

			conflictObj.cl = conflict.cl
			conflictObj.sourceCl = conflict.sourceCl

			const target = conflict.targetBranchName || null
			if (target) {
				conflictObj.target = target
				if (conflict.targetStream) {
					conflictObj.targetStream = conflict.targetStream
				}
			}

			conflictObj.kind = conflict.kind
			conflictObj.author = conflict.author
			conflictObj.owner = conflict.owner

			conflictsForStatus.push(conflictObj)
		}
	}

	private setConflicts(conflicts: PersistentConflict[]) {
		this.conflicts = conflicts
		this.persist()
	}

	private conflicts: PersistentConflict[] = []
}
