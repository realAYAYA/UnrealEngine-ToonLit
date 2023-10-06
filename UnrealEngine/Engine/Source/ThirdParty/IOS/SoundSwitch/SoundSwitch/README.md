# SoundSwitch
#### Detect the silent switch state in iOS

It seems many folks out there want to alter the UI of their iOS apps when the sound switch is on.
iOS doesn't have any public API for this, and the audio route methods from iOS 4 stopped working in iOS 5 (I may be missing a version here...).
To make a long story short, if for some reason you need to detect the silent switch state, here's a simple solution:

__Before you read on, please remember:__

* This solution will not give you real time detection, the check is ran every second.
* It will only work on audio categories that respect the silent switch (the default ambient category does, but play & record does not).
* It's a dirty hack, but 100% public API based.
* Never pat a burning dog.

The trick is quite simple, all we need to do is to play a system sound, measure the amount of time it took for the sound to complete playing, and determine if we're in silent mode or not.
We'll need to install a sound completion routine for that.
Something like:
```objectivec
void SharkfoodSoundMuteNotificationCompletionProc(SystemSoundID  ssID,void* clientData); // sound completion proc
/** ... **/
if (AudioServicesCreateSystemSoundID((__bridge CFURLRef)url, &_soundId) == kAudioServicesNoError){
  AudioServicesAddSystemSoundCompletion(self.soundId, CFRunLoopGetMain(), kCFRunLoopDefaultMode, SharkfoodSoundMuteNotificationCompletionProc,(__bridge void *)(self));
}
```
All we need to do now is to periodically play a system sound (silent sound, probably) and check how long it took to play it - near zero value will mean the silent switch is on.

See the sample project in this repository for an example implementation.
