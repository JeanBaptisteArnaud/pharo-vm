debug printing
newStackPage
	"MRUP-->used page<->used page<->used page<->used page<--LRUP
	               ^                        <-next-prev->                         ^
	                |                                                                       |
	                v                        <-prev-next->                         v
	                free page<->free page<->free page<->free page"
	| lruOrFree |
	<var: #lruOrFree type: #'StackPage *'>
	<returnTypeC: #'StackPage *'>
	lruOrFree := stackPages mostRecentlyUsedPage nextPage.
	self print:'asd'.
	(stackPages isFree: lruOrFree) ifTrue:
		[^lruOrFree].
	self print:'asd2'.
	self divorceFramesIn: lruOrFree.
	self print:'asd3'.
	^lruOrFree