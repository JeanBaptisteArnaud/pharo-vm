system primitives
primBriefMessage
	| msg vmcls vmobj bmmeth jmsg |
	<var: 'msg' type: 'char *'>
	<var: 'vmcls' type: 'jclass'>
	<var: 'vmobj' type: 'jobject'>
	<var: 'bmmeth' type: 'jmethodID'>
	<var: 'jmsg' type: 'jstring'>
	<export: true>
	msg :=  self stringArgumentAt: 0.
	vmcls := self getVMClass.
	vmcls ifNil: [^ interpreterProxy primitiveFail].
	vmobj := self getVMObject.
	vmobj ifNil: [^ interpreterProxy primitiveFail].
	bmmeth := self getMethod: 'briefMessage' withSig: '(Ljava/lang/String;)V' inClass: vmcls.
	bmmeth ifNil: [^ interpreterProxy primitiveFail].
	jmsg := self asJavaString: msg.
	jmsg ifNil: [^ interpreterProxy primitiveFail].
	self callVoidMethod: bmmeth On: vmobj with:jmsg.
	interpreterProxy pop: 1. "leave rcvr on stack"
