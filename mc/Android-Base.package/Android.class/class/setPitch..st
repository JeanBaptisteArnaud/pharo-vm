system primitives
setPitch: aFloat
	"set TTS synthesized speech pitch"
	<primitive: 'primSetPitch' module: 'AndroidPlugin'>
	self primitiveFailed.