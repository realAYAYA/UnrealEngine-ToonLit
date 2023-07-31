// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualLogger } from '../common/logger';
import { runTests as gate_runTests } from './gate';
import { runTests as graph_runTests } from '../new/graph';
import { runTests as notifications_runTests } from './notifications';
import { runTests as targets_runTests } from './targets';
import { runTests as ztag_runTests } from '../common/ztag';

const unitTestsLogger = new ContextualLogger('Unit Tests')

const tests = [
	gate_runTests,
	graph_runTests,
	notifications_runTests,
	targets_runTests,
	ztag_runTests
]

async function run() {
	let failed = 0
	for (const test of /**/tests/*/tests.slice(0, 3)/**/) {
		failed += await test(unitTestsLogger)	
	}

	process.exitCode = failed
}

run()
