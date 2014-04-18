system primitives
primShortCut
	"path label cmd width height bits"
	| vmcls vmobj meth imgpath jpath imglabel jlabel cmd jcmd width height bits bitlen bitptr jbits |
	<var: 'imgpath' type: 'char *'>
	<var: 'vmcls' type: 'jclass'>
	<var: 'vmobj' type: 'jobject'>
	<var: 'meth' type: 'jmethodID'>
	<var: 'jpath' type: 'jstring'>
	<export: true>
	imgpath :=  self stringArgumentAt: 5.
	jpath := self asJavaString: imgpath.
	jpath ifNil: [^ interpreterProxy primitiveFail].
	imglabel :=  self stringArgumentAt: 4.
	jlabel := self asJavaString: imglabel.
	jlabel ifNil: [^ interpreterProxy primitiveFail].
	cmd :=  self stringArgumentAt: 3.
	jcmd := self asJavaString: cmd.
	jcmd ifNil: [^ interpreterProxy primitiveFail].
	width :=  interpreterProxy stackIntegerValue: 2.
	height :=  interpreterProxy stackIntegerValue: 1.
	bits := interpreterProxy stackValue: 0.
	bits ifNotNil: [
		(interpreterProxy isBytes: bits) ifFalse: [^ interpreterProxy primitiveFail].
		bitlen := interpreterProxy byteSizeOf: bits.
		bitptr := interpreterProxy firstIndexableField: bits.
		jbits := self asJavaByte: bitptr Array: bitlen.
		jbits ifNil: [^ interpreterProxy primitiveFail].
	] ifNil: [
		jbits := nil.
	].
	vmcls := self getVMClass.
	vmcls ifNil: [^ interpreterProxy primitiveFail].
	vmobj := self getVMObject.
	vmobj ifNil: [^ interpreterProxy primitiveFail].
	meth := self getMethod: 'imageShortCut' withSig: '(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II[B)V'
				 inClass: vmcls.
	meth ifNil: [^ interpreterProxy primitiveFail].
      self callVoidMethod: meth On: vmobj with: jpath m: jlabel a: jcmd n: width y: height args: jbits.
	interpreterProxy pop: 6. "leave rcvr on stack"
