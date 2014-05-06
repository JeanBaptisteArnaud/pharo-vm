debug printing
printNum: l
	"For testing in Smalltalk, this method should be overridden in a subclass."
	<api>
	<var: #l type: #'long'>
	self cCode: 'jniputlong(l)'.
	