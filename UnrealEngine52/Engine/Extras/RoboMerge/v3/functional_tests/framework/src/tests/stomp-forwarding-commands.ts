import { FunctionalTest, P4Client, P4Util, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'DevChild', streamType: 'development', parent: 'Main'},
	{name: 'DevGrandchild', streamType: 'development', parent: 'DevChild'},
]

export class StompForwardingCommands extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		this.mainClient = this.getClient('Main')
		await P4Util.addFileAndSubmit(this.mainClient, 'test.uasset', 'Initial content', true)

		// popluateStreams does all in parallel, ignoring dependencies
		await this.populateStreams(streams.filter(s => s.name !== 'DevGrandchild'))
		await this.populateStreams(streams.slice(2)) // Dev-Grandchild

		// await this.populateStreams(streams)

		const childClient = this.getClient('DevChild')
		await childClient.sync()

		// set up conflict
		await P4Util.editFileAndSubmit(childClient, 'test.uasset', 'Changed content')

	}

	async run() {
		await P4Util.editFileAndSubmit(this.mainClient, 'test.uasset', 'Conflicting change',
			'-' + this.fullBranchName('DevGrandchild'))

		await this.waitForRobomergeIdle()

		await this.ensureBlocked('Main', 'DevChild')
		await this.verifyAndPerformStomp('Main', 'DevChild')
	}

	verify() {
		// make sure skip command was included in stomped file
		return Promise.all([
			this.checkHeadRevision('DevChild', 'test.uasset', 3),
			this.checkHeadRevision('DevGrandchild', 'test.uasset', 2),
		])
	}

	getBranches() {
		const childSpec = this.makeForceAllBranchDef('DevChild', ['DevGrandchild'])
		childSpec.initialCL = 1

		return [
			this.makeForceAllBranchDef('Main', ['DevChild']),
			childSpec,
			this.makeForceAllBranchDef('DevGrandchild', []),
		]
	}

	mainClient: P4Client
}

