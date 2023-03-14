// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, Stream } from './framework'


export abstract class MultipleDevAndReleaseTestBase extends FunctionalTest {

	protected streams: Stream[] = [
		{name: 'Main', streamType: 'mainline'},
		{name: 'Release-2.0', streamType: 'release', parent: 'Main'},
		{name: 'Release-1.0', streamType: 'release', parent: 'Release-2.0'},
		{name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
		{name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
	]

	createStreamsAndWorkspaces() {
		return super.createStreamsAndWorkspaces(this.streams)
	}

	protected initialPopulate() {
		const desc = 'Initial branch of files from Main'
		return Promise.all([
			this.p4.populate(this.getStreamPath('Release-2.0'), desc)
				.then(() => this.p4.populate(this.getStreamPath('Release-1.0'), 
					'Initial branch of files from Release-2.0')),
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])
	}

	private branch(stream: string, to: string[]) {
		const branch = this.makeForceAllBranchDef(stream, to)
		branch.forceAll = this.allForceFlow()
		return branch
	}

	allForceFlow() {
		return true
	}

	getBranches() {
		return [
			this.branch('Main', ['Dev-Perkin', 'Dev-Pootle']),
			this.branch('Dev-Perkin', []),
			this.branch('Dev-Pootle', []),
			this.branch('Release-2.0', ['Main']),
			this.branch('Release-1.0', ['Release-2.0'])
		]
	}
}
