system primitives
primSetSpeechRate
	| rate vmcls vmobj bmmeth jmsg res |
	<var: 'rate' type: 'float'>
	<var: 'vmcls' type: 'jclass'>
	<var: 'vmobj' type: 'jobject'>
	<var: 'bmmeth' type: 'jmethodID'>
	<export: true>
	rate :=  interpreterProxy stackFloatValue: 0.
	vmcls := self getVMClass.
	vmcls ifNil: [^ interpreterProxy primitiveFail].
	vmobj := self getVMObject.
	vmobj ifNil: [^ interpreterProxy primitiveFail].
	bmmeth := self getMethod: 'setSpeechRate' withSig: '(F)I' inClass: vmcls.
	bmmeth ifNil: [^ interpreterProxy primitiveFail].
	res := self callIntMethod: bmmeth On: vmobj with: rate.
	interpreterProxy pop: 2.
	interpreterProxy pushInteger: res.
