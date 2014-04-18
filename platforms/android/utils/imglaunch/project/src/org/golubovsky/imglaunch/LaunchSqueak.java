package org.golubovsky.imglaunch;

import android.app.Activity;
import android.os.Bundle;

import org.golubovsky.imglaunch.LaunchActivity;

public class LaunchSqueak extends LaunchActivity {

        /** Called when the activity is first created. */
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            String path = getText(R.string.squeak_imgpath).toString();
	    launchImage(path);
   	    this.finish();
        }
}
	
