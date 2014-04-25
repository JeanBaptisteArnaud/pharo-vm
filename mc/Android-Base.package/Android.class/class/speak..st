system primitives
speak: aString
	"speak the given string via TTS"
	<primitive: 'primSpeak' module: 'AndroidPlugin'>
	self primitiveFailed.