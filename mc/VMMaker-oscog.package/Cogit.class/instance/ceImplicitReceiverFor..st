in-line cacheing
ceImplicitReceiverFor: selector
	"Cached implicit receiver implementation.  Caller looks like
		mov selector, ClassReg
				call ceImplicitReceiver
				br continue
		Lclass	.word
		Lmixin:	.word
		continue:
	 If class matches class of receiver then mixin contains either 0 or the implicit receiver.
	 If mixin is 0, answer the actual receiver, otherwise mixin."

	| receiver retpc classpc mixinpc rcvrClass mixin |
	receiver := coInterpreter mframeReceiverExport.
	retpc := coInterpreter stackTop.
	classpc := retpc + backEnd jumpShortByteSize.
	mixinpc := retpc + backEnd jumpShortByteSize + BytesPerOop.
	rcvrClass := objectMemory fetchClassOf: receiver.
	rcvrClass ~= (backEnd unalignedLongAt: classpc) ifTrue:
		[mixin := coInterpreter
					implicitReceiverFor: receiver
					mixin: coInterpreter mMethodClass
					implementing: selector.
		 backEnd unalignedLongAt: classpc put: rcvrClass.
		 backEnd unalignedLongAt: mixinpc put: (mixin = receiver ifTrue: [0] ifFalse: [mixin]).
		 ^mixin].
	self breakOnImplicitReceiver ifTrue:
		[self sendBreak: selector + BaseHeaderSize
			point: (objectMemory lengthOf: selector)
			receiver: nil].
	mixin := backEnd unalignedLongAt: mixinpc.
	^mixin = 0 ifTrue: [receiver] ifFalse: [mixin]