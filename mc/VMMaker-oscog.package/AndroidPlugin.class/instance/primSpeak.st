system primitives
primSpeak
	| msg vmcls vmobj bmmeth jmsg res |
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
	bmmeth := self getMethod: 'speak' withSig: '(Ljava/lang/String;)I' inClass: vmcls.
	bmmeth ifNil: [^ interpreterProxy primitiveFail].
	jmsg := self asJavaString: msg.
	jmsg ifNil: [^ interpreterProxy primitiveFail].
	res := self callIntMethod: bmmeth On: vmobj with:jmsg.
	interpreterProxy pop: 2.
	interpreterProxy pushInteger: res.
