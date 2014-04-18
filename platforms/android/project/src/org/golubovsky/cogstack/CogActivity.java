package org.golubovsky.cogstack;

import android.util.Log;

import android.app.Activity;
import android.os.Bundle;

import org.golubovsky.cogstack.CogVM;
import org.golubovsky.cogstack.CogView;
import org.golubovsky.cogstack.CogImgList;

import android.widget.Toast;
import android.widget.ListAdapter;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.view.View;
import android.widget.TextView;
import android.view.Gravity;
import android.view.View.OnKeyListener;
import android.view.KeyEvent;

import android.speech.tts.TextToSpeech;
import java.util.Locale;

import android.os.Environment;
import android.content.Intent;
import android.content.res.AssetManager;

import java.io.File;
import java.io.InputStream;
import java.io.FileFilter;
import java.io.FileOutputStream;

import java.lang.Exception;

import java.util.Arrays;
import java.util.ArrayList;
import android.util.DisplayMetrics;
import java.util.HashSet;

import android.net.Uri;

import android.content.Context;
import android.app.NotificationManager;
import android.app.Notification;
import android.app.PendingIntent;

import java.util.zip.ZipEntry; 
import java.util.zip.ZipInputStream; 

public class CogActivity extends Activity implements TextToSpeech.OnInitListener {
	CogVM vm;
	CogView view;
	CogImgList imgl;
	TextToSpeech mTts;
	private Toast busy = null;
	boolean canspeak = false;
	boolean imgasset = false;

    // Implements TextToSpeech.OnInitListener.
    public void onInit(int status) {
        // status can be either TextToSpeech.SUCCESS or TextToSpeech.ERROR.
	    toastMsg("status: " + status);
    	    if (status == TextToSpeech.SUCCESS) {
	    Locale loc = Locale.getDefault();
            // Set preferred language to US english.
            // Note that a language may not be available, and the result will indicate this.
            int result = mTts.setLanguage(loc);
            // Try this someday for some interesting results.
            // int result mTts.setLanguage(Locale.FRANCE);
            if (result == TextToSpeech.LANG_NOT_SUPPORTED) {
               // Lanuage data is missing or the language is not supported.
                toastMsg(loc.toString() + ": Language is not supported.");
	    } else if (result == TextToSpeech.LANG_MISSING_DATA) {
               // Lanuage data is missing or the language is not supported.
                toastMsg(loc.toString() + ": Missing language data.");
            } else {
                // The TTS engine has been successfully initialized.
 		    canspeak = true;
		    if(vm != null) {
			vm.mTts = mTts;
		    }
            }
        } else {
            // Initialization failed.
            toastMsg("Could not initialize TextToSpeech.");
        }
    }

	/** Walk along the image search path (colon-separated) and look
	 * for files with extension .image, recursing into directories when needed
	 * Return a list of files found.
	 */

    File[] findImages(String dir) {
	try {
	    File fdir = new File(dir);
	    if(!fdir.isDirectory()) return new File[0];
	    File[] images = fdir.listFiles(new FileFilter() {
		public boolean accept(File f) {
		    return f.getName().endsWith(".image");
		}
	    });
	    File[] subdirs = fdir.listFiles(new FileFilter() {
		public boolean accept(File f) {
		    return f.isDirectory();
		}
	    });
	    ArrayList<String> sdnames = new ArrayList<String>();
	    for(int i = 0; i < subdirs.length; i++) {
		sdnames.add(subdirs[i].getAbsolutePath());
	    }
	    String[] sdstrn = (String[])sdnames.toArray(new String[sdnames.size()]);
	    File[] subfiles = findImageFiles(sdstrn);
	    ArrayList<File>fimages = new ArrayList<File>(Arrays.asList(images));
	    for(int i = 0; i < subfiles.length; i++) {
		fimages.add(subfiles[i]);
	    }
	    return (File[])fimages.toArray(new File[fimages.size()]);
	} catch (Exception e) {
            return new File[0];
	}
    }

    File[] findImageFiles(String[] dirs) {
	ArrayList<File> res = new ArrayList<File>();
	for(int i = 0; i < dirs.length; i++) {
	    File[] imgs = findImages(dirs[i]);
	    ArrayList<File> aimg = new ArrayList<File>(Arrays.asList(imgs));
	    res.addAll(aimg);
	}
	HashSet<File> hs = new HashSet<File>();
	hs.addAll(res);
	return (File[])hs.toArray(new File[hs.size()]);
    }

	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
    	super.onCreate(savedInstanceState);
	Intent intnt = getIntent();
	Uri uri = intnt.getData();
	if(uri == null) {
	    if(!loadFromAssets(this)) loadFromList(this);
	} else {
	    String imgpath = uri.getPath();
	    if(imgpath == null || !uri.getScheme().equals("file")) {
		toastMsg("intent contains no file path");
		finish();
	    }
	    File imgf = new File(imgpath);
            if(!imgf.exists()) {
		toastMsg("file " + imgpath + " does not exist");
		finish();
	    }
	    String cmd = intnt.getStringExtra("command");
	    if(cmd == null) cmd = "";
	    startVM(imgpath, cmd);
	}
    }


	/**
	 * Try to load an image from bundled assets. Determine if assets do contain
	 * a bundled image (under the "image" path in assets), if they do, create a
	 * temporary file in the cache (gets overwritten every time), read all assets
	 * consecutively into that file, and start VM with this image. Return false
	 * if by any reason loading from assets failed: standard image selection from
	 * list will follow.
	 */


    boolean loadFromAssets(final CogActivity ctx) {
	AssetManager am = ctx.getAssets();
	try {
	    String asroot = "image";
  	    String path = "tmpimage.image";
	    int buflen = 65536;
	    byte[] buf = new byte[buflen];
	    int ofs, len;
	    String as[] = am.list(asroot);
	    if (as.length == 0) return false;
	    FileOutputStream fos = openFileOutput(path, MODE_PRIVATE);
	    File imgpath = getFileStreamPath(path);
	    String tmpi = imgpath.getAbsolutePath();
	    String imgdir = imgpath.getParent();
	    for(int i = 0; i < as.length; i++) {
		InputStream is = am.open(asroot + "/" + as[i], AssetManager.ACCESS_STREAMING);
                    while((len = is.read(buf, 0, buflen)) > 0) {
                        fos.write(buf, 0, len);
		    }
		    is.close();
	    }
	    fos.close();
	    unzipFiles(am, imgdir);
	    imgasset = true;
	    startVM(tmpi, "");
	} catch (Exception e) {
	    ctx.toastMsg(e.toString());
	    return false;
	}
        return true;
    }

        /** If image is not loaded from assets, set title to its path */

    void setWindowTitle(String t) {
	if(!imgasset) super.setTitle(t);
    }


	/** Unzip pre-packed files along with the image.
	 *  The assets facility is not very convenient when it comes to packong multiple
	 *  files in a tree hierarchy. This method obtains the list of zipped files
	 *  stored under the "zipped" directory of assets and unzips them along with the
	 *  earlier unpacked image. Such unzipping occurs each time VM is started.
	 */

    void unzipFiles(AssetManager am, String imgdir) throws Exception {
	String zfroot = "zipped";
	String[] zips = am.list(zfroot);
	if(zips.length == 0) return;
        int buflen = 65536;
	byte[] buf = new byte[buflen];
	int ofs, len;
	for(int i = 0; i < zips.length; i++) {
	    ZipInputStream zin = new ZipInputStream(am.open(zfroot + "/" + zips[i], 
			  	                            AssetManager.ACCESS_STREAMING));
	    ZipEntry ze = null;
	    while ((ze = zin.getNextEntry()) != null) {
	        Log.v("Cog Assets", "Unzipping " + ze.getName());
		String dest = imgdir + "/" + ze.getName();
                if(ze.isDirectory()) {
		    Log.v("Cog Assets", "Creating directory " + dest);
		    new File(dest).mkdirs();
		} else {
		    File df = new File(dest);
		    Log.v("Cog Assets", "Writing to " + dest);
		    FileOutputStream fos = new FileOutputStream(df);
                    while((len = zin.read(buf, 0, buflen)) > 0) {
                        fos.write(buf, 0, len);
		    }
		    fos.close();
		    zin.closeEntry();
		}
	    }
	}
    }

    void loadFromList(final CogActivity ctx) {
  	toastMsg("Select an image to load");
	String extdir = Environment.getExternalStorageDirectory().getAbsolutePath();
	String imgdirs = extdir + File.pathSeparator + getText(R.string.imgdirs).toString();
	File[] imgfiles = findImageFiles(imgdirs.split(File.pathSeparator));
	imgl = new CogImgList(this);
	imgl.setAdapter(new ArrayAdapter<File>(this, R.layout.list_item, imgfiles));
	setContentView(imgl);
	imgl.setFocusable(true);
	imgl.requestFocus();
	imgl.setOnItemClickListener(new OnItemClickListener() {
	    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
	        imgl = null;
	 	ctx.startVM(((TextView) view).getText().toString());
            }
	});
    }

    public void startVM(String imgpath) {
	startVM(imgpath, "");
    }

    public void startVM(String imgpath, String cmd) {

    	/* stupid setup dance but I'm not sure who is going to need what here */
    	vm = new CogVM();
    	vm.context = this;
	vm.setLogLevel(9);
    	view = new CogView(this);
    	view.vm = vm;
    	vm.view = view;
	if(canspeak) vm.mTts = mTts;
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics); setContentView(view);
	vm.setScreenSize(metrics.widthPixels, metrics.heightPixels);
    	vm.loadImage(imgpath, cmd);
	/* Unicode characters may be passed as extra characters array with action code
	 * ACTION_MULTIPLE. Examine this, an if not the case, return false to pass the
	 * event along, otherwise call the view's onKeyDown directly and consume the event.
	 * Use the first character of the array.
	 */
	view.setOnKeyListener(new OnKeyListener() {
		public boolean onKey(View v, int keyCode, KeyEvent event) {
			int action = event.getAction();
			if (action != KeyEvent.ACTION_MULTIPLE) return false;
			String cs = event.getCharacters();
			if ((cs != null) && (cs.length() >= 1)) {
				v.onKeyDown(-1, event);
				return true;
			}
			return false;
		}
	});
        view.setFocusable(true);
	view.setFocusableInTouchMode(true);
        view.requestFocus();
	Intent checkIntent = new Intent();
	checkIntent.setAction(TextToSpeech.Engine.ACTION_CHECK_TTS_DATA);
	startActivityForResult(checkIntent, 0);
	mTts = new TextToSpeech(this, this);
	String ns = Context.NOTIFICATION_SERVICE;
	try {
	    NotificationManager notmgr = (NotificationManager) getSystemService(ns);
	    Notification ntf = new Notification(R.drawable.ntficon, "", System.currentTimeMillis());
            Context context = getApplicationContext();
            CharSequence contentTitle = "CogDroid";
            CharSequence contentText = imgpath;
            Intent notificationIntent = new Intent(this, CogActivity.class);
            PendingIntent contentIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);

            ntf.setLatestEventInfo(context, contentTitle, contentText, contentIntent);
	    ntf.flags |= Notification.FLAG_NO_CLEAR;
	    notmgr.notify(1, ntf);
	} catch (Exception e) {
	    toastMsg(e.toString());
	}

    }

    @Override
    public void onDestroy() {
        // Don't forget to shutdown!
        if (mTts != null) {
            mTts.stop();
            mTts.shutdown();
        }
 
        super.onDestroy();
    }


    public void toastMsg(String txt) {
	Toast toast=Toast.makeText(this, txt, 4000);
	    toast.setGravity(Gravity.TOP, -30, 50);
	    toast.show();
    }

    public void showBusyMsg() {
	if (busy != null) busy.cancel();
	busy = Toast.makeText(this, "*Busy*", 2000);
	busy.setGravity(Gravity.CENTER, 0, 0);
	busy.show();
    }

    public void hideBusyMsg() {
	if (busy != null) {
	    busy.cancel();
	    busy = null;
	}
    }
}

