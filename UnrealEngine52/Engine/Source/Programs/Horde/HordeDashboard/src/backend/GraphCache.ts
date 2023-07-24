// Copyright Epic Games, Inc. All Rights Reserved.

import backend from '.';
import { GetGraphResponse } from './Api';
import { LocalCache } from './LocalCache';

export type GraphQuery = {
	jobId: string;
	graphHash: string;
}

export class GraphCache {

	getGraphs(query: GraphQuery[]): Promise<GetGraphResponse[]> {

		return new Promise<GetGraphResponse[]>((resolve, reject) => {

			const results: Map<string, GetGraphResponse> = new Map();

			const unresolved = query.filter(q => {

				const graph = this.cache.get(q.graphHash);
				if (graph) {
					results.set(q.graphHash, graph);
					return false;
				}
				return true;
			});

			Promise.all(unresolved.map(q => {
				return this.get(q);
			})).then(values => {
				resolve(query.map(q => this.cache.get(q.graphHash)!));
			}).catch(reason => {
				reject(reason);
			});
		});
	}

	get(query: GraphQuery): Promise<GetGraphResponse> {

		return new Promise<GetGraphResponse>(async (resolve, reject) => {

			let graph = this.cache.get(query.graphHash);

			if (!graph) {
				graph = await graphDatabase.getItem(query.graphHash);
				if (graph) {
					this.cache.set(query.graphHash, graph);
				}
			}

			if (graph) {
				return resolve(graph);
			}

			backend.getGraph(query.jobId).then(value => {
				graph = value as GetGraphResponse;
				this.cache.set(query.graphHash, graph);
				graphDatabase.storeItem(query.graphHash, graph);
				resolve(graph);
			}).catch(reason => {
				reject(reason);
			});

		});

	}

	initialize() {
		graphDatabase.initialize();
	}

	cache: Map<string, GetGraphResponse> = new Map();

}

class GraphDatabase extends LocalCache<GetGraphResponse> {

	constructor() {
		super("Horde", "GraphCache", "Cache for Horde graph objects");
	}

}

const graphDatabase = new GraphDatabase();
const graphCache = new GraphCache();

export default graphCache;