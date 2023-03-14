// Copyright Epic Games, Inc. All Rights Reserved.

import * as fs from 'fs';
import * as path from 'path';
import { ContextualLogger } from '../common/logger';
import { P4_FORCE, PerforceContext, Workspace } from '../common/perforce';
import { Bot } from './bot-interfaces';
import { BranchDefs } from './branchdefs';
import { GraphInterface } from './graph-interface';
import * as p4util from '../common/p4util';

const DISABLE = false

type AutoBranchUpdaterConfig = {
	rootPath: string		//				process.env('ROBO_BRANCHSPECS_ROOT_PATH')
	workspace: Workspace	// { directory:	process.env('ROBO_BRANCHSPECS_DIRECTORY')
}							// , name:		process.env('ROBO_BRANCHSPECS_WORKSPACE') }

type MirrorPaths = {
	name: string
	directory: string
	stream: string
	depotFolder: string
	depotpath: string
	realFilepath: string
	mirrorFilepath: string
}

export class AutoBranchUpdater implements Bot {

	private p4: PerforceContext
	private filePath: string
	private workspace: Workspace

	private readonly abuLogger: ContextualLogger
	
	lastCl: number

	// public: used by NodeBot
	isRunning = false
	isActive = false

	static p4: PerforceContext
	static config: AutoBranchUpdaterConfig
	static initialCl: number

	tickCount = 0

	constructor(private graphBot: GraphInterface, parentLogger: ContextualLogger) {
		this.p4 = AutoBranchUpdater.p4!

		const config = AutoBranchUpdater.config
		this.filePath = `${config.rootPath}/${graphBot.filename}`
		this.workspace = config.workspace

		this.lastCl = AutoBranchUpdater.initialCl
		this.abuLogger = parentLogger.createChild('ABU')
	}

	static async init(deps: any, config: AutoBranchUpdaterConfig, parentLogger: ContextualLogger) {
		this.p4 = deps.p4
		this.config = config
		const logger = parentLogger.createChild('ABU')

		logger.info(`Finding most recent branch spec changelist from ${config.rootPath}`)
		var bsRoot = config.rootPath + '/...'
		const change = await this.p4.latestChange(bsRoot)
		if (change === null)
			throw new Error(`Unable to query for most recent branch specs CL`)

		this.initialCl = change.change

		if (DISABLE) {
			logger.warn('Auto branch update disabled!');
			return
		}

		logger.info(`Syncing branch specs at CL ${change.change}`);
		let workspaceDir : string = path.resolve(config.workspace.directory);
		if (!fs.existsSync(workspaceDir)) {
			logger.info(`Creating local branchspec directory ${workspaceDir}`)
			fs.mkdirSync(workspaceDir);
		}

		await this.p4.sync(config.workspace, `${bsRoot}@${change.change}`, [P4_FORCE])
	}
	
	async start() {
		this.isRunning = true;
		this.abuLogger.info(`Began monitoring ${this.graphBot.branchGraph.botname} branch specs at CL ${this.lastCl}`);

// this got lost in the mirror refactor

		// if (!DISABLE && this.mirror) {
		// 	const workspace = await this.p4.find_workspace_by_name(this.mirror.workspace.name)
		// 	if (workspace.length === 0) {
		// 		await this.p4.newWorkspace(this.mirror.workspace.name, {
		// 			Stream: this.mirror.stream,
		// 			Root: AutoBranchUpdater.config!.workspace.directory
		// 		})
		// 	}
		// }

	}

	async tick() {
		if (DISABLE) {
			return false
		}

		let change;
		try {
			change = await this.p4.latestChange(this.filePath);
		}
		catch (err) {
			this.abuLogger.printException(err, 'Branch specs: error while querying P4 for changes');
			return false
		}

		if (change !== null && change.change > this.lastCl) {
			await this.p4.sync(this.workspace, `${this.filePath}@${change.change}`);

			// set this to be the last changelist regardless of success - if it failed due to a broken
			// .json file, the file will have to be recommitted anyway
			this.lastCl = change.change;

			await this._tryReloadBranchDefs();
		}
		return true
	}

	async _tryReloadBranchDefs() {
		let branchGraphText
		try {
			branchGraphText = require('fs').readFileSync(`${this.workspace.directory}/${this.graphBot.filename}`, 'utf8')
		}
		catch (err) {
			// @todo email author of changes!
			this.abuLogger.printException(err, 'ERROR: failed to reload branch specs file')
			return
		}

		const validationErrors: string[] = []
		const result = BranchDefs.parseAndValidate(validationErrors, branchGraphText, await this.p4.streams())
		if (!result.branchGraphDef) {
			// @todo email author of changes!
			let errText = 'failed to parse/validate branch specs file\n'
			for (const error of validationErrors) {
				errText += `${error}\n`
			}
			this.abuLogger.error(errText.trim())
			return
		}

		const botname = this.graphBot.branchGraph.botname
		if (this.graphBot.ensureStopping()) {
			this.abuLogger.info(`Ignoring changes to ${botname} branch specs, since bot already stopping`)
			return
		}

		this.abuLogger.info(`Stopped monitoring ${botname} branches, in preparation for reloading branch definitions`)

		// NOTE: not awaiting next tick. Waiters on this function carry on as soon as we return
		// this doesn't wait until all the branch bots have stopped, but that's ok - we're creating a new set of branch bots
		process.nextTick(async () => {
			await this.p4.sync(this.workspace, this.filePath)

			this.abuLogger.info(`Branch spec change detected: reloading ${botname} from CL#${this.lastCl}`)

			await this.graphBot.reinitFromBranchGraphsObject(result.config, result.branchGraphDef!)

			const mirrorWorkspace = AutoBranchUpdater.getMirrorWorkspace(this.graphBot)
			if (!mirrorWorkspace) {
				return
			}

			for (let retryCount = 0; ; ++retryCount) {
				try {
					await this.updateMirror(mirrorWorkspace)
					return
				}
				catch (err) {

					if (retryCount < 5) {
						this.abuLogger.warn(`Mirror file reloading error (retries ${retryCount}: ${err}`)
						p4util.cleanWorkspaces(this.p4, [[mirrorWorkspace.name, mirrorWorkspace.depotpath]])
					}
					else {
						this.abuLogger.error('Mirror file reloading failed, retries exhausted')
						throw err
					}
				}
			}
		})
	}

	forceUpdate() {
		this.lastCl = -1
	}

	get fullName() {
		return `${this.graphBot.branchGraph.botname} auto updater`
	}

	get fullNameForLogging() {
		return this.abuLogger.context
	}

	private async updateMirror(workspace: MirrorPaths) {
		this.abuLogger.info("Updating branchmap mirror")

		const stream = this.graphBot.branchGraph.config.mirrorPath[0]

		const workspaceQueryResult = await this.p4.find_workspace_by_name(workspace.name)
		if (workspaceQueryResult.length === 0) {
			await this.p4.newWorkspace(workspace.name, {
				Stream: stream,
				Root: AutoBranchUpdater.config!.workspace.directory
			})
		}

		const {depotpath, realFilepath, mirrorFilepath} = workspace

		await this.p4.sync(workspace, depotpath, [P4_FORCE])
		const cl = await this.p4.new_cl(workspace, "Updating mirror file\n#jira none\n#robomerge ignore\n")
		await this.p4.edit(workspace, cl, depotpath)
		await new Promise((done, _) => fs.copyFile(realFilepath, mirrorFilepath, done))
		await this.p4.submit(workspace, cl)
	}

	static getMirrorWorkspace(bot: GraphInterface): MirrorPaths | null {
		const mirrorPathBits = bot.branchGraph.config.mirrorPath
		if (mirrorPathBits.length === 0) {
			return null
		}
		
		if (mirrorPathBits.length !== 3) {
			throw new Error('If mirrorPath is specified, expecting [stream, sub-path, filename]')
		}

		const directory = AutoBranchUpdater.config!.workspace.directory
		const depotFolder = mirrorPathBits.slice(0, 2).join('/')
		const depotpath = mirrorPathBits.join('/')
		const realFilepath = path.join(directory, bot.filename)
		const mirrorFilepath = path.join(directory, ...mirrorPathBits.slice(1))
		return {
			name: `${AutoBranchUpdater.config!.workspace.name}-${bot.branchGraph.botname}-mirror`,
			stream: mirrorPathBits[0], depotFolder, depotpath, realFilepath, mirrorFilepath, directory
		}
	}
}
