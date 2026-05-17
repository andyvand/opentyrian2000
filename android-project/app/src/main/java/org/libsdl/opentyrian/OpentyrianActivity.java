
package org.libsdl.opentyrian;

import android.content.SharedPreferences;
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
    private static final String TAG = "OpentyrianActivity";

    // Bumped automatically by the build: changes whenever assets are re-staged.
    // Stored in SharedPreferences so we only re-copy when the APK's data
    // differs from what's already in the per-app data directory.
    private static final String PREFS_NAME = "tyrian_assets";
    private static final String KEY_INSTALLED_VERSION = "installed_version";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            installDataFiles();
        } catch (IOException e) {
            Log.e(TAG, "Failed to install Tyrian data files", e);
        }
        super.onCreate(savedInstanceState);
    }

    // Copy bundled assets/data/* into <filesDir>/data/ and ensure the save
    // directory exists. Files are skipped when already present and the
    // installed-version marker matches the current APK build, so this is
    // effectively a no-op after the first launch.
    private void installDataFiles() throws IOException {
        File filesDir = getFilesDir();
        File dataDir = new File(filesDir, "data");
        File saveDir = new File(filesDir, "save");

        if (!dataDir.exists() && !dataDir.mkdirs()) {
            throw new IOException("Cannot create " + dataDir);
        }
        if (!saveDir.exists() && !saveDir.mkdirs()) {
            throw new IOException("Cannot create " + saveDir);
        }

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        long installedVersion = prefs.getLong(KEY_INSTALLED_VERSION, -1);
        long currentVersion;
        try {
            currentVersion = getPackageManager()
                .getPackageInfo(getPackageName(), 0).lastUpdateTime;
        } catch (Exception e) {
            currentVersion = 0;
        }

        boolean forceCopy = installedVersion != currentVersion;
        copyAssetDir("data", dataDir, forceCopy);

        prefs.edit().putLong(KEY_INSTALLED_VERSION, currentVersion).apply();
    }

    private void copyAssetDir(String assetPath, File outDir, boolean force) throws IOException {
        AssetManager am = getAssets();
        String[] names = am.list(assetPath);
        if (names == null || names.length == 0) {
            Log.w(TAG, "No assets under '" + assetPath + "'");
            return;
        }
        for (String name : names) {
            String childAsset = assetPath + "/" + name;
            File outFile = new File(outDir, name);
            String[] sub = am.list(childAsset);
            if (sub != null && sub.length > 0) {
                if (!outFile.exists() && !outFile.mkdirs()) {
                    throw new IOException("Cannot create " + outFile);
                }
                copyAssetDir(childAsset, outFile, force);
            } else {
                if (!force && outFile.exists()) {
                    continue;
                }
                copyAssetFile(childAsset, outFile);
            }
        }
    }

    private void copyAssetFile(String assetPath, File outFile) throws IOException {
        try (InputStream in = getAssets().open(assetPath);
             OutputStream out = new FileOutputStream(outFile)) {
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        }
    }
}
