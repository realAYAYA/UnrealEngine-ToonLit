// Copyright Epic Games, Inc. All Rights Reserved.
import { EdgeProperties, FunctionalTest, P4Util, Stream } from '../framework'
import { Change } from '../test-perforce'

const streams: Stream[] =
	[ {name: 'Main', streamType: 'mainline'}
	, {name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
	, {name: 'Dev-Pootle', streamType: 'development', parent: 'Main'}
	]

export class IncognitoEdge extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		const desc = 'Initial populate'

		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])
	}

	run() {
		return P4Util.editFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content\n\nMore stuff')
	}

	verify() {
		const testNameLower = this.testName.toLowerCase()

		this.info('Ensuring Dev-Pootle commit has no incriminating information')

		return Promise.all([
			this.getClient('Dev-Perkin').changes(1)
				.then((changes: Change[]) => {
					const description = changes[0]!.description.toLowerCase()
					if (description.indexOf(testNameLower) < 0) {
						throw new Error('Expected test name to appear in description')
					}
					if (description.indexOf('main') < 0) {
						throw new Error('Expected parent branch name to appear in description')
					}
				}),
			this.getClient('Dev-Pootle').changes(1)
				.then((changes: Change[]) => {
					const description = changes[0]!.description.toLowerCase()
					if (description.indexOf(testNameLower) >= 0) {
						throw new Error('Expected test name not to appear in description')
					}
					if (description.indexOf('main') >= 0) {
						throw new Error('Expected parent branch name not to appear in description')
					}
				}),
			this.checkHeadRevision('Main', 'test.txt', 2),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 2)
		])
	}

	getBranches() {
		return [ 
		  this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle'])
		  , this.makeForceAllBranchDef('Dev-Perkin', [])
		  , this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}

	getEdges(): EdgeProperties[] {
		return [
		  { from: this.fullBranchName('Main'), to: this.fullBranchName('Dev-Pootle')
		  , incognitoMode: true
		  }
		]
	}
}
