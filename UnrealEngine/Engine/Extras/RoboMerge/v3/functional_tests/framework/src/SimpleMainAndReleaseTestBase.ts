// Copyright Epic Games, Inc. All Rights Reserved.
import {FunctionalTest, Stream} from './framework'

export const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Release', streamType: 'release', parent: 'Main'}
]

export abstract class SimpleMainAndReleaseTestBase extends FunctionalTest {

	protected initialPopulate() {
		return this.p4.populate(this.getStreamPath('Release'), 'Initial branch of files from Main')
	}

	private branch(stream: string, to: string[]) {
		return {
			streamDepot: this.constructor.name,
			name: this.fullBranchName(stream),
			streamName: stream,
			flowsTo: to.map(str => this.fullBranchName(str)),
			forceAll: true
		}
	}

	getBranches() {
		return [
			this.branch('Main', []),
			this.branch('Release', ['Main'])
		]
	}
}
