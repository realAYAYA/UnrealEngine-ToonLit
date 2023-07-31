// Copyright Epic Games, Inc. All Rights Reserved.
import { Plugin, PluginMount } from "..";
import { ExampleBar } from "./routes/ExampleBar";
import { JobDetailPluginExample } from "./components/JobDetailPluginExample";

class ExamplePlugin implements Plugin {

    name = "ExamplePlugin";
    routes = [{
        path: "/example/bar",
        component: ExampleBar
    }];
    components = [{
        mount: PluginMount.JobDetailPanel,
        component: JobDetailPluginExample
    }]

}

export default new ExamplePlugin();