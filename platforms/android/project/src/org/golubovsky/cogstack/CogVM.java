package org.golubovsky.cogstack;

import android.content.res.AssetManager;
import java.lang.Exception;
import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;

import java.util.Locale;
import java.text.DateFormatSymbols;
import java.text.SimpleDateFormat;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;

import org.golubovsky.cogstack.CogActivity;
import org.golubovsky.cogstack.CogView;

import android.os.Environment;
import android.content.Context;
import android.text.ClipboardManager;
import android.app.NotificationManager;
import android.speech.tts.TextToSpeech;
import android.graphics.Bitmap;

import android.content.Intent;
import android.net.Uri;


public class CogVM {
	CogActivity context;
	CogView view;
	File imageDir;
	TextToSpeech mTts = null;
	float pitch = 1.0f;
	float rate = 1.0f;

/* Store the desired speech rate value */

    int setSpeechRate(float r) {
	if(mTts == null) return TextToSpeech.ERROR;
	rate = r;
	return TextToSpeech.SUCCESS;
    }

/* Store the desired pitch value */

    int setPitch(float p) {
	if(mTts == null) return TextToSpeech.ERROR;
	pitch = p;
	return TextToSpeech.SUCCESS;
    }

/* Stop the speech */

    int stop() {
	if(mTts == null) return TextToSpeech.ERROR;
	return mTts.stop();
    }

/* Open the given string in a browser (broadcast an URI intent) */

    int openURI(String url) {
	final Intent intent = new Intent(Intent.ACTION_VIEW).setData(Uri.parse(url));
	context.startActivity(intent);
	return 0;
    }


/* Speak a given string if text */

    int speak(String txt) {
	if(mTts == null) return -1;
	context.toastMsg("speaking: " + txt);
	mTts.setPitch(pitch);
	mTts.setSpeechRate(rate);
 	mTts.speak(txt, TextToSpeech.QUEUE_ADD, null);
	mTts.speak("", TextToSpeech.QUEUE_ADD, null);
	return txt.length();
    }

/* Finish the whole activity */

    public void finish() {
	context.toastMsg("Cog VM finishing");
	context.finish();
       	String ns = Context.NOTIFICATION_SERVICE;
        NotificationManager notmgr = (NotificationManager) context.getSystemService(ns);
	if(notmgr != null) notmgr.cancelAll();
	surelyExit();
    }

/* Helper method to place a shortcut icon on the home screen for the given image */

    public void imageShortCut(String imagePath,              // path to the image (not checked)
		              String label,                  // shortcut label
			      String cmd,                    // command to run with image (rsvd)
			      int icnwh,                     // icon bitmap width<<16 | height in px
			      int icnflg,                    // shortcut flags (rsvd)
			      byte icnbits[])                // icon bits (ARGB_8888)
    {

        // Intent to be placed on the shortcut

	Intent sci = new Intent();
	sci.setAction(Intent.ACTION_VIEW);
	Uri uri = new Uri.Builder().scheme("file").path(imagePath).build();
	sci.setDataAndType(uri, "application/x-squeak-image");
	sci.putExtra("command", cmd);
	
	// Intent to create the shortcut. If bitmap address is 0 then use a generic icon
	// from resources. Otherwise build a bitmap.

        Intent addIntent = new Intent();
	addIntent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, sci);
	addIntent.putExtra(Intent.EXTRA_SHORTCUT_NAME, label);
	if(icnbits == null) {
	    addIntent.putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE, 
			Intent.ShortcutIconResource.fromContext(context, R.drawable.smalltalk));
	} else {
	    int i, j;
	    int colors[] = new int[1 + icnbits.length / 4];
	    for(i = j = 0; i < icnbits.length; i++) {
		int m = i % 4;
		if(m == 0) colors[j] = 0;
		switch(i % 4) {
		    case 0:
			colors[j] |= ((icnbits[i] << 24) & 0xFF000000);
			break;
		    case 1:
			colors[j] |= ((icnbits[i] << 16) & 0x00FF0000);
			break;
		    case 2:
			colors[j] |= ((icnbits[i] << 8) & 0x0000FF00);
			break;
		    case 3:
			colors[j] |= (icnbits[i] & 0x00FF);
			j++;
		        break;
		    default:
			;
		}
	    }
	    Bitmap bmp = Bitmap.createBitmap(colors, 
			    (icnwh >> 16) & 0xFFFF, icnwh & 0xFFFF, Bitmap.Config.ARGB_8888);
            Bitmap scaled = Bitmap.createScaledBitmap(bmp, 48, 48, true);
	    addIntent.putExtra(Intent.EXTRA_SHORTCUT_ICON, scaled);
	}
	addIntent.setAction("com.android.launcher.action.INSTALL_SHORTCUT");
	context.sendBroadcast(addIntent);


    }



    public void loadImage(String imageName, String cmd) {
	try {
	        String imgpath = imageName;
	        File imgfile = new File(imgpath);
	        long fsize = imgfile.length();
	        context.toastMsg("image found size: " + fsize);
	        int irc = setImagePath(imageName, cmd);
	        if (irc != 0) {
		       context.toastMsg("Failed to load image " + imageName);
	        } else {
	            imageDir = new File(imgpath).getParentFile();
      	            context.setWindowTitle("Cog: " + imgpath);
	            interpret();
	        }
    	    } catch (Exception e) {
	    context.toastMsg("Failed to load image " + imageName + ": " + e.toString());
	}
    }

    public int postEvent(int type, int stamp, int arg3, int arg4,
			 int arg5, int arg6, int arg7, int arg8) {
        int rc = sendEvent(type, stamp, arg3, arg4, arg5, arg6, arg7, arg8);
	return rc;
    }
   
    /* VM callbacks */
    public void invalidate(int left, int top, int right, int bottom) {
    	/* System.out.println("Invalidating: (" + left + "," + top + " -- " + right + "," + bottom + ")"); */
    	view.invalidate(left, top, right, bottom);
    }

    /* Show/hide soft keyboard: needed by a Smalltalk primitive */

    public void showHideKbd(int what) {
	if (view != null) view.showHideKbd(what);
    }

    /* Display a brief message (toast) - to be called by the interpreter */

    public void briefMessage(String s) {
        context.toastMsg(s);
    }

    /* Obtain a string of text from Android clipboard, if available */

    public String getClipboardString() {
	ClipboardManager cmgr = 
	    (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
	if (cmgr == null) return "";
	CharSequence paste = cmgr.getText();
	String ptxt = (paste != null)?paste.toString():"";
	return ptxt;
    }

    /* Obtain the time format per current locale */

    public String getTimeFormat(int longfmt) {
	Locale loc = Locale.getDefault();
	int jlfmt = 
	    (longfmt == 1)?java.text.SimpleDateFormat.LONG:java.text.SimpleDateFormat.SHORT;
        SimpleDateFormat sdf = 
	    (SimpleDateFormat)SimpleDateFormat.getTimeInstance(jlfmt, loc);
	return sdf.toLocalizedPattern();
    }

    /* Obtain the date format per current locale */

    public String getDateFormat(int longfmt) {
	Locale loc = Locale.getDefault();
	int jlfmt = 
	    (longfmt == 1)?java.text.SimpleDateFormat.LONG:java.text.SimpleDateFormat.SHORT;
        SimpleDateFormat sdf = 
	    (SimpleDateFormat)SimpleDateFormat.getDateInstance(jlfmt, loc);
	return sdf.toLocalizedPattern();
    }

    /* Obtain the current/default Locale string */

    public String getLocaleString() {
	Locale loc = Locale.getDefault();
	return loc.toString();
    }

    /* Obtain the thousand and decimal separators per current locale */

    public String getSeparators() {
	Locale loc = Locale.getDefault();
	DecimalFormatSymbols dfs = new DecimalFormatSymbols(loc);
	char dec = dfs.getDecimalSeparator();
	char thou = dfs.getGroupingSeparator();
	return new String(new char[] {dec, thou});
    }

    /* Obtain the currency symbol per current locale */

    public String getCurrencySymbol() {
	Locale loc = Locale.getDefault();
	DecimalFormatSymbols dfs = new DecimalFormatSymbols(loc);
	return dfs.getCurrencySymbol();
    }

    /* Set VM idle timer interval */

    public void setVMTimerInterval(int d) {
	if (view != null) view.setTimerDelay(d);
    }

    /* Get VM idle timer interval */

    public int getVMTimerInterval() {
	if (view != null) return view.getTimerDelay();
	else return -1;
    }

    /* Get SD card root directory */

    public String getSDCardRoot() {
	return Environment.getExternalStorageDirectory().getAbsolutePath();
    }

    /* PRELOAD functions */
    public native int setLogLevel(int logLevel);

    /* Main entry points */
    public native int setScreenSize(int w, int h);
    public native int setImagePath(String imageName, String cmd);
    public native int sendEvent(int type, int stamp, int arg3, int arg4,
				int arg5, int arg6, int arg7, int arg8);
    public native int updateDisplay(int bits[], int w, int h, int d, int l, int t, int r, int b);
    public native int interpret();

    public native void surelyExit();

    /* Load the CogVM module */
    static {
    	System.out.println("Loading cogvm shared library");
        System.loadLibrary("cogvm");
    }
}
