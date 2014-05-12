debug printing
print: s
	"In Android, prints the given string to the JNI debug log file."
	<api>
	<var: #s type: #'char *'>
	self cCode: 'jniputstr(s)'.