// Copyright Epic Games, Inc. All Rights Reserved.
import { execFile, ExecFileOptions } from 'child_process';
import * as path from 'path';
// should really separate out RM-specific operations, but no real gain at the moment
import { postToRobomergeAlerts } from '../robo/notifications';
import { roboAnalytics } from '../robo/roboanalytics';
import { ContextualLogger, NpmLogLevel } from './logger';
import { VersionReader } from './version';

import * as ztag from './ztag'

const p4exe = process.platform === 'win32' ? 'p4.exe' : 'p4';
const RETRY_ERROR_MESSAGES = [
	'socket: Connection reset by peer',
	'socket: Broken pipe'
]

const INTEGRATION_FAILURE_REGEXES: [RegExp, string][] = [
	[/^(.*[\\\/])(.*) - can't \w+ exclusive file already opened$/, 'partial_integrate'],
	[/Move\/delete\(s\) must be integrated along with matching move\/add\(s\)/, 'split_move'],
]

export const EXCLUSIVE_CHECKOUT_REGEX = INTEGRATION_FAILURE_REGEXES[0][0]

const changeResultExpectedShape: ztag.ParseOptions = {
	expected: {change: 'integer', client: 'string', user: 'string', desc: 'string', time: 'integer', status: 'string', changeType: 'string'},
	optional: {oldChange: 'integer'}
}

const ztag_group_rex = /\n\n\.\.\.\s/;
const ztag_field_rex = /(?:\n|^)\.\.\.\s/;
const newline_rex = /\r\n|\n|\r/g;
const integer_rex = /^[1-9][0-9]*\s*$/;

export interface BranchSpec {
	name: string;
	reverse: boolean;
}

export interface IntegrationSource {
	branchspec?: BranchSpec;
	changelist: number;
	depot: string;
	path_from: string;
	stream?: string;
}

export interface IntegrationTarget {
	depot: string;
	path_to: string;
	stream?: string;
}

export interface OpenedFileRecord {
	depotFile: string
	clientFile: string
	rev: string
	haveRev: string
	action: string
	type: string
	user: string

	movedFile?: string
}

interface DescribeEntry {
	depotFile: string
	action: string
	rev: number
	type: string // Perforce filetypes -- 'text', 'binary', 'binary+l', etc.
}

export interface DescribeResult {
	user: string
	status: string
	description: string
	entries: DescribeEntry[]
	date: Date | null
}

/**
	Example tracing output

	2020/09/12 19:40:33 576199000 pid 23850: <-  NetTcpTransport 10.200.65.101:63841 closing 10.200.21.246:1667
 */

const TRACE_OUTPUT_REGEX = /(\d\d\d\d\/\d\d\/\d\d \d\d:\d\d:\d\d \d+) pid (\d+): /

function parseTrace(response: string): [string, string] {
	const trace: string[] = []
	const nonTrace: string[] = []
	for (const line of response.split('\n')) {
		(line.match(TRACE_OUTPUT_REGEX) ? trace : nonTrace).push(line)
	}
	return [trace.join('\n'), nonTrace.join('\n')]
}


// parse the perforce tagged output format into an array of objects
// TODO: probably should switch this to scrape Python dictionary format (-G) since ztag is super inconsistent with multiline fields
export function parseZTag(buffer: string, multiLine?: boolean) {
	let output = [];

	// check for error lines ahead of the first ztag field
	let ztag_start = buffer.indexOf('...');
	if (ztag_start > 0) {
		// split the start off the buffer, then split it into newlines
		let preamble = buffer.substr(0, ztag_start).trim();
		output.push(preamble.split(newline_rex));
		buffer = buffer.substr(ztag_start);
	}
	else if (ztag_start < 0) {
		let preamble = buffer.trim();
		if (preamble.length > 0)
			output.push(preamble.split(newline_rex));
		buffer = "";
	}

	// split into groups
	let groups = buffer.split(ztag_group_rex);
	for (let i = 0; i < groups.length; ++i) {
		// make an object for each group
		let group: any = {};
		let text: string[] = [];

		// split fields
		let pairs = groups[i].split(ztag_field_rex);
		if (pairs[0] === "") {
			pairs.shift();
		}

		let setValue = false;
		for (let j = 0; j < pairs.length; ++j) {
			// each field is a key-value pair
			let pair = pairs[j].trim();
			if (pair === "")
				continue;

			let key, value;
			let s = pair.indexOf(' ');
			if (s >= 0) {
				key = pair.substr(0, s);
				value = pair.substr(s + 1);
				if (value.indexOf('\n') >= 0 && !multiLine) {
					let lines = value.split('\n');
					value = lines.shift();
					text = text.concat(lines.filter((str) => { return str !== ""; }));
				}

				// if it's an integer, convert
				if (value!.match(integer_rex))
					value = parseInt(value!);
			}
			else {
				key = pair;
				value = true;
			}

			// set it on the group
			group[key] = value;
			setValue = true;
		}

		// if we have no values, omit this output
		if (!setValue)
			continue;

		// set to output
		output.push(group);

		// if we have raw text, add it at the end
		if (text.length > 0)
			output.push(text);
	}

	return output;
}

function readObjectListFromZtagOutput(obj: any, keysToLookFo: string[], logger: ContextualLogger) {
	const result: any = []

	for (let index = 0; ; ++index) {
		// must be higher than file limit!
		if (index === 50000) {
			logger.warn('Parse -ztag WARNING: broke out after 50000 items')
			break
		}

		const item: any = {}
		for (const lookFor of keysToLookFo) {
			const key = lookFor + index
			if (obj[key]) {
				item[lookFor] = obj[key]
			}
		}
		if (Object.keys(item).length === 0)
			break

		result.push(item)
	}
	return result
}

class CommandRecord {
	constructor(public cmd: string, public start: Date = new Date()) {
	}
}


export interface Change {
	// from Perforce (ztag)
	change: number;
	client: string;
	user: string;
	// path?: string;
	desc: string;
	// status?: string;
	shelved?: number;

	// also from Perforce, but maybe not useful
	changeType?: string;
	time?: number;

	// hacked in
	isUserRequest?: boolean;
	ignoreExcludedAuthors?: boolean
	forceCreateAShelf?: boolean
	sendNoShelfEmail?: boolean // Used for requesting stomps for internal RM usage, such as stomp changes
	commandOverride?: string
	accumulateCommandOverride?: boolean

	// Stomp Changes support
	forceStompChanges?: boolean
	additionalDescriptionText?: string // Used for '#fyi' string to notify stomped authors
}


// After TS conversion, tidy up workspace usage
// Workspace and RoboWorkspace are fudged parameter types for specifying a workspace
// ClientSpec is a (partial) Perforce definition of a workspace

export interface Workspace {
	name: string,
	directory: string
}

// temporary fudging of workspace string used by main Robo code
export type RoboWorkspace = Workspace | string | null;

export interface ClientSpec {
	client: string;
	Stream?: string
}

export type StreamSpec = {
	stream: string
	name: string
	parent: string
	desc: string
}

export type StreamSpecs = Map<string, StreamSpec>

export function isExecP4Error(err: any): err is [Error, string] {
	return Array.isArray(err) && err.length === 2 && err[0] instanceof Error && typeof err[1] === "string"
}

interface ExecOpts {
	stdin?: string
	quiet?: boolean
	noCwd?: boolean
	noUsername?: boolean
	numRetries?: number
	trace?: boolean
	edgeServerAddress?: string
}

interface ExecZtagOpts extends ExecOpts {
	multiline?: boolean;
}

export interface EditOwnerOpts {
	newWorkspace?: string;
	changeSubmitted?: boolean;
	edgeServerAddress?: string
}

export interface ConflictedResolveNFile {
	clientFile: string // Workspace path on local disk
	targetDepotFile?: string // Target Depot path in P4 Depot
	fromFile: string // Source Depot path in P4 Depot
	startFromRev: number
	endFromRev: number
	resolveType: string // e.g. 'content', 'branch'
	resolveFlag: string // 'c' ?
	contentResolveType?: string  // e.g. '3waytext', '2wayraw' -- not applicable for delete/branch merges
	branchOrDeleteResolveRequired?: boolean
}

export class ResolveResult {
	private rResultLogger: ContextualLogger
	private resolveOutput: any[] | null
	private dashNOutput: any[]
	private resolveDashNRan: boolean
	private remainingConflicts: ConflictedResolveNFile[] = []
	private hasUnknownConflicts = false
	private parsingError: string = ""
	private successfullyParsedRemainingConflicts = false

	constructor(_resolveOutput: any[] | null, resolveDashNOutput: string | any[], parentLogger: ContextualLogger) {
		this.rResultLogger = parentLogger.createChild('ResolveResult')
		this.resolveOutput = _resolveOutput

		// Non ztag resolve
		if (typeof resolveDashNOutput === 'string') {
			this.dashNOutput = [ resolveDashNOutput ]
			this.hasUnknownConflicts = resolveDashNOutput !== 'success'
			this.resolveDashNRan = false
		}
		// ZTag format
		else {
			this.dashNOutput = resolveDashNOutput
			this.resolveDashNRan = true
			if (this.dashNOutput.length > 0 && this.dashNOutput[0] !== 'success') {
				this.parseConflictsFromDashNOutput()
			}
		}
	}

	/* resolve -N will display the remaining files to have conflicts. It will display the following information in this order:
	 	... clientFile
		... fromFile 
		... startFromRev 
		... endFromRev 
		... resolveType
		... resolveFlag
		... contentResolveType

		Sometimes it will NOT have contentResolveType, but will have "Branch Resolve" information in the form of something similar to this:
			Branch resolve:
			at: branch
			ay: ignore
		
		Skip that but note it in 'branchOrDeleteResolveRequired'.
	*/
	private parseConflictsFromDashNOutput() {
		this.successfullyParsedRemainingConflicts = true
		try {
			for (let ztagGroup of this.dashNOutput) {
				if (ztagGroup[0] === "Branch resolve:" || ztagGroup[0] === "Delete resolve:") {
					if (!this.remainingConflicts[this.remainingConflicts.length - 1]) {
						throw new Error(`Encountered branch/delete resolve information, but could not find appicable conflict file: ${ztagGroup.toString()}`)
					}

					this.remainingConflicts[this.remainingConflicts.length - 1].branchOrDeleteResolveRequired = true
					continue
				}

				["clientFile", "fromFile", "startFromRev", "endFromRev", "resolveType", "resolveFlag"].forEach(function (keyValue) {
					if (!ztagGroup[keyValue]) {
						throw new Error(`Resolve output missing ${keyValue} in ztag: ${ztagGroup.toString()}`)
					}
				})

				let remainingConflict: ConflictedResolveNFile = {
					clientFile: ztagGroup["clientFile"],
					fromFile: ztagGroup["fromFile"],
					// This handles the use case where only one revision is conflicting and it's the first one
					startFromRev: ztagGroup["startFromRev"] === "none" ? 1 : parseInt(ztagGroup["startFromRev"]),
					endFromRev: parseInt(ztagGroup["endFromRev"]),
					resolveType: ztagGroup["resolveType"],
					resolveFlag: ztagGroup["resolveFlag"]
				}

				if (ztagGroup["contentResolveType"]) {
					remainingConflict.contentResolveType = ztagGroup["contentResolveType"]
				}

				// Create new remaining conflict
				this.remainingConflicts.push(remainingConflict)
			}
		}
		catch (err) {
			this.rResultLogger.printException(err, 'Error processing \'resolve -N\' output')
			this.parsingError = err.toString()
			this.successfullyParsedRemainingConflicts = false
			return
		}
	}

	hasConflict() {
		return this.remainingConflicts.length > 0 || this.hasUnknownConflicts
	}

	getResolveOutput() {
		return this.resolveOutput
	}

	getConflictsText(): string {
		const lines: string[] = this.dashNOutput.map(entry => typeof entry === 'string' ? entry : JSON.stringify(entry))
		return lines.join('\n')
	}

	successfullyParsedConflicts() {
		return this.successfullyParsedRemainingConflicts
	}

	getParsingError() {
		return this.parsingError
	}

	getConflicts() {
		return this.remainingConflicts
	}

	hasDashNResult() {
		return this.resolveDashNRan
	}
}

export type ChangelistStatus = 'pending' | 'submitted' | 'shelved'

const runningPerforceCommands = new Set<CommandRecord>();
export const getRunningPerforceCommands = () => [...runningPerforceCommands]
export const P4_FORCE = '-f';
const robomergeVersion = `v${VersionReader.getBuildNumber()}`

// check if we are logged in and p4 is set up correctly
let perforceUsername: string
export function getPerforceUsername() {
	if (!perforceUsername) {
		throw new Error('Attempting to access Perforce username information before login completed.')
	}
	return perforceUsername
}

/**
 * This method must succeed before PerforceContext can be used. Otherwise retrieving the Perforce username through
 * getPerforceUsername() will error.
 */
export async function initializePerforce(logger: ContextualLogger) {
	setInterval(() => {
		const now = Date.now()
		for (const command of runningPerforceCommands) {
			const durationMinutes = (now - command.start.valueOf()) / (60*1000)
			if (durationMinutes > 10) {
				logger.info(`Command still running after ${Math.round(durationMinutes)} minutes: ` + command.cmd)
			}
		}
	}, 5*60*1000)

	const output = await PerforceContext._execP4Ztag(logger, null, ["login", "-s"], { noUsername: true });
	let resp = output[0];

	if (resp && resp.User) {
		perforceUsername = resp.User;
	}
	return resp;
}

/**
 * This is how our app interfaces with Perforce. All Perforce operations act through an
 * internal serialization object.
 */
export class PerforceContext {
	username = getPerforceUsername()

	// Use same logger as instance owner, to cut down on context length
	constructor(private readonly logger: ContextualLogger) {
	}

	// get a list of all pending changes for this user
	async get_pending_changes(workspaceName?: string, edgeServerAddress?: string) {
		if (!this.username) {
			throw new Error("username not set");
		}

		let args = ['changes', '-u', this.username, '-s', 'pending']
		if (workspaceName) {
			args = [...args, '-c', workspaceName]
		}

		return this._execP4Ztag(null, args, { multiline: true, edgeServerAddress });
	}

	/** get a single change in the format of changes() */
	async getChange(path_in: string, changenum: number, status?: ChangelistStatus) {
		const list = await this.changes(`${path_in}@${changenum},${changenum}`, -1, 1, status)
		if (list.length <= 0) {
			throw new Error(`Could not find changelist ${changenum} in ${path_in}`);
		}
		return <Change>list[0];
	}

	/**
	 * Get a list of changes in a path since a specific CL
	 * @return Promise to list of changelists
	 */
	changes(path_in: string, since: number, limit?: number, status?: ChangelistStatus): Promise<Change[]> {
		const path = since > 0 ? path_in + '@>' + since : path_in;
		const args = ['changes', '-l',
			(status ? `-s${status}` : '-ssubmitted'),
			...(limit ? [`-m${limit}`] : []),
			path];

		return this.execAndParse(null, args, {quiet: true}, {
			expected: {change: 'integer', client: 'string', user: 'string', desc: 'string'},
			optional: {shelved: 'integer', oldChange: 'integer', IsPromoted: 'integer'}
		}) as Promise<unknown> as Promise<Change[]>
	}

	async latestChange(path: string): Promise<Change> {

		// temporarily waiting 30 seconds - filing ticket   - was: wait no longer than 5 seconds, retry up to 3 times
		const args = ['-vnet.maxwait=30', '-r3', 'changes', '-l', '-ssubmitted', '-m1', path]

		const startTime = Date.now()

		const result = await this.execAndParse(null, args, {quiet: true, trace: true}, changeResultExpectedShape)
		if (!result || result.length !== 1) {
			throw new Error("Expected exactly one change")
		}

		const durationSeconds = (Date.now() - startTime) / 1000;

		if (durationSeconds > 5.0) {
			this.logger.warn(`p4.latestChange took ${durationSeconds}s`)
		}

		return result[0] as unknown as Change
	}

	changesBetween(path: string, from: number, to: number) {
		const args = ['changes', '-l', '-ssubmitted', `${path}@${from},${to}`]
		return this.execAndParse(null, args, {quiet: true}, changeResultExpectedShape) as Promise<unknown> as Promise<Change[]>
	}

	async streams() {
		const rawStreams = await this.execAndParse(null, ['streams'], {quiet: true}, {
			expected: {Stream: 'string', Update: 'integer', Access: 'integer', Owner: 'string', Name: 'string', Parent: 'string', Type: 'string', desc: 'string',
				Options: 'string', firmerThanParent: 'string', changeFlowsToParent: 'boolean', changeFlowsFromParent: 'boolean', baseParent: 'string'}
		})

		const streams = new Map<string, StreamSpec>()
		for (const raw of rawStreams) {
			const stream: StreamSpec = {
				stream: raw.Stream as string,
				name: raw.Name as string,
				parent: raw.Parent as string,
				desc: raw.desc as string,
			}
			streams.set(stream.stream, stream)
		}
		return streams
	}

	// find a workspace for the given user
	// output format is an array of workspace names
	async find_workspaces(user?: string, edgeServerAddress?: string) {
		// -a to include workspaces on edge servers


		let args = ['clients', '-u', user || this.username]

		if (!edgeServerAddress) {
			// find all
			args.push('-a')
		}

		let opts: ExecZtagOpts = { multiline: true }
		if (edgeServerAddress && edgeServerAddress !== 'commit') {
			opts.edgeServerAddress = edgeServerAddress
		}

		let parsedClients = await this._execP4Ztag(null, args, opts);
		let workspaces = [];
		for (let clientDef of parsedClients) {
			if (clientDef.client) {
				workspaces.push(clientDef);
			}
		}
		return workspaces as ClientSpec[];
	}

	find_workspace_by_name(workspaceName: string) {
		return this._execP4Ztag(null, ['clients', '-E', workspaceName]);
	}

	async getWorkspaceEdgeServer(workspaceName: string): Promise<{id: string, address: string} | null> {
		const serverIdLine = await this._execP4(null, ['-ztag', '-F', '%ServerID%', 'client', '-o', workspaceName]) 
		if (serverIdLine) {
			const serverId = serverIdLine.trim()
			const address = await this.getEdgeServerAddress(serverId)
			if (!address) {
				throw new Error(`Couldn't find address for edge server '${serverId}'`)
			}
			return {id: serverId, address: address.trim()}
		}
		return null
	}

	getEdgeServerAddress(serverId: string): Promise<string> {
		return this._execP4(null, ['-ztag', '-F', '%Address%', 'server', '-o', serverId])
	}

	async clean(roboWorkspace: RoboWorkspace) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		let result;
		try {
			result = await this._execP4(workspace, ['clean', '-e', '-a']);
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}
			const [err, output] = reason

			// ignore this error
			if (!output.trim().startsWith('No file(s) to reconcile')) {
				throw err
			}
			result = output;
		}
		this.logger.info(`p4 clean:\n${result}`);
	}

	// sync the depot path specified
	async sync(roboWorkspace: RoboWorkspace, depotPath: string, opts?: string[], edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		let args = edgeServerAddress ? ['-p', edgeServerAddress] : []
		args.push('sync')
		if (opts) {
			args = [...args, ...opts]
		}
		args.push(depotPath)
		try {
			await this._execP4(workspace, args)
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}
			const [err, output] = reason
			// this is an acceptable non-error case for us
			if (!output || typeof output !== "string" || !output.trim().endsWith("up-to-date."))
				throw err
		}
	}

	async syncAndReturnChangelistNumber(roboWorkspace: RoboWorkspace, depotPath: string, opts?: string[]) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		const change = await this.latestChange(depotPath);
		if (!change) {
			throw new Error('Unable to find changelist');
		}
		await this.sync(workspace, `${depotPath}@${change.change}`, opts);
		return change.change;
	}

	async newWorkspace(workspaceName: string, params: any, edgeServer?: {id: string, address: string}) {
		params.Client = workspaceName
		if (!('Root' in params)) {
			params.Root = 'd:/ROBO/' + workspaceName // default windows path
		}

		if (!('Owner' in params)) {
			params.Owner = this.username
		}

		if (!('Options' in params)) {
			params.Options = 'noallwrite clobber nocompress nomodtime'
		}

		if (!('SubmitOptions' in params)) {
			params.SubmitOptions = 'submitunchanged'
		}

		if (!('LineEnd' in params)) {
			params.LineEnd = 'local'
		}

		let args = ['client', '-i']
		if (edgeServer) {
			args = ['-p', edgeServer.address, ...args, `--serverid=${edgeServer.id}`]
		}

		let workspaceForm = ''
		for (let key in params) {
			let val = params[key];
			if (Array.isArray(val)) {
				workspaceForm += `${key}:\n`;
				for (let item of val) {
					workspaceForm += `\t${item}\n`;
				}
			}
			else {
				workspaceForm += `${key}: ${val}\n`;
			}
		}

		// run the p4 client command
		this.logger.info(`Executing: 'p4 client -i' to create workspace ${workspaceName}`);
		return this._execP4(null, args, { stdin: workspaceForm, quiet: true });
	}

	// Create a new workspace for Robomerge GraphBot
	async newGraphBotWorkspace(name: string, extraParams: any, edgeServer?: {id: string, address: string}) {
		return this.newWorkspace(name, {Root: '/src/' + name, ...extraParams}, edgeServer);
	}

	// Create a new workspace for Robomerge to read branchspecs from
	async newBranchSpecWorkspace(workspace: Workspace, bsDepotPath: string) {
		let roots: string | string[] = '/app/data' // default linux path
		if (workspace.directory !== roots) {
			roots = [roots, workspace.directory] // specified directory
		}

		const params: any = {
			AltRoots: roots
		}

		// Perforce paths are mighty particular 
		if (bsDepotPath.endsWith("/...")) {
			params.View = bsDepotPath + ` //${workspace.name}/...`
		} else if (bsDepotPath.endsWith("/")) {
			params.View = bsDepotPath + `... //${workspace.name}/...`
		} else {
			params.View = bsDepotPath + `/... //${workspace.name}/...`
		}

		return this.newWorkspace(workspace.name, params);
	}


	// create a new CL with a specific description
	// output format is just CL number
	async new_cl(roboWorkspace: RoboWorkspace, description: string, files?: string[], edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		// build the minimal form
		let form = 'Change:\tnew\nStatus:\tnew\nType:\tpublic\n';
		if (workspace)
			form += `Client:\t${workspace.name}\n`;

		if (files) {
			form += 'Files:\n';
			for (const filename of files) {
				form += `\t${filename}\n`;
			}
		}
		form += 'Description:\n\t' + this._sanitizeDescription(description);

		// run the P4 change command
		this.logger.info("Executing: 'p4 change -i' to create a new CL");
		const output = await this._execP4(workspace, ['change', '-i'], { stdin: form, quiet: true, edgeServerAddress });
		// parse the CL out of output
		const match = output.match(/Change (\d+) created./);
		if (!match) {
			throw new Error('Unable to parse new_cl output:\n' + output);
		}

		// return the changelist number
		return parseInt(match[1]);
	}

	// integrate a CL from source to destination, resolve, and place the results in a new CL
	// output format is true if the integration resolved or false if the integration wasn't necessary (still considered a success)
	// failure to resolve is treated as an error condition
	async integrate(roboWorkspace: RoboWorkspace, source: IntegrationSource, dest_changelist: number, target: IntegrationTarget, edgeServerAddress?: string)
		: Promise<[string, (Change | string)[]]> {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		// build a command
		let cmdList = [
			"integrate",
			"-Ob",
			"-Or",
			"-Rd",
			"-Rb",
			"-c" + dest_changelist
		];
		const range = `@${source.changelist},${source.changelist}`

		let noSuchFilesPossible = false // Helper variable for error catching
		// Branchspec -- takes priority above other possibilities
		if (source.branchspec) {
			cmdList.push("-b");
			cmdList.push(source.branchspec.name);
			if (source.branchspec.reverse) {
				cmdList.push("-r");
			}
			cmdList.push(target.path_to + range);
			noSuchFilesPossible = true
		}
		// Stream integration
		else if (source.depot === target.depot && source.stream && target.stream) {
			cmdList.push("-S")
			cmdList.push(source.stream)
			cmdList.push("-P")
			cmdList.push(target.stream)
			cmdList.push(target.path_to + range)
			noSuchFilesPossible = true
		}
		// Basic branch to branch
		else {
			cmdList.push(source.path_from + range);
			cmdList.push(target.path_to);
		}

		// execute the P4 command
		let changes;
		try {
			changes = await this._execP4Ztag(workspace, cmdList, { numRetries: 0, edgeServerAddress });
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason

			// if this change has already been integrated, this is a special return (still a success)
			if (output.match(/already integrated\.\n/)) {
				return ["already_integrated", []];
			}
			else if (output.match(/already integrated in pending changelist\./)) {
				return ["integration_already_in_progress", []];
			}
			else if (output.match(/No file\(s\) at that changelist number\.\n/)) {
				return ["no_files", []];
			}
			else if (noSuchFilesPossible && output.match(/No such file\(s\)\.\n/)) {
				return ["no_files", []];
			}
			else if (output.match(/no target file\(s\) in branch view\n/)) {
				return ["no_files", []];
			}
			else if (output.match(/resolve move to/)) {
				return ["partial_integrate", output.split('\n')];
			}
			else {
				const knownIntegrationFailure = this.parseIntegrationFailure(err.toString().split('\n'))
				if (knownIntegrationFailure) {
					return knownIntegrationFailure
				}
			}

			// otherwise pass on error
			this.logger.printException(err, "Error encountered during integrate: ")
			throw err;
		}

		// annoyingly, p4 outputs failure to integrate locked files here on stdout (not stderr)

		const failures: string[] = [];
		for (const change of changes) {
			if (Array.isArray(change)) {
				// P4 emitted some error(s) in the stdout
				failures.push(...change);
			}
		}

		// if there were any failures, return that
		if (failures.length > 0) {
			return ["partial_integrate", failures];
		}

		// everything looks good
		return ["integrated", <Change[]>changes];
	}

	async resolveHelper(workspace: Workspace | null, flag: string, changelist: number, edgeServerAddress?: string): Promise<any> {
		try {
			// Perform merge
			return await this._execP4Ztag(workspace, ['resolve', flag, `-c${changelist}`], {edgeServerAddress})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			let result: string | null = null

			if (output.startsWith('No file(s) to resolve.')) {
				result = 'success'
			}
			else if (
				output.match(/can't move \(open for delete\)/) ||
				output.match(/can't delete moved file;/) ||
				output.match(/resolve skipped/)) {
				result = output
			}

			if (!result) {
				throw err
			}

			return output
		}
	}


	// output format a list of conflicting files (p4 output)
	async resolve(roboWorkspace: RoboWorkspace, changelist: number, resolution: string, disallowDashN?: boolean, edgeServerAddress?: string)
		: Promise<ResolveResult> {
		const workspace = coercePerforceWorkspace(roboWorkspace)
		let flag = null
		switch (resolution) {
			case 'safe': flag = '-as'; break
			case 'clobber': // Clobber should perform a normal merge first, then resolve with '-at' afterwards
			case 'normal': flag = '-am'; break
			case 'null': flag = '-ay'; break
			default:
				throw new Error(`Invalid resultion type ${resolution}`)
		}

		// Perform merge
		let fileInfo = await this.resolveHelper(workspace, flag, changelist, edgeServerAddress)

		// Clobber remaining files after first merge, if requested
		if (resolution === 'clobber') {
			// If resolveHelper() returned a string, we have nothing to clobber due to no files remaining or an error being thrown.
			// Either case is a bad clobber request. Throw an error.
			if (typeof fileInfo === 'string') {
				if (fileInfo === 'success') {
					return new ResolveResult(null, fileInfo, this.logger)
				}
				throw new Error(`Cannot continue with clobber request -- merge attempt before clobber returned "${fileInfo}"`)
			}

			// Otherwise, fileInfo should be updated with the clobber result
			fileInfo = await this.resolveHelper(workspace, '-at', changelist, edgeServerAddress)
		}

		// If resolveHelper() returned a string, we do not need to return a dashNResult
		if (typeof fileInfo === 'string') {
			return new ResolveResult(null, fileInfo, this.logger)
		}

		if (disallowDashN) {
			this.logger.warn('Skipping resolve -N')
			return new ResolveResult(null, 'skipped resolve -N', this.logger)
		}

		let dashNresult: string[]
		try {
			//dashNresult = await this._execP4(workspace, ['resolve', '-N', `-c${changelist}`])
			dashNresult = await this._execP4Ztag(workspace, ['resolve', '-N', `-c${changelist}`], {edgeServerAddress})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			
			// If our error is just there are no files to resolve, 
			if (output.startsWith('No file(s) to resolve.')) {
				dashNresult = ['success']
			} else {
				throw err
			}

		}

		return new ResolveResult(fileInfo, dashNresult, this.logger)
	}

	// submit a CL
	// output format is final CL number or false if changes need more resolution
	async submit(roboWorkspace: RoboWorkspace, changelist: number): Promise<number | string> {
		const workspace = coercePerforceWorkspace(roboWorkspace)
		let rawOutput: string
		try {
			rawOutput = await this._execP4(workspace, ['-ztag', 'submit', '-f', 'submitunchanged', '-c', changelist.toString()])
		}
		catch ([errArg, output]) {
			const err = errArg.toString().trim()
			const out: string = output.trim()
			if (out.startsWith('Merges still pending --')) {
				// concurrent edits (try again)
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return 0
			}
			else if (out.startsWith('Out of date files must be resolved or reverted')) {
				// concurrent edits (try again)
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return 0
			}
			else if (out.search(/Change \d+ unknown/) >= 0 || out.search(/Change \d+ is already committed./) >= 0)
			{
				/* Sometimes due to intermittent internet issues, we can succeed in submitting but we'll try to submit
				 * again, and the pending changelist doesn't exist anymore.
 				 * By returning zero, RM will waste a little time resolving again but should find no needed changes,
				 * handling this issue gracefully.
				 */
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return 0
			}
			else if (out.indexOf('File(s) couldn\'t be locked.') >= 0)
			{
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return err.toString()
			}
			else if (out.startsWith('No files to submit.')) {
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				await this.deleteCl(workspace, changelist)
				return 0
			}
			else if (out.startsWith('Submit validation failed')) {
				// unable to submit due to validation trigger
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return err.toString()
			}
			throw err
		}

		// success, parse the final CL
		const result: any[] = parseZTag(rawOutput)
		// expecting list of committed files, followed by submitted CL, optionally followed by list of refreshed files
		const numResults = result.length
		for (let index = numResults - 1; index >= 0; --index) {
			if (result[index].depotFile) {
				break
			}
			const finalCL = result[index].submittedChange
			if (finalCL) {
				if (typeof finalCL === 'number') {
					return finalCL
				}
				break
			}
		}

		this.logger.error(`=== SUBMIT FAIL === \nOUT:${rawOutput}`)
		throw new Error(`Unable to find submittedChange in P4 results:\n${rawOutput}`)
	}


	// SHOULDN'T REALLY LEAK [err, result] FORMAT IN REJECTIONS

	// 	- thinking about having internal and external exec functions, but, along
	// 	- with ztag variants, that's quite messy

	// should refactor so that:
	//	- exec never fails, instead returns a rich result type
	//	- ztag is an option
	//	- then can maybe have a simple wrapper


	// delete a CL
	// output format is just error or not
	deleteCl(roboWorkspace: RoboWorkspace, changelist: number, edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		return this._execP4(workspace, ["change", "-d", changelist.toString()], {edgeServerAddress});
	}

	// run p4 'opened' command: lists files in changelist with details of edit state (e.g. if a copy, provides source path)
	opened(roboWorkspace: RoboWorkspace | null, arg: number | string) {
		const workspace = roboWorkspace && coercePerforceWorkspace(roboWorkspace);
		const args = ['opened']
		if (typeof arg === 'number') {
			// see what files are open in the given changelist
			args.push('-c', arg.toString())
		}
		else {
			// see which workspace has a file checked out/added
			args.push('-a', arg)
		}
		return this._execP4Ztag(workspace, args) as Promise<OpenedFileRecord[]>
	}

	// revert a CL deleting any files marked for add
	// output format is just error or not
	async revert(roboWorkspace: RoboWorkspace, changelist: number, opts?: string[], edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		const path = workspace && workspace.name ? `//${workspace.name}/...` : ''
		try {
			await this._execP4(workspace, ['revert',
				/*"-w", seems to cause ENOENT occasionally*/ ...(opts || []), '-c', changelist.toString(), path],
				{edgeServerAddress});
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			// this happens if there's literally nothing in the CL. consider this a success
			if (!output.match(/file\(s\) not opened (?:on this client|in that changelist)\./)) {
				throw err;
			}
		}
	}

	// list files in changelist that need to be resolved
	async listFilesToResolve(roboWorkspace: RoboWorkspace, changelist: number) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		try {
			return await this._execP4Ztag(workspace, ['resolve', '-n', '-c', changelist.toString()]);
		}
		catch (err) {
			if (err.toString().toLowerCase().includes('no file(s) to resolve')) {
				return [];
			}
			throw (err);
		}
	}

	async move(roboWorkspace: RoboWorkspace, cl: number, src: string, target: string, opts?: string[]) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		return this._execP4(workspace, ['move', ...(opts || []), '-c', cl.toString(), src, target]);
	}

	/**
	 * Run a P4 command by name on one or more files (one call per file, run in parallel)
	 */
	run(roboWorkspace: RoboWorkspace, action: string, cl: number, filePaths: string[], opts?: string[]) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		const args = [action, ...(opts || []), '-c', cl.toString()]
		return Promise.all(filePaths.map(
			path => this._execP4(workspace, [...args, path])
		));
	}

	// shelve a CL
	// output format is just error or not
	async shelve(roboWorkspace: RoboWorkspace, changelist: number, edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace)
		try {
			await this._execP4(workspace, ['shelve', '-f', '-c', changelist.toString()],
				{ numRetries: 0, edgeServerAddress })
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason

			if (output && output.match && output.match(/No files to shelve/)) {
				return false
			}
			throw err
		}
		return true
	}

	// shelve a CL
	// output format is just error or not
	async unshelve(roboWorkspace: RoboWorkspace, changelist: number) {
		const workspace = coercePerforceWorkspace(roboWorkspace)
		try {
			await this._execP4(workspace, ['unshelve', '-s', changelist.toString(), '-f', '-c', changelist.toString()])
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			if (output && output.match && output.match(/No files to unshelve/)) {
				return false
			}
			throw err
		}
		return true
	}

	// delete shelved files from a CL
	delete_shelved(roboWorkspace: RoboWorkspace, changelist: number) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		return this._execP4(workspace, ["shelve", "-d", "-c", changelist.toString(), "-f"]);
	}

	// get the email (according to P4) for a specific user
	async getEmail(username: string) {
		const output = await this._execP4(null, ['user', '-o', username]);
		// look for the email field
		let m = output.match(/\nEmail:\s+([^\n]+)\n/);
		return m && m[1];
	}

	/** Check out a file into a specific changelist ** ASYNC ** */
	edit(roboWorkspace: RoboWorkspace, cl: number, filePath: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		return this._execP4(workspace, ['edit', '-c', cl.toString(), filePath]);
	}

	async describe(cl: number, maxFiles?: number, includeShelved?: boolean) {
		const args = ['describe',
			...(maxFiles ? ['-m', maxFiles.toString()] : []),
			...(includeShelved ? ['-S'] : []),
			cl.toString()]
		const ztagResult = await this._execP4Ztag(null, args, { multiline:true });

		if (ztagResult.length > 2) {
			throw new Error('Unexpected describe result')
		}

		const clInfo: any = ztagResult[0];
		const result: DescribeResult = {
			user: clInfo.user || '',
			status: clInfo.status || '',
			description: clInfo.desc || '',
			date: clInfo.time ? new Date(clInfo.time * 1000) : null,
			entries: []
		};

		for (const obj of readObjectListFromZtagOutput(ztagResult[ztagResult.length === 2 ? 1 : 0], ['depotFile', 'action', 'rev', 'type'], this.logger)) {
			let rev = -1;
			if (obj.rev) {
				const num = parseInt(obj.rev);
				if (!isNaN(num)) {
					rev = num;
				}
			}
			result.entries.push({
				depotFile: obj.depotFile || '',
				action: obj.action || '',
				rev,
				type: obj.type || ''
			});
		}
		return result;
	}

	// change the description on an existing CL
	// output format is just error or not
	async editDescription(roboWorkspace: RoboWorkspace, changelist: number, description: string, edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);

		// get the current changelist description
		const output = await this._execP4(workspace, ['change', '-o', changelist.toString()], {edgeServerAddress});
		// replace the description
		let new_desc = '\nDescription:\n\t' + this._sanitizeDescription(description);
		let form = output.replace(/\nDescription:\n(\t[^\n]*\n)*/, new_desc.replace(/\$/g, '$$$$'));

		// run the P4 change command to update
		this.logger.info("Executing: 'p4 change -i -u' to edit description on CL" + changelist);
		await this._execP4(workspace, ['change', '-i', '-u'], { stdin: form, quiet: true, edgeServerAddress });
	}

	// change the owner of an existing CL
	// output format is just error or not
	async editOwner(roboWorkspace: RoboWorkspace, changelist: number, newOwner: string, opts?: EditOwnerOpts) {
		const workspace = coercePerforceWorkspace(roboWorkspace);

		opts = opts || {}; // optional newWorkspace:string and/or changeSubmitted:boolean
		// get the current changelist description
		const output = await this._execP4(workspace, ['change', '-o', changelist.toString()],
			{edgeServerAddress: opts.edgeServerAddress});

		// replace the description
		let form = output.replace(/\nUser:\t[^\n]*\n/, `\nUser:\t${newOwner}\n`);
		if (opts.newWorkspace) {
			form = form.replace(/\nClient:\t[^\n]*\n/, `\nClient:\t${opts.newWorkspace}\n`);
		}

		// run the P4 change command to update
		const changeFlag = opts.changeSubmitted ? '-f' : '-u';
		this.logger.info(`Executing: 'p4 change -i ${changeFlag}' to edit user/client on CL${changelist}`);

		await this._execP4(workspace, ['change', '-i', changeFlag],
				{ stdin: form, quiet: true, edgeServerAddress: opts.edgeServerAddress })
	}

	where(roboWorkspace: RoboWorkspace, clientPath: string) {
		return this._execP4Ztag(roboWorkspace, ['where', clientPath])
	}

	async filelog(roboWorkspace: RoboWorkspace, depotPath: string, beginRev: string, endRev: string, longOutput = false) {
		let args = ['filelog', '-i']
		if (longOutput) {
			args.push('-l')
		}
		args.push(`${depotPath}#${beginRev},${endRev}`)

		try {
			return await this._execP4Ztag(roboWorkspace, args, { multiline: longOutput })
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			// If perforce doesn't detect revisions in the given range, return an empty set of revisions
			if (output.match(/no revision\(s\) (?:below|above) that revision/)) {
				return []
			}

			throw err
		}
	}

	print(path: string) {
		return this._execP4(null, ['print', '-q', path], { noCwd: true, quiet: true })
	}

	private _sanitizeDescription(description: string) {
		return description.trim().replace(/\n\n\.\.\.\s/g, "\n\n ... ").replace(/\n/g, "\n\t");
	}

	private parseIntegrationFailure(errorLines: string[]): [string, string[]] | null {
		for (const [regex, tag] of INTEGRATION_FAILURE_REGEXES)
		{
			const matchingLines = errorLines.filter(line => line.match(regex))
			if (matchingLines.length > 0) {
				return [tag, matchingLines]
			}
		}
		return null
	}

	// execute a perforce command
	static _execP4(logger: ContextualLogger, roboWorkspace: RoboWorkspace, args: string[], optsIn?: ExecOpts) {
		// We have some special behavior regarding Robomerge being in verbose mode (able to be set through the IPC) --
		// basically 'debug' is for local development and these messages are really spammy
		let logLevel : NpmLogLevel =
			ContextualLogger.getLogLevel() === "verbose" ? "verbose" :
			"silly"
			

		const workspace = coercePerforceWorkspace(roboWorkspace);
		// add the client explicitly if one is set (should be done at call time)

		const opts = optsIn || {}

		// Perforce can get mighty confused if you log into multiple accounts at any point (only relevant to local debugging)
		if (!opts.noUsername) {
			args = ['-u', getPerforceUsername(), ...args]
		}

		if (workspace && workspace.name) {
			args = ['-c', workspace.name, ...args]
		}

		if (opts.edgeServerAddress) {
			args = ['-p', opts.edgeServerAddress, ...args]
		}

		args = ['-zprog=robomerge', '-zversion=' + robomergeVersion, ...args]

		// log what we're running
		let cmd_rec = new CommandRecord('p4 ' + args.join(' '));
		// 	logger.verbose("Executing: " + cmd_rec.cmd);
		runningPerforceCommands.add(cmd_rec);

		// we need to run within the workspace directory so p4 selects the correct AltRoot
		let options: ExecFileOptions = { maxBuffer: 500 * 1024 * 1024 };
		if (workspace && workspace.directory && !opts.noCwd) {
			options.cwd = workspace.directory;
		}

		const cmdForLog = `  cmd: ${p4exe} ${args.join(' ')} ` + (options.cwd ? `(from ${options.cwd})` : '')
		if (!opts.quiet) {
			logger.info(cmdForLog)
		}
		else {
			logger[logLevel](cmdForLog)
		}

		// darwin p4 client seems to need this
		if (options.cwd) {
			options.env = {};
			for (let key in process.env)
				options.env[key] = process.env[key];
			options.env.PWD = path.resolve(options.cwd);
		}

		const doExecFile = function(retries: number): Promise<string> {
			return new Promise<string>((done, fail) => {
				const child = execFile(p4exe, args, options, (err, stdout, stderr) => {
					logger[logLevel]("Command Completed: " + cmd_rec.cmd);
					// run the callback
					if (stderr) {
						let errstr = "P4 Error: " + cmd_rec.cmd + "\n";
						errstr += "STDERR:\n" + stderr + "\n";
						errstr += "STDOUT:\n" + stdout + "\n";
						if (opts.stdin)
							errstr += "STDIN:\n" + opts.stdin + "\n";
						fail([new Error(errstr), stderr.toString().replace(newline_rex, '\n')]);
					}
					else if (err) {
						logger.printException(err)
						let errstr = "P4 Error: " + cmd_rec.cmd + "\n" + err.toString() + "\n";

						if (stdout || stderr) {
							if (stdout)
								errstr += "STDOUT:\n" + stdout + "\n";
							if (stderr)
								errstr += "STDERR:\n" + stderr + "\n";
						}

						if (opts.stdin)
							errstr += "STDIN:\n" + opts.stdin + "\n";

						fail([new Error(errstr), stdout ? stdout.toString() : '']);
					}
					else {
						const response = stdout.toString().replace(newline_rex, '\n')
						if (response.length > 10 * 1024) {
							logger.info(`Response size: ${Math.round(response.length / 1024.)}K`)
						}

						if (opts.trace) {
							const [traceResult, rest] = parseTrace(response)
							const durationSeconds = (Date.now() - cmd_rec.start.valueOf()) / 1000
							if (durationSeconds > 30) {
								logger.info(`Cmd: ${cmd_rec.cmd}, duration: ${durationSeconds}s\n` + traceResult)
							}
							done(rest)
						}
						else {
							done(response)
						}
					}
				});

				// write some stdin if requested
				if (opts.stdin) {
					try {
						logger[logLevel](`-> Writing to p4 stdin:\n${opts.stdin}`);

						const childStdin = child.stdin!
						childStdin.write(opts.stdin);
						childStdin.end();

						logger[logLevel]('<-');
					}
					catch (ex) {
						// usually means P4 process exited immediately with an error, which should be logged above
						logger.info(ex);
					}
				}
			}).catch((reason: any) => {
				if (!isExecP4Error(reason)) {
					logger.printException(reason, 'Caught unknown/malformed rejection while in execP4:')
					throw reason
				}
				const [err, output] = reason
				
				try {
					let retry = false
					for (const retryable of RETRY_ERROR_MESSAGES) {
						if (err.message.indexOf(retryable) >= 0) {
							retry = true
							break
						}	
					}
					if (retry) {
						const msg = `${logger.context} encountered connection reset issue, retries remaining: ${retries} `
						logger.warn(msg + `\nCommand: ${cmd_rec.cmd}`)
	
						if (roboAnalytics) {
							roboAnalytics.reportPerforceRetries(1)
						}
	
						if (retries === 1)  {
							postToRobomergeAlerts(msg + ` \`\`\`${cmd_rec.cmd}\`\`\``)
						}
	
						if (retries > 0)  {
							return doExecFile(--retries)
						}
					}
				}
				catch (err) {
					logger.printException(err, `Rejection array caught during execP4 seems to be malformed and caused an error during processing.\nArray: ${JSON.stringify(reason)}\nError:`)
				}
				throw [err, output]	
			}).finally( () => {
				runningPerforceCommands.delete(cmd_rec);
			})
		}

		return doExecFile(optsIn && typeof optsIn.numRetries === 'number' && optsIn.numRetries >= 0 ? optsIn.numRetries : 3)
	}

	private _execP4(roboWorkspace: RoboWorkspace, args: string[], optsIn?: ExecOpts) {
		return PerforceContext._execP4(this.logger, roboWorkspace, args, optsIn)
	}

	static async _execP4Ztag(logger: ContextualLogger, roboWorkspace: RoboWorkspace, args: string[], opts?: ExecZtagOpts) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		return parseZTag(await PerforceContext._execP4(logger, workspace, ['-ztag', ...args], opts), opts && opts.multiline);
	}

	private async _execP4Ztag(roboWorkspace: RoboWorkspace, args: string[], opts?: ExecZtagOpts) {
		return PerforceContext._execP4Ztag(this.logger, roboWorkspace, args, opts)
	}


	////////////
	// new -ztag parsing - use with care. Initially using for p4.changes and p4.describe

	async execAndParse(roboWorkspace: RoboWorkspace, args: string[], opts: ExecOpts, options?: ztag.ParseOptions) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		const rawOutput = await PerforceContext._execP4(this.logger, workspace, ['-ztag', ...args], opts)
		return ztag.parseZtagOutput(rawOutput, this.logger, options)

	}

	async execAndParseArray(roboWorkspace: RoboWorkspace, args: string[], opts: ExecOpts, headerOptions?: ztag.ParseOptions, arrayEntryOptions?: ztag.ParseOptions) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		const rawOutput = await PerforceContext._execP4(this.logger, workspace, ['-ztag', ...args], opts)
		return ztag.parseHeaderAndArray(rawOutput, this.logger, headerOptions, arrayEntryOptions)
	}
}

export function getRootDirectoryForBranch(name: string): string {
	return process.platform === "win32" ? `d:/ROBO/${name}` : `/src/${name}`;
}

// temp (ha)
export function coercePerforceWorkspace(workspace: any): Workspace | null {
	if (!workspace)
		return null

	if (typeof (workspace) === "string") {
		workspace = { name: workspace };
	}

	if (!workspace.directory) {
		workspace.directory = getRootDirectoryForBranch(workspace.name);
	}
	return <Workspace>workspace
}
