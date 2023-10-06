// Copyright Epic Games, Inc. All Rights Reserved.
import 'dart:async';

import 'package:flutter/material.dart';

/// Maps transient preferences to names so they can be accessed from one shared place.
class TransientSharedPreferences {
  final Map<String, TransientPreference> _preferences = {};

  /// Get the preference with the given [name].
  TransientPreference<T> get<T>(String name, {required T defaultValue}) {
    TransientPreference? preference = _preferences[name];

    if (preference != null) {
      if (!(preference is TransientPreference<T>)) {
        throw Exception('Tried to get transient preference "$name" of type $T,'
            'but stream already exists as ${preference.runtimeType}');
      }

      return preference;
    }

    preference = TransientPreference<T>._(defaultValue);
    _preferences[name] = preference;
    return preference as TransientPreference<T>;
  }
}

// Code below based largely on the streaming_shared_preferences package.
// Original license:
//
// Copyright 2019 Iiro Krankka
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// A single user value which does not persist outside the app. Exposed as a stream which emits a value whenever the
/// preference's value changes.
class TransientPreference<T> extends Stream<T> {
  TransientPreference._(T defaultValue) : _currentValue = defaultValue;

  final StreamController<T> _changeStream = StreamController<T>.broadcast(sync: true);

  /// The current value of the preference.
  T _currentValue;

  /// Set the value of the preference and notify any listeners.
  void setValue(T value) {
    _currentValue = value;

    if (_changeStream.hasListener) {
      _changeStream.add(value);
    }
  }

  /// Get the latest value of the preference.
  T getValue() {
    return _currentValue;
  }

  @override
  StreamSubscription<T> listen(void Function(T event)? onData,
      {Function? onError, void Function()? onDone, bool? cancelOnError}) {
    return _changeStream.stream.listen(
      onData,
      onError: onError,
      onDone: onDone,
      cancelOnError: cancelOnError,
    );
  }
}

/// A function that builds a widget whenever a [Preference] has a new value.
typedef TransientPreferenceWidgetBuilder<T> = Function(BuildContext context, T value);

/// A builder which gets a value from a [TransientPreference] and updates whenever it changes.
class TransientPreferenceBuilder<T> extends StatefulWidget {
  TransientPreferenceBuilder({
    required this.preference,
    required this.builder,
  });

  /// The preference on which you want to react and rebuild your widgets based on.
  final TransientPreference<T> preference;

  /// The function that builds a widget when a [preference] has new data.
  final TransientPreferenceWidgetBuilder<T> builder;

  @override
  _TransientPreferenceBuilderState<T> createState() => _TransientPreferenceBuilderState<T>();
}

class _TransientPreferenceBuilderState<T> extends State<TransientPreferenceBuilder<T>> {
  late final T _initialData;
  late final Stream<T> _preference;

  @override
  void initState() {
    super.initState();
    _initialData = widget.preference.getValue();
    _preference = widget.preference.transform(_EmitOnlyChangedValues(_initialData));
  }

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<T>(
      initialData: _initialData,
      stream: _preference,
      builder: (context, snapshot) => widget.builder(context, snapshot.data!),
    );
  }
}

/// Makes sure that [PreferenceBuilder] does not run its builder function if the
/// new value is identical to the last one.
class _EmitOnlyChangedValues<T> extends StreamTransformerBase<T, T> {
  _EmitOnlyChangedValues(this.startValue);
  final T startValue;

  @override
  Stream<T> bind(Stream<T> stream) {
    return StreamTransformer<T, T>((input, cancelOnError) {
      T? lastValue = startValue;

      late final StreamController<T> controller;
      late final StreamSubscription<T> subscription;

      controller = StreamController<T>(
        sync: true,
        onListen: () {
          subscription = input.listen(
            (value) {
              if (value != lastValue) {
                controller.add(value);
                lastValue = value;
              }
            },
            onError: controller.addError,
            onDone: controller.close,
            cancelOnError: cancelOnError,
          );
        },
        onPause: ([resumeSignal]) => subscription.pause(resumeSignal),
        onResume: () => subscription.resume(),
        onCancel: () {
          lastValue = null;
          return subscription.cancel();
        },
      );

      return controller.stream.listen(null);
    }).bind(stream);
  }
}
