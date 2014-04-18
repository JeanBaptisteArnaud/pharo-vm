system primitives
primSetPitch
	| pitch vmcls vmobj bmmeth jmsg res |
	<var: 'pitch' type: 'float'>
	<var: 'vmcls' type: 'jclass'>
	<var: 'vmobj' type: 'jobject'>
	<var: 'bmmeth' type: 'jmethodID'>
	<export: true>
	pitch :=  interpreterProxy stackFloatValue: 0.
	vmcls := self getVMClass.
	vmcls ifNil: [^ interpreterProxy primitiveFail].
	vmobj := self getVMObject.
	vmobj ifNil: [^ interpreterProxy primitiveFail].
	bmmeth := self getMethod: 'setPitch' withSig: '(F)I' inClass: vmcls.
	bmmeth ifNil: [^ interpreterProxy primitiveFail].
	res := self callIntMethod: bmmeth On: vmobj with: pitch.
	interpreterProxy pop: 2.
	interpreterProxy pushInteger: res.
