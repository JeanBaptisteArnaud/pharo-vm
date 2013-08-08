C code generator
fileHeaderVersionStampForSourceClass: sourceClass
	"Answer a suitable version stamp to include in the header."
	| exportBuildInfo slangDescription sourceDescription |
	[exportBuildInfo := sourceClass isInterpreterClass
						ifTrue: ['char *__interpBuildInfo = __buildInfo;']
						ifFalse:
							[sourceClass isCogitClass
								ifTrue: ['char *__cogitBuildInfo = __buildInfo;']
								ifFalse:
									[sourceClass isPluginClass ifTrue:
										[sourceClass exportBuildInfoOrNil]]]]
		on: MessageNotUnderstood
		do: [:ex| ex resume: false].
	[slangDescription := self class monticelloDescriptionFor: self class.
	 sourceClass ifNotNil:
		[sourceDescription := [sourceClass monticelloDescription]
								on: MessageNotUnderstood
								do: [:ex| self class monticelloDescriptionFor: sourceClass]]]
		on: Error
		do: [:ex| | now |
			now := Time dateAndTimeNow printString.
			^String streamContents:
				[:s|
				s nextPutAll: '/* Automatically generated from Squeak on '.
				s nextPutAll: now.
				s nextPutAll: ' */'; cr; cr.
				s nextPutAll: 'static char __buildInfo[] = "Generated on '.
				s nextPutAll: now.
				s nextPutAll: '. Compiled on "'.
				s nextPutAll: '__DATE__ ;'; cr.
				exportBuildInfo ifNotNil:
					[s nextPutAll: exportBuildInfo; cr].
				s cr]].
	^String streamContents:
		[:s|
		s nextPutAll: '/* Automatically generated by\	' withCRs.
		s nextPutAll: slangDescription.
		sourceDescription ifNotNil:
			[s nextPutAll: '\   from\	' withCRs; nextPutAll: (sourceDescription copyReplaceAll: '\' withCRs with: '\	' withCRs)].
		s cr; nextPutAll: ' */'; cr.
		sourceDescription ifNotNil:
			[s nextPutAll: 'static char __buildInfo[] = "'.
			 s nextPutAll: (sourceDescription copyReplaceAll: '\' withCRs with: '\n\', '\' withCRs).
			 s nextPutAll: ' " __DATE__ ;'; cr.
			exportBuildInfo ifNotNil:
				[s nextPutAll: exportBuildInfo; cr].
			s cr]]