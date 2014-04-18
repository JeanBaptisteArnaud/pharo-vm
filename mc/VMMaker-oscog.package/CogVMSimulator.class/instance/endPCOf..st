debug printing
endPCOf: aMethod
	| pc end latestContinuation prim |
	pc := latestContinuation := self startPCOfMethod: aMethod.
	(prim := self primitiveIndexOf: aMethod) > 0 ifTrue:
		[(self isQuickPrimitiveIndex: prim) ifTrue:
			[^pc - 1]].
	end := objectMemory byteSizeOf: aMethod.
	[pc <= end] whileTrue:
		[| byte byte2 byte3 byte4 type offset jumpTarget |
		 byte := objectMemory fetchByte: pc ofObject: aMethod.
		 type := byte // 16.
		 offset := byte \\ 16.  
		 (type =7 and: [offset >= 8 and: [pc >= latestContinuation]]) ifTrue:"Return bytecodes (possible lastPC here)"
			[end := pc].
		 (type = 8 and: [offset = 15]) ifTrue: "closure creation; update latest continuation"
			[byte3 := objectMemory fetchByte: pc + 2 ofObject: aMethod.
			 byte4 := objectMemory fetchByte: pc + 3 ofObject: aMethod.
			 jumpTarget := (byte3 * 256) + byte4 + pc + 4.
			 jumpTarget > latestContinuation ifTrue: [latestContinuation := jumpTarget]].
		 type=9 ifTrue: "Short jumps (need to update latest continuation"
			[jumpTarget := (offset < 8 ifTrue: [offset] ifFalse: [offset - 8]) + pc + 2.
			jumpTarget > latestContinuation ifTrue: [latestContinuation := jumpTarget]].
		 type=10 ifTrue: "Long jumps (need to update latest continuation)"
			[byte2 := objectMemory fetchByte: pc + 1 ofObject: aMethod.
			 jumpTarget := (offset < 8 ifTrue: [offset - 4] ifFalse: [offset bitAnd: 3]) * 256 + byte2 + pc + 2.
			 jumpTarget > latestContinuation ifTrue: [latestContinuation := jumpTarget]].
		"Note we cannot skip by going to the latestContinuation because that may jump over jumps."
		pc := type = 8 "extensions"
				ifTrue: [pc + (#(2 2 2 2 3 2 2 1 1 1 2 1 3 3 3 4) at: byte \\ 16 + 1)]
				ifFalse: [type = 10 "long jumps"
							ifTrue: [pc + 2]
							ifFalse: [pc + 1]]].
	^end