// Copyright Epic Games, Inc. All Rights Reserved.

import * as Sentry from '@sentry/node';
import { OnUncaughtException, OnUnhandledRejection } from '@sentry/node/dist/integrations';
import { ChildProcess, fork, ForkOptions, spawn } from 'child_process';
import * as fs from 'fs';
import * as os from 'os';
import { Analytics } from '../common/analytics';
import { Arg, readProcessArgs } from '../common/args';
import { closeSentry } from '../common/helper';
import { ContextualLogger } from '../common/logger';
import { VersionReader } from '../common/version';
import { branchesRequests, RoboServer } from './roboserver';
import { Session } from './session';

const tlsKeyFilename = 'rm-2022-05.key'

// Begin by intializing our logger and version reader
const watchdogStartupLogger = new ContextualLogger('Watchdog Startup')
VersionReader.init(watchdogStartupLogger)

const ENTRY_POINT = 'dist/robo/robo.js'
const RESPAWN_DELAY = 30
const AUTO_RESPAWN = false
const MEM_USAGE_INTERVAL_SECONDS = 5 * 60

const COMMAND_LINE_ARGS: {[param: string]: (Arg<any>)} = {
	hostInfo: {
		match: /^-hostInfo=(.+)$/,
		env: 'ROBO_HOST_INFO',
		dflt: os.hostname()
	},

	externalUrl: {
		match: /^-externalUrl=(.+)$/,
		env: 'ROBO_EXTERNAL_URL',
		dflt: 'https://127.0.0.1'
	},

	// Sentry environment designation -- use 'PROD' to enable Sentry bug tracking
	epicEnv: {
		match: /^(-epicEnv)$/,
		env: 'EPIC_ENV',
		parse: (str: string) => {
			return str.toLowerCase()
		},
		dflt: 'dev'
	},
	nodeEnv: {
		match: /^(-nodeEnv)$/,
		env: 'NODE_ENV',
		parse: (str: string) => {
			return str.toLowerCase()
		},
		dflt: 'development'
	},
	epicDeployment: {
		match: /^(-epicDeployment)$/,
		env: 'EPIC_DEPLOYMENT',
		parse: (str: string) => {
			return str.toLowerCase()
		},
		dflt: 'unknown'
	},
	watchdogPort: {
		match: /^-watchdogPort=(\d+)$/,
		env: 'ROBO_WATCHDOG_PORT',
		parse: (str: string) => {
			const val = parseInt(str)
			return isNaN(val) ? 4433 : val
		},
		dflt: 4433
	},
	sentryDsn: {
		match: /^-sentryDsn=(.+)$/,
		env: 'SENTRY_DSN',
		dflt: 'https://f68a5bce117743a595d871f3ddac26bf@sentry.io/1432517' // Robomerge sentry project: https://sentry.io/organizations/to/issues/?project=1432517
	}
}

const maybeNullArgs = readProcessArgs(COMMAND_LINE_ARGS, watchdogStartupLogger)
if (!maybeNullArgs) {
	process.exit(1)
}
const args = maybeNullArgs

const env = args.epicEnv || (args.nodeEnv === "production" ? "prod" : "dev")
const sentryEnv = `${env}-${args.epicDeployment}`
// Only run Sentry for the main module
if (env === 'prod' 
	&& args.sentryDsn) {
	watchdogStartupLogger.verbose(`(Watchdog) Sentry enabled for environment ${sentryEnv}. Uploading error/event reports to ${args.sentryDsn}`)
	Sentry.init({
		dsn: args.sentryDsn,
		release: VersionReader.getShortVersion(),
		environment: sentryEnv,
		serverName: args.hostInfo,
		integrations: [...Sentry.defaultIntegrations, new OnUnhandledRejection(), new OnUncaughtException() ]
	});
}

class Watchdog {
	child: ChildProcess | null = null
	statusServer: RoboServer

	shutdown = false
	paused = false
	lastSpawnStart = -1

	lastCrash: string | null = null

	cbMap = new Map<number, Function>()
	nextCbId = 1
	lastError = ""

	private readonly watchdogLogger = new ContextualLogger('Watchdog')

	constructor() {
		// expose a webserver that can be used to check status
		this.statusServer = new RoboServer(args.externalUrl,
			(name: string, args?: any[]) => this.sendMessage(name, args), 
			() => this.watchdogLogger.getLogTail(), () => this.getLastCrash(), () => this.stopBot(), () => this.startBot()
		)

		setImmediate(() => this.spawnEntryPoint())
		this.respawnTimer = null
		this.memUsageTimer = setInterval(() => {
			if (this.analytics) {
				spawn('du', ['-s', '/src']).stdout.on('data', data => {
					const match = data.toString().match(/(\d+)/)
					if (match) {
						const sizeBytes = parseInt(match[1])
						if (!isNaN(sizeBytes)) {
							this.analytics.reportDiskUsage('watchdog', sizeBytes)
						}
					}
				})
				this.analytics.reportMemoryUsage('watchdog', process.memoryUsage().heapUsed)
				this.analytics.reportBranchesRequests(branchesRequests)
			}
		}, MEM_USAGE_INTERVAL_SECONDS * 1000) 
	}

	getLastCrash() {
		return this.lastCrash
	}

	sendMessage(name: string, args?: any[]) {
		return new Promise<Object | null>((done, _fail) => {
			if (this.child && !this.paused) {
				// send the message
				let cbid = this.nextCbId++
				this.cbMap.set(cbid, done)
				this.child.send({cbid, name, args})
			}
			else {
				done(null)
			}
		})
	}

	stopBot() {
		if (!this.paused) {
			this.watchdogLogger.info("Pausing bot")
			this.paused = true
		}

		// cancel any timers
		if (this.respawnTimer) {
			clearTimeout(this.respawnTimer)
			this.respawnTimer = null
		}

		// tell the child to shutdown
		if (this.child !== null) {
			this.child.kill('SIGINT')
		}
	}

	startBot() {
		if (this.paused) {
			this.watchdogLogger.info("Unpausing bot")
			this.paused = false
		}

		// cancel any timers
		if (this.respawnTimer) {
			clearTimeout(this.respawnTimer)
			this.respawnTimer = null
		}

		// respawn now
		if (this.child === null) {
			this.respawn(true)
		}
	}

	startServer() {
		// for now, require HTTPS vault settings - eventually have a proper dev
		// setting that can allow local testing without
		try {
			this.watchdogLogger.info('Checking for files in ' + Session.VAULT_PATH)
			fs.readdirSync(Session.VAULT_PATH)
		}
		catch (err) {
			console.log(err)
			this.statusServer.open(8877, 'http').then(() => 
				this.watchdogLogger.warn(`HTTP web server opened on port 8877`)
			)
			return
		}

		const certFiles = {
			key: fs.readFileSync(`${Session.VAULT_PATH}/${tlsKeyFilename}`, 'ascii'),
			cert: fs.readFileSync('./certs/robomerge.pem', 'ascii')
		}
		// Removed ca: [fs.readFileSync('./certs/thwate.cer')] - JR 10/4

		this.statusServer.open(args.watchdogPort, 'https', certFiles).then(() => 
			this.watchdogLogger.info(`Web server opened on port ${args.watchdogPort}`)
		)
	}

	private childExited() {
		if (!this.child) {
			return
		}
		this.child = null

		// capture crash logs
		this.lastCrash = this.watchdogLogger.getLogTail()

		// cancel callbacks
		for (const cb of this.cbMap.values()) {
			setTimeout(() => cb(null), 0)
		}
		this.cbMap.clear()
	}

	// spawn the main app
	private spawnEntryPoint() {
		this.lastSpawnStart = process.hrtime()[0]
		this.respawnTimer = null
		if (this.child !== null)
			throw new Error('child is not null')
		const args = process.argv.slice(2);
		this.watchdogLogger.info(`Spawning ${ENTRY_POINT} with args ${args}`)
		let options : ForkOptions = { stdio: ['pipe', 'pipe', 'pipe', 'ipc'] }

		// prevent passing same debugger port under VSCode (FIXME: shouldn't be necessary due to autoAttachChildProcesses)
		let opts = [...process.execArgv];
		for (let i=0;i<opts.length;++i) {
			if (opts[i].startsWith("--inspect-brk="))
				opts[i] = "--inspect";
		}
		options.execArgv = opts;
		
		// fork the child
		this.child = fork(ENTRY_POINT, args, options)
		this.child.once('close', (code, signal) => {
			this.childExited()
			if (code !== null) {
				this.watchdogLogger.warn(`Child process exited with code=${code}\n${this.lastError}`);
				if (code !== 0) {
					this.respawn()
				}
			}
			else {
				this.watchdogLogger.warn(`Child process exited with signal=${signal}`)
				this.respawn()
			}
		})
		this.child.once('error', err => {
			this.watchdogLogger.printException(err, `Error from ${ENTRY_POINT}`)
			this.childExited()
			this.respawn()
		})
		this.child.on('message', (msg) => {
			if (this.child === null)
				return
			
			// Capture Sentry information from child process. Should only recieve such a message before it dies
			if (msg.sentry && msg.error) {
				const childErr = new Error(msg.error.message)
				childErr.stack = msg.error.stack
				childErr.name = msg.error.name

				this.watchdogLogger.error(`Watchdog: ${ENTRY_POINT} sent error message for Sentry: ${msg.error.message}`)
				Sentry.withScope((scope) => {
					scope.setTag('handled', 'no')
					Sentry.captureException(childErr)
				})
				
				return
			}

			// Capture log events from child process.
			if (msg.logmsg) {
				this.watchdogLogger.addToTail(msg.logmsg)
				return
			}

			let callback = this.cbMap.get(msg.cbid)
			if (callback !== undefined) {
				this.cbMap.delete(msg.cbid)
				if (msg.error)
					this.watchdogLogger.error(msg.error)
				callback(msg.args)
			}
		})
		this.child.stderr!.on('data', (chunk) => {
			this.lastError = chunk.toString();
			process.stderr.write(chunk)
		})
		this.child.stdout!.on('data', (chunk) => {
			process.stdout.write(chunk)
		})
	}

	respawn(now?: boolean) {
		if (this.shutdown || this.paused) {
			return // shutting down or paused
		}
		if (this.respawnTimer) {
			return // already queued
		}
		if (!AUTO_RESPAWN && !now)
		{
			this.watchdogLogger.info("Not configured to auto respawn")
			return
		}

		// log respawn
		this.watchdogLogger.info('---------------------------------------------------------------------')
		let delay = now ? 0 : RESPAWN_DELAY
		this.watchdogLogger.info(`Respawning in ${delay} seconds`)
		this.respawnTimer = setTimeout(() => this.spawnEntryPoint(), delay * 1000);
	}

	shutDownProcess(sig: NodeJS.Signals) {
		this.watchdogLogger.info(`Watchdog caught ${sig} (will not reboot)`)
		if (this.shutdown) {
			this.watchdogLogger.info("Immediate exit")
			process.exit()
		}

		this.shutdown = true
		this.stopBot()

		// close the status server
		if (this.statusServer) {
			this.watchdogLogger.info('Closing Web Status server')
			this.statusServer.close().then(() =>
				this.watchdogLogger.info('Web server closed.')
			)
		}

		if (this.analytics) {
			this.analytics.stop()
		}

		clearTimeout(this.memUsageTimer)

		setTimeout(() => {
			const dump = (x: any) => (x.constructor ? x.constructor.name : x);

			const proc = process as any
			const handles = proc._getActiveHandles()
			if (handles.length !== 0) {
				this.watchdogLogger.info('Active handles: ' + handles.map(dump))
			}

			const requests = proc._getActiveRequests()
			if (requests.length !== 0) {
				this.watchdogLogger.info('Active requests: ' + requests.map(dump))
			}
		}, 3000);
	}

	private analytics: Analytics

	private respawnTimer: NodeJS.Timer | null = null
	private memUsageTimer: NodeJS.Timer
}

const watchdog = new Watchdog
async function onSignal(sig: NodeJS.Signals) {
	const signalLogger = new ContextualLogger('Watchdog Signal')
	await closeSentry(signalLogger)
	watchdog.shutDownProcess(sig)
}

process.on('SIGINT', onSignal)
process.on('SIGTERM', onSignal) // docker stop sends SIGTERM

watchdog.startServer()
