// Copyright Epic Games, Inc. All Rights Reserved.

import backend from '.';
import { GetTemplateRefResponse, StreamData, TemplateData } from './Api';
import { LocalCache } from './LocalCache';

export class TemplateCache {

	get(ref: GetTemplateRefResponse): TemplateData | undefined {

		const key = ref.id + ref.hash;
		return this.cache.get(key);
	}

	set(ref: GetTemplateRefResponse, template: TemplateData) {

		const key = ref.id + ref.hash;
		this.cache.set(key, template);

	}

	getStreamTemplates(stream: StreamData): Promise<TemplateData[]> {

		return new Promise<TemplateData[]>(async (resolve, reject) => {

			const templates: TemplateData[] = [];

			const unresolved: GetTemplateRefResponse[] = [];

			for (let i = 0; i < stream.templates.length; i++) {

				const tref = stream.templates[i];

				let template = this.get(tref);

				if (!template) {
					template = await templateDatabase.getItem(tref.id + tref.hash);
					if (template) {						
						this.set(tref, template);
					}
				}

				if (template) {
					templates.push(template);
				} else {
					unresolved.push(tref);
				}

			}

			// get stream templates
			Promise.all(unresolved.map(async (tref) => {
				return backend.getTemplate(stream.id, tref.id).then((response) => {
					response.ref = tref;
					this.set(tref, response);

					templateDatabase.storeItem(tref.id + tref.hash, response);

					templates.push(response);
				});
			})).then(() => {
				resolve(templates);
			}).catch(reason => reject(reason));
		});
	}

	initialize() {
		templateDatabase.initialize();
	}

	cache: Map<string, TemplateData> = new Map();

}

class TemplateDatabase extends LocalCache<TemplateData> {

	constructor() {
		super("Horde", "TemplateCache", "Cache for Horde template objects");
	}

}

const templateDatabase = new TemplateDatabase();

export default new TemplateCache();