// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] =
	[ {name: 'Main', streamType: 'mainline'}
	, {name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
	, {name: 'Dev-Perkin-Child', streamType: 'development', parent: 'Dev-Perkin'}
	, {name: 'Dev-Pootle', streamType: 'development', parent: 'Main'}
	, {name: 'Dev-Pootle-Child', streamType: 'development', parent: 'Dev-Pootle'}
	, {name: 'Dev-Pootle-Grandchild', streamType: 'development', parent: 'Dev-Pootle-Child'}
	]

export class IncognitoTest extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		const desc = 'Initial populate'

		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc)
				.then(() => this.p4.populate(this.getStreamPath('Dev-Perkin-Child'), desc)),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
				.then(() => this.p4.populate(this.getStreamPath('Dev-Pootle-Child'), desc)) 
				.then(() => this.p4.populate(this.getStreamPath('Dev-Pootle-Grandchild'), desc)) 
		])
	}

	async run() {
		const mainClient = this.getClient('Main')
		await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nMore stuff')
		// await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nMore stuff\n\nand some more', 'grandchild')
	}

	verify() {
		this.info('Ensuring Dev-Pootle-Child commit has no incriminating information')

		return Promise.all([
			this.checkDescriptionContainsEdit('Dev-Perkin-Child', [this.testName, 'Dev-Perkin']),
			this.checkDescriptionContainsEdit('Dev-Pootle-Child', [], [this.testName, 'Dev-Pootle']),

			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 2),
			this.checkHeadRevision('Dev-Perkin-Child', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle-Child', 'test.txt', 2)
		])
	}

	private branch(stream: string, to: string[], incognito?: boolean) {
		const branch = this.makeForceAllBranchDef(stream, to)
		if (incognito) {
			branch.incognitoMode = true
		}
		branch.forceAll = true
		return branch
	}

	getBranches() {
		const grandchildBranch = this.branch('Dev-Pootle-Grandchild', [])
		grandchildBranch.aliases = ['grandchild']
		return [ this.branch('Main', ['Dev-Perkin', 'Dev-Pootle'])
		       , this.branch('Dev-Perkin', ['Dev-Perkin-Child'])
		       , this.branch('Dev-Pootle', ['Dev-Pootle-Child'], true)
		       , this.branch('Dev-Pootle-Child', ['Dev-Pootle-Grandchild'])
		       , this.branch('Dev-Perkin-Child', [])
		       , grandchildBranch
		       ]
	}
}
