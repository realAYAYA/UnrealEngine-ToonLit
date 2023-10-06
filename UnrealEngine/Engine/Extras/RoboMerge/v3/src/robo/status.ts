// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from '../common/logger';
import { Branch } from './branch-interfaces';
import { BranchDefForStatus, BranchStatus, GraphBotState, UserStatusData } from './status-types';
import { OperationReturnType } from './ipc';
import { AuthData } from './session';

export class Status {
	private readonly statusLogger: ContextualLogger
	constructor(private startTime: Date, private version: string, parentLogger: ContextualLogger) {
		this.statusLogger = parentLogger.createChild('Status')
	}

	addBranch(branch: Branch) {
		const status: BranchStatus = {def: Status.makeBranchDef(branch), bot: branch.parent.botname, branch_spec_cl: -1}
		this.allBranches.push(status)

		if (branch.bot) {
			branch.bot.applyStatus(status)
		}
	}

	reportBotState(botName: string, state: GraphBotState) {
		this.botStates.push([botName, state])
	}

	getForUser(user: AuthData) {
		const branches: BranchStatus[] = []
		const botStates: [string, GraphBotState][] = []

		const result: UserStatusData = {
			started: this.startTime,
			version: this.version,
			user: {
				userName: user.user,
				displayName: user.displayName
			},
			branches, botStates
		}

		if (user.tags.size !== 0) {
			result.user.privileges = [...user.tags]
		}

		const includedBotNames = new Set<string>()

		result.branches = branches
		result.botStates = botStates
		for (const status of this.allBranches) {
			// @todo: check for list of tags
			if (this.includeBranch(status.def, user.tags)) {
				branches.push(status)
				includedBotNames.add(status.bot)
			}
		}

		for (const [name, botState] of this.botStates) {
			if (includedBotNames.has(name)) {
				botStates.push([name, botState])
			}
		}

		if (this.allBranches.length > 0 && branches.length === 0) {
			result.insufficientPrivelege = true
		}
		return result
	}

	static fromIPC(obj: OperationReturnType, logger: ContextualLogger) {
		if (!obj || !obj.data) {
			throw new Error('Invalid format of obj sent to Status.fromIPC ' + (obj && JSON.stringify(obj)))
		}

		const data = obj.data

		if (!('allBranches' in data && 'startTime' in data && 'version' in data)) {
			throw new Error('Invalid status from IPC')
		}

		const result = new Status(data.startTime, data.version, logger)
		result.allBranches = data.allBranches
		result.botStates = data.botStates
		return result
	}

	private includeBranch(branch: BranchDefForStatus, userPrivileges: Set<string>) {
		if (branch.visibility === 'all') {
			return true
		}

		if (branch.visibility === 'none') {
			return false
		}

		if (!Array.isArray(branch.visibility)) {
			// for now, only keywords 'all' and 'none' supported
			this.statusLogger.warn('Unknown visibility keyword: ' + branch.visibility)
			return false
		}

		for (const vis of branch.visibility) {
			if (userPrivileges.has(vis)) {
				// @todo filter flow lists if individual branch bot filtered
				return true
			}
		}
		return false
	}

	static makeBranchDef(branch: Branch) {
		const def: any = {}
		for (const key in branch) {
			let val = (branch as any)[key]
			if (val) {	
				const isIterable = typeof val !== 'string' && typeof val[Symbol.iterator] === 'function'
				// expand any iterable to an array
				def[key] = isIterable ? [...val] : val
			}
		}
		return def
	}

	private allBranches: BranchStatus[] = []
	private botStates: [string, GraphBotState][] = []
}

export function preprocess(data: UserStatusData) {

	for (const node of data.branches) {
		const conflicts = node.conflicts!
		if (conflicts.length === 0) {
			continue
		}

		const edges = node.edges!
		for (const edgeName of Object.keys(edges)) {
			const edge = edges[edgeName]
			if (!edge.is_blocked && edge.lastBlockage) {
				if (conflicts.find(c => c.cl === edge.lastBlockage && c.target === edge.target.toUpperCase())) {
					edge.retry_cl = edge.lastBlockage
				}
			}
		}

		if (!node.is_blocked && node.lastBlockage) {
			if (conflicts.find(c => c.cl === node.lastBlockage && !c.target)) {
				node.retry_cl = node.lastBlockage
			}
		}


		// look for unblocked edges with a current conflict
		// conflicts.find(c => c.cl === edge.last_cl)
	}
}
