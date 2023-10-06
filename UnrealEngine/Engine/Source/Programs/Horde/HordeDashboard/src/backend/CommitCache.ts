// Copyright Epic Games, Inc. All Rights Reserved.
import { action, makeObservable, observable } from 'mobx';
import backend from '.';
import { GetChangeSummaryResponse } from "./Api";

export class CommitCache {

    constructor() {
        makeObservable(this);
    }

    @action
    setUpdated() {
        this.updated++;
    }

    @observable
    updated: number = 0;

    getCommit(streamId: string, change: number): GetChangeSummaryResponse | undefined {

        let map = this.summaries.get(streamId); 
                
        if (!map) {
            return undefined;
        }
        
        const commit = map.get(change);

        if (!commit || !commit.authorInfo?.id) {
            return undefined;
        }

        return commit;

    }

    async set(streamId: string, changes: number[]) {

        let map = this.summaries.get(streamId);
        if (!map) {
            map = new Map();
            this.summaries.set(streamId, map);
        }

        if (!changes.length) {
            return;
        }

        const cminCL = 999999999999;
        const cmaxCL = -1;
        const maxDelta = 131072;

        let minCL = cminCL;
        let maxCL = cmaxCL;

        changes.forEach(c => {
            const change = map!.get(c);

            if (!change) {

                if (c < minCL && (minCL === cminCL || Math.abs(minCL - c) < maxDelta)) {
                    minCL = c;
                }

                if (c > maxCL && (maxCL === cmaxCL || Math.abs(maxCL - c) < maxDelta)) {
                    maxCL = c;
                }
            }
        })

        if (maxCL === cmaxCL && minCL === cminCL) {
            return;
        }

        changes.forEach(c => {

            const change = map!.get(c);
            if (change) {
                return;
            }

            map!.set(c, {
                number: 0, description: "", authorInfo : { id : "", email:"", name: ""}
            })

        })

        // new query
        try {
            const commits = await backend.getChangeSummaries(streamId, minCL, maxCL, maxCL - minCL + 1);
            commits.forEach(c => map!.set(c.number, c));
            this.setUpdated();
        } catch (reason) {
            console.error(reason);
            // @todo: we could clear out the place holders on error
        }

    }

    private summaries: Map<string, Map<number, GetChangeSummaryResponse>> = new Map();
}
