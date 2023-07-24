import { BranchDefs } from './branchdefs';
import { BranchGraph } from './branchgraph';
import { ContextualLogger } from '../common/logger';
import { PerforceContext } from '../common/perforce';
import { Status } from './status';

const logger = new ContextualLogger('preview')
let p4: PerforceContext | null = null

// let branchSpecsRootPath = ''
export function init(_root: string) {
	// branchSpecsRootPath = root
}

export async function getPreview(cl: number, singleBot?: string) {
	// @todo print content from shelf
	if (!p4) {
		p4 = new PerforceContext(logger)
	}

	const status = new Status(new Date(), 'preview', logger)

	const bots: [string, string][] = []
	for (const entry of (await p4.describe(cl, undefined, true)).entries) {
		if (entry.action !== 'delete') {
			const match = entry.depotFile.match(/.*\/(.*)\.branchmap\.json$/)
			if (match) {
				bots.push([match[1], match[0]])
			}
		}
	}

	const allStreamSpecs = await p4.streams()

	const errors = []
	for (const [bot, path] of bots) {
		if (singleBot && bot.toLowerCase() !== singleBot.toLowerCase()) {	
			continue
		}
		const fileText = await p4.print(`${path}@=${cl}`)

		const validationErrors: string[] = []
		const result = BranchDefs.parseAndValidate(validationErrors, fileText, allStreamSpecs)

		const errorPrefix = `\n\t${bot} validation failed: `
		if (!result.branchGraphDef) {
			errors.push(errorPrefix + (validationErrors.length === 0 ? 'unknown error' : validationErrors.join('')))
			continue
		}

		const graph = new BranchGraph(bot)
		graph.config = result.config
		try {
			graph._initFromBranchDefInternal(result.branchGraphDef)
		}
		catch (err) {
			errors.push(errorPrefix + err.toString())
			continue
		}

		for (const branch of graph.branches) {
			status.addBranch(branch)
		}
	}

	if (errors.length > 0) {
		throw new Error(errors.join('\n\n'))
	}
	return status
}
