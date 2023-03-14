// Copyright Epic Games, Inc. All Rights Reserved.
import { EdgeProperties, FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] =
	[ {name: 'Main', streamType: 'mainline'}
	, {name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
	, {name: 'Dev-Pootle', streamType: 'development', parent: 'Main'}
	]

export class ExcludeAuthorsPerEdge extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams, undefined, ['testuser1', 'testuser2', 'buildmachine'])

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

// somehow test buildmachine skip being overridden

		const user2client = this.getClient('Main', 'testuser2')
		await user2client.sync()
		content += '\n\nuser2 content'
		await P4Util.editFileAndSubmit(user2client, 'test.txt', content)

		const buildmachineClient = this.getClient('Main', 'buildmachine')
		await buildmachineClient.sync()
		content += '\n\nbuildmachine content'
		await P4Util.editFileAndSubmit(buildmachineClient, 'test.txt', content)
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 4),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 3),
			this.ensureBlocked('Main', 'Dev-Pootle') // skips testuser2 commit but _does_ try to integrate buildmachine commit
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
		  , excludeAuthors: ['testuser2']
		  }
		]
	}
}
