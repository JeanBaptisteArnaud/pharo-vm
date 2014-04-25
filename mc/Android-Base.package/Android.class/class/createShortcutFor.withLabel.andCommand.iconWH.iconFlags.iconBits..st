system primitives
createShortcutFor: aPath withLabel: aString andCommand: aCmd iconWH: xy iconFlags: flg iconBits: b
	"create a home screen shortcut for a given image"
	<primitive: 'primShortCut' module: 'AndroidPlugin'>
	self primitiveFailed.
