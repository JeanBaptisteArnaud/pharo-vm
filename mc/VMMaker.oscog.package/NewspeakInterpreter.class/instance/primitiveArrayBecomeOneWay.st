object access primitives
primitiveArrayBecomeOneWay
	"We must flush the method cache here, to eliminate stale references
	to mutated classes and/or selectors.  This version fails for immutables."

	| arg rcvr ec |
	arg := self stackTop.
	rcvr := self stackValue: 1.
	ec := self become: rcvr with: arg twoWay: false copyHash: true forceImmutables: false.
	ec = PrimNoErr
		ifTrue: [self pop: 1]
		ifFalse: [self primitiveFailFor: ec]