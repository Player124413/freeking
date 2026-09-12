package org.freeking.launcher;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import org.freeking.GameActivity;

/**
 * Entry point of the app: makes sure the original Kingpin game files are
 * present (imported by the user via the system file picker), extracts the
 * engine assets from the APK on first run, then starts the game.
 */
public class LauncherActivity extends Activity {
    private static final int REQ_TREE = 1;
    private static final int REQ_DOCS = 2;

    private TextView statusText;
    private TextView progressLabel;
    private ProgressBar progressBar;
    private Button playButton;
    private Button folderButton;
    private Button filesButton;
    private Button helpButton;

    private boolean busy = false;
    private boolean assetsReady = false;
    private boolean crashShown = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!assetsReady) {
            runEngineAssetExtract();
        } else {
            refreshStatus();
        }
    }

    // ---- UI ----

    private void buildUi() {
        boolean ru = isRussian();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(24);
        root.setPadding(pad, pad, pad, pad);
        root.setGravity(Gravity.CENTER_HORIZONTAL);

        TextView title = new TextView(this);
        title.setText("Freeking");
        title.setTextSize(30);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText(ru
            ? "Kingpin: Life of Crime — порт на Android"
            : "Kingpin: Life of Crime — Android port");
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, dp(4), 0, dp(16));
        root.addView(subtitle);

        statusText = new TextView(this);
        statusText.setGravity(Gravity.CENTER);
        statusText.setPadding(0, 0, 0, dp(16));
        root.addView(statusText);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setVisibility(View.GONE);
        progressBar.setMax(1000);
        LinearLayout.LayoutParams barParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        root.addView(progressBar, barParams);

        progressLabel = new TextView(this);
        progressLabel.setGravity(Gravity.CENTER);
        progressLabel.setVisibility(View.GONE);
        progressLabel.setPadding(0, dp(4), 0, dp(12));
        root.addView(progressLabel);

        playButton = makeButton(root, ru ? "▶  Играть" : "▶  Play");
        folderButton = makeButton(root, ru ? "Выбрать папку с игрой" : "Choose game folder");
        filesButton = makeButton(root, ru ? "Выбрать файлы .pak" : "Choose .pak files");
        helpButton = makeButton(root, ru ? "Где взять файлы игры?" : "Where to get game files?");

        playButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startActivity(new Intent(LauncherActivity.this, GameActivity.class));
            }
        });
        folderButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                startActivityForResult(i, REQ_TREE);
            }
        });
        filesButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.setType("*/*");
                i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                startActivityForResult(Intent.createChooser(i,
                    isRussian() ? "Выберите .pak файлы" : "Choose .pak files"), REQ_DOCS);
            }
        });
        helpButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showHelp();
            }
        });

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);
    }

    private Button makeButton(LinearLayout root, String text) {
        Button b = new Button(this);
        b.setText(text);
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        p.topMargin = dp(6);
        root.addView(b, p);
        return b;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private boolean isRussian() {
        return Locale.getDefault().getLanguage().equals("ru");
    }

    // ---- State ----

    private void refreshStatus() {
        boolean ru = isRussian();
        List<File> paks = FileSetup.findPaks(this);
        if (paks.isEmpty()) {
            statusText.setText(ru
                ? "Файлы игры не найдены.\nСкопируйте папку Kingpin с ПК на телефон и выберите её ниже."
                : "Game files not found.\nCopy your Kingpin folder to the phone, then choose it below.");
            playButton.setEnabled(false);
        } else {
            long mb = FileSetup.totalBytes(paks) / (1024 * 1024);
            StringBuilder names = new StringBuilder();
            for (File pak : paks) {
                if (names.length() > 0) {
                    names.append(", ");
                }
                names.append(pak.getName());
            }
            statusText.setText(ru
                ? "Найдено архивов: " + paks.size() + " (" + mb + " МБ)\n" + names
                : "Archives found: " + paks.size() + " (" + mb + " MB)\n" + names);
            playButton.setEnabled(!busy);
        }
        setImportEnabled(!busy);
        checkCrashLog();
    }

    // If the native engine died on a fatal signal it leaves
    // <internal storage>/last_crash.log — surface it so the user can report it.
    private void checkCrashLog() {
        if (crashShown) {
            return;
        }
        final File crash = new File(getFilesDir(), "last_crash.log");
        if (!crash.exists()) {
            return;
        }
        crashShown = true;
        final boolean ru = isRussian();
        String body;
        try {
            java.io.FileInputStream in = new java.io.FileInputStream(crash);
            try {
                java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
                body = new String(out.toByteArray(), "UTF-8");
            } finally {
                in.close();
            }
        } catch (Exception e) {
            body = "(unreadable: " + e.getMessage() + ")";
        }
        if (body.length() > 4000) {
            body = body.substring(0, 4000) + "\n…";
        }
        final String report = body;

        final TextView view = new TextView(this);
        view.setText(report);
        view.setTextIsSelectable(true);
        view.setPadding(dp(16), dp(8), dp(16), dp(8));
        ScrollView scroll = new ScrollView(this);
        scroll.addView(view);

        new AlertDialog.Builder(this)
            .setTitle(ru ? "Прошлая сессия аварийно завершилась" : "Previous session crashed")
            .setMessage(ru
                ? "Движок записал отчёт. Отправьте его разработчику вместе с названием карты:"
                : "The engine wrote a crash report. Send it to the developer with the map name:")
            .setView(scroll)
            .setPositiveButton(ru ? "Очистить" : "Clear", new android.content.DialogInterface.OnClickListener() {
                @Override
                public void onClick(android.content.DialogInterface d, int w) {
                    crash.delete();
                    d.dismiss();
                }
            })
            .setNegativeButton(ru ? "Закрыть" : "Dismiss", new android.content.DialogInterface.OnClickListener() {
                @Override
                public void onClick(android.content.DialogInterface d, int w) {
                    d.dismiss();
                }
            })
            .setNeutralButton(ru ? "Копировать" : "Copy", new android.content.DialogInterface.OnClickListener() {
                @Override
                public void onClick(android.content.DialogInterface d, int w) {
                    android.content.ClipboardManager cm =
                        (android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
                    cm.setPrimaryClip(android.content.ClipData.newPlainText("crash", report));
                    d.dismiss();
                }
            })
            .show();
    }

    private void setImportEnabled(boolean enabled) {
        folderButton.setEnabled(enabled);
        filesButton.setEnabled(enabled);
    }

    private void setBusy(boolean value, boolean indeterminate) {
        busy = value;
        progressBar.setVisibility(value ? View.VISIBLE : View.GONE);
        progressLabel.setVisibility(value ? View.VISIBLE : View.GONE);
        progressBar.setIndeterminate(indeterminate);
        setImportEnabled(!value);
        playButton.setEnabled(!value && !FileSetup.findPaks(this).isEmpty());
    }

    // ---- Background work ----

    private void runEngineAssetExtract() {
        setBusy(true, true);
        progressLabel.setText(isRussian() ? "Подготовка…" : "Preparing…");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    FileSetup.extractEngineAssets(LauncherActivity.this);
                    assetsReady = true;
                } catch (final Exception e) {
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            showError(e.getMessage());
                        }
                    });
                }
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        setBusy(false, false);
                        refreshStatus();
                    }
                });
            }
        }).start();
    }

    private void runImport(final ImportJob job) {
        setBusy(true, false);
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    job.run(new FileSetup.Progress() {
                        @Override
                        public void onProgress(final long done, final long total,
                            final String name) {
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (total > 0) {
                                        progressBar.setIndeterminate(false);
                                        progressBar.setProgress(
                                            (int) (1000 * done / total));
                                        progressLabel.setText(name + " — "
                                            + (done / (1024 * 1024)) + "/"
                                            + (total / (1024 * 1024)) + " MB");
                                    } else {
                                        progressBar.setIndeterminate(true);
                                        progressLabel.setText(name);
                                    }
                                }
                            });
                        }
                    });
                } catch (final Exception e) {
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            showError(e.getMessage());
                        }
                    });
                }
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        setBusy(false, false);
                        refreshStatus();
                    }
                });
            }
        }).start();
    }

    private interface ImportJob {
        void run(FileSetup.Progress progress) throws Exception;
    }

    // ---- Picker results ----

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null) {
            return;
        }
        if (requestCode == REQ_TREE && data.getData() != null) {
            final Uri tree = data.getData();
            runImport(new ImportJob() {
                @Override
                public void run(FileSetup.Progress progress) throws Exception {
                    FileSetup.importTree(LauncherActivity.this, tree, progress);
                }
            });
        } else if (requestCode == REQ_DOCS) {
            final List<Uri> uris = new ArrayList<Uri>();
            if (data.getData() != null) {
                uris.add(data.getData());
            } else if (data.getClipData() != null) {
                for (int i = 0; i < data.getClipData().getItemCount(); i++) {
                    uris.add(data.getClipData().getItemAt(i).getUri());
                }
            }
            if (!uris.isEmpty()) {
                runImport(new ImportJob() {
                    @Override
                    public void run(FileSetup.Progress progress) throws Exception {
                        FileSetup.importDocuments(LauncherActivity.this, uris, progress);
                    }
                });
            }
        }
    }

    // ---- Dialogs ----

    private void showHelp() {
        boolean ru = isRussian();
        new AlertDialog.Builder(this)
            .setTitle(ru ? "Где взять файлы игры?" : "Where to get game files?")
            .setMessage(ru
                ? "Freeking — это движок, файлы оригинальной игры нужны отдельно:\n\n"
                + "1. Установите Kingpin: Life of Crime на ПК (GOG, Steam или диск).\n"
                + "2. Скопируйте папку игры на телефон (нужна папка main с файлами .pak).\n"
                + "3. Нажмите «Выбрать папку с игрой» и укажите скопированную папку.\n\n"
                + "Либо выберите отдельные .pak файлы кнопкой «Выбрать файлы .pak»."
                : "Freeking is an engine — you need the original game files:\n\n"
                + "1. Install Kingpin: Life of Crime on a PC (GOG, Steam or disc).\n"
                + "2. Copy the game folder to your phone (the main folder with .pak files).\n"
                + "3. Tap “Choose game folder” and pick the copied folder.\n\n"
                + "Or pick individual .pak files with “Choose .pak files”.")
            .setPositiveButton("OK", null)
            .show();
    }

    private void showError(String message) {
        new AlertDialog.Builder(this)
            .setTitle(isRussian() ? "Ошибка" : "Error")
            .setMessage(message != null ? message : "Unknown error")
            .setPositiveButton("OK", null)
            .show();
    }
}
