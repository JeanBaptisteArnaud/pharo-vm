system primitives
setSpeechRate: aFloat
	"set TTS synthesized speech rate"
	<primitive: 'primSetSpeechRate' module: 'AndroidPlugin'>
	self primitiveFailed.