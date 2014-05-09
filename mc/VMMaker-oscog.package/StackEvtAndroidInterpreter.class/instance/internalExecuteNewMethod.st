debug printing
internalExecuteNewMethod
	| succeeded |
	<inline: true>
	self print: '-1-'.
	primitiveFunctionPointer ~= 0 ifTrue:
		[self isPrimitiveFunctionPointerAnIndex ifTrue:
			[^self internalQuickPrimitiveResponse].
	
	self print: '-2-'.
		 self externalizeIPandSP.
		
	self print: '-3-'.
		 succeeded := self slowPrimitiveResponse.
		
	self print: '-4-'.
		 self internalizeIPandSP.
	
	self print: '-5-'.
		 succeeded ifTrue:
			[
			
	self print: '-6-'.
			self browserPluginReturnIfNeeded.
			^nil]].
	"if not primitive, or primitive failed, activate the method"
	
	self print: '-7-'.
	^self internalActivateNewMethod