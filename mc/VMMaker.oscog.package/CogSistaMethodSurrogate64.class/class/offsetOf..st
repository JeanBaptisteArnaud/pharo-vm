accessing
offsetOf: aByteSymbol
	"These should be generated!!"
	self assert: self objectMemoryClass baseHeaderSize = BaseHeaderSize.
	^aByteSymbol == #counters
		ifTrue: [32 + BaseHeaderSize]
		ifFalse: [super offsetOf: aByteSymbol]