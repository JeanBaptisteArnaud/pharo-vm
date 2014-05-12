debug printing
printFloat: f
	"For testing in Smalltalk, this method should be overridden in a subclass."
	<cmacro: '(f) jprintf("%g", f)'>
	self print: f