// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';

// render components with top level routes
export type PluginRoute = {
    path: string;
    component: React.FC;
}

// render components with specific mount points
export enum PluginMount {
    JobDetailPanel,
    TestReportPanel,
    TestReportLink,
    BuildHealthSummary,
}

export type PluginComponent = {
    mount: PluginMount;       
    component: React.FC<any>;
    // optional unique identifier for the component
    id?:string;
}

export interface Plugin {
    name: string;
    routes?: PluginRoute[];
    components?: PluginComponent[];
}

export class Plugins {

    get routes(): PluginRoute[] {
        return this.plugins.map(p => p.routes ?? []).flat();
    }

    getComponents(mount: PluginMount, id?:string): PluginComponent[] {
        return this.plugins.map(p => {
            return p.components?.filter(c => c.mount === mount && c.id === id) ?? [];
        }).flat();
    }

    loadPlugins(pluginList: string[] | undefined): Promise<void> {

        console.log("loading plugins...");

        return new Promise<void>((resolve) => {

            if (!pluginList || !pluginList.length || this.plugins.length) {
                return resolve();
            }

            Promise.all(pluginList.map(async (plugin: any) => {
                await import(`./${plugin}`).then((m: any) => {
                    this.plugins.push(m.default);
                    console.log(`loaded plugin: ${plugin}`);
                }).catch((reason: any) => {
                    console.error(`unable to load plugin: ${plugin}, ${reason}`);
                });
            })).finally(() => {
                resolve();
            });
        });
    }

    private plugins: Plugin[] = [];

}

export default new Plugins();