// Copyright Epic Games, Inc. All Rights Reserved.

export const ANALYTICS_SIMPLE_COUNTERS = {
	merges: 'merge',

	// Keep track of the number of calculation issues from stomp verify
	// to detemine how often we're encountering this in practice
	stompedRevisionsCalculationIssues: 'stomprevscalissue',

	// Keep track of how many successful stomp requests have been queued
	stompQueued: 'stompreqqueued',

	// how many trace routes called (from CIS/build tools)
	traces: 'traceroute',
	failedTraces: 'tracefail'
}

export type TickJournalFields = {
	conflicts: number
	integrationErrors: number
	syntaxErrors: number

	monitored: boolean
}

export type SimpleCounters = {[K in keyof typeof ANALYTICS_SIMPLE_COUNTERS]: number}
export type TickJournal = SimpleCounters & TickJournalFields
export function newTickJournal() {
	const fields: TickJournalFields = {conflicts: 0, integrationErrors: 0, syntaxErrors: 0, monitored: false}

	const journalIndexer = fields as unknown as {[key: string]: number}
	for (const counter in ANALYTICS_SIMPLE_COUNTERS) {
		journalIndexer[counter] = 0
	}
	return journalIndexer as unknown as TickJournal
}
