translation
declareCVarsIn: aCCodeGenerator
	"define the jmpbuf for bailing out of the interpreter: its definition will be added
	to the definitions made by the parent classes - no super call is necessary"
	
	self declareC: #(jmpBufExit)
		as: #jmp_buf
		in: aCCodeGenerator.
	
	