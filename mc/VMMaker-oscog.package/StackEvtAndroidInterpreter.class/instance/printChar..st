debug printing
printChar: aByte
	<api>
	"In Android, prints the given character to the JNI debug log file."
	self jniputchar: aByte.