// Copyright Epic Games, Inc. All Rights Reserved.

import { createRoot } from 'react-dom/client';
import App from './App';
import './index.css';
import * as serviceWorker from './serviceWorker';
import { configure } from 'mobx';

// configure mobx
configure({
   reactionRequiresObservable: true,
   enforceActions: "observed"
});


const root = createRoot(document.getElementById('root')!);
root.render(<App />);

// If you want your app to work offline and load faster, you can change
// unregister() to register() below. Note this comes with some pitfalls.
// Learn more about service workers: https://bit.ly/CRA-PWA
serviceWorker.unregister();




