indexing primitive support
stObject: array at: index
	"Return what ST would return for <obj> at: index."

	| hdr fmt totalLength fixedFields stSize |
	<inline: false>
	hdr := self baseHeader: array.
	fmt := self formatOfHeader: hdr.
	totalLength := self lengthOf: array baseHeader: hdr format: fmt.
	fixedFields := self fixedFieldsOf: array format: fmt length: totalLength.
	(fmt = 3 and: [self isContextHeader: hdr])
		ifTrue: [stSize := self fetchStackPointerOf: array]
		ifFalse: [stSize := totalLength - fixedFields].
	((self oop: index isGreaterThanOrEqualTo: 1)
	 and: [self oop: index isLessThanOrEqualTo: stSize]) ifTrue:
		[^self subscript: array with: (index + fixedFields) format: fmt].
	self primitiveFailFor: (fmt <= 1 ifTrue: [PrimErrBadReceiver] ifFalse: [PrimErrBadIndex]).
	^0