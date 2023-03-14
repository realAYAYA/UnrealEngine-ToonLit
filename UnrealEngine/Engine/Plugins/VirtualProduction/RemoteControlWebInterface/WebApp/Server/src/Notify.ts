import { IView, PropertyValue } from '@Shared';
import { Server } from 'http';
import socketio from 'socket.io';
import { UnrealEngine } from './UnrealEngine';

                  
export namespace Notify {
  var io = socketio({ path: '/api/io' });

  export async function initialize(server: Server): Promise<void> {
    io.attach(server);

    io.sockets.on('connection', (socket: socketio.Socket) => {
      socket
        .on('client', onPing)
        .on('view', onViewChange)
        .on('value', UnrealEngine.setPayloadValue)
        .on('execute', UnrealEngine.executeFunction)
        .on('metadata', UnrealEngine.setPresetPropertyMetadata)
        .on('rebind', UnrealEngine.rebindProperties)
        .on('search', UnrealEngine.search);

      if (UnrealEngine.isConnected())
        socket.emit('connected', true);
        
      socket.emit('opened', UnrealEngine.isOpenConnection());

      if (UnrealEngine.isLoading())
        socket.emit('loading', true);
    });
  }

  export function emit(what: 'presets' | 'payloads' | 'connected' | 'loading' | 'opened', ...args: any[]) {
    io.emit(what, ...args);
  }

  export function onViewChange(preset: string, view: IView, supressUnrealNotification?: boolean) {
    if (!supressUnrealNotification)
      UnrealEngine.setView(preset, view);
    io.emit('view', preset, view);
  }

  export function emitValueChange(preset: string, property: string, value: PropertyValue) {
    io.emit('value', preset, property, value);
  }

  export function emitValuesChanges(preset: string, changes: { [key: string]: PropertyValue }) {
    io.emit('values', preset, changes);
  }

  export function emitPassphraseChanged(wrongPassphrase: string) {
    io.emit('passphrase', wrongPassphrase);
  }

  export function onPing(time: number) {
    io.emit('pong', time, new Date().getTime());
  }
}