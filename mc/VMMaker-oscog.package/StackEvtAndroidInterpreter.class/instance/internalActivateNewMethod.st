debug printing
internalActivateNewMethod
	| methodHeader numTemps rcvr errorCode |
	<inline: true>

	self print: '1-??-'.
	methodHeader := self headerOf: newMethod.
	
	self print: '1-????-'.
	numTemps := self temporaryCountOfMethodHeader: methodHeader.

	rcvr := self internalStackValue: argumentCount. "could new rcvr be set at point of send?"

	self print: '1-1-'.
	self internalPush: localIP.
	self internalPush: localFP.
	localFP := localSP.
	self internalPush: newMethod.
	self setMethod: newMethod methodHeader: methodHeader.
	
	self print: '1-2-'.
	self internalPush: (self
						encodeFrameFieldHasContext: false
						isBlock: false
						numArgs: (self argumentCountOfMethodHeader: methodHeader)).
						
	self print: '1-3-'.
	self internalPush: objectMemory nilObject. "FxThisContext field"
	self internalPush: rcvr.

	"Initialize temps..."
	argumentCount + 1 to: numTemps do:
		[:i | self internalPush: objectMemory nilObject].

	"-1 to account for pre-increment in fetchNextBytecode"
	
	self print: '1-5-'.
	localIP := self pointerForOop: (self initialPCForHeader: methodHeader method: newMethod) - 1.
	self print: '1-5-bis'.
	
	(self methodHeaderHasPrimitive: methodHeader) ifTrue:
		["Skip the CallPrimitive bytecode, if it's there, and store the error code if the method starts
		  with a long store temp.  Strictly no need to skip the store because it's effectively a noop."
		 localIP := localIP + (self sizeOfCallPrimitiveBytecode: methodHeader).
		 primFailCode ~= 0 ifTrue:
			[(objectMemory byteAt: localIP + 1)
			  = (self longStoreBytecodeForHeader: methodHeader) ifTrue:
				[errorCode := self getErrorObjectFromPrimFailCode.
				 self internalStackTopPut: errorCode "nil if primFailCode == 1, or primFailCode"].
			 primFailCode := 0]].

	self print: '1-t'.
	self assert: (self frameNumArgs: localFP) == argumentCount.
	
	self print: '1-t5'.
	self assert: (self frameIsBlockActivation: localFP) not.
	
	self print: '1-41234'.
	self assert: (self frameHasContext: localFP) not.

	"Now check for stack overflow or an event (interrupt, must scavenge, etc)."
	localSP < stackLimit ifTrue:
		[ self print:'bad things happen'.
		self externalizeIPandSP.
		 self handleStackOverflowOrEventAllowContextSwitch: (self canContextSwitchIfActivating: newMethod header: methodHeader).
		 self internalizeIPandSP].
	
	self print: 'asdmhsdfl'.