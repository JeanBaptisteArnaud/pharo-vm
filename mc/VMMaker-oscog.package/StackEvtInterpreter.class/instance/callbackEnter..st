callback support
callbackEnter: callbackID
	"Callbacks are disabled in Event VM"
	<export: true>
	<var: #callbackID type: #'sqInt *'>
		[^false].