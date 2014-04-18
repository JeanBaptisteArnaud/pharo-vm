package org.golubovsky.imglaunch;

import android.app.Activity;
import android.os.Bundle;

import android.widget.Toast;
import android.view.Gravity;

import android.content.Intent;

import android.net.Uri;

public class LaunchActivity extends Activity {


    public void toastMsg(String txt) {
            Toast toast=Toast.makeText(this, txt, 2000);
            toast.setGravity(Gravity.TOP, -30, 50);
            toast.show();
    }

    public void launchImage(String imgpath) {
	    toastMsg(imgpath);
	    Intent cog = new Intent();
            cog.setAction(Intent.ACTION_VIEW);
	    Uri uri = new Uri.Builder().scheme("file").path(imgpath).build();
	    cog.setDataAndType(uri, "application/x-squeak-image");
	    try {
		    startActivity(cog);
	    } catch (Exception e) {
		toastMsg(e.toString());
	    }
    }

}

