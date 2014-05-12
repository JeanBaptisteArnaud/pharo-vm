memory access
isBigEnder
	"Ported from the classic VM as it works well on Android"
	"Answer true (non-zero) if running on a big endian machine."
	| |
	<var: 'cString' type: 'char *'>
	<var: 'endianness' declareC: 'static sqInt endianness = -1'>

	^ false
