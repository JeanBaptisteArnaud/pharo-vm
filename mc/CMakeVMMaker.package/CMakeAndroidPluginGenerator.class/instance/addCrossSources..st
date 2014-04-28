sources management
addCrossSources: sources

	^ self addSources: sources prefixed: '/builds/workspace/ARM-Android/platform/Cross/plugins/', plugin moduleName, '/'.
