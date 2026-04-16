
package org.libsdl.opentyrian;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class OpentyrianActivity extends SDLActivity
{
    private static final String TAG = "OpenTyrian";

    // Bump this when shipping updated game data so it re-extracts automatically.
    private static final String DATA_STAMP = ".data_v1";

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        extractGameData();
        super.onCreate(savedInstanceState);
    }

    private void extractGameData()
    {
        File dest = getFilesDir();
        File stamp = new File(dest, DATA_STAMP);
        if (stamp.exists()) return;

        Log.i(TAG, "Extracting bundled game data to " + dest.getAbsolutePath());
        AssetManager am = getAssets();

        try
        {
            String[] files = am.list("");
            if (files == null) return;

            for (String name : files)
            {
                File out = new File(dest, name);
                if (out.exists()) continue;
                try (InputStream in = am.open(name);
                     OutputStream os = new FileOutputStream(out))
                {
                    byte[] buf = new byte[65536];
                    int n;
                    while ((n = in.read(buf)) != -1) os.write(buf, 0, n);
                }
                catch (IOException e)
                {
                    Log.w(TAG, "Skipping asset: " + name + " (" + e.getMessage() + ")");
                }
            }

            stamp.createNewFile();
            Log.i(TAG, "Game data extraction complete.");
        }
        catch (IOException e)
        {
            Log.e(TAG, "Failed to list/extract game data: " + e.getMessage());
        }
    }
}
