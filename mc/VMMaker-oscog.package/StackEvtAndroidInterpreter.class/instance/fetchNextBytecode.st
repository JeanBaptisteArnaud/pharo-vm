debug printing
fetchNextBytecode
	"This method fetches the next instruction (bytecode). Each bytecode method is responsible for fetching the next bytecode, preferably as early as possible to allow the memory system time to process the request before the next dispatch."

	self cppIf: MULTIPLEBYTECODESETS
		ifTrue: [
			self print: 'T'.
			currentBytecode := self fetchByte + bytecodeSetSelector]
		ifFalse: [
			self print: 'F'.
			currentBytecode := self fetchByte].
		
		self printNum: currentBytecode.