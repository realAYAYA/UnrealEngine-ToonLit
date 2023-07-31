// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] =
	[ {name: 'Main', streamType: 'mainline'}
	, {name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
	, {name: 'Dev-Pootle', streamType: 'development', parent: 'Main'}
	]

export class ExcludeAuthors extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams, undefined, ['testuser1', 'testuser2'])

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		const desc = 'Initial populate'

		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])
	}

	async run() {
		let content = 'Initial content'
		content += '\n\nuser1 content'
		await P4Util.editFileAndSubmit(this.getClient('Main', 'testuser1'), 'test.txt', content)

		const user2client = this.getClient('Main', 'testuser2')
		await user2client.sync()
		content += '\n\nuser2 content'
		await P4Util.editFileAndSubmit(user2client, 'test.txt', content)
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 3),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'test.txt', 2)
		])
	}

	getBranches() {
		const mainBranchDef = this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle'])
		mainBranchDef.excludeAuthors = ['testuser2']
		return [ 
		    mainBranchDef
		  , this.makeForceAllBranchDef('Dev-Perkin', [])
		  , this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}
}
