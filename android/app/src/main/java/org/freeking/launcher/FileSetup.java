package org.freeking.launcher;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * File plumbing between the launcher and the native engine.
 *
 * Contract with C++ (Source/Core/Paths.cpp):
 *  - Engine assets (Fonts/, Shaders/, Textures/) live at the internal
 *    files dir root: context.getFilesDir().
 *  - Original game files live at getExternalFilesDir("kingpin"), i.e.
 *    <external>/kingpin/main/*.pak — Paths::KingpinDir() on Android.
 */
public final class FileSetup {
    private FileSetup() {
    }

    public interface Progress {
        void onProgress(long doneBytes, long totalBytes, String currentName);
    }

    /** App-specific external dir holding the user's Kingpin install copy. */
    public static File kingpinDir(Context context) {
        File base = context.getExternalFilesDir("kingpin");
        if (base == null) {
            // External storage unavailable: fall back to internal storage.
            // (The engine only checks external; Play stays disabled then.)
            return new File(context.getFilesDir(), "kingpin-unavailable");
        }
        return base;
    }

    /** Sorted list of game archives found under kingpin/main/. */
    public static List<File> findPaks(Context context) {
        List<File> paks = new ArrayList<File>();
        File main = new File(kingpinDir(context), "main");
        File[] files = main.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isFile() && f.getName().toLowerCase().endsWith(".pak")) {
                    paks.add(f);
                }
            }
        }
        Collections.sort(paks);
        return paks;
    }

    public static long totalBytes(List<File> files) {
        long total = 0;
        for (File f : files) {
            total += f.length();
        }
        return total;
    }

    /**
     * Copies engine assets out of the APK into the internal files dir.
     * Skipped when the marker matches the current versionCode.
     *
     * @return true if assets were (re-)extracted, false if already fresh.
     */
    public static boolean extractEngineAssets(Context context) throws IOException {
        int version = 0;
        try {
            PackageInfo info = context.getPackageManager()
                .getPackageInfo(context.getPackageName(), 0);
            version = info.versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            version = 0;
        }

        File marker = new File(context.getFilesDir(), "engine_assets.version");
        if (marker.exists()) {
            try {
                java.util.Scanner s = new java.util.Scanner(marker).useDelimiter("\\A");
                if (s.hasNext() && s.next().trim().equals(String.valueOf(version))) {
                    s.close();
                    return false;
                }
                s.close();
            } catch (IOException ignored) {
            }
        }

        AssetManager assets = context.getAssets();
        copyAssetDir(assets, "", context.getFilesDir());

        FileOutputStream markerOut = new FileOutputStream(marker);
        try {
            markerOut.write(String.valueOf(version).getBytes("UTF-8"));
        } finally {
            markerOut.close();
        }
        return true;
    }

    private static void copyAssetDir(AssetManager assets, String path, File dest)
        throws IOException {
        String[] entries = assets.list(path);
        if (entries == null || entries.length == 0) {
            return;
        }
        for (String entry : entries) {
            String assetPath = path.isEmpty() ? entry : path + "/" + entry;
            String[] children = assets.list(assetPath);
            File target = new File(dest, assetPath);
            if (children != null && children.length > 0) {
                target.mkdirs();
                copyAssetDir(assets, assetPath, dest);
            } else {
                // An empty dir lists like a file; open() then throws.
                try {
                    copyStream(assets.open(assetPath), new FileOutputStream(target));
                } catch (FileNotFoundException e) {
                    target.mkdirs();
                }
            }
        }
    }

    /** Copies a user-picked document tree into the kingpin dir. */
    public static void importTree(Context context, Uri treeUri, Progress progress)
        throws IOException {
        File dest = kingpinDir(context);

        // If the picked folder IS "main" (paks at top level), land in main/;
        // if it looks like a kingpin root (has main/), land at root level.
        String treeId = DocumentsContract.getTreeDocumentId(treeUri);
        List<DocEntry> top = listChildren(context, treeUri, treeId);
        boolean hasMainDir = false;
        boolean hasPakAtTop = false;
        for (DocEntry e : top) {
            if (e.isDir && e.name.equalsIgnoreCase("main")) {
                hasMainDir = true;
            } else if (!e.isDir && e.name.toLowerCase().endsWith(".pak")) {
                hasPakAtTop = true;
            }
        }

        File base = dest;
        if (!hasMainDir && hasPakAtTop) {
            base = new File(dest, "main");
        }
        base.mkdirs();

        long total = 0;
        for (DocEntry e : top) {
            total += treeSize(context, treeUri, e);
        }
        long[] done = new long[] { 0 };
        for (DocEntry e : top) {
            copyTreeEntry(context, treeUri, e, base, progress, total, done);
        }
    }

    /** Copies individually picked documents (usually *.pak) into kingpin/main/. */
    public static void importDocuments(Context context, List<Uri> uris, Progress progress)
        throws IOException {
        File main = new File(kingpinDir(context), "main");
        main.mkdirs();

        long total = 0;
        for (Uri uri : uris) {
            total += querySize(context, uri);
        }
        long done = 0;
        for (Uri uri : uris) {
            String name = queryName(context, uri);
            if (name == null || name.isEmpty()) {
                name = "file.pak";
            }
            done = copyOne(context, uri, new File(main, name), progress, total, done);
        }
    }

    // ---- SAF tree plumbing (raw DocumentsContract, no AndroidX) ----

    private static final class DocEntry {
        final String docId;
        final String name;
        final boolean isDir;
        final long size;

        DocEntry(String docId, String name, boolean isDir, long size) {
            this.docId = docId;
            this.name = name;
            this.isDir = isDir;
            this.size = size;
        }
    }

    private static List<DocEntry> listChildren(Context context, Uri treeUri, String parentId) {
        List<DocEntry> out = new ArrayList<DocEntry>();
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId);
        Cursor c = null;
        try {
            c = context.getContentResolver().query(childrenUri,
                new String[] {
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                    DocumentsContract.Document.COLUMN_SIZE
                },
                null, null, null);
            if (c == null) {
                return out;
            }
            while (c.moveToNext()) {
                String id = c.getString(0);
                String name = c.getString(1);
                String mime = c.getString(2);
                long size = c.isNull(3) ? 0 : c.getLong(3);
                if (id != null && name != null && mime != null) {
                    out.add(new DocEntry(id, name,
                        DocumentsContract.Document.MIME_TYPE_DIR.equals(mime), size));
                }
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return out;
    }

    private static long treeSize(Context context, Uri treeUri, DocEntry entry) {
        if (!entry.isDir) {
            return entry.size;
        }
        long total = 0;
        for (DocEntry child : listChildren(context, treeUri, entry.docId)) {
            total += treeSize(context, treeUri, child);
        }
        return total;
    }

    private static void copyTreeEntry(Context context, Uri treeUri, DocEntry entry,
        File destDir, Progress progress, long total, long[] done) throws IOException {
        if (entry.isDir) {
            File sub = new File(destDir, entry.name);
            sub.mkdirs();
            for (DocEntry child : listChildren(context, treeUri, entry.docId)) {
                copyTreeEntry(context, treeUri, child, sub, progress, total, done);
            }
            return;
        }
        Uri docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, entry.docId);
        done[0] = copyOne(context, docUri, new File(destDir, entry.name),
            progress, total, done[0]);
    }

    private static long copyOne(Context context, Uri src, File dest,
        Progress progress, long total, long done) throws IOException {
        progress.onProgress(done, total, dest.getName());
        File tmp = new File(dest.getParentFile(), dest.getName() + ".part");
        InputStream in = null;
        try {
            in = context.getContentResolver().openInputStream(src);
            if (in == null) {
                throw new IOException("Cannot open " + dest.getName());
            }
            OutputStream out = new FileOutputStream(tmp);
            try {
                byte[] buf = new byte[256 * 1024];
                int n;
                while ((n = in.read(buf)) >= 0) {
                    if (n > 0) {
                        out.write(buf, 0, n);
                        done += n;
                        progress.onProgress(done, total, dest.getName());
                    }
                }
            } finally {
                out.close();
            }
        } finally {
            if (in != null) {
                in.close();
            }
        }
        if (!tmp.renameTo(dest)) {
            tmp.delete();
            throw new IOException("Cannot write " + dest.getName());
        }
        return done;
    }

    private static String queryName(Context context, Uri uri) {
        Cursor c = null;
        try {
            c = context.getContentResolver().query(uri,
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null);
            if (c != null && c.moveToFirst()) {
                return c.getString(0);
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return null;
    }

    private static long querySize(Context context, Uri uri) {
        Cursor c = null;
        try {
            c = context.getContentResolver().query(uri,
                new String[] { OpenableColumns.SIZE }, null, null, null);
            if (c != null && c.moveToFirst() && !c.isNull(0)) {
                return c.getLong(0);
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return 0;
    }

    private static void copyStream(InputStream in, OutputStream out) throws IOException {
        try {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) >= 0) {
                if (n > 0) {
                    out.write(buf, 0, n);
                }
            }
        } finally {
            try {
                in.close();
            } finally {
                out.close();
            }
        }
    }
}
