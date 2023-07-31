import { FunctionalTest, P4Client, P4Util, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-Child', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Grandchild', streamType: 'development', parent: 'Dev-Child'},
	{name: 'Dev-OtherChild', streamType: 'development', parent: 'Main'},
]

export class MultipleRoutesToSkip extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		this.mainClient = this.getClient('Main')
		await P4Util.addFile(this.mainClient, 'test.txt', 'Initial content')

		await P4Util.submit(this.mainClient, 'Initial file')

		// popluateStreams does all in parallel, ignoring dependencies
		await this.populateStreams(streams.filter(s => s.name !== 'Dev-Grandchild'))
		await this.populateStreams(streams.slice(2, 3)) // Dev-Grandchild
		// await this.getClient('Release').sync()
	}

	run() {
		// skip grand child (indirect target via both childrent)
		return P4Util.editFileAndSubmit(this.mainClient, 'test.txt', 'change',
													'-' + this.fullBranchName('Dev-Grandchild'))
	}

	verifyCommon() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Dev-Child', 'test.txt', 2), 
			this.checkHeadRevision('Dev-Grandchild', 'test.txt', 1),
			this.checkHeadRevision('Dev-OtherChild', 'test.txt', 2)
			])
	}

	verify() {
		return this.verifyCommon()
	}

	getBranches() {
		return [
			this.makeForceAllBranchDef('Main', ['Dev-Child', 'Dev-OtherChild']),
			this.makeForceAllBranchDef('Dev-Child', ['Dev-Grandchild']),
			this.makeForceAllBranchDef('Dev-Grandchild', []),
			this.makeForceAllBranchDef('Dev-OtherChild', ['Dev-Grandchild'])
		]
	}

	mainClient: P4Client
}
