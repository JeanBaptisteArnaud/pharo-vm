debug printing
printChar: aByte
	<api>
	"In Android, prints the given character to the JNI debug log file."
	<var: #aByte type: #'char'>
	self cCode: 'jniputchar(aByte)'.