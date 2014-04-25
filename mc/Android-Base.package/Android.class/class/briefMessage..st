system primitives
briefMessage: aString
	"display a brief message (a toast) by means of Android runtime"
	<primitive: 'primBriefMessage' module: 'AndroidPlugin'>
	self primitiveFailed.