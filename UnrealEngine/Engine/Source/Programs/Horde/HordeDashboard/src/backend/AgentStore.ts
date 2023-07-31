// Copyright Epic Games, Inc. All Rights Reserved.

import { action, observable } from 'mobx';
import { AgentData, PoolData } from './Api';
import backend from '.';

export class AgentStore {

    @observable.ref
	agents: AgentData[] = [];
	
	@observable.ref
	pools: PoolData[] = [];

	// don't need this for now
    modifiedAfterDate: Date = new Date(0);

    @action 
    private _setPools(data: PoolData[]) {
        this.pools = data;
    }

    @action
    private _setAgents(data: AgentData[]) {
        if(data.length !== 0) {
            const toSet: AgentData[] = [...this.agents];
            data.forEach(updatedAgent => {
                const newUpdateIdx = toSet.findIndex(existingAgent => existingAgent.id === updatedAgent.id);
                if(newUpdateIdx !== -1) {
                    toSet[newUpdateIdx] = updatedAgent;
                }
                else {
                    toSet.push(updatedAgent);
                }
            });
            this.agents = toSet;
        }
	}

    async update(slim = false): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            const promises: any[] = [];
            promises.push(backend.getAgents({modifiedAfter: this.modifiedAfterDate?.toISOString()}));
            if(!slim) {
                promises.push(backend.getPools());
            }
			Promise.all(promises).then(values => {
                this.modifiedAfterDate = new Date();
                this._setAgents(values[0]);
                if(!slim){
                    this._setPools(values[1]);
                }
                resolve();
            }).catch(reason => {
                console.error(`Error getting agents and pools: ${reason}`);
            });
        });
    }
}

export const agentStore = new AgentStore();