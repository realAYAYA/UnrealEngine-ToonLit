// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from "../common/logger";
import { Change, PerforceContext } from "../common/perforce";
import { gatesSame, GateEventContext, GateInfo } from "./branch-interfaces"
import { DAYS_OF_THE_WEEK, CommonOptionFields, IntegrationWindowPane } from "./branchdefs"
import { BotEventTriggers } from "./events"
import { Context } from "./settings"


const GATE_INFO_KEY = 'gate-info'
const BYPASS_KEY = 'bypass'


type GateOptions = Partial<CommonOptionFields>

export type GateContext = {
	options: GateOptions
	p4: PerforceContext | null // only allowed to be null for unit tests
	logger: ContextualLogger
}

async function getRequestedGateCl(context: GateContext, previousGateInfo?: GateInfo | null): Promise<GateInfo | null> {

	const lastGoodCLPath = context.options.lastGoodCLPath
	if (!lastGoodCLPath) {
		return null
	}

	if (typeof(lastGoodCLPath) === 'number') {
		return {cl: lastGoodCLPath}
	}

	let clString = null
	try {
		clString = await context.p4!.print(lastGoodCLPath)
	}
	catch (err) {
		context.logger.printException(err, `Error reading last good CL from ${lastGoodCLPath}`)
	}
	if (!clString) {
		return null
	}

	let clInfo: any;
	try {
		clInfo = JSON.parse(clString)
	}
	catch (err) {
		context.logger.printException(err, `Error parsing last good CL from ${lastGoodCLPath}`)
	}

	if (!clInfo.Change) {
		context.logger.warn(`No last good CL found in ${lastGoodCLPath}`)
		return null
	}

	const cl = typeof(clInfo.Change) === 'string' ? parseInt(clInfo.Change) : clInfo.Change
	if (!Number.isInteger(cl) || cl < 0) {
		context.logger.warn(`No last good CL found in ${lastGoodCLPath}`)
		return null
	}

	const result: GateInfo = {cl}
	// new goodCL: update link and date
	if (clInfo.Url) {
		result.link = clInfo.Url
	}
	if (previousGateInfo && previousGateInfo.cl === cl) {
		if (previousGateInfo.timestamp) {
			result.timestamp = previousGateInfo.timestamp
		}
	}
	else {
		try {
			const description = await context.p4!.describe(cl)
			if (description.date) {
				result.timestamp = description.date.getTime()
			}
		}
		catch (err) {
			console.log('error getting gate CL description', err)
		}
	}

	if (clInfo.integrationWindow) {
		result.integrationWindow = clInfo.integrationWindow
		if (clInfo.invertIntegrationWindow) {
			result.invertIntegrationWindow = true
		}
	}

	return result
}

type EventTriggersAndStuff = GateEventContext & {
	eventTriggers: BotEventTriggers
}

type CurrentWindowStats = {
	targetCl: number
	pending: number
	integrated: number
	blockages: number
}

export class Gate {
	constructor(	private eventContext: EventTriggersAndStuff | DummyEventTriggers,
					private context: GateContext, private persistence?: Context) {
		this.loadFromPersistence()
	}

	bypass = false

	private processGateChange(newGate: GateInfo) {
		// degrees of freedom:
		// 	order of lastCl/gate.cl/newGate.cl (L/G/N)
		// 	empty queue (!Q)

		// 	first block is N <= G || (G <= L && !Q)

		// 	so remainder if N > G && G > L

		if (this.currentGateInfo) {
			// check for special case of new gate being before (or equal to) current catch-up gate
			//	replace and wipe queue

			if (newGate.cl <= this.currentGateInfo.cl) {
				// report if we were catching up and the replacement gate means we're now caught up
				if (this.lastCl < this.currentGateInfo.cl && this.lastCl >= newGate.cl) {
					this.context.logger.info('Caught up due to earlier gate ' +
						`(previous: ${this.currentGateInfo.cl}, new: ${newGate.cl},lastCl = ${this.lastCl}`)
					this.reportCaughtUp()
				}
				this.currentGateInfo = newGate
				this.queuedGates.length = 0
				return
			}

			// case where we're already caught up and just need to replace the gate
			if (this.currentGateInfo.cl <= this.lastCl && this.queuedGates.length === 0) {
				this.currentGateInfo = null
				this.queuedGates.push(newGate)
				return
			}
		}

		// now we know we're either waiting for window or new gate is after catch-up gate, so update the queue

		// e.g.
		//	- queued gates at CLs [5, 8, 15]
		//	- new gate at CL 7
		//	- chopIndex = 1
		//	- set length to 1, queue becomes [5]
		//	- push CL 7 so queue becomes [5, 7]
		const chopIndex = this.queuedGates.findIndex(queued => queued.cl >= newGate.cl)
		if (chopIndex >= 0) {
			this.queuedGates.length = chopIndex
		}
		this.queuedGates.push(newGate)
	}

	getMostRecentGate() {
		return this.queuedGates.length > 0 ? this.queuedGates[this.queuedGates.length - 1] : this.currentGateInfo
	}

	async tick() {
		if (this.lastCl < 0) {
			// do nothing until cl has been set
			return
		}

		let dirty = false
		try {
			if (this.bypass && this.nowIsWithinAllowedCatchupWindow()) {
				this.context.logger.info('Clearing bypass on entering an integration window')
				this.bypass = false
				dirty = true
			}

			const mostRecentGate = this.getMostRecentGate()

			const gateInfo = await getRequestedGateCl(this.context, mostRecentGate)
			if (!gateInfo) {
				if (this.currentGateInfo) {
					// we were waiting for a gate, so unpause and clear gate info
					if (this.isGateOpen()) {
						this.context.logger.info(`Gate ${this.currentGateInfo.cl} removed while catching up`)
						this.reportCaughtUp()
					}

					this.currentGateInfo = null
					this.queuedGates.length = 0
					dirty = true
				}

				return
			}

			if (!mostRecentGate) {
				// ooh look, our first gate
				this.queuedGates.push(gateInfo)
				dirty = true
			}
			else if (!gatesSame(mostRecentGate, gateInfo)) {
				this.processGateChange(gateInfo)
				dirty = true
			}

			// sanity check
			if (!this.currentGateInfo && this.queuedGates.length === 0) {
				throw new Error('seem to have ignored gate!')
			}

			// if no current and in window, kick off new
			if (!this.currentGateInfo && this.queuedGates.length > 0) {
				if (this.tryNextGate(this.lastCl)) {
					this.reportCatchingUp()
					dirty = true
				}
			}
		}
		finally {
			if (dirty) {
				this.persist()
			}
		}
	}

	preIntegrate(cl: number) {
		if (this.currentGateInfo) {
			this.context.logger.verbose(`${this.lastCl} -> ${cl} ${this.currentGateInfo.cl}`)
		}

		if (!this.currentGateInfo || cl <= this.currentGateInfo.cl || this.currentGateInfo.cl <= this.lastCl) {
			return null
		}

		// we're waiting for currentGateInfo.cl but got a higher cl

		// if there are no more gates, basically we drag lastCl forward to
		// currentGateInfo.cl so isGateOpen returns false

		this.context.logger.verbose(`Current gate info: ${this.currentGateInfo && this.currentGateInfo.cl}`)

		if (this.tryNextGate(cl)) {
			return null
		}

		// new gate, check again
		if (!this.currentGateInfo || cl <= this.currentGateInfo.cl || this.currentGateInfo.cl <= this.lastCl) {
			return null
		}

		this.reportCaughtUp()
		this.context.logger.info(`Adjusting cl from ${this.lastCl} to ${this.currentGateInfo.cl} ` +
														`to match gate due to encountering CL#${cl}`)
		this.setLastCl(this.currentGateInfo.cl)
		this.persist()
		return this.currentGateInfo.cl
	}

	/**
	 Assumptions: currentGateInfo is valid and incoming cl > currentGateInfo.cl
	 @return whether we started catching up to a new gate
	 */
	private tryNextGate(cl: number) {
		if (this.queuedGates.length === 0) {
			this.context.logger.verbose('nothing queued')
			return false
		}

		const nextGateIndex = this.queuedGates.findIndex(queued => queued.cl > cl)
		if (nextGateIndex < 0) {
			this.context.logger.verbose('all done')

			// all done (incoming CL is after all queued gates)
			this.currentGateInfo = this.queuedGates[this.queuedGates.length - 1]
			this.queuedGates.length = 0
			return false
		}

		if (!this.bypass && !this.nowIsWithinAllowedCatchupWindow()) {
			// want to make this an info log, but would spam every tick at the moment
			// this.context.logger.verbose('delaying gate catch-up due to configured window')

			// wait for next window before catching up with queued gates
			this.currentGateInfo = null
			return false
		}

		this.context.logger.verbose('next!')

		// on to next gate
		this.currentGateInfo = this.queuedGates[nextGateIndex]
		this.queuedGates = this.queuedGates.slice(nextGateIndex + 1)
		return true
	}

	updateLastCl(changesFetched: Change[], changeIndex: number, targetCl?: number) {
		const cl = changesFetched[changeIndex].change

		let notifyCaughtUp = false
		if (this.currentGateInfo) {
			// ignore going backwards; did we reach the gate?
			if (cl > this.lastCl && cl >= this.currentGateInfo.cl) {
				if (!this.tryNextGate(cl)) {
					// either all done or waiting for next window to restart catching up
					notifyCaughtUp = true
				}
			}
		}

		this.setLastCl(cl)

		this.numChangesRemaining = this.calcNumChangesRemaining(changesFetched, changeIndex)

		if (this.currentWindowStats) {
			if (targetCl) {
				this.currentWindowStats.targetCl = targetCl
				++this.currentWindowStats.integrated
			}
			this.currentWindowStats.pending = this.numChangesRemaining
		}

		if (notifyCaughtUp) {
			this.reportCaughtUp()
		}
	}

	onBlockage() {
		if (this.currentWindowStats) {
			++this.currentWindowStats.blockages
		}
	}

	numChangesRemaining = 0

	calcNumChangesRemaining(changesFetched: Change[], changeIndex: number) {
		const mostRecentGate = this.getMostRecentGate()
		if (!mostRecentGate) {
			return changesFetched.length - changeIndex - 1
		}
		
		if (this.lastCl >= mostRecentGate.cl) {
			return 0
		}

		// e.g. say relevant changes sequential multiples of 10
		//  integrating changes 30->80 inclusive, gate set to 60
		//  index 0 is 30, 1 is 40 etc, so gateIndex to find is 3
		//  non-exact matches:
		//		if gate file was set to 45, we'll integrate up to 40, so find first CL >= to gate CL
		let gateIndex = changeIndex
		for (; gateIndex < changesFetched.length; ++gateIndex) {
			if (changesFetched[gateIndex].change >= mostRecentGate.cl) {
				break
			}
		}
		return gateIndex - changeIndex;
	}

	isGateOpen() {
		return !this.getGateClosedMessage()
	}

	getGateClosedMessage() {
		// gate prevents integrations in two cases:
		//	- we've caught up with the most recent gate
		//	- integration window is preventing catching up to the next gate

		if (this.currentGateInfo) {
			// should show last cl in queue on web page, null means caught up
			return this.lastCl >= this.currentGateInfo.cl ? 'waiting for CIS' : null
		}

		// null means no gate
		return this.queuedGates.length > 0 ? 'waiting for integration window' : null
	}

	applyStatus(outStatus: { [key: string]: any }) {
		// for now, just equivalent of what was there before
		// @todo info about intermediate gate

		const mostRecentGate = this.getMostRecentGate()
		if (mostRecentGate) {
			outStatus.lastGoodCL = mostRecentGate.cl
			outStatus.lastGoodCLJobLink = mostRecentGate.link
			if (mostRecentGate.timestamp) {
				outStatus.lastGoodCLDate = new Date(mostRecentGate.timestamp)
			}
		}

		const closedMessage = this.getGateClosedMessage()
		if (closedMessage) {
			outStatus.gateClosedMessage = closedMessage
		}

		if (this.nextWindowOpenTime) {
			outStatus.nextWindowOpenTime = this.nextWindowOpenTime
		}

		if (this.bypass) {
			outStatus.windowBypass = true
		}
	}

	logSummary() {
		const logger = this.context.logger
		logger.info('current gate: ' + (this.currentGateInfo ? this.currentGateInfo.cl.toString() : 'none'))
		logger.info(`queued: ${this.queuedGates.length}`)
		logger.info(`last cl: ${this.lastCl}`)
	}

	private nowIsWithinAllowedCatchupWindow() {
		this.nextWindowOpenTime = null

		const mostRecentGate = this.getMostRecentGate()
		let integrationWindow: IntegrationWindowPane[] | null = null
		let invert = false

		if (mostRecentGate && mostRecentGate.integrationWindow) {
			integrationWindow = mostRecentGate.integrationWindow
			invert = !!mostRecentGate.invertIntegrationWindow
		}
		else if (this.context.options.integrationWindow) {
			integrationWindow = this.context.options.integrationWindow
			invert = !!this.context.options.invertIntegrationWindow
		}

		if (!integrationWindow) {
			return true
		}

		const now = new Date
		const nowHour = now.getUTCHours()
		const nowHourInWeek = now.getUTCDay() * 24 + nowHour
		let inWindow = false

	outer:
		for (const pane of integrationWindow) {
			if (pane.daysOfTheWeek) {
				for (const day of pane.daysOfTheWeek) {
					const dayIndex = DAYS_OF_THE_WEEK.indexOf(day)
					if (dayIndex < 0 || dayIndex > 6) {
						throw new Error('invalid day, should have been caught in validation')
					}

					const paneStart = dayIndex * 24 + pane.startHourUTC

					// same logic as for daily below, except 168 hours per week
					if ((nowHourInWeek + 168 - paneStart) % 168 < pane.durationHours) {
						inWindow = true
						break outer
					}
				}
			}
			else {
				// subtlely going over midnight, e.g. start at 11pm for 4 hours
				// say current time is 1am, we do (1 + (24 - 11)) % 24, and see that it is < 4
				if ((nowHour + 24 - pane.startHourUTC) % 24 < pane.durationHours) {
					inWindow = true
					break outer
				}
			}
		}

		if (invert) {
			inWindow = !inWindow
		}

		if (!inWindow && !invert && integrationWindow.length > 0) {
			// handling everything except inverted ranges
			let earliestHour = 1000 // just has to be two weeks or more
			const consider = (dayIndex: number, pane: IntegrationWindowPane) => {
				let hourInWeek = dayIndex * 24 + pane.startHourUTC
				if (hourInWeek < nowHourInWeek) {
					hourInWeek += 168
				}
				earliestHour = Math.min(earliestHour, hourInWeek)				
			}
			for (const pane of integrationWindow) {
				if (pane.daysOfTheWeek) {
					for (const day of pane.daysOfTheWeek) {
						consider(DAYS_OF_THE_WEEK.indexOf(day), pane)
					}
				}
				else {
					const nowDay = now.getUTCDay()
					// covers window being before or after now
					consider(nowDay, pane)
					consider((nowDay + 1) % 7, pane)
				}
			}

			// for now, only provide message if we have one non-inverted window
			// more general solution to follow
			this.nextWindowOpenTime = now
			this.nextWindowOpenTime.setUTCHours(earliestHour - now.getUTCDay() * 24, 0, 0, 0);
		}

		return inWindow
	}

	nextWindowOpenTime: Date | null = null

	private reportCatchingUp() {
		const mostRecent = this.getMostRecentGate()
		if (!mostRecent) {
			throw new Error('what are we catching up to?')
		}
		this.eventContext.eventTriggers.reportBeginIntegratingToGate({
			context: this.eventContext,
			info: mostRecent,
			changesRemaining: this.numChangesRemaining
		})

		this.currentWindowStats = Gate.newCurrentWindowStats()
	}

	private reportCaughtUp() {
// log stats here for now, but if it works out, use the type in the event
		this.context.logger.info('Catch-up window stats: ' + (this.currentWindowStats
			? JSON.stringify(this.currentWindowStats) : '<null>'))

		this.eventContext.eventTriggers.reportEndIntegratingToGate({
			context: this.eventContext,
			targetCl: this.currentWindowStats ? this.currentWindowStats.targetCl : -1
		})

		this.currentWindowStats = null
	}

	persist() {
		if (!this.persistence) {
			return
		}

		const data: any = {version: 1}
		if (this.currentGateInfo) {
			data.current = this.currentGateInfo
		}
		if (this.queuedGates.length > 0) {
			data.queued = this.queuedGates
		}
		data.lastCl = this.lastCl
		this.persistence.set(GATE_INFO_KEY, data)
		this.persistence.set(BYPASS_KEY, this.bypass)
	}

	private loadFromPersistence() {
		if (!this.persistence) {
			return
		}
		const saved = this.persistence.get(GATE_INFO_KEY)
		if (saved) {

			this.context.logger.info(`Restoring saved gate info: current ${saved.current && saved.current.cl}`)

			const version = saved.version || 0
			if (saved.lastCl) {
				this.setLastCl(saved.lastCl)
			}

			if (saved.current) {
				this.currentGateInfo = saved.current

				if (saved.lastCl && saved.lastCl > saved.current.cl) {
					this.reportCatchingUp()
				}
			}

			if (version < 1) {
				if (saved.current) {
					delete saved.current.date
				}
				return
			}

			if (saved.queued) {
			// need to convert strings to dates for one thing
				this.context.logger.info('Queue: ' + saved.queued.map((info: GateInfo) => info.cl).join(', '))
				this.queuedGates = saved.queued
			}
		}
		this.bypass = !!this.persistence.get(BYPASS_KEY)
	}

	getEventContextForTests() {
		return this.eventContext
	}

	get lastCl() {
		return this.eventContext.edgeLastCl
	}

	private setLastCl(cl: number) {
		this.eventContext.edgeLastCl = cl
	}

	private currentGateInfo: GateInfo | null = null
	private queuedGates: GateInfo[] = []

	// info only

	private static newCurrentWindowStats(): CurrentWindowStats
	{
		return { targetCl: -1, pending: -1, integrated: 0, blockages: 0 }
	}

	private currentWindowStats: CurrentWindowStats | null = null
}

/**
Integration window flow
- if outside window, gates get queued (no current)
	(in tick, always queued, but tryNextGate not called if outside window)
- if no current, window checked every tick
- if finish catch-up and gates still queued outside of window
	: updateLastCl calls tryNextGate
		- if cl past all queued gates, current gate set and queue cleared
		- if not within window, current set to null

- first gate coming in outside window while waiting at gate
	: in processGateChange, current gets set to null and new gate queued
*/

//////////////////
// TESTS

class DummyEventTriggers {

	// pretend GateEventContext 
	from: any = ''
	to: any = ''
	pauseCIS = false

	constructor(public edgeLastCl: number) {
	}

	static Inner = class {
		beginCalls = 0
		endCalls = 0
		reportBeginIntegratingToGate(_arg: any) {
			++this.beginCalls
		}

		reportEndIntegratingToGate(_arg: any) {
			++this.endCalls
		}
	}

	get beginCalls() { return this.eventTriggers.beginCalls }
	get endCalls() { return this.eventTriggers.endCalls }

	// named to match EventTriggersAndStuff.context
	eventTriggers = new DummyEventTriggers.Inner
}

const colors = require('colors')
colors.enable()
colors.setTheme(require('colors/themes/generic-logging.js'))

function setLastCl(gate: Gate, cl: number) {
	gate.updateLastCl([{change: cl, client: '', user: '', desc: ''}], 0)
}

export async function runTests(parentLogger: ContextualLogger) {
	const logger = parentLogger.createChild('Gate')

	const makeTestGate = (cl: number, options?: GateOptions): [DummyEventTriggers, Gate] => {
		const et = new DummyEventTriggers(cl)
		return [et, new Gate(et, { options: options || {}, p4: null, logger})]
	}

	let fails = 0
	let assertions = 0
	let testName = ''
	const assert = (b: boolean, msg: string) => {
		if (!b) {
			logger.error(`"${testName}" failed: ${colors.error(msg)}`)
			++fails
		}
		++assertions
	}

	// rules (maybe encapsulate in helper functions)
	// any setLastCl preceded by preIntegrate, any preIntegrate by tick 

	const simpleGateTest = async (exact: boolean) => {

		const options: GateOptions = {
			lastGoodCLPath: 1
		}

		// initial CL and gate are both 1
		const [et, gate] = makeTestGate(1, options)
		await gate.tick()
		assert(et.beginCalls + et.endCalls === 0, 'no events yet')
		assert(!gate.isGateOpen() && gate.getGateClosedMessage()!.includes('CIS'), 'gate closed')

		// nothing should happen here - CL 2 has been committed but gate prevents it being integrated
		gate.preIntegrate(2)
		await gate.tick()
		assert(et.beginCalls + et.endCalls === 0, 'no events yet')
		assert(!gate.isGateOpen() && gate.getGateClosedMessage()!.includes('CIS'), 'gate closed')

		// gate is now > 1 so we can 
		options.lastGoodCLPath = exact ? 2 : 3
		await gate.tick()
		gate.preIntegrate(2)
		assert(et.beginCalls === 1, 'catching up')
		assert(gate.isGateOpen(), 'gate open')

		setLastCl(gate, 2)
		await gate.tick()

		if (!exact) {
			const newCl = gate.preIntegrate(4) // only know we've caught up when higher cl comes in
			assert(newCl === 3, 'last cl adjustment requested')
			setLastCl(gate, 3)
		}
		assert(et.beginCalls === 1 && et.endCalls === 1, 'caught up')
	}

	const openWindow = (opts: GateOptions) => {
		opts.integrationWindow!.push({
			startHourUTC: (new Date).getUTCHours(),
			durationHours: 2 // more than 1 to avoid edge cases
			})
	}

	const replaceQueuedGates = async (replacement: string) => {

		// queue gates until window opens
		const options: GateOptions = {
			lastGoodCLPath: 4,
			integrationWindow: []
		}

		const [et, gate] = makeTestGate(2, options)
		await gate.tick()
		gate.preIntegrate(4)
		assert(et.beginCalls + et.endCalls === 0, 'no events yet')
		assert(!gate.isGateOpen() && gate.getGateClosedMessage()!.includes('window'), 'gate closed')

		options.lastGoodCLPath = 6
		await gate.tick()

		options.lastGoodCLPath = 8
		await gate.tick()

		openWindow(options)
		await gate.tick()
		assert(et.beginCalls === 1, 'catching up')
		assert(gate.isGateOpen(), 'gate open')

		let incoming: number[] = []
		switch (replacement) {
		case 'after':
			options.lastGoodCLPath = 5
			incoming = [4, 5]
			break

		case 'before':
			options.lastGoodCLPath = 3
			incoming = [3]
			break

		case 'lastCl':
			options.lastGoodCLPath = 2
			break

		case 'before lastCl':
			options.lastGoodCLPath = 1
			break
		}

		await gate.tick()
		for (const cl of incoming) {

			gate.preIntegrate(cl)
			setLastCl(gate, cl)
			await gate.tick()
		}

		assert(et.beginCalls === 1 && et.endCalls === 1, 'caught up')
		assert(!gate.isGateOpen() && gate.getGateClosedMessage()!.includes('CIS'), 'gate closed')
	}

	const queueNoWindow = async (middleIntegration: boolean) => {
 		// add gates at 2 and 3 while 1 still pending, integrate 1, 2 and 3 (should make 2 optional)

		const options: GateOptions = { lastGoodCLPath: 2 }

		// initial CL is 1, gate is 2
		const [et, gate] = makeTestGate(1, options)
		await gate.tick()
		assert(et.beginCalls === 1, 'no events yet') // initial catching up message
		assert(gate.isGateOpen(), 'gate initally open')

		options.lastGoodCLPath = 3

		await gate.tick()
		assert(et.beginCalls + et.endCalls === 1, 'no more events')
		assert(gate.isGateOpen(), 'still open after gate queued')

		// start integrating
		gate.preIntegrate(1); setLastCl(gate, 1); await gate.tick()
		if (middleIntegration) {
			gate.preIntegrate(2); setLastCl(gate, 2); await gate.tick()
		}
		gate.preIntegrate(3);
		assert(gate.lastCl < 3, 'exact gate cl gets integrated')
		setLastCl(gate, 3); await gate.tick()

		assert(et.beginCalls === 1 && et.endCalls === 1, 'catch-up notified')

	}

	////
	testName = 'golden path'
	const [et1, gate1] = makeTestGate(1)

	await gate1.tick()
	gate1.preIntegrate(2)
	setLastCl(gate1, 2)
	assert(et1.beginCalls + et1.endCalls === 0, 'no events')
	assert(gate1.isGateOpen(), 'gate open')

	////
	testName = 'with gate'
	await simpleGateTest(true)

	////
	testName = 'with inexact gate'
	await simpleGateTest(false)

	///
	testName = 'window'
	// start on cl 1, catch up to gate at 2 when window opens
	const optionsw: GateOptions = {
		lastGoodCLPath: 2,
		integrationWindow: []
	}

	const [etw, gatew] = makeTestGate(1, optionsw)
	await gatew.tick()
	gatew.preIntegrate(2)
	assert(etw.beginCalls + etw.endCalls === 0, 'no events yet')
	assert(!gatew.isGateOpen() && gatew.getGateClosedMessage()!.includes('window'), 'gate closed')

	openWindow(optionsw)

	await gatew.tick()
	assert(etw.beginCalls === 1, 'catching up')
	assert(gatew.isGateOpen(), 'gate open')

	///
	testName = 'queued gates'
	await (async () => {

		// queue gates until window opens
		const options: GateOptions = {
			lastGoodCLPath: 2,
			integrationWindow: []
		}

		const [et, gate] = makeTestGate(1, options)
		await gate.tick()
		gate.preIntegrate(2)
		assert(et.beginCalls + et.endCalls === 0, 'no events yet')
		assert(!gate.isGateOpen() && gate.getGateClosedMessage()!.includes('window'), 'gate closed')

		options.lastGoodCLPath = 5
		await gate.tick()

		options.lastGoodCLPath = 7
		await gate.tick()

		openWindow(options)
		await gate.tick()
		assert(et.beginCalls === 1, 'catching up')
		assert(gate.isGateOpen(), 'gate open')

		gate.preIntegrate(5)
		setLastCl(gate, 5)
		await gate.tick()
		assert(et.beginCalls === 1, 'no more events')
		assert(gate.isGateOpen(), 'gate open')

		gate.preIntegrate(7)
		setLastCl(gate, 7)
		assert(et.beginCalls === 1 && et.endCalls === 1, 'caught up')
		assert(!gate.isGateOpen() && gate.getGateClosedMessage()!.includes('CIS'), 'gate closed')

	})()

	///
	testName = 'replace queued gates'
	await replaceQueuedGates('after')

	///
	testName = 'replace queued gates (before current)'
	await replaceQueuedGates('before')

	///
	testName = 'replace queued gates (lastCl)'
	await replaceQueuedGates('lastCl')

	///
	testName = 'replace queued gates (before lastCl)'
	await replaceQueuedGates('before lastCl')

	///
	testName = 'queue no window'
 	await queueNoWindow(true)

	///
	testName = 'queue no window (skip integration)'
 	await queueNoWindow(false)

	// try to test the case where I was missing a caught up message

	if (fails === 0) {
		logger.info(colors.info(`Gate tests succeeded (${assertions} assertions)`))
	}
	return fails
}
