package org.golubovsky.cogstack;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.View;
import android.view.MotionEvent;
import android.view.KeyEvent;

import android.os.Bundle;
import android.os.ResultReceiver;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputMethodManager;

import java.util.Arrays;

import org.golubovsky.cogstack.CogVM;

import java.lang.System;

public class CogView extends View {
	CogVM vm;
	int bits[];
	int width;
	int height;
	int depth;
	int buttonBits;
	long timestamp = System.currentTimeMillis();
	int lastX = -1, lastY = -1, lastBut = -1;
	final int redButtonBit = 4;
	final int yellowButtonBit = 2;
	final int blueButtonBit = 1;
	final int radius = 2;
	boolean softKbdOn;
	int ctrlOn, shiftOn;
	CogActivity ctx;
	Paint paint;
	int timerDelay;

	public int getTimerDelay() {
	    return timerDelay;
	}

	public void setTimerDelay(int d) {
            timerDelay = d;
	}

	RR rr = new RR(this);

	/* Closure -- ha-ha */

	private class RR extends ResultReceiver {
		CogView owner;
		public RR(CogView sv) {
			super(getHandler());
			owner = sv;
		}
		protected void  onReceiveResult  (int resultCode, Bundle resultData)
		{
			super.onReceiveResult(resultCode, resultData);
			switch(resultCode) {
				case InputMethodManager.RESULT_HIDDEN:
				case InputMethodManager.RESULT_UNCHANGED_HIDDEN:
					owner.softKbdOn = false;
					break;
				case InputMethodManager.RESULT_SHOWN:
				case InputMethodManager.RESULT_UNCHANGED_SHOWN:
					owner.softKbdOn = true;
					break;
				default:
			}
		}
	}


	public void showHideKbd(int what) {
		InputMethodManager imm = (InputMethodManager)
			ctx.getSystemService(Context.INPUT_METHOD_SERVICE);
		if (what == 1) {
			imm.showSoftInput(this, 0, rr);
		} else {
			imm.hideSoftInputFromWindow(this.getWindowToken(), 0);
		}
}

	public void timerEvent(final int r, final int d) {
		final class CogTimer implements Runnable {
			public void run() {
				timerEvent(r - d, d);
			}
		}
		if(r <= 0) return;
		int rc = 0;
		if(vm != null) rc = vm.interpret();
		postDelayed(new CogTimer(), (rc != 0) ? 0 : timerDelay);
	}

	public CogView(Context context) {
		super(context);
		ctx = (CogActivity)context;
		timerDelay = 200;
		width = 0;
		height = 0;
		depth = 32;
		softKbdOn = false;
		ctrlOn = 0;
		shiftOn = 0;
		bits = null;
		buttonBits = redButtonBit;
    		paint = new Paint();
    		timerEvent(100, 0);
	}

	protected void onLayout(boolean changed, int left, int top, int right, int bottom)
	{
		if(!changed) return;
		this.width = right - left;
		this.height = bottom - top;
		this.bits = new int[this.width * this.height];
		Arrays.fill(bits, 0);
		vm.setScreenSize(this.width, this.height);
	}


	// Process ctrl keys (keyCode = 126 or 127). Since isControlPressed is not
	// available at this API level (4), emulate it by catching presses/releases 
	// of Ctrl keys. Similarly, process shift keys (59, 60).

        public boolean onKeyUp(int keyCode, KeyEvent event) {
		switch(keyCode) {
			case 59:
			case 60:
				shiftOn = 0;
				break;
			case 126:
			case 127:
				ctrlOn = 0;
				break;
			default:
				break;
		}
		return true;
	}

	// Key down: show/hide soft keyboard on menu button. Back button turns the mouse
	// yellow for one click. Page up button turns the mouse blue for one click.
	// Unicode characters will be sent in the extra characters string attached to the
	// message as multiple action. The preinstalled onKeyListener (see CogActivity)
	// catches these multiple action events and redirects them to the view's
	// onKeyDown callback with keyCode = -1. It is the callback's responsibility
	// to extract the unicode character from the extra characters string:
	// event.getCharacters().codePointAt(0).

	private void emulate(int keycode) {
		emulate(keycode, 0);
	}

	private void emulate(int keycode, int mods) {
		vm.postEvent(	2 /* EventTypeKeyboard */,
				0 /* timeStamp */,
				keycode /* charCode */,
				0 /* EventKeyChar */,
				mods | (shiftOn > 0 ? 1 : 0) /* modifiers */,
				keycode /* utf32Code */,
				0 /* reserved1 */,
				0 /* windowIndex */);
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
//		ctx.toastMsg("Key Event: " + event + " " + keyCode);
		int rc;
		switch(keyCode) {
			case 59:
			case 60:
				shiftOn = 1;
				return true;
			case 126:
			case 127:
				ctrlOn = 1;
				return true;
			case KeyEvent.KEYCODE_BACK:
				switch(buttonBits)
				{
				    case yellowButtonBit:
				        buttonBits = blueButtonBit;
				        ctx.toastMsg("mouse blue");
					break;
				    default:
				        buttonBits = yellowButtonBit;
				        ctx.toastMsg("mouse yellow");
					break;
				}
				return true;
			case 92: //KeyEvent.KEYCODE_PAGE_UP:
				buttonBits = blueButtonBit;
				ctx.toastMsg("mouse blue");
				return true;
			case KeyEvent.KEYCODE_MENU:
				InputMethodManager imm = (InputMethodManager)
					ctx.getSystemService(Context.INPUT_METHOD_SERVICE);
				imm.showSoftInput(this, 0, rr);
				return true;
			case 78: // received for Home and End, but scancodes are different
				switch(event.getScanCode()) {
					case 102:	// Home
						emulate(1);
						rc = vm.interpret();
						break;
					case 107:	// End
						emulate(4);
						rc = vm.interpret();
						break;
					default:
						rc = 0;
						break;
				}
				break;
			case KeyEvent.KEYCODE_ENTER:      // special handling for Enter: send ^M
				vm.postEvent(	2 /* EventTypeKeyboard */,
						0 /* timeStamp */,
						13 /* charCode */,
						0 /* EventKeyChar */,
						0 /* modifiers */,
						13 /* utf32Code */,
						0 /* reserved1 */,
						0 /* windowIndex */);
				rc = vm.interpret();
				break;
			case KeyEvent.KEYCODE_DPAD_RIGHT: // special handling for right arrow
				emulate(29);
				rc = vm.interpret();
				break;
			case KeyEvent.KEYCODE_DPAD_LEFT: // special handling for left arrow
				emulate(28);
				rc = vm.interpret();
				break;
			case KeyEvent.KEYCODE_DPAD_UP: // special handling for up arrow
				emulate(30, (event.getScanCode() == 0) ? 2 : 0); // 2 for wheel
				rc = vm.interpret();
				break;
			case KeyEvent.KEYCODE_DPAD_DOWN: // special handling for down arrow
				emulate(31, (event.getScanCode() == 0) ? 2 : 0); // 2 for wheel
				rc = vm.interpret();
				break;
			case KeyEvent.KEYCODE_DEL: // special handling for DEL
				emulate(8);
				rc = vm.interpret();
				break;
			default:		 // send key event
				int uchar = (keyCode != -1)?
					event.getUnicodeChar():
					event.getCharacters().codePointAt(0);
				if (uchar == 0) return false;
				int mod = (ctrlOn > 0) ? 2 : 0;
				if (ctrlOn > 0) uchar = uchar & 0x1F;
				vm.postEvent(	2 /* EventTypeKeyboard */,
						0 /* timeStamp */,
						uchar /* charCode */,
						0 /* EventKeyChar */,
						mod /* modifiers */,
						uchar /* utf32Code */,
						0 /* reserved1 */,
						0 /* windowIndex */);
				rc = vm.interpret();
		}
		if(rc != 0) timerEvent(20, 1);
		return true;
	}

	// Touch the screen and possibly move while pressing. Current button bits
	// will be used until the pressure removed, i. e. forward/back button modifiers
	// last exactly for one touch (or click).

	public boolean onTouchEvent(MotionEvent event) {
		int buttons = 0;
		int modifiers = 0;
		int ex = (int)event.getX();
		int ey = (int)event.getY();
		int dx = ex - lastX;
		int dy = ey - lastY;
		long ts = System.currentTimeMillis();

		switch(event.getAction()) {
			case MotionEvent.ACTION_DOWN: // 0
				buttons = buttonBits;
				break;
			case MotionEvent.ACTION_MOVE: // 2
				buttons = buttonBits;
				break;
			case MotionEvent.ACTION_UP: // 1
				buttons = 0;
				buttonBits = redButtonBit;
				break;
			default:
				System.out.println("Unsupported mtn. action: " + event.getAction());
				return false;
		}
		if((dx * dx + dy * dy < radius * radius) && buttons == lastBut) return true;
		timestamp = ts;
		//ctx.toastMsg(event.toString());
		vm.postEvent(1 /* EventTypeMouse */, 0 /* timestamp */, 
					ex, ey, 
					buttons, modifiers, 0, 0);
		int rc = vm.interpret();
		lastX = ex;
		lastY = ey;
		lastBut = buttons;
		if(rc != 0) timerEvent(20, 1);
		return true;
	}

	/**
     * Render me
     * 
     * @see android.view.View#onDraw(android.graphics.Canvas)
     */
    @Override
    protected void onDraw(Canvas canvas) {
	if (bits == null) return;
    	Rect dirtyRect = new Rect(0,0,0,0);
    	if(canvas.getClipBounds(dirtyRect)) {
    		/* System.out.println("dirtyRect: " + dirtyRect); */
    		vm.updateDisplay(bits, width, height, depth, 
				dirtyRect.left, dirtyRect.top, dirtyRect.right, dirtyRect.bottom);
    	}
        super.onDraw(canvas);
        canvas.drawColor(-1);
    	canvas.drawBitmap(bits, 0, width, 0, 0, width, height, false, paint);
    }
    
    /**
     * Text Input handling
     */
    @Override
    public boolean onCheckIsTextEditor() {
    	return true;
    }
    
    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
    	if(!onCheckIsTextEditor()) return null;
    	return new BaseInputConnection(this, false);
    }
    
    public void sendText(CharSequence text) {
		System.out.println("sendText: " + text);
    	for(int index=0; index<text.length(); index++) {
    		vm.postEvent(2 /* EventTypeKeyboard */, 0 /* timestamp */, 
    			(int)text.charAt(index),
    			0 /* EventKeyChar */,
    			0 /* Modifiers */, 
    			0, 0, 0);
    	}
		vm.interpret();
    }
}
