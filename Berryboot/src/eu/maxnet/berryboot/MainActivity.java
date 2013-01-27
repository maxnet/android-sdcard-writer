/**
 * Berryboot image writer tool for Android
 * 
 * Copyright (C) Floris Bos 2012
 * Dual licensed: Simplified BSD / GPL
 */

package eu.maxnet.berryboot;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.os.Bundle;
import android.os.Handler;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;

public class MainActivity extends Activity implements OnClickListener {

	protected Button buttonWriteImage;
	protected Button buttonExit;
	protected EditText editImageFile;
	protected EditText editOutputDevice;
	protected CheckBox checkPatch;
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        buttonWriteImage = (Button) findViewById(R.id.buttonWriteImage);
        buttonExit       = (Button) findViewById(R.id.buttonExit);
        editImageFile    = (EditText) findViewById(R.id.editImagefile);
        editOutputDevice = (EditText) findViewById(R.id.editOutputdevice);
        checkPatch       = (CheckBox) findViewById(R.id.checkPatch);
        
        editImageFile.setText(getFilesDir().getPath()+"/berryboot.img");
        buttonExit.setOnClickListener(this);
        buttonWriteImage.setOnClickListener(this);
        buttonExit.requestFocus();
        
        if (!isA10()) {
        	checkPatch.setChecked(false);
        	checkPatch.setEnabled(false);
        	showMessage("Your device does not have an A10 CPU. Disabling patch functions");
        }
        else if (!hasNAND()) {
        	checkPatch.setChecked(false);
        	checkPatch.setEnabled(false);
        	showMessage("Your device does not have NAND. Disabling patch functions as we do not know how to get script.bin");
        }
    }
    
    public void onClick(View v) {
    	if (v == buttonExit) {
    		finish();
    	} else if (v == buttonWriteImage) {
    		writeImage();
    	}
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.activity_main, menu);
        return true;
    }
    
    
    protected boolean isA10()
    {
    	try
    	{
	    	FileInputStream fis = new FileInputStream("/proc/cpuinfo");
	    	byte[] buffer = new byte[1024];
	    	fis.read(buffer);
	    	fis.close();
	    	String s = new String(buffer);
	    	
	    	return s.contains("sun4i") || s.contains("sun5i");
	    	
    	} catch (Exception e) {
    		return false;
    	}
    }
    
    protected boolean hasNAND()
    {
    	return new File("/dev/block/nanda").exists();
    }

    
    protected void writeImage()
    {
    	// Verify that root access is obtainable 
    	if (!execCommandAsRoot("true")) {
        	showMessage("Your device is not rooted. Cannot proceed.");
        	return;
    	}

    	// Verify that SD card device exists
    	File sdDevice = new File(editOutputDevice.getText().toString());
        if (!sdDevice.exists()) {
        	showMessage("SD card device does not exist, or no card inserted");
        	return;
        }
        
        // Progress dialog
    	final Handler progressHandler = new Handler();    	
        final ProgressDialog progress = new ProgressDialog(this);
        progress.setCancelable(false);
        progress.setMessage("Starting thread");
        progress.show();

        // Write image in worker thread 
        new Thread(new Runnable() {
			public void run() {

				String imagefile = editImageFile.getText().toString();
				String sd = editOutputDevice.getText().toString(); 
				String datadir = getFilesDir().getPath();

				/* The image files need to be extracted from the .apk to the data folder
				 * unless the user has run the app before, and this has already been done.
				 * We store the version (lastUpdate timestamp) of the extracted files in the preferences.
				 */
				SharedPreferences preferences = getPreferences(MODE_PRIVATE);
				long berrybootVersion = -1;

				try {
					berrybootVersion = getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime;
				} catch (NameNotFoundException e) {
					e.printStackTrace();
				}
				if (!new File(datadir+"/a10-patchspl").exists()
						|| preferences.getLong("berrybootimgversion", 0) != berrybootVersion)
				{
					updateProgress("Uncompressing files from .apk");
					try	{
						uncompressFile("berryboot.img", datadir);
						uncompressFile("sunxi-spl.bin", datadir);
						uncompressFile("a10-patchspl", datadir);
						new File(datadir+"/a10-patchspl").setExecutable(true);
						new File(datadir+"/mntNand").mkdir();
						new File(datadir+"/mntSD").mkdir();
						preferences.edit().putLong("berrybootimgversion", berrybootVersion).commit();
					} catch (IOException ie)	{
						error("Error uncompressing files. Check disk space.");
						return;
					}
				}
				
				updateProgress("Unmounting SD card");
				execCommandAsRoot("umount "+sd);
				execCommandAsRoot("umount "+sd+"p1");
				/* Also unmount everything mounted to /mnt/extsd,
				 * as vold seems to mount /dev/vold/somedevice instead of /dev/block/mmcblk0p1 */
				String[] extsdFolders = new File("/mnt/extsd").list();
				for (String extsdFolder : extsdFolders)
				{
					execCommandAsRoot("umount /mnt/extsd/"+extsdFolder);
				}
				execCommandAsRoot("umount /mnt/extsd");
								
				updateProgress("Writing image to SD card");
				if (!execCommandAsRoot("dd 'if="+imagefile+"' of="+sd+" bs=1024k"))
				{
					error("Error writing image file to SD card");
					return;
				}
				
				if (checkPatch.isChecked())
				{
					updateProgress("Patching u-boot SPL");
					if (!execCommandAsRoot(datadir+"/a10-patchspl "+datadir+"/sunxi-spl.bin "+sd))
					{
						error("Error patching u-boot SPL");
						return;
					}
					
					updateProgress("Copying script.bin from NAND to SD");
					if (!execCommandAsRoot("mount -t vfat /dev/block/nanda "+datadir+"/mntNand"))
					{
						error("Error mounting /dev/block/nanda");
						return;
					}
					if (!execCommandAsRoot("mount -t vfat "+sd+"p1 "+datadir+"/mntSD"))
					{
						error("Error mounting SD card device");
						return;
					}
					
					String scriptbin;
					if ( new File(datadir+"/mntNand/script.bin").exists() )
						scriptbin = datadir+"/mntNand/script.bin";
					else if ( new File(datadir+"/mntNand/evb.bin").exists() )
						scriptbin = datadir+"/mntNand/evb.bin";
					else
					{
						error("Neither script.bin nor evb.bin exists in NAND");
						return;
					}
					if (!execCommandAsRoot("cat "+scriptbin+ " >"+datadir+"/mntSD/script.bin"))
					{
						error("Error copying "+scriptbin+" to SD");
						return;
					}
					
					/* Save human readable a10-memdump info to a10-meminfo.txt on the SD card for debugging purposes */
					execCommandAsRoot(datadir+"/a10-patchspl -dump > "+datadir+"/mntSD/a10-meminfo.txt");

					if (!execCommandAsRoot("umount "+datadir+"/mntNand"))
					{
						error("Error unmounting /dev/block/nanda");
						return;
					}
					if (!execCommandAsRoot("umount "+datadir+"/mntSD"))
					{
						error("Error unmounting SD card device");
						return;
					}					
				}
				
				updateProgress("Finish writing... (sync)");
				execCommandAsRoot("sync");
				
				finished();
			}

	        // Update the progress in the UI thread 
			protected void updateProgress(final String msg)
			{
				progressHandler.post(new Runnable() {
					@Override
					public void run() {
						progress.setMessage(msg);
					}
				});
			}
			
			// Show error in UI thread
			protected void error(final String msg)
			{
				progressHandler.post(new Runnable() {
					@Override
					public void run() {
						progress.hide();
						showMessage(msg);
					}
				});
			}
			
			// Notify UI thread we are finished
			protected void finished() {
				progressHandler.post(new Runnable() {
					@Override
					public void run() {
						progress.hide();
						askReboot();
					}
				});
			}
			
			/*
			 * Uncompress utility file stored in .apk to datadir 
			 */
			protected void uncompressFile(String filename, String dest) throws IOException
			{
				InputStream  is = getAssets().open(filename);
				OutputStream os = new FileOutputStream(dest+"/"+filename);
				
				byte[] buf = new byte[4096];
				int len;
				while ((len =is.read(buf)) > 0) {
					os.write(buf, 0, len);
				}
				os.close();
				is.close();
			}
		}).start(); // start thread
    }
    
    protected void showMessage(String msg)
    {
    	new AlertDialog.Builder(this)
		.setMessage(msg)
		.setNeutralButton("Ok", null)
		.create().show();    	
    }
    
    protected void askReboot()
    {
        /* Ask if user wants to reboot */
    	new AlertDialog.Builder(this)
		.setMessage("All done! Reboot now?")
		.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
			
			@Override
			public void onClick(DialogInterface dialog, int which) {
				execCommandAsRoot("reboot");
			}
		})
		.setNegativeButton("No", null)
		.create().show();     	
    }
    
    protected boolean execCommandAsRoot(String path)
    {
    	try {
    		String[] cmd = {"su","-c",path};
			Process p = Runtime.getRuntime().exec(cmd);
			int exitCode = p.waitFor();
			
			return (exitCode == 0);
			
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}
    }
}
