interpreter shell
interpret
	"This is the main interpreter loop. It normally loops forever, fetching and executing bytecodes. When running in the context of a browser plugin VM, however, it must return control to the browser periodically. This should done only when the state of the currently running Squeak thread is safely stored in the object heap. Since this is the case at the moment that a check for interrupts is performed, that is when we return to the browser if it is time to do so. Interrupt checks happen quite frequently. The EventVM version of interpreter also has a special jmp_buf used to bail out of the interpreter once no processes are ready to run. If returned for no events available, return value is 0, and if returned as normal VM termination, return value is 1. The host program may use this information to determine when to loop"

	| result |
	<inline: false>
	"If stacklimit is zero then the stack pages have not been initialized."
	stackLimit = 0 ifTrue:
		[^self initStackPagesAndInterpret].
	"record entry time when running as a browser plug-in"
	self browserPluginInitialiseIfNeeded.
	"set up a jmp_buf to bail out when no processes are ready to run"
	result := self cCode: 'setjmp(jmpBufExit)' 
				inSmalltalk:[jmpBufExit := [^self]. 0].
	result = 0 ifFalse:[^0].	"return 0 when suspended for an event"
	self internalizeIPandSP.
	self fetchNextBytecode.
	[true] whileTrue: [self dispatchOn: currentBytecode in: BytecodeTable].
	localIP := localIP - 1.  "undo the pre-increment of IP before returning"
	self externalizeIPandSP.
	^1	"return 1 if normal VM termination"
