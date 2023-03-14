// Copyright Epic Games, Inc. All Rights Reserved.

import { Branch, MergeAction, MergeMode } from './branch-interfaces';
import { BranchGraphInterface, PendingChange, Target, TargetInfo } from './branch-interfaces';
import { BotName, Edge, GraphAPI, Node, makeTargetName } from '../new/graph';
import { ContextualLogger } from '../common/logger';

// const NEW_STUFF = false

export function getIntegrationOwner(targetBranch: Branch, overriddenOwner?: string): string | null
export function getIntegrationOwner(pending: PendingChange): string
export function getIntegrationOwner(arg0: Branch | PendingChange, overriddenOwner?: string) {
	// order of priority for owner:

	//  1)	a) Change flagged 'manual', i.e. will create a shelf
	//			- the 'manual' flag itself is problematic, really ought to be a per target thing somehow
	//		b) shelf/stomp request
	//		c) edge reconsider
	//	2) resolver - need resolver to take priority, even over node reconsider, since recon might be from branch with
	//					multiple targets, so instigator might not even know about target with resolver
	//	3) node reconsider
	//	4) propagated/manually added tag
	//	5) author - return null here for that case

	let targetBranch: Branch | null = null
	let pending: PendingChange | null = null
	if ((arg0 as PendingChange).action) {
		pending = arg0 as PendingChange
	}
	else {
		targetBranch = arg0 as Branch
	}


	// Manual requester or edge reconsider
	if (pending && (
			pending.action.flags.has('manual') ||
			pending.change.forceCreateAShelf ||
			pending.change.userRequest == 'edge-reconsider'
		)) {
		return pending.change.owner
	}

	const branch = pending ? pending.action.branch : targetBranch
	const owner = pending ? pending.change.owner : overriddenOwner

	return branch!.resolver || owner || null
}

export function getNodeBotFullName(botname: string, branchName: string) {
	return `${botname} ${branchName}`
}

export function getNodeBotFullNameForLogging(botname: string, branchName: string) {
	return `${botname}:${branchName}`
}

// #commands in commit messages we should neuter so they don't spam (use only UPPERCASE)
const NEUTER_COMMANDS = [
	"CODEREVIEW",
	"FYI",
	"QAREVIEW",
	"RN",
	"DESIGNCHANGE",
	"REVIEW",
]

const ROBO_TAGS = [
	'ROBOMERGE-AUTHOR',
	'ROBOMERGE-BOT',
	'ROBOMERGE-COMMAND',
	'ROBOMERGE-CONFLICT',
	'ROBOMERGE-EDIGRATE',
	'ROBOMERGE-OWNER',
	'ROBOMERGE-SOURCE',
]

type ChangeFlag = 'manual' | 'null' | 'ignore' | 'disregardexcludedauthors'

// mapping of #ROBOMERGE: flags to canonical names
// use these with a pound like #ROBOMERGE: #stage
const FLAGMAP: {[name: string]: ChangeFlag} = {
	// don't merge, only shelf this change and review the owner
	manual: 'manual',
	nosubmit: 'manual',
	stage: 'manual',

	// expand any automerge targets as null merges
	'null': 'null',

	// ignore this commit, do not merge anywhere (special "#ROBOMERGE: ignore" syntax without pound also triggers this)
	ignore: 'ignore',
	deadend: 'ignore',

	// process this change even if the author is on the excluded list
	disregardexcludedauthors: 'disregardexcludedauthors',
}

const ALLOWED_RAW_FLAGS = ['null','ignore','deadend']



function parseTargetList(targetString: string) {
	return targetString.split(/[ ,]/).filter(Boolean)
}


type Token_Command = {
	bot: string
	param: string
}

type Token_Other = {
	initialWhitespace: string
	tag: string
	rest: string

	isRobo: boolean
}

// don't think this supports bare robomerge command with no args to disable
//   default flow

// result is whether we found and ignored a bare robomerge command
function tokenizeLine(line: string): Token_Command[] | Token_Other | null {

	// trim end - keep any initial whitespace
	const comp = line.replace(/\s+$/, '')

	// check for control hashes
	const match = comp.match(/^(\s*)#([-\w[\]]+)[:\s]*(.*)$/)
	if (!match) {
		return null
	}

	const initialWhitespace = match[1] 
	const tag = match[2].toUpperCase()
	const rest = match[3].trim()

	// check for any non-robomerge # tags we care about
	if (initialWhitespace || !tag.startsWith('ROBOMERGE')) {
		return {initialWhitespace, tag, rest, isRobo: false}
	}

	const bareCommand = tag === 'ROBOMERGE'
	let botName = ''
	if (!bareCommand)
	{
		const specificBotMatch = tag.match(/ROBOMERGE\[([-_a-z0-9]+)\]/i)
		if (specificBotMatch) {
			botName = specificBotMatch[1].toUpperCase()
		}
	}
	if (!bareCommand && !botName) {
		return {initialWhitespace, tag, rest, isRobo: true}
	}

	return parseTargetList(rest).map(param => ({bot: botName, param}))
}

export class DescriptionParser {
	source = ''
	owner: string | null = null
	authorTag: string | null = null

	descFinal: string[] = []

	propagatingNullMerge = false
	useDefaultFlow = true

	// arguments: string[] = []

	// per target bot arguments
	arguments: string[] = []
	otherBotArguments: Token_Command[] = []

	expandedMacros: string[] = []

	errors: string[] = []

	constructor(
		private isDefaultBot: boolean,
		private graphBotName: string,
		private cl: number,
		private aliasesUpper: string[],
		private macros: {[name: string]: string[]}

	) {
	}

	/** override anything parsed from robomerge tags */
	override(other: DescriptionParser) {

		this.useDefaultFlow = other.useDefaultFlow
		this.arguments = other.arguments
		this.expandedMacros = other.expandedMacros
		this.otherBotArguments = other.otherBotArguments

		this.errors = other.errors
	}

	append(other: DescriptionParser) {
		// ignore default flow here
		this.arguments = [...this.arguments, ...other.arguments]
		this.expandedMacros = [...this.expandedMacros, ...other.expandedMacros]
		this.otherBotArguments = [...this.otherBotArguments, ...other.otherBotArguments]

		this.errors = [...this.errors, ...other.errors]
	}

	private processNonRoboToken(token: Token_Other) {
		if (token.isRobo) {
			if (ROBO_TAGS.indexOf(token.tag) >= 0) {
				if (token.tag === 'ROBOMERGE-AUTHOR') {
					// tag intended to be purely for propagating
					this.authorTag = token.rest
				}
				else if (token.tag === 'ROBOMERGE-OWNER') {
					this.owner = token.rest.toLowerCase()
				}
				else if (token.tag === 'ROBOMERGE-SOURCE') {
					this.source = `${token.rest} via CL ${this.cl}`
				}
				else if (token.tag === 'ROBOMERGE-BOT') {
					// if matches description written in _mergeCl
					if (token.rest.startsWith(this.graphBotName + ' (')) {
						this.useDefaultFlow = false
					}
				}
			}
			else {
				// add syntax error for unknown command
				this.errors.push(`Unknown command '${token.tag}`)
			}
			return false
		}

		// check for commands to neuter
		if (NEUTER_COMMANDS.indexOf(token.tag) >= 0) {
			// neuter codereviews so they don't spam out again
			this.descFinal.push(`${token.initialWhitespace}[${token.tag}] ${token.rest}`)
			return false
		}
		if (token.tag.match(/REVIEW-(\d+)/)) {
							// remove swarm review numbers entirely (they can't be neutered and generate emails)
			if (token.rest) {
				this.descFinal.push(`${token.initialWhitespace}[REVIEW] ${token.rest}`)
			}
			return false
		}

		return true
	}

	parse(lines: string[]) {
		for (const line of lines) {
			const result = tokenizeLine(line)
			if (!result) {
				// strip beginning blanks
				if (this.descFinal.length > 0 || line.trim()) {
					if (line.indexOf('[NULL MERGE]') !== -1) {
						this.propagatingNullMerge = true
					}
					this.descFinal.push(line)
				}
				continue
			}

			const otherResult = result as Token_Other
			if (otherResult.tag) {
				if (this.processNonRoboToken(otherResult)) {
					// default behavior is to keep any hashes we don't recognize
					this.descFinal.push(line)
				}
				continue
			}

			const thisBotNames = ['', this.graphBotName, ...this.aliasesUpper, 'ALL']
			const commands: Token_Command[] = []
			for (const tokenCommand of (result as Token_Command[])) {
				const macroLines = this.macros[tokenCommand.param.toLowerCase()]
				if (macroLines) {

					// @todo: expand macros when loading config (with limited validation for preview),
					// can be recursive
					// for now, run parse line
					for (const macro of macroLines) {
						const macroResult = tokenizeLine(macro) as Token_Command[]
						for (const expanded of macroResult) {
							if (expanded.bot === undefined) {
								this.errors.push(`Invalid macro '${tokenCommand.param}'`)
								return
							}
							commands.push(expanded)
						}
					}
					this.expandedMacros.push(tokenCommand.param)
				}
				else {
					commands.push(tokenCommand)
				}
			}

			for (const command of commands) {
				if (thisBotNames.indexOf(command.bot) < 0) {
					// other bot
					this.otherBotArguments.push(command)
					continue
				}

				// ignore bare ROBOMERGE tags if we're not the default bot (should not affect default flow)
				if (!command.bot && !isFlag(command.param)) {
					if (this.isDefaultBot) {
						this.useDefaultFlow = false
					}
					else {
						continue
					}
				}
				this.arguments.push(command.param)
			}
		}
	}
}

export function parseDescriptionLines(args: {
		lines: string[],
		isDefaultBot: boolean, 
		graphBotName: string, 
		cl: number, 
		aliasesUpper: string[],
		macros: {[name: string]: string[]},
		logger: ContextualLogger
	}
	) {
	const lineParser = new DescriptionParser(args.isDefaultBot, args.graphBotName, args.cl, args.aliasesUpper, args.macros)
	lineParser.parse(args.lines)
	return lineParser
}

function isFlag(arg: string) {
	return ALLOWED_RAW_FLAGS.indexOf(arg.toLowerCase()) >= 0 || '$#'.indexOf(arg[0]) >= 0
}


/**

Algorithm:
	For each command
		* find all routes (skip some non-viable ones, like initial skip)
		* if none, error, listing reasons for discounting non-viable routes
		* otherwise add complete command list (including flags) to the further merges list of each target branch
			that is the first step of a route

	So we have a set of branches that are the first hop for routes relevant to a particular other bot
 */

type OtherBotInfo = {
	firstHopBranches: Set<Branch>
	commands: string[]
	aliasOrName: string
}

export function processOtherBotTargets(
	parsedLines: DescriptionParser,
	sourceBranch: Branch,
	ubergraph: GraphAPI,
	actions: Readonly<Readonly<MergeAction>[]>,
	errors: string[]
	) {

	const ensureNode = (bot: BotName, branch: Branch) => {
		const node = ubergraph.getNode(makeTargetName(bot, branch.upperName))

		if (!node) {
			throw new Error(`Node look-up in ubergraph failed for '${bot}:${branch.upperName}'`)
		}
		return node
	}

	const otherBotInfo = new Map<BotName, OtherBotInfo>()
	const botInfo = (bg: BranchGraphInterface) => {
		const key = bg.botname as BotName
		let info = otherBotInfo.get(key)
		if (!info) {
			info = {
				firstHopBranches: new Set<Branch>(),
				commands: [],
				aliasOrName: bg.config.aliases.length > 0 ? bg.config.aliases[0] : bg.botname
			}
			otherBotInfo.set(key, info)
		}
		return info
	}

	const sourceBotName = sourceBranch.parent.botname as BotName

	// calculate skip set first time we need it
	let skipNodes: Node[] | null = null

	for (const token of parsedLines.otherBotArguments) {
		const targetBranchGraph = ubergraph.getBranchGraph(token.bot.toUpperCase() as BotName)
		if (!targetBranchGraph) {
			errors.push(`Bot '${token.bot}' not found in ubergraph`)
			continue
		}

		const targetBotName = targetBranchGraph.botname as BotName

		const arg = token.param
		if (isFlag(arg)) {
			// no further checking for flags
			botInfo(targetBranchGraph).commands.push(arg)
			continue
		}

		const target = '!-'.indexOf(arg[0]) < 0 ? arg : arg.substr(1)
		const branch = targetBranchGraph.getBranch(target)
		if (!branch) {
			if (!targetBranchGraph.config.macros[target.toLowerCase()] &&
				targetBranchGraph.config.branchNamesToIgnore.indexOf(target.toUpperCase()) < 0) {
				errors.push(`Branch '${target}' not found in ${token.bot}`)
			}
			continue
		}

		const sourceNode = ensureNode(sourceBotName, sourceBranch)
		const targetNode = ensureNode(targetBotName, branch)

		if (sourceNode === targetNode) {
			// ignore accidental self reference: not sure if this can happen legitimately (maybe try making stricter later)
			continue
		}

		if (!skipNodes) {
			// would ideally add all non-forced edges that aren't explicit targets
			skipNodes = actions
				.filter(action => action.mergeMode === 'skip')
				.map(action => ensureNode(sourceBotName, action.branch))
		}

		const routes = ubergraph.findAllRoutesBetween(sourceNode, targetNode, [], new Set(skipNodes))
		if (routes.length === 0)
		{
			errors.push(`No route between '${sourceNode.debugName}' and '${targetNode.debugName}'`)
			continue
		}

		for (const route of routes) {
			if (route.length === 0) {
				throw new Error('empty route!')
			}

			// if first edge is outside of bot, ignore (the other bot will pick this up)
			if (route[0].bot !== sourceBotName) {
				continue
			}

			// check if route uses the target bot
			// mostly this just means at least the last edge should belong to the target bot
			let relevant = false
			for (const edge of route) {
				if (edge.bot === targetBotName) {
					relevant = true
					break
				}
			}

			if (!relevant) {
				continue
			}

			const firstBranchOnRoute = sourceBranch.parent.getBranch(route[0].targetName)
			if (!firstBranchOnRoute) {
				throw new Error(`Can't find branch ${route[0].target.debugName} (${route[0].targetName}) from ubergraph`)
			}

			botInfo(targetBranchGraph).firstHopBranches.add(firstBranchOnRoute)
		}
		botInfo(targetBranchGraph).commands.push(arg)
	}

	if (errors.length > 0 || otherBotInfo.size === 0) {
		return
	}

	// no more errors after this point: add or do not add.
	// (we allow some non-viable routes for simplicity: if we want to catch them, do so above)

	// always use bot aliases here if available, on the assumption that they are the more obfuscated/less
	// likely to trip up commit hooks

	for (const info of otherBotInfo.values()) {
		for (const firstHop of info.firstHopBranches) {
			for (const action of actions) {
				if (action.branch === firstHop) {
					// slight hack here: mergemode is always normal and branchName is all the original commands/flags unaltered
					action.furtherMerges.push({branchName: info.commands.join(' '), mergeMode: 'normal', otherBot: info.aliasOrName})
				}
			}
		}
	}
}

class RequestedIntegrations {

	readonly flags = new Set<ChangeFlag>()
	integrations: [string, MergeMode][] = []

	parse(commandArguments: Readonly<string[]>, cl: number, logger: ContextualLogger) {
		for (const arg of commandArguments) {
			const argLower = arg.toLowerCase()

			// check for 'ignore' and remap to #ignore flag
			if (ALLOWED_RAW_FLAGS.indexOf(argLower) >= 0)
			{
				this.flags.add(FLAGMAP[argLower]);
				continue;
			}

			const firstChar = arg[0]

			// see if this has a modifier
			switch (firstChar)
			{
			default:
				this.integrations.push([arg, 'normal'])
				break
			
			case '!':
				this.integrations.push([arg.substr(1), 'null'])
				break;

			case '-':
				this.integrations.push([arg.substr(1), 'skip'])
				break

			case '$':
			case '#':
				// set the flag and continue the loop
				const flagname = FLAGMAP[argLower.substr(1)]
				if (flagname) {
					this.flags.add(flagname)
				}
				else {
					logger.warn(`Ignoring unknown flag "${arg}" in CL#${cl}`)
				}
				break
			}
		}

	}
}

type ComputeTargetsResult =
	{ computeResult:
	  { merges: Map<Branch, Branch[]> | null
	  , flags: Set<ChangeFlag>
	  , targets: Map<Branch, MergeMode>
	  } | null
	, errors: string[] 
	}

function computeTargetsImpl(
	sourceBranch: Branch, 
	ubergraph: GraphAPI,
	cl: number,
	commandArguments: Readonly<string[]>,
	defaultTargets: Readonly<string[]>, 
	logger: ContextualLogger
): ComputeTargetsResult {

	// list of branch names and merge mode (default targets first, so merge mode gets overwritten if added explicitly)

	const ri = new RequestedIntegrations
	ri.parse(commandArguments, cl, logger)

	if (ri.flags.has('null')) {
		for (const branchName of sourceBranch.forceFlowTo) {
			ri.integrations.push([branchName, 'null'])
		}
	}

	const branchGraph = sourceBranch.parent
	const requestedMergesArray = [
		...defaultTargets.map(name => [name, 'normal'] as [string, MergeMode]),
		...ri.integrations.filter(([name, _]) => branchGraph.config.branchNamesToIgnore.indexOf(name.toUpperCase()) < 0)
	]

	// compute the targets map
	const skipBranches = new Set<Branch>()
	const targets = new Map<Branch, MergeMode>()
	const errors: string[] = []

	// process parsed targets
	for (const [targetName, mergeMode] of requestedMergesArray) {
		// make sure the target exists
		const targetBranch = branchGraph.getBranch(targetName)
		if (!targetBranch) {
			errors.push(`Unable to find branch "${targetName}"`)
			continue
		}

		if (targetBranch === sourceBranch) {
			// ignore merge to self to prevent indirect target loops
			continue
		}

		if (mergeMode === 'skip') {
			skipBranches.add(targetBranch)
		}

// note: this allows multiple mentions of same branch, with flag of last mention taking priority. Could be an error instead
		targets.set(targetBranch, mergeMode)
	}

	const botname = sourceBranch.parent.botname as BotName

	const sourceNode = ubergraph.getNode(makeTargetName(botname, sourceBranch.upperName))
	if (!sourceNode) {
		throw new Error(`Source node ${sourceBranch.upperName} not found in ubergraph`)
	}

// note: this allows multiple mentions of same branch, with flag of last mention taking priority. Could be an error instead
	const requestedMerges = new Map<string, MergeMode>(requestedMergesArray)

	const requestedTargets = new Map<Node, MergeMode>()
	for (const [targetName, mergeMode] of requestedMerges) {
		const node = ubergraph.getNode(makeTargetName(botname, targetName))
		if (node) {
			if (node !== sourceNode) {
				requestedTargets.set(node, mergeMode)
			}
		}
		else {
			errors.push(`Target node ${targetName} not found in ubergraph`)
		}
	}

	if (errors.length > 0) {
		return { computeResult: null, errors }
	}

	const {status, integrations, unreachable} = ubergraph.computeImplicitTargets(sourceNode, requestedTargets, [botname])

	// send errors if we had any
	if (status !== 'succeeded') {
		const sourceNodeDebugName = sourceNode.debugName
		const errors = unreachable ? unreachable.map((node: Node) => 
					`Branch '${node.debugName}' is not reachable from '${sourceNodeDebugName}'`) : ['need more info!']

		return { computeResult: null, errors }
	}

	// report targets
	if (!integrations || integrations.size === 0) {
		return {computeResult: null, errors: []}
	}

	// handle multiple routes to skip targets
	//	
	const skipTargets: Branch[] = [...requestedMerges]
		.filter(([_, mergeMode]) => mergeMode === 'skip')
		.map(([branchName, _]) => branchGraph.getBranch(branchName)!)


	const merges = new Map<Branch, Branch[]>()
	for (const [initialEdge, furtherEdges] of integrations) {

		const edgeTargetBranch = (e: Edge) => {
			const branch = branchGraph.getBranch(e.targetName)
			if (!branch) {
				throw new Error('Unknown branch ' + e.targetName)
			}
			return branch
		}

		const targetBranch = edgeTargetBranch(initialEdge)
		const furtherEdgeBranches = furtherEdges.map(edgeTargetBranch)
		for (const skipTarget of skipTargets) {
			if (furtherEdgeBranches.indexOf(skipTarget) < 0
				&&

			// make sure skipTarget is present directly if reachable
				[...furtherEdgeBranches, targetBranch]
					.map(b => b.forcedDownstream)
					.filter(ds => (ds || []).map(b => branchGraph.getBranch(b)).indexOf(skipTarget) >= 0)
					.length > 0
				) {

				logger.info(`adding skip of ${skipTarget.name} to onward integrations for ` +
					`${initialEdge.source.debugName}->${initialEdge.target.debugName} due to ... todo`)
				furtherEdgeBranches.push(skipTarget)
			}
		}
		merges.set(targetBranch, furtherEdgeBranches)
	}

	return {computeResult: {merges, targets, flags: ri.flags}, errors}
}


export function computeTargets(
	sourceBranch: Readonly<Branch>,
	ubergraph: GraphAPI,
	cl: number,
	info: TargetInfo,
	commandArguments: Readonly<string[]>,
	defaultTargets: Readonly<string[]>,
	logger: ContextualLogger,
	optTargetBranch?: Branch | null
) : [MergeAction[], Set<ChangeFlag>] | null {
	const { computeResult, errors } = computeTargetsImpl(sourceBranch, ubergraph, cl, commandArguments, defaultTargets, logger)

	if (errors.length > 0) {
		info.errors = errors
		return null
	}

	if (!computeResult) {
		return null
	}

	let { merges, flags, targets } = computeResult
	if (!merges) {
		// shouldn't get here - there will have been errors
		return null
	}

	// Now that all targets from original changelist description are computed, compare to requested target branch, if applicable
	if (optTargetBranch) {
		// Ensure optTargetBranch is a (possibly indirect) target of this change
		if (!merges.has(optTargetBranch)) {
			errors.push(`Requested target branch ${optTargetBranch.name} not a valid target for this changelist`)
		}
		// Ensure we provide informative error around requesting to remerge when it should be null or skipped (though we shouldn't get in this state)
		else {
			const targetMergeMode = targets.get(optTargetBranch) || 'normal'
			if (targetMergeMode === 'null' || targetMergeMode === 'skip') {
				errors.push(`Invalid request to merge to ${optTargetBranch.name}: Changelist originally specified '${targetMergeMode}' merge for this branch`)
			}
			else {
				// Success!
				// Override targets with ONLY optTargetBranch
				merges = new Map([[optTargetBranch, merges.get(optTargetBranch)!]])
			}
		}
	}

	// compute final merge modes of all targets
	let mergeActions: MergeAction[] = []
	const allDownstream = new Set<Branch>()

	for (const [direct, indirectTargets] of merges.entries()) {
		const mergeMode : MergeMode = targets.get(direct) || 'normal'
		if (mergeMode === 'skip') {
			continue
		}
		allDownstream.add(direct)

		const furtherMerges: Target[] = []
		for (const branch of indirectTargets) {
			furtherMerges.push({branchName: branch.name, mergeMode: targets.get(branch) || 'normal'})
			allDownstream.add(branch)
		}

		mergeActions.push({branch: direct, mergeMode, furtherMerges, flags})
	}

	// If forceStompChanges, add the target as a stomp if it isn't a flagged target
	if (info.forceStompChanges) {
		mergeActions = mergeActions
			.filter(action => action.mergeMode === 'normal')
			.map(action => ({...action, mergeMode: 'clobber'}))
	}

	const branchGraph = sourceBranch.parent

	// add indirect forced branches to allDownstream
	for (const branch of [...allDownstream]) {
		branchGraph._computeReachableFrom(allDownstream, 'forceFlowTo', branch)
	}

	// send errors if we had any
	if (errors.length > 0) {
		info.errors = errors
	}

	// provide info on all targets that should eventually be merged too
	if (allDownstream && allDownstream.size !== 0) {
		info.allDownstream = [...allDownstream]
	}

	return [mergeActions, flags]
}

const colors = require('colors')
colors.enable()
colors.setTheme(require('colors/themes/generic-logging.js'))

export function runTests(parentLogger: ContextualLogger) {
	const logger = parentLogger.createChild('Targets')

	const defaultArgs = {
		isDefaultBot: true,
		graphBotName: 'test',
		cl: 1,
		aliasesUpper: [],
		macros: {'m': ['#robomerge X, Y'], 'otherbot': ['#robomerge[A] B']},
		logger
	}

	let fails = 0
	let assertions = 0
	let testName = ''
	const assert = (b: boolean, msg: string, details: string[] = []) => {
		if (!b) {
			logger.error(`"${testName}" failed: ${colors.error(msg)}` + 
				(details || []).map(s => `\t${s}\n`).join(''))
			++fails
		}
		++assertions
	}

	const parserAssert = (parser: DescriptionParser,
							descLines: number, commands: number, otherBot: number, macros: number, errors: number,
							additional: boolean) => {
		assert(
			parser.descFinal.length === descLines &&
			parser.arguments.length === commands &&
			parser.otherBotArguments.length === otherBot &&
			parser.expandedMacros.length === macros &&
			parser.errors.length === errors &&
			additional, 'Counts mismatch:', [
				`descLines: got ${parser.descFinal.length}, expected ${descLines}`,
				`commands: got ${parser.arguments.length}, expected ${commands}`,
				`otherBot: got ${parser.otherBotArguments.length}, expected ${otherBot}`,
				`macros: got ${parser.expandedMacros.length}, expected ${macros}`,
				`errors: got ${parser.errors.length}, expected ${errors}`
			]
		)
	}

	(() => {
		testName = 'no commands'
		const parser = parseDescriptionLines({lines: ['no command'], ...defaultArgs})
		parserAssert(parser, 1, 0, 0, 0, 0, parser.descFinal[0] === 'no command')
	})();

	(() => {
		testName = 'multiple targets'
		const parser = parseDescriptionLines({lines: ['#robomerge A', '#robomerge B'], ...defaultArgs})
		parserAssert(parser, 0, 2, 0, 0, 0, true)
	})();

	(() => {
		testName = 'comma separated targets'
		const parser = parseDescriptionLines({lines: ['#robomerge A, B'], ...defaultArgs})
		parserAssert(parser, 0, 2, 0, 0, 0, true)
	})();

	(() => {
		testName = 'space separated targets'
		const parser = parseDescriptionLines({lines: ['#robomerge A B'], ...defaultArgs})
		parserAssert(parser, 0, 2, 0, 0, 0, true)
	})();

	(() => {
		testName = 'macro'
		const parser = parseDescriptionLines({lines: ['#robomerge M'], ...defaultArgs})
		parserAssert(parser, 0, 2, 0, 1, 0, true)
	})();

	(() => {
		testName = 'other bot'
		const parser = parseDescriptionLines({lines: ['#robomerge[B] A'], ...defaultArgs})
		parserAssert(parser, 0, 0, 1, 0, 0, true)
	})();

	(() => {
		testName = 'all bots'
		const parser = parseDescriptionLines({lines: ['#robomerge[ALL] A'], ...defaultArgs})
		parserAssert(parser, 0, 1, 0, 0, 0, true)
	})();

	(() => {
		testName = 'other bot macro'
		const parser = parseDescriptionLines({lines: ['#robomerge otherbot'], ...defaultArgs})
		parserAssert(parser, 0, 0, 1, 1, 0, true)
	})();

	(() => {
		testName = 'unknown tag'
		const parser = parseDescriptionLines({lines: ['#unknown blah'], ...defaultArgs})
		parserAssert(parser, 1, 0, 0, 0, 0, parser.descFinal[0] === '#unknown blah')
	})();

	(() => {
		testName = 'sanitize known tag'
		const parser = parseDescriptionLines({lines: ['#fyi blah'], ...defaultArgs})
		assert(parser.descFinal.length === 1 &&
			parser.descFinal[0].indexOf('#') < 0 && parser.descFinal[0].toLowerCase().indexOf('fyi') >= 0,
			'known tag sanitized')
	})();

	(() => {
		testName = 'sanitize swarm tag'
		const parser = parseDescriptionLines({lines: ['#review-123 blah'], ...defaultArgs})
		assert(parser.descFinal.length === 1 &&
			parser.descFinal[0].indexOf('#') < 0 && parser.descFinal[0].toLowerCase().indexOf('blah') >= 0,
			'swarm tag sanitized')
	})();

	if (fails === 0) {
		logger.info(colors.info(`Targets tests succeeded (${assertions} assertions)`))
	}

	return fails
}