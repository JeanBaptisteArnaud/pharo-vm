in-line cacheing
unlinkIfLinkedSend: annotation pc: mcpc ignored: superfluity
	<var: #mcpc type: #'char *'>
	| entryPoint |
	(self isSendAnnotation: annotation) ifTrue:
		[entryPoint := backEnd callTargetFromReturnAddress: mcpc asInteger.
		 entryPoint > methodZoneBase
			ifTrue: "It's a linked send."
				[self targetMethodAndSendTableFor: entryPoint into:
					[:targetMethod :sendTable| | unlinkedRoutine |
					 unlinkedRoutine := sendTable at: (targetMethod cmNumArgs min: NumSendTrampolines - 1).
					 backEnd
						rewriteInlineCacheAt: mcpc asInteger
						tag: targetMethod selector
						target: unlinkedRoutine]]
			ifFalse:
				[self cppIf: NewspeakVM ifTrue:
					[entryPoint = ceImplicitReceiverTrampoline ifTrue:
						[self voidImplicitReceiverCacheAt: mcpc]]]].
	^0 "keep scanning"