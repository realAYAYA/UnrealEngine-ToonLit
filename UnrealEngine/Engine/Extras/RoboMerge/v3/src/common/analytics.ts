// Copyright Epic Games, Inc. All Rights Reserved.

import { ANALYTICS_SIMPLE_COUNTERS, newTickJournal, SimpleCounters, TickJournal } from '../robo/tick-journal';
import { ContextualLogger } from './logger';
import * as request from './request';

const INFLUX_URL = '<influx end-point goes here>';
const MEASUREMENT = 'robomerge';
const FLUSH_ANALYTICS_INTERVAL_SECONDS = 5 * 60;

interface LoginCounts {success: number, fail: number, error: number}

class JournalAccumulator {
	bbJournals = new Map<string, TickJournal>()
	unmonitored = 0
	ticks = 0

	add(addend: Map<string, TickJournal>, outsideOfTick?: boolean) {
		for (const [nodeBot, journal] of addend.entries()) {

			if (!outsideOfTick && !journal.monitored) {
				++this.unmonitored
				continue
			}

			// do accumulate
			let accumJournal = this.bbJournals.get(nodeBot)
			if (!accumJournal) {
				accumJournal = newTickJournal()
				this.bbJournals.set(nodeBot, accumJournal)
			}

			accumJournal.conflicts += journal.conflicts
			accumJournal.integrationErrors += journal.integrationErrors
			accumJournal.syntaxErrors += journal.syntaxErrors

			const counterAccum = accumJournal as unknown as {[key: string]: number}
			const counterJournal = journal as unknown as {[key: string]: number}
			for (const counter in ANALYTICS_SIMPLE_COUNTERS) {
				counterAccum[counter] += counterJournal[counter]
			}
		}

		if (!outsideOfTick) {
			this.ticks += addend.size
		}
	}

	reset() {
		this.bbJournals.clear()
		this.unmonitored = 0
		this.ticks = 0
	}
}

export class Analytics {

	constructor(private hostInfo: string, private analyticsLogger?: ContextualLogger) {
		this._linePrefix = `${MEASUREMENT},host=${hostInfo.replace(/\W/g, '')},`;

		this.flushInterval = setInterval(() => this.flush(), FLUSH_ANALYTICS_INTERVAL_SECONDS * 1000);

		this.analyticsLogger = this.analyticsLogger || new ContextualLogger(`${this._linePrefix} Analytics`)
	}

	flush() {
		const lines = [];

		for (const [bot, accumulator] of this.accumulators.entries()) {
			const botPrefix = this._linePrefix + `bot=${bot},`;

			if (accumulator.unmonitored !== 0) {
				lines.push(botPrefix + `event=unmonitored value=${accumulator.unmonitored}`);
			}

			if (accumulator.ticks !== 0) {
				lines.push(botPrefix + `event=tick value=${accumulator.ticks}`);
			}

			for (const [nodeBot, bbActivity] of accumulator.bbJournals.entries()) {
				const prefix = botPrefix + `nodeBot=${bot}_${nodeBot},`;

				const counterIndexer: {[key: string]: string} = ANALYTICS_SIMPLE_COUNTERS
				for (const counter in counterIndexer) {
					const count = (bbActivity as unknown as {[key: string]: number})[counter]
					if (count !== 0) {
						lines.push(prefix + `event=${counterIndexer[counter]} value=${count}`);
					}
				}

				const blockages = bbActivity.conflicts + bbActivity.integrationErrors + bbActivity.syntaxErrors;
				if (blockages !== 0) {
					// too specifically named in Grafana charts
					lines.push(prefix + `event=conflict value=${blockages}`);
				}
			}
		}

		for (const key in this.loginAttempts) {
			const val: number = (this.loginAttempts as unknown as {[key: string]: number})[key]
			if (val !== 0) {
				lines.push(this._linePrefix + `event=login,result=${key} value=${val}`)
			}
		}

		this.loginAttempts = {
			success: 0,
			fail: 0,
			error: 0
		}

		for (const [procName, usage] of this.memUsage) {
			lines.push(this._linePrefix + `event=mem,proc=${procName} value=${usage}`)
		}

		this.memUsage.clear()

		for (const [procName, usage] of this.diskUsage) {
			lines.push(this._linePrefix + `event=disk,proc=${procName} value=${usage}`)
		}

		this.diskUsage.clear()

		const branchesRequestsDelta = this.branchesRequests - this.lastReportedBranchesRequests
		this.lastReportedBranchesRequests = this.branchesRequests
		if (branchesRequestsDelta > 0) {
			lines.push(this._linePrefix + `event=branches_requests value=${branchesRequestsDelta}`)
		}

		if (this.perforceRetries > 0) {
			lines.push(this._linePrefix + `event=perforce_retries value=${this.perforceRetries}`)
		}
		this.perforceRetries = 0

		if (lines.length !== 0) {
			this._post(lines.join('\n'));
		}

		// clear all the buffers
		this.accumulators = new Map;
	}

	reportActivity(bot: string, activity: Map<string, TickJournal>, outsideOfTick?: boolean) {
		let accumulator = this.accumulators.get(bot);
		if (!accumulator) {
			accumulator = new JournalAccumulator();
			this.accumulators.set(bot, accumulator);
		}
		accumulator.add(activity, outsideOfTick);
	}

	updateActivityCounters(counters: Partial<SimpleCounters>, optBot?: string, optBranch?: string) {
		const bot = optBot || 'any'
		const branch = optBranch || 'any'
		const deltas = newTickJournal()

		const journalIndexer = deltas as unknown as {[key: string]: number}
		for (const key in counters) {
			journalIndexer[key] = (counters as {[key: string]: number})[key]
		}
		this.reportActivity(bot, new Map([[branch, deltas] as [string, TickJournal]]), true)
	}

	reportEmail(numSent: number, numIntendedRecipients: number) {
		const lines = [this._linePrefix + `event=email value=${numSent}`];
		if (numIntendedRecipients > numSent) {
			lines.push(this._linePrefix + `event=noemail value=${numIntendedRecipients - numSent}`);
		}

		this._post(lines.join('\n'));
	}

	reportLoginAttempt(result: string) {
		switch (result) {
			case 'success': ++this.loginAttempts.success; break
			case 'fail': ++this.loginAttempts.fail; break
			case 'error': ++this.loginAttempts.error; break
		}
	}

	reportMemoryUsage(procName: string, usage: number) {
		this.memUsage.set(procName, usage)
	}

	reportDiskUsage(procName: string, usage: number) {
		this.diskUsage.set(procName, usage)
	}

	reportBranchesRequests(requests: number) {
		this.branchesRequests = requests
	}

	reportPerforceRetries(retries: number) {
		this.perforceRetries += retries
	}

	stop() {
		clearTimeout(this.flushInterval)
	}

	private _post(body: string) {
		if (this.hostInfo === '__TEST__') {
			this.analyticsLogger!.info(body)
			return
		}

		request.post({url: INFLUX_URL, body})
		.catch(error => this.analyticsLogger!.printException(error, 'Analytics error'));
	}

	public readonly _linePrefix: string;

	private accumulators = new Map<string, JournalAccumulator>();

	private loginAttempts: LoginCounts = {
		success: 0,
		fail: 0,
		error: 0
	}

	private memUsage = new Map<string, number>()
	private diskUsage = new Map<string, number>()
	private branchesRequests = 0
	private lastReportedBranchesRequests = 0

	private perforceRetries = 0

	private flushInterval: NodeJS.Timer	
}

export function runTests(parentLogger: ContextualLogger) {
	const unitTestLogger = parentLogger.createChild('Graph')
	const test = new Analytics('__TEST__', unitTestLogger)

	unitTestLogger.info('... tick journal ...')
	unitTestLogger.info('	- inactive')
	const tj = newTickJournal()
	tj.merges += 3
	tj.syntaxErrors += 7
	test.reportActivity('test', new Map([['testbranch', tj] as [string, TickJournal]]))
	test.flush()
	unitTestLogger.info('	- active')

	tj.monitored = true
	test.reportActivity('test', new Map([['testbranch', tj] as [string, TickJournal]]))
	test.flush()

	// test up
	unitTestLogger.info('... activity counters ...')
	test.updateActivityCounters({merges: 50, stompQueued: 11, traces: 13})
	test.flush()

	test.stop()

	return 0
}
