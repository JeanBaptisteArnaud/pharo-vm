accessing
numCounters: aValue
	^memory
		unsignedLongAt: address + baseHeaderSize + 21
		put: aValue