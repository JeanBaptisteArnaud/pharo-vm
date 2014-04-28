gmake commands
generate

	| name |
	output := String new writeStream.

	name := plugin moduleName.
	self printHeader.
	
	self addDefinitions: '-DSQUEAK_BUILTIN_PLUGIN'.
	
	self addSources: { name , '.c' } prefixed: '/builds/workspace/ARM-Android/build/', name.

	self includeDirectories: '-I/builds/workspace/ARM-Android/src/ -I/builds/workspace/ARM-Android/platforms/Cross/plugins/ -I$/builds/workspace/ARM-Android/platforms/android/plugins/', name.

	self addDefinitions: vmGen config compilerFlags.

	" perform config's configureXYZ: message to apply per-plugin custom rules, if any "
	
	vmGen config configurePlugin: plugin with: self.
	
	extraRules ifNotNil: [ extraRules value: self ].
	
	vmGen config extraPluginSettings: self.
	
	self saveFile.