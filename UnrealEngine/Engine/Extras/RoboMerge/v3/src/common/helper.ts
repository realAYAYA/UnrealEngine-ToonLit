// Copyright Epic Games, Inc. All Rights Reserved.

import * as Sentry from '@sentry/node';
import { promisify } from 'util';
import { ContextualLogger } from './logger';

const helperLogger = new ContextualLogger('Helper')

// fail hard on exceptions thrown in async functions
process.on('unhandledRejection', async (reason, promise) => {
	console.log('Encountered unhandledRejection at: ', promise, '\n\nreason:', reason)

	if (process.env.EPIC_ENV === 'prod') {
		// Sentry integration has a unhandledRejection listener, need to give it time to send to Sentry before throwing fatal err
		await closeSentry(helperLogger)
	}
	process.exit(-1) // this is unexpected but not the fault of the committed changelist
});

/**
 * This is a nasty hack to get Sentry to finish up it's work -- close and flush do not work as documented (read: at all)
 * @param logger ContextualLogger to send messages
 * @param timeout Timeout to sleep in milliseconds (default 5000)
 */
async function waitForSentry(logger: ContextualLogger, timeout = 5000) {
	logger.verbose("Giving Sentry time to think.")
	return new Promise<void>( 
		(resolve) => {
			setTimeout(() => { 
				logger.verbose("Gave Sentry time to think.")
				resolve() 
			}, 
			timeout)
		}
	).catch( (_reason) => { /* Do nothing */ })
}

/**
 * Wait for Sentry to finish up work [waitForSentry()] and close the client.
 * @param logger ContextualLogger to send messages
 */
export async function closeSentry(logger: ContextualLogger) {
	// Wait a few seconds before closing Sentry I/O
	await waitForSentry(logger)
	return Sentry.close().catch( (_reason) => { /* Do nothing */ } ) // Useless?
}

export const _nextTick = promisify(process.nextTick);
export const _setTimeout = promisify(setTimeout);

export function setDefault<K, V>(map: Map<K, V>, key: K, def: V): V {
	const val = map.get(key)
	if (val) {
		return val
	}
	map.set(key, def)
	return def
}

export function sortBy<T>(arr: T[], projection: (x: T) => number|string) {
	if (arr.length === 0)
		return

	type Pair = [number|string, T]
	const decorated: Pair[] = arr.map(x => [projection(x), x] as Pair)
	if (typeof decorated[0][0] === 'string') {
		(decorated as [string, T][]).sort(([x], [y]) => x.localeCompare(y))
	}
	else {
		(decorated as [number, T][]).sort(([x], [y]) => x - y)
	}
	return decorated.map(p => p[1])
}

export class Random {
	static randint(minInc: number, maxInc: number) {
		return Math.floor(Math.random() * (maxInc - minInc)) + minInc
	}

	static choose(container: any[]) {
		return container[Random.randint(0, container.length - 1)]
	}
}
