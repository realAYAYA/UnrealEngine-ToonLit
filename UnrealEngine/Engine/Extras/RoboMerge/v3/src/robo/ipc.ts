// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger, isNpmLogLevel, NpmLogLevelCompare, NpmLogLevelValues } from '../common/logger';
import { getRunningPerforceCommands } from '../common/perforce';
import { Trace } from '../new/graph';
import { IPCControls, EdgeBotInterface, NodeBotInterface } from './bot-interfaces';
import { OperationResult } from './branch-interfaces';
import { GraphBot } from './graphbot';
import { RoboMerge } from './robo';
import { roboAnalytics } from './roboanalytics';
import { Status } from './status';
import { getPreview } from './preview'
import * as p4util from '../common/p4util';

// Reminder that RoboMerge variable is null in this file! ${RoboMerge}

const START_TIME = new Date();

export interface Message {
	name: string
	args?: any[]
	userPrivileges?: string[]

	cbid?: string
}

// roboserver.ts -- getQueryFromSecure()
type Query = {[key: string]: string};

export type OperationReturnType = {
	statusCode: number
	message: string // Goal: to provide meaningful error messaging to the end user

	// Open Ended return
	data?: any
}

const OPERATION_SUCCESS: OperationReturnType = {
	statusCode: 200,
	message: 'OK'
}

export class IPC {
	private readonly ipcLogger: ContextualLogger = new ContextualLogger('IPC')

	constructor(private robo: RoboMerge) {
	}

	async handle(msg: Message): Promise<OperationReturnType> {
		switch (msg.name) {
			case 'getBranches': return this.getBranches()
			case 'getBranch': return this.getBranch(msg.args![0] as string, msg.args![1] as string)
			case 'sendTestDirectMessage': return this.sendTestDirectMessage(msg.args![0] as string)
			case 'setVerbose': return this.setVerbose(!!msg.args![0])
			case 'setVerbosity': return this.setVerbosity(msg.args![0] as string)
			case 'getp4tasks': return { ...OPERATION_SUCCESS, data: getRunningPerforceCommands() } 
			case 'getWorkspaces': return this.getWorkspaces(msg.args![0] as string)
			case 'getPersistence': return this.getPersistence(msg.args![0] as string)
			case 'traceRoute': return this.traceRoute(msg.args![0] as Query)
			case 'dumpGraph': return this.dumpGraph()
			case 'getIsRunning': return this.isRunning(msg.args![0] as string)
			case 'restartBot': return this.restartBot(msg.args![0] as string, msg.args![1] as string)
			case 'crashGraphBot': return this.crashGraphBot(msg.args![0] as string, msg.args![1] as string)
			case 'crashAPI': throw new Error(`API Crash requested by ${msg.args![0]}`)
			case 'forceBranchmapUpdate': return this.forceBranchmapUpdate(msg.args![0] as string)
			case 'preview': return this.preview(msg.args![0] as string, msg.args![1] as (string|undefined))

			case 'doNodeOp': return this.doOperation(
				msg.args![0] as string,
				msg.args![1] as string, 
				msg.args![2] as string,
				msg.args![3] as Query)
			
			case 'doEdgeOp': return this.doOperation(
				msg.args![0] as string, // botname
				msg.args![1] as string, // nodename
				msg.args![3] as string, // edge operation
				msg.args![4] as Query,  // Query params
				msg.args![2] as string  // edgename
			)

			default:
				return {statusCode: 500, message: `Did not understand msg "${msg.name}"`};
		}
	}

	private getBranches() {
		const status = new Status(START_TIME, this.robo.VERSION_STRING, this.ipcLogger)
		for (let graphBot of this.robo.graphBots.values()) {
			graphBot.applyStatus(status)
			for (let branch of graphBot.branchGraph.branches) {
				if (branch.isMonitored) {
					status.addBranch(branch)
				}
			}
		}

		return { ...OPERATION_SUCCESS, data: status }
	}

	private getBranch(botname: string, branchname: string) {
		const status = new Status(START_TIME, this.robo.VERSION_STRING, this.ipcLogger)
		botname = botname.toUpperCase()
		branchname = branchname.toUpperCase()

		const graphBot = this.robo.graphBots.get(botname)
		
		if (graphBot) {
			const branch = graphBot.branchGraph.getBranch(branchname)
			if (branch) {
				status.addBranch(branch)
			}
		}

		return { ...OPERATION_SUCCESS, data: status }
	}

	private sendTestDirectMessage(user: string): OperationReturnType {
		let msgGraphBot : GraphBot | undefined
		for (let [graphBotKey, graphBot] of this.robo.graphBots) {
			if (graphBotKey === "FORTNITE" || graphBotKey === "TEST" || graphBotKey === "ROBOMERGEQA1") {
				msgGraphBot = graphBot
				break
			}
			
		}

		if (!msgGraphBot) {
			return {
				statusCode: 500,
				message: "Unable to find appropriate GraphBot for sending test messages." +
					" Check that you're performing this operation on the correct Robomerge instance."
				}
		}
		
		try
		{ 
			msgGraphBot.sendTestMessage(user)
			return {statusCode: 200, message: `Sent message to "${user}"`}
		} 
		catch {
			return {statusCode: 500, message: `Unable to send message to "${user}"`}
		}
	}

	private async getWorkspaces(username: string): Promise<OperationReturnType> {
		const workspaces = await p4util.getWorkspacesForUser(this.robo.p4, username)
		
		if (!workspaces) {
			return {
				statusCode: 500,
				message: 'Could not retrieve workspaces for your user.'
			}
		}

		return { ...OPERATION_SUCCESS, data: workspaces } 
	}

	private getPersistence(botname: string): OperationReturnType {
		const dump = this.robo.dumpSettingsForBot(botname)
		if (dump) {
			return { ...OPERATION_SUCCESS, data: dump } 
		}
		return {statusCode: 400, message: `Unknown bot '${botname}'`}
	}

	private async traceRoute(query: Query): Promise<OperationReturnType> {

		const cl = parseInt(query.cl)
		if (isNaN(cl)) {
			return {statusCode: 400, message: 'Invalid CL parameter: ' + query.cl}
		}

		const tracer = new Trace(this.robo.graph.graph, this.ipcLogger)

		let success = false
		try {
			const routeOrError = await tracer.traceRoute(cl, query.from, query.to)
			if (typeof routeOrError === 'string')
				return { statusCode: 400, message: routeOrError } 

			const result: any[] = []
			for (const edge of routeOrError) {
				const bot = edge.sourceAnnotation as NodeBotInterface
				result.push({
					name: bot.branchGraph.botname + ':' + bot.branch.name,
					stream: edge.source.stream,
					lastCl: bot.lastCl
				})
			}

			success = true
			return { ...OPERATION_SUCCESS, data: result } 
		}
		catch (err) {
			return {statusCode: 400, message: err.toString()}
		}
		finally {
			roboAnalytics!.updateActivityCounters(success ? {traces: 1} : {failedTraces: 1})
		}
	}

	private dumpGraph(): OperationReturnType {
		return { ...OPERATION_SUCCESS, data: this.robo.graph.graph.dump() } 
	}

	private isRunning(botname: string): OperationReturnType {
		const graphBot = this.robo.graphBots.get(botname)
		if (!graphBot) {
			return { statusCode: 400, message: `Unknown bot '${botname}'` } 
		}

		return { ...OPERATION_SUCCESS, data: graphBot.isRunningBots() } 
	}

	private restartBot(botname: string, who: string): OperationReturnType {
		const graphBot = this.robo.graphBots.get(botname)
		if (!graphBot) {
			return {statusCode: 400, message: `Unknown bot '${botname}'`}
		}

		if (graphBot.isRunningBots()) {
			return {statusCode: 400, message: `'${botname}' already running`}
		}

		graphBot.restartBots(who)
		return OPERATION_SUCCESS
	}

	private crashGraphBot(botname: string, who: string): OperationReturnType {
		const graphBot = this.robo.graphBots.get(botname)
		if (!graphBot) {
			return {statusCode: 400, message: `Unknown bot '${botname}'`}
		}

		graphBot.danger_crashGraphBot(who)
		return OPERATION_SUCCESS
	}

	private forceBranchmapUpdate(botname: string): OperationReturnType {
		const graphBot = this.robo.graphBots.get(botname.toUpperCase())
		if (!graphBot) {
			return {statusCode: 400, message: `Unknown bot '${botname}'`}
		}

		graphBot.forceBranchmapUpdate()
		return OPERATION_SUCCESS
	}

	private async preview(clStr: string, botname?: string): Promise<OperationReturnType> {
		const cl = parseInt(clStr)
		if (isNaN(cl)) {
			throw new Error(`Failed to parse alleged CL '${clStr}'`)
		}

		try {
			return {
				statusCode: 200,
				message: JSON.stringify(await getPreview(cl, botname))
			}
		}
		catch (err) {
			return {
				statusCode: 400,
				message: err.toString()
			}
		}
	}

	private setVerbose(enabled: boolean): OperationReturnType {
		if (enabled) {
			return this.setVerbosity("verbose")
		}

		return this.setVerbosity(ContextualLogger.initLogLevel)
	}

	private setVerbosity(level: string): OperationReturnType {
		if (!isNpmLogLevel(level)) {
			return { 
				statusCode: 400, 
				message: `${level} is not an appropriate value. Try one of these values: ` +
												Object.keys(NpmLogLevelValues).join(', ')
			}
		}

		if (level === ContextualLogger.getLogLevel()) {
			return { 
				statusCode: 204, 
				message: `Logger is already set to ${level}`
			}
		}
		
		const previousLevel = ContextualLogger.setLogLevel(level, this.ipcLogger)
		if (NpmLogLevelCompare(level, "verbose") >= 0) {
			for (let cmd_rec of getRunningPerforceCommands()) {
				this.ipcLogger.verbose(`Running: ${cmd_rec.cmd} since ${cmd_rec.start.toLocaleDateString()}`)
			}
		}
		
		return { statusCode: 200, message: `${level.toLowerCase()} logging enabled (was: ${previousLevel}).`}
	}

	private async doOperation(botname: string, nodeName: string, operation: string, query: Query, edgeName?: string)
		: Promise<OperationReturnType> {
		if (!query.who) {
			// probably ought to be a 500 now
			return {statusCode: 400, message: `Attempt to run operation ${operation}: user name must be supplied.`}
		}
		
		// find the bot
		const map = this.robo.getBranchGraph(botname)

		if (!map) {
			return {statusCode: 404, message: 'Could not find bot ' + botname}
		}

		let branch = map.getBranch(nodeName)
		if (!branch) {
			return {statusCode: 404, message: 'Could not find node ' + nodeName}
		}

		if (!branch.isMonitored) {
			return {statusCode: 400, message: `Branch ${branch.name} is not being monitored.`}
		}

		const bot = branch.bot!

		// Some operations are the same for NodeBots and EdgeBots -- we can cheat a little here by
		// creating a general variable
		let generalOpTarget: IPCControls = bot
		let edge: EdgeBotInterface | null = null
		if (edgeName) {
			edge = bot.getImmediateEdge(edgeName)
			const edgeIPC = edge && edge.ipcControls
			if (!edgeIPC) {
				return {statusCode: 400, message: `Node ${nodeName} does not have edge ${edgeName}`}
			}
			generalOpTarget = edgeIPC
		}
		
		let cl = NaN
		let reason : string
		let operationResult : OperationResult
		let target: string
		switch (operation) {
		case 'pause':	
			generalOpTarget.pause(query.msg, query.who)
			return OPERATION_SUCCESS

		case 'unpause':
			generalOpTarget.unpause(query.who)
			return OPERATION_SUCCESS

		case 'retry':
			reason = `retry request by ${query.who}`
			generalOpTarget.unblock(reason)
			return OPERATION_SUCCESS

		case 'set_last_cl':
			cl = parseInt(query.cl)

			if (isNaN(cl)) {
				return {statusCode: 400, message: 'Invalid CL parameter: ' + cl}
			}

			let prevCl = generalOpTarget.forceSetLastClWithContext(cl, query.who, query.reason, !!query.unblock && query.unblock === 'true')

			this.ipcLogger.info(`Forcing last CL=${cl} on ${botname} : ${branch.name} (was CL ${prevCl}), ` +
														`requested by ${query.who} (Reason: ${query.reason})`)
			return OPERATION_SUCCESS

		case 'reconsider':
			const argMatch = query.cl.match(/(\d+)\s*(.*)/)
			if (argMatch) {
				cl = parseInt(argMatch[1])
			}
			if (isNaN(cl)) {
				return {statusCode: 400, message: 'Invalid CL parameter: ' + query.cl}
			}
			try {
				generalOpTarget.reconsider(query.who, cl, {commandOverride: argMatch![2]})
			}
			catch (err) {
				throw err
			}
			return OPERATION_SUCCESS
		
		case 'acknowledge':
			cl = parseInt(query.cl)

			if (isNaN(cl)) {
				return {statusCode: 400, message: 'Invalid CL parameter: ' + cl}
			}

			operationResult = generalOpTarget.acknowledge(query.who, cl)
			if (operationResult.success) {
				return OPERATION_SUCCESS
			} else {
				return { statusCode: 500, message: operationResult.message }
			}
			
		case 'unacknowledge':
			cl = parseInt(query.cl)

			if (isNaN(cl)) {
				return {statusCode: 400, message: 'Invalid CL parameter: ' + cl}
			}

			operationResult = generalOpTarget.unacknowledge(cl)
			if (operationResult.success) {
				return OPERATION_SUCCESS
			} 
			return { statusCode: 500, message: operationResult.message }

		// Requires: cl, workspace, target
		case 'create_shelf':
			cl = parseInt(query.cl)

			if (isNaN(cl)) {
				return { statusCode: 400, message: 'Invalid CL parameter: ' + cl }
			}
			const workspace : string = query.workspace
			if (!workspace) {
				return { statusCode: 400, message: 'Workspace parameter is required' }
			}

			target = query.target
			if (!target) {
				return { statusCode: 400, message: 'Target parameter is required' }
			}

			if (bot.getNumConflicts() === 0) {
				return { statusCode: 400, message: 'No conflicts found.' }
			}

			// Attempt to create shelf
			operationResult = bot.createShelf(query.who, workspace, cl, target)
			if (operationResult.success) {
				return { statusCode: 200, message: 'ok' }
			}
			return { statusCode: 500, message: operationResult.message }
			

		// Requires: cl, target
		case 'verifystomp':
			cl = parseInt(query.cl)
			if (isNaN(cl)) {
				return { statusCode: 400, message: 'Invalid CL parameter: ' + cl }
			}

			target = query.target
			if (!target) {
				return { statusCode: 400, message: 'Target parameter is required' }
			}

			// Attempt to stomp changes
			let stompVerification = await bot.verifyStomp(cl, target)
			if (stompVerification.success) {
				return { 
					statusCode: 200,
					message: JSON.stringify({
						message: stompVerification.message,
						nonBinaryFilesResolved: stompVerification.nonBinaryFilesResolved,
						remainingAllBinary: stompVerification.remainingAllBinary,
						branchOrDeleteResolveRequired: stompVerification.branchOrDeleteResolveRequired,
						files: stompVerification.svFiles,
						validRequest: stompVerification.validRequest
					} )
				}
			}

			this.ipcLogger.error('Error verifying stomp: ' + stompVerification.message)
			return { statusCode: 500, message: stompVerification.message }

		case 'stompchanges':
			cl = parseInt(query.cl)
			if (isNaN(cl)) {
				return { statusCode: 400, message: 'Invalid CL parameter: ' + cl }
			}

			target = query.target
			if (!target) {
				return { statusCode: 400, message: 'Target parameter is required' }
			}

			// Attempt to stomp changes
			operationResult = await bot.stompChanges(query.who, cl, target)
			if (operationResult.success) {
				return { statusCode: 200, message: operationResult.message }
			}
			this.ipcLogger.error('Error processing stomp: ' + operationResult.message)
			return { statusCode: 500, message: operationResult.message }

		case 'bypassgatewindow':
			const sense = query.sense.toLowerCase().startsWith('t');
			const prefix = sense ? 'en' : 'dis'
			this.ipcLogger.info(`${query.who} ${prefix}abled gate window bypass on ${nodeName}->${edgeName}`)
			edge!.bypassGateWindow(sense)
			return { statusCode: 200, message: 'ok' }
		}

		return {statusCode: 404, message: 'Unrecognized node op: ' + operation}
	}
}
