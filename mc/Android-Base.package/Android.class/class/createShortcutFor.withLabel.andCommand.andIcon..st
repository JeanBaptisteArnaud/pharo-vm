system primitives
createShortcutFor: aPath withLabel: aString andCommand: aCmd andIcon: aForm
	"create a home screen shortcut for a given image"
	Android createShortcutFor: aPath 
		withLabel: aString 
		andCommand: aCmd 
		iconWH: ((aForm width << 16) bitOr: (aForm height))
		iconFlags: 0 
		iconBits: (aForm bits asByteArray).