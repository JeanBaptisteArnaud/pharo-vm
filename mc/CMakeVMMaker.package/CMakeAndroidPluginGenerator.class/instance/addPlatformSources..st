sources management
addPlatformSources: sources
	^ self addSources: sources prefixed: '/builds/workspace/ARM-Android/platforms/android/plugins/', plugin moduleName , '/'.
