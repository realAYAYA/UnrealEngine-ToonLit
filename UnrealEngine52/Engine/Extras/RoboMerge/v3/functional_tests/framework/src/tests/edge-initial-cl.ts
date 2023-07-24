// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, retryWithBackoff, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
	{name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
]

export class EdgeInitialCl extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		// commit something, assume rvn > 1
		const mainClient = this.getClient('Main', 'testuser1')
		await P4Util.addFileAndSubmit(mainClient, 'test.txt', 'Initial content')

		const desc = 'Initial branch of files from Main'
		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])

		/*const firstEditCl = */await P4Util.editFileAndSubmit(mainClient, 'test.txt', 'Initial content\n\nFirst addition')
	}

	async run() {
		await this.addTargetBranches(
			[ this.makeForceAllBranchDef('Dev-Perkin', [])
			, this.makeForceAllBranchDef('Dev-Pootle', [])
			], 'Main',
			[ { from: this.fullBranchName('Main'), to: this.fullBranchName('Dev-Perkin')
			  , initialCL: 1
			  }
			]
		)

		await retryWithBackoff('Loading updated branchmap', () =>
			this.getBranchState('Dev-Perkin')
			.catch(_ => false)
		);
	}

	async verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 1), // no initial CL, so starts at latest
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2) 
		])
	}

	getBranches() {
		return [this.makeForceAllBranchDef('Main', [])]
	}

}
