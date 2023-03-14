// Copyright Epic Games, Inc. All Rights Reserved.
import * as Sentry from '@sentry/node';
import * as fs from 'fs';
import {marked} from 'marked';
import * as querystring from 'querystring';
import { Arg, readProcessArgs } from '../common/args';
import { ContextualLogger } from '../common/logger';
import { AppInterface, ensureRegExp, getCookie, Handler, RequestOpts, WebRequest, WebServer } from '../common/webserver';
import { OperationReturnType } from './ipc';
import { postToRobomergeAlerts } from './notifications';
import { AuthData, getSentryUser, Session } from './session';
import { Status } from './status';

import * as Mustache from 'mustache'

const toc = require('markdown-toc')

// Allows instance to skip over sign-in and HTTPS
const roboserverStartupLogger = new ContextualLogger('Roboserver Startup')

// counting calls to branches just out of interest
export let branchesRequests = 0

let ENVIRONMENT: {[param: string]: any}
(() => {
	const COMMAND_LINE_ARGS: {[param: string]: (Arg<any>)} = {
		devMode: {
			match: /^(-devMode)$/,
			parse: _str => true,
			env: 'ROBO_DEV_MODE',
			dflt: false
		},
		devModeUser: {
			match: /^-devModeUser=(.+)$/,
			env: 'ROBO_DEV_MODE_USER',
			dflt: ''
		}
	}

	const env = readProcessArgs(COMMAND_LINE_ARGS, roboserverStartupLogger)
	if (!env) {
		process.exit(1)
	}
	ENVIRONMENT = env
})()

if (ENVIRONMENT.devMode) {
	roboserverStartupLogger.warn('Running in DEV_MODE')
}

function readUtf8File(path: string) {
	return new Promise<string>((done, _fail) => 
		fs.readFile(path, 'utf8', (_err: Error, content: string) => done(content)))
}

interface SecureRequestOpts extends RequestOpts {
	requiredTags?: string[]
}

function SecureHandler(verb: string, route: RegExp | string, opts?: SecureRequestOpts) {
	return (_target: RoboWebApp, _funcName: string, desc: PropertyDescriptor) => {
		const originalMethod = desc.value

		const func: any = async function (this: RoboWebApp, ...args: any[]) {
			const redirectResult = this.redirect(this.secure, opts && opts.requiredTags)
			if (redirectResult) {
				return redirectResult
			}
			return await originalMethod.call(this, ...args)
		}

		func.verb = verb
		func.route = ensureRegExp(route)
		if (opts) {
			func.opts = opts
		}

		desc.value = func
		return desc
	}
}

interface BackendFunctions {
	sendMessage: (msg: string, args?: any[]) => any,
	getLogTail: Function | null
	getLastCrash: Function
	stopBot: Function
	startBot: Function
}

export class RoboServer {

	constructor(externalUrl: string, sendMessage: (msg: string, args?: any[]) => any, getLogTail: Function | null, 
		getLastCrash: Function, stopBot: Function, startBot: Function) {
		this.server = new WebServer(this.roboserverLogger)
		this.functions = {
			sendMessage: sendMessage,
			getLogTail: getLogTail,
			getLastCrash: getLastCrash,
			stopBot: stopBot,
			startBot: startBot
		}

		//this.server.addFileMapping('/', 'index.html')
		this.server.addFileMapping('/login', 'login.html', {secureOnly: true})
		this.server.addFileMapping('/allbots', 'allbots.html')
		this.server.addFileMapping('/js/*.wasm', 'bin/$1.wasm.gz', {
			filetype: "application/wasm", 
			headers: [
				['Cache-Control', 'max-age=' + 60*60*24*7],
				['Content-Encoding', 'gzip']
			]
			})
		this.server.addFileMapping('/js/*.js', 'js/$1.js')
		this.server.addFileMapping('/css/*.css', 'css/$1.css')
		this.server.addFileMapping('/img/*.png', 'images/$1.png')
		this.server.addFileMapping('/img/*.jpg', 'images/$1.jpg')
		this.server.addFileMapping('/img/*.gif', 'images/$1.gif')
		this.server.addFileMapping('/img/background/*.png', 'images/background/$1.png')
		this.server.addFileMapping('/img/background/*.jpg', 'images/background/$1.jpg')
		this.server.addFileMapping('/img/background/*.gif', 'images/background/$1.gif')

		// Fonts
		this.server.addFileMapping('/webfonts/*.eot', 'webfonts/$1.eot', {filetype: 'application/octet-stream'})
		this.server.addFileMapping('/webfonts/*.svg', 'webfonts/$1.svg', {filetype: 'application/octet-stream'})
		this.server.addFileMapping('/webfonts/*.ttf', 'webfonts/$1.ttf', {filetype: 'application/octet-stream'})
		this.server.addFileMapping('/webfonts/*.woff', 'webfonts/$1.woff', {filetype: 'application/octet-stream'})
		this.server.addFileMapping('/webfonts/*.woff2', 'webfonts/$1.woff2', {filetype: 'application/octet-stream'})
		
		this.server.addApp(RoboWebApp, (req: WebRequest) => new RoboWebApp(req, this.server.secure, this.functions, externalUrl, this.roboserverLogger));
	}

	open(...args: any[]) {
		return this.server.open(...args)
	}

	close() {
		return this.server.close()
	}

	isSecure() {
		return this.server.secure
	}

	// Don't create a child from Watchdog process, just make a new toplevel logger
	private readonly roboserverLogger: ContextualLogger = new ContextualLogger('RoboServer')
	private server: WebServer
	private functions: BackendFunctions
}

class RoboWebApp implements AppInterface {
	private readonly webAppLogger: ContextualLogger
	private authData: AuthData | null

	// could put access to various parts of request in an App base class?
	constructor(
		private request: WebRequest, 
		public secure: boolean, 
		private functions: BackendFunctions, 
		private externalUrl: string, 
		parentLogger: ContextualLogger) {
		this.webAppLogger = parentLogger.createChild('WebApp')
	}

	@SecureHandler('GET', '/', {filetype: 'text/html'}) 
	indexPage() {
		return readUtf8File('public/index.html')
	}

	@Handler('GET', '/preview/*', {filetype: 'text/html'}) 
	previewNoBot(clStr: string) {
		return this.preview(clStr)
	}

	@Handler('GET', '/preview.html', {filetype: 'text/html'}) 
	previewNoBotHtml() {
		const clStr = this.request.url.searchParams.get('cl')
		return clStr ? this.preview(clStr) : {statusCode: 400, message: "Query argument cl required"}
	}

	@Handler('GET', '/preview/*/*', {filetype: 'text/html'}) 
	async preview(clStr: string, singleBot?: string) {

		const cl = parseInt(clStr)
		if (isNaN(cl)) {
			throw new Error(`Failed to parse alleged CL '${clStr}'`)
		}

		let query = `/preview?cl=${cl}`
		if (singleBot) {
			query += '&bot=' + singleBot
		}

		const template = await readUtf8File('public/preview.html')
		return Mustache.render(template, {cl, query})
	}

	@Handler('GET', '/preview') 
	previewData() {
		const queryObj: {[key: string]: string} = {}
		for (const [key, val] of this.request.url.searchParams) {
			queryObj[key] = val
		}

		return this.sendMessage('preview', [queryObj.cl, queryObj.bot || ''])
	}

	private static getMarkdownRenderer(): marked.Renderer {
		// Add some bootstrap classes to enhance rendering
		const renderer = new marked.Renderer({
			gfm: true,
			headerIds: true,
			sanitizer: require('sanitize-html').sanitizeHtml,
			silent: true
		})

		renderer.table = function (header, body): string {
			let html = marked.Renderer.prototype.table.call(this, header, body)

			html = html.replace('<table>', '<table class="table table-responsive table-hover table-bordered table-striped" align="center">')
			html = html.replace('<thead>', '<thead class="thead-light">')

			return html
		}

		renderer.image = function (href, title, text): string {
			const html = marked.Renderer.prototype.image.call(this, href, title, text)
			//const html = (new marked.Renderer()).image(href, title, text)
			return html.replace('<img', '<img class="rounded mx-auto d-block"')
		}

		return renderer
	}

	private static async renderMarkdownFile(filename: string) {
		return new Promise<string>((done, fail) => {
			const helpWrapper = fs.readFileSync('public/markdown-template.html', 'utf8')
			let markdown = fs.readFileSync(filename, 'utf8')

			// Generate a table of contents and replace the designated token (<!--toc-->) with it
			const generatedTOC = toc(markdown).content
			markdown = markdown.replace(/<!--\s*toc\s*-->/ig, generatedTOC)

			marked.parse(markdown, {renderer: RoboWebApp.getMarkdownRenderer()}, (error, parseResult) => {
				if (error) {
					fail(error)
					return
				}
				
				done(helpWrapper.replace('[RENDERED_MARKDOWN_GOES_HERE]', parseResult))
			})
		})
	}

	@SecureHandler('GET', '/help', {filetype: 'text/html'}) 
	renderHelp() { return RoboWebApp.renderMarkdownFile('README.md') }

	@SecureHandler('GET', '/contact', {filetype: 'text/html'}) 
	renderContactInfo() { return RoboWebApp.renderMarkdownFile('ContactInfo.md') }

	@SecureHandler('GET', '/api/logs', {requiredTags: ['fte']})
	getLogs() {
		// logs are only available in IPC mode
		return this.functions.getLogTail ? this.functions.getLogTail() : '<logs not available>'
	}

	@SecureHandler('GET', '/api/last_crash')
	getLastCrash() {
		return this.functions.getLastCrash && this.functions.getLastCrash() || 'No crashes (yay!).'
	}

	@SecureHandler('POST', '/api/control/start', {requiredTags: ['fte']})
	startBot() {
		this.webAppLogger.info('Restart requested by: ' + this.authData!.user)
		this.functions.startBot()
		return 'OK'
	}

	@SecureHandler('POST', '/api/control/stop', {requiredTags: ['fte']})
	stopBot() {
		this.webAppLogger.info('Emergency stop requested by: ' + this.authData!.user)

		this.functions.stopBot()
		return 'OK'
	}

	// Used in automation, return simple true or false
	@SecureHandler('GET', '/api/control/isrunning/*')
	async getIsRunning(botname: string): Promise<boolean> {
		const result = await this.sendMessage('getIsRunning', [botname])
		return result.data ? new Boolean(result.data).valueOf() : false
	}

	@SecureHandler('POST', '/api/control/restart-bot/*', {requiredTags: ['fte']})
	async restartBot(botname: string) {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		return await this.sendMessage('restartBot', [botname, this.authData.user])
	}

	// not SecureHandler, so it doesn't get redirected
	@Handler('POST', '/dologin')
	async login() {
		if (!this.request.reqData) {
			return {statusCode: 400, message: 'no log-in data received'}
		}

		const creds = querystring.parse(this.request.reqData)
		if (!creds.user || Array.isArray(creds.user) || !creds.password || Array.isArray(creds.password)) {
			return {statusCode: 400, message: 'invalid log-in data'}
		}
		
		const token = await Session.login({user: creds.user, password: creds.password}, this.webAppLogger);
		return token || {statusCode: 401, message: 'invalid credentials'};
	}

	@SecureHandler('PUT', '/api/control/verbose/*')
	async setVerbose(onOff: string): Promise<OperationReturnType> {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}
		return await this.sendMessage('setVerbose', [onOff.toLowerCase() === 'on'])
	}

	//@SecureHandler('POST', '/api/control/verbosity/*', {requiredTags: ['admin']})
	@SecureHandler('PUT', '/api/control/verbosity/*', {requiredTags: ['admin']})
	async setVerbosity(level: string): Promise<OperationReturnType> {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		return await this.sendMessage('setVerbosity', [level.toLowerCase()])
	}

	@SecureHandler('GET', '/api/user/workspaces')
	async getWorkspaces(): Promise<OperationReturnType> {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		return await this.sendMessage('getWorkspaces', [this.authData.user])
	}

	@SecureHandler('GET', '/api/branches')
	async getBranches() {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		++branchesRequests

		const setDebugData = (scope: Sentry.Scope, status: any) => {
			scope.setTag('operation', 'getBranches')
			scope.setUser(getSentryUser(this.authData!))
			if (!status) {
				scope.setExtra('status', status)
			} else {
				scope.setExtra('status', JSON.stringify(status))
				scope.setExtra('status.statusCode', status.statusCode)
			}

		}

		// Status might be null if sendMessage fails, in which case we send only auth data
		const status: any | null = await this.sendMessage('getBranches', [])
		const onlyAuthData = {
			branches: [], started: false,
			user: {
				userName: this.authData.user,
				displayName: this.authData.displayName,
				privileges: this.authData.tags
			}
		}

		if (!status) {
			Sentry.withScope((scope) => {
				setDebugData(scope, status)
				Sentry.captureMessage('getBranches returned a null or undefined result', Sentry.Severity.Warning)
			})
			return onlyAuthData
		}
		else if (status.statusCode && status.statusCode === 503) { // Handle 503 as Robomerge is still starting up
			return onlyAuthData
		}
		else if (status.statusCode && status.statusCode !== 200) {
			// Unknown error case. Log it in Sentry but continue
			Sentry.withScope((scope) => {
				setDebugData(scope, status)
				Sentry.captureMessage('Warning: getBranches returned a status code that was neither 200 nor 503')
			})
			return onlyAuthData
		}

		const statusFromIPC = Status.fromIPC(status, this.webAppLogger)

		if (!statusFromIPC) {
			// This shouldn't return null since we handled error cases earlier. Log it in Sentry but continue
			Sentry.withScope((scope) => {
				setDebugData(scope, status)
				Sentry.captureMessage('Status.fromIPC() returned a null result') // This shouldn't happen after our error catching.
			})
			return onlyAuthData
		}
		
		return statusFromIPC.getForUser(this.authData) 
	}

	@SecureHandler('GET', '/api/bot/*/branch/*')
	async getBranch(botname: string, branchname: string) {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		const branchStatus: OperationReturnType | null = await this.sendMessage('getBranch', [botname, branchname])

		// sendMessage succeeds
		if (branchStatus) {
			const filteredStatus = Status.fromIPC(branchStatus, this.webAppLogger).getForUser(this.authData)
			
			if (filteredStatus.branches.length > 0) {
				return {
					user: {
						userName: this.authData.user,
						displayName: this.authData.displayName,
						privileges: this.authData.tags
					},
					branch: filteredStatus.branches[0]
				}
			}

			// No branches found (or authorized)
			return {
				statusCode: 400,
				message: `No branch found for bot "${botname}" and branch "${branchname}"`
			}
		}
		else {
			// Server error
			return {
				statusCode: 500,
				message: `Robomerge failed to retrieve branch information for ${botname}::${branchname}.`
			}
		}
	}

	@SecureHandler('POST', '/api/op/bot/*/node/*/edge/*/op/*')
	async edgeOp(botname: string, nodeName: string, edgeName: string, edgeOp: string) {
		return await this.sendMessage('doEdgeOp', [botname, nodeName, edgeName, edgeOp, this.getQueryFromSecure()])
	}

	@SecureHandler('POST', '/api/op/bot/*/node/*/op/*')
	async nodeOp(botname: string, nodeName: string, nodeOp: string) {
		return await this.sendMessage('doNodeOp', [botname, nodeName, nodeOp, this.getQueryFromSecure()])
	}

	@SecureHandler('GET', '/api/p4tasks')
	async getP4Tasks() {
		return (await this.sendMessage('getp4tasks')).data || []
	}

	@SecureHandler('POST', '/api/crashserver', {requiredTags: ['admin']})
	async crashServer() {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		const msg = `${this.authData.user} requested total Robomerge crash for ${this.externalUrl}.`
		this.webAppLogger.error(msg)
		await postToRobomergeAlerts(msg)
		throw new Error(msg)
	}

	@SecureHandler('POST', '/api/crashapi', {requiredTags: ['admin']})
	async crashAPI() {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		this.webAppLogger.error(`API Crash requested by ${this.authData.user}`)
		return this.sendMessage('crashAPI', [this.authData.user])
	}

	@SecureHandler('POST', '/api/bot/*/crashgraphBot', {requiredTags: ['admin']})
	async crashGraphBot(botname: string) {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		this.webAppLogger.error(`GraphBot crash of ${botname} requested by ${this.authData.user}`)
		return await this.sendMessage('crashGraphBot', [botname, this.authData.user])
	}

	// Simple test message which assumes user has an email address taking the form of user@companyname.com
	// Example usage:
	// curl -X POST -k -v --cookie "auth=AUTH_COOKIE_FROM_BROWSER" https://robomerge.companyname.net/api/test/directmessage/jake.romigh
	@SecureHandler('POST', '/api/test/directmessage/*', {requiredTags: ['admin']})
	sendTestDirectMessage(user : string) {
		this.webAppLogger.verbose('Received request for direct message')
		return this.sendMessage('sendTestDirectMessage', [user])
	}

	@SecureHandler('GET', '/debug/persistence/*')
	async getPersistence(botname: string) {
		// hack in query here
		if (botname === 'branches-requests') {
			return branchesRequests
		}

		const result = await this.sendMessage('getPersistence', [botname])
		if (!result.data) {
			throw new Error("unexpected data format")
		}
		return result.data
	}

	@Handler('GET', '/api/trace-route')
	async traceRoute() {
		const query = this.getQuery()
		if (!query.cl || !query.from || !query.to) {
			return {statusCode: 400, message: '"cl", "from" and "to" query arguments required'}
		}
		const result = await this.sendMessage('traceRoute', [query])
		return result.data ? {...result, success: true, route: result.data} :
			{...result, success: false, code: result.message}
			
	}

	@SecureHandler('GET', '/debug/dump-graph')
	async dumpGraph() {
		return (await this.sendMessage('dumpGraph')).data
	}

	// https://localhost:4433/op/acknowledge?bot=TEST&branch=Main&cl=1237983421
	@SecureHandler('GET', '/op/*', {filetype: 'text/html'})
	async nodeOpLanding(operation: string) {
		let query = this.getQueryFromSecure()

		// Require these query variables: bot, branch and CL
		if (!query["bot"]) {
			return {statusCode: 400, message: `ERROR: Must specify bot for node operation.`}
		}
		if (!query["branch"]) {
			return {statusCode: 400, message: `ERROR: Must specify branch for node operation.`}
		}
		if (!query["cl"]) {
			return {statusCode: 400, message: `ERROR: Must specify changelist for node operation.`}
		}

		switch (operation) {
			case "acknowledge":
				return await readUtf8File('public/acknowledge.html')

			case "skip":
				return await readUtf8File('public/skip.html')
			
			case "createshelf":
				return await readUtf8File('public/createshelf.html')

			case "stomp":
				return await readUtf8File('public/stomp.html')

			default:
				return {statusCode: 404, message: `Unknown node operation requested: "${operation}"`}
		}
	}

	public redirect(secureServer: boolean, requiredTags?: string[]) {
		if (secureServer) {
			const authToken = getCookie(this.getCookies(), 'auth')
			if (authToken) {
				this.authData = Session.tokenToAuthData(authToken)
			}

			if (!this.authData) {
				let redirectString = this.request.url.pathname
				if (this.request.url.search) {
					redirectString = `${redirectString}${this.request.url.search}`
				}

				return {statusCode: 302, message: 'Must log in', headers: [
					['Location', `/login?redirect=${encodeURIComponent(redirectString)}`]
				]}
			}
		}
		else if (ENVIRONMENT.devMode) {
			const user = ENVIRONMENT.devModeUser || 'dev'
			this.authData = {
				user,
				displayName: `${user} (dev mode)`,
				tags: new Set(['fte', 'admin'])
			}
		}
		else {
			return {statusCode: 403 /* Forbidden */, message: 'Sign-in over HTTPS required'}
		}

		if (requiredTags) {
			for (const tag of requiredTags) {
				if (!this.authData.tags.has(tag)) {
					return {statusCode: 403 /* Forbidden */, message: `Tag <${tag}> required`}
				}
			}
		}

		// fine, carry on
		return null
	}

	private sendMessage(msg: string, args?: any[]): OperationReturnType | Promise<OperationReturnType> {
		// Standard way to capture sendMessage errors
		const captureError = (err: Error) => {
			if (err) {
				Sentry.withScope((scope) => {
					scope.setTag('message', msg)
					scope.setUser(this.getSentryUser())
					if (args) {
						args.forEach((arg, index) => { scope.setExtra(`arg${index}`, arg) })
					}
					Sentry.captureException(err)
				})
			}
		}

		let returnVal
		try {
			returnVal = this.functions.sendMessage(msg, args)
		}
		catch (err) {
			captureError(err)
			returnVal = { statusCode: 500, message: `Error processing request` }
		}

		// Some implementations of sendMessage are Promises. Check for imbedded errors
		if (returnVal instanceof Promise) {
			returnVal.then((result) => {
				if (result && result.data && result.data.error) {
					const err = new Error(result.data.error.message)
					err.stack = result.data.error.stack
					err.name = result.data.error.name
					captureError(err)
				}
			})
		}

		return returnVal
	}

 	private getCookies() {
		return decodeURIComponent(this.request.cookies).split(';')
	}

	private getQueryFromSecure() : {[key: string]: string} {
		if (!this.authData) {
			throw new Error('Secure call but no auth data?')
		}

		const queryObj: {[key: string]: string} = {}
		for (const [key, val] of this.request.url.searchParams) {
			queryObj[key] = val
		}

		// overwrite any incoming 'who' on query string - should not be affected by client
		queryObj.who = this.authData.user
		return queryObj
	}

	private getQuery() : {[key: string]: string} {
		if (this.authData) {
			throw new Error('do not use getQuery for secure calls')
		}

		const queryObj: {[key: string]: string} = {}
		for (const [key, val] of this.request.url.searchParams) {
			queryObj[key] = val
		}
		return queryObj
	}

	getSentryUser() : Sentry.User {
		if (!this.authData) {
			throw new Error('Requesting security information for sentry, but no auth data?')
		}

		return getSentryUser(this.authData)
	}
}

export interface BlockageNodeOpUrls {
	acknowledgeUrl: string
	createShelfUrl?: string
	skipUrl?: string
	stompUrl?: string
}
export class OperationUrlHelper {
	static createAcknowledgeUrl(externalRobomergeUrl: string, botname: string, branchname: string, changelistNum: string, edge?: string) {
		return `${externalRobomergeUrl}/op/acknowledge?` +
			`bot=${encodeURIComponent(botname)}`+ 
			`&branch=${encodeURIComponent(branchname)}` +
			`&cl=${encodeURIComponent(changelistNum)}` +
			(edge ? `&edge=${encodeURIComponent(edge)}` : "")
	}
	
	static createCreateShelfUrl(externalRobomergeUrl: string, botname: string, branchname: string, changelistNum: string, target: string, targetStream?: string) {
		let url = `${externalRobomergeUrl}/op/createshelf?` +
			`bot=${encodeURIComponent(botname)}`+ 
			`&branch=${encodeURIComponent(branchname)}` +
			`&cl=${encodeURIComponent(changelistNum)}` +
			`&target=${encodeURIComponent(target)}` +
			`${targetStream ? `&targetStream=${encodeURIComponent(targetStream)}` : ""}`
	
		return url
	}
	
	static createSkipUrl(externalRobomergeUrl: string, botname: string, branchname: string, changelistNum: string, targetEdge: string, reason?: string) {
		return `${externalRobomergeUrl}/op/skip?` +
			`bot=${encodeURIComponent(botname)}`+ 
			`&branch=${encodeURIComponent(branchname)}` +
			`&cl=${encodeURIComponent(changelistNum)}` +
			`&edge=${encodeURIComponent(targetEdge)}` +
			(reason ? `&reason=${encodeURIComponent(reason)}` : "")
	}

	static createStompUrl(externalRobomergeUrl: string, botname: string, branchname: string, changelistNum: string, target: string) {
		return `${externalRobomergeUrl}/op/stomp?` +
			`bot=${encodeURIComponent(botname)}`+ 
			`&branch=${encodeURIComponent(branchname)}` +
			`&cl=${encodeURIComponent(changelistNum)}` +
			`&target=${encodeURIComponent(target)}`
	}
	
}
