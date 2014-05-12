debug printing
divorceFramesIn: aStackPage
	| theFP calleeFP theSP theIP calleeContext theContext |
	<inline: false>
	<var: #aStackPage type: #'StackPage *'>
	<var: #theFP type: #'char *'>
	<var: #calleeFP type: #'char *'>
	<var: #theSP type: #'char *'>

	self print: '123e1dvdfsvfdvdfwdw,l'.
	statStackPageDivorce := statStackPageDivorce + 1.
	
	self print: '123e1d12e112e'.
	theFP := aStackPage headFP.
	
	self print: '123e1dsadasvc fd'.
	theSP := aStackPage headSP.
	
	self print: '123e1dasdac sdivnoimopkj'.
	theIP := stackPages longAt: theSP.
	
	self print: '123e1dasdac sdivnoimopkjvubninjjk'.
	theSP := theSP + BytesPerWord. "theSP points at hottest item on frame's stack"
	
	self print: '123e1dasdac sdivndasdasoimopkj'.
	calleeContext := nil.
	self print: '123e1d'.
	[theContext := self ensureFrameIsMarried: theFP SP: theSP.
	 self updateStateOfSpouseContextForFrame: theFP WithSP: theSP.
	 objectMemory storePointerUnchecked: InstructionPointerIndex
		ofObject: theContext
		withValue: (self contextInstructionPointer: theIP frame: theFP).
	 self assert: (self frameReceiver: theFP)
				= (objectMemory fetchPointer: ReceiverIndex ofObject: theContext).
	 calleeContext ~~ nil ifTrue:
		[objectMemory storePointer: SenderIndex
			ofObject: calleeContext
			withValue: theContext].
		
	self print: '123e1dsdsdv'.
	 calleeContext := theContext.
	 calleeFP := theFP.
	
	self print: '123excvx1d'.
	theIP := (self frameCallerSavedIP: theFP) asInteger.
	self print: '123fdsbe1d'. 
	theFP := self frameCallerFP: theFP.
	 theFP ~= 0] whileTrue:
		["theSP points at stacked hottest item on frame's stack"
		 theSP := self frameCallerSP: calleeFP].

	self print: '123ffddsfe1d'.
	objectMemory storePointer: SenderIndex
		ofObject: theContext
		withValue: (self frameCallerContext: calleeFP).

	self print: '123esadas1d'.
	"The page is now free; mark it so."
	aStackPage baseFP: 0