# live_link_vcam

A rebuild of the Live Link VCAM app for Flutter, enabling cross-platform deployment.

## Tentacle Integration

This project depends on the Tentacle SDK located in Engine/Restricted/NotForLicensees/Source/ThirdParty/TentacleSDK.

We use some Tentacle SDK functions through Dart's Foreign Function Interface (FFI). Dart bindings for these functions are
automatically generated using ffigen and checked into source control.

If you need to regenerate them, first set up ffigen (see https://pub.dev/packages/ffigen), then run the following command
in the root VCAM project directory:

    `dart run ffigen --config ffigen_tentacle.yaml`

## Pigeon

This project uses Pigeon to automatically generate platform channel bindings for native APIs.
See `pigeons/README` for more information.

## Signing Certificates 

To setup signing certificates, follow the instructions outlined in this article 

 - [Deploying Flutter Apps to The PlayStore](https://medium.com/@bernes.dev/deploying-flutter-apps-to-the-playstore-1bd0cce0d15c)

Add your `android-key.tks` to `android\app` folder and `key.properities` to `android\`.  The gradle files are already setup
to read these files when app bundling