multi-threading simulation switch
loadInitialContext
	"This method includes or excludes CoInterpreterMT methods as required.
	 Auto-generated by CogVMSimulator>>ensureMultiThreadingOverridesAreUpToDate"

	^self perform: #loadInitialContext
		withArguments: {}
		inSuperclass: (cogThreadManager ifNil: [CoInterpreterPrimitives] ifNotNil: [CoInterpreterMT])