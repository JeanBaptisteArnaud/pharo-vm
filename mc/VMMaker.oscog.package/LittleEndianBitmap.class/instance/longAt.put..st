accessing
longAt: byteIndex put: aValue
	"Compatibility with the ByteArray method of the same name."
	| lowBits wordIndex value mask |
	lowBits := byteIndex - 1 bitAnd: 3.
	wordIndex := byteIndex - 1 // 4 + 1.
	value := aValue < 0
				ifTrue: [16rFFFFFFFF bitAnd: aValue]
				ifFalse: [16rFFFFFFFF < aValue ifTrue:
							[self errorImproperStore].
						aValue].
	lowBits = 0 ifTrue:
		[self at: wordIndex put: value.
		 ^aValue].
	mask := 16rFFFFFFFF bitAnd: 16rFFFFFFFF << (lowBits * 8).
	self at: wordIndex put: (((self at: wordIndex) bitAnd: mask bitInvert) bitXor: (value << (lowBits * 8) bitAnd: mask)).
	self at: wordIndex + 1 put: (((self at: wordIndex + 1) bitAnd: mask) bitXor: (value >> (4 - lowBits * 8) bitAnd: mask bitInvert)).
	^aValue

	"(1 to: 8) collect:
		[:ba| | bm |
		bm := LittleEndianBitmap new: 4.
		bm at: 1 put: 16r55555555.
		bm at: 2 put: 16rAAAAAAAA.
		bm longAt: ba put: 16r04030201.
		{ (bm at: 1) hex. (bm at: 2) hex }]"