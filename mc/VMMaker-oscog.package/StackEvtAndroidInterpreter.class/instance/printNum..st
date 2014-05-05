debug printing
printNum: n
	"For testing in Smalltalk, this method should be overridden in a subclass."
	
	self cCode: 'jprintf("%ld", (long) n)'.