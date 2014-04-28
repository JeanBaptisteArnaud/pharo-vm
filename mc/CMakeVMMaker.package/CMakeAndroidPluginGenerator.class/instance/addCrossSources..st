sources management
addCrossSources: sources

	^ self addSources: sources prefixed: '/builds/workspace/ARM-Android/platforms/Cross/plugins/', plugin moduleName, '/'.
