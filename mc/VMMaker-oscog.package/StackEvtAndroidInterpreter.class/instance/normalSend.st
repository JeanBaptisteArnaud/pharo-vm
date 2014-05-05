debug printing
normalSend
	"Send a message, starting lookup with the receiver's class."
	"Assume: messageSelector and argumentCount have been set, and that 
	the receiver and arguments have been pushed onto the stack,"
	"Note: This method is inlined into the interpreter dispatch loop."
	<sharedCodeNamed: 'normalSend' inCase: #singleExtendedSendBytecode>
	| rcvr |
	rcvr := self internalStackValue: argumentCount.
	self printOop: rcvr.
	lkupClass := objectMemory fetchClassOf: rcvr.
	self assert: lkupClass ~= objectMemory nilObject.
	self commonSend