#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    GtkWidget *url_entry;
    GtkWidget *dir_entry;
    GtkWidget *status_label;
    GtkWidget *download_btn;
    GtkWidget *progress;
    GtkWidget *mp3_check;
    GtkWidget *window;
    GtkWidget *speed_label;
    GSubprocess *process;
    GInputStream *stdout_stream;
} AppWidgets;

static gboolean read_progress(GObject *stream, GAsyncResult *result, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    GError *error = NULL;
    char buffer[1024];
    
    gssize bytes_read = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        // Parse progress from yt-dlp output
        char *percent_str = strstr(buffer, "%");
        if (percent_str) {
            // Find the number before %
            char *start = percent_str - 1;
            while (start > buffer && (*start == ' ' || (*start >= '0' && *start <= '9') || *start == '.')) {
                start--;
            }
            start++;
            
            double progress = atof(start);
            if (progress > 0 && progress <= 100) {
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->progress), progress / 100.0);
                
                char status[256];
                snprintf(status, sizeof(status), "⏳ Downloading... %.1f%%", progress);
                gtk_label_set_text(GTK_LABEL(widgets->status_label), status);
            }
        }
        
        // Parse download speed (look for patterns like "1.5MiB/s" or "500KiB/s")
        char *speed_str = NULL;
        if ((speed_str = strstr(buffer, "MiB/s")) != NULL || 
            (speed_str = strstr(buffer, "KiB/s")) != NULL ||
            (speed_str = strstr(buffer, "GiB/s")) != NULL) {
            
            // Find the start of the speed number
            char *speed_start = speed_str - 1;
            while (speed_start > buffer && (*speed_start == ' ' || 
                   (*speed_start >= '0' && *speed_start <= '9') || *speed_start == '.')) {
                speed_start--;
            }
            speed_start++;
            
            // Extract speed string
            char speed_display[64];
            int len = speed_str - speed_start + 5; // Include "XiB/s"
            if (len < sizeof(speed_display)) {
                strncpy(speed_display, speed_start, len);
                speed_display[len] = '\0';
                
                char speed_label[128];
                snprintf(speed_label, sizeof(speed_label), "⚡ Speed: %s", speed_display);
                gtk_label_set_text(GTK_LABEL(widgets->speed_label), speed_label);
            }
        }
        
        // Continue reading
        g_input_stream_read_async(G_INPUT_STREAM(stream), buffer, sizeof(buffer) - 1,
                                  G_PRIORITY_DEFAULT, NULL,
                                  (GAsyncReadyCallback)read_progress, widgets);
        return G_SOURCE_CONTINUE;
    }
    
    if (error) {
        g_error_free(error);
    }
    
    return G_SOURCE_REMOVE;
}

static void download_finished(GSubprocess *process, GAsyncResult *result, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    GError *error = NULL;
    
    gboolean success = g_subprocess_wait_finish(process, result, &error);
    
    gtk_widget_set_sensitive(widgets->download_btn, TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->progress), 1.0);
    gtk_label_set_text(GTK_LABEL(widgets->speed_label), "");
    
    if (success && g_subprocess_get_exit_status(process) == 0) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "✅ Download completed successfully!");
    } else {
        int exit_code = g_subprocess_get_exit_status(process);
        char error_msg[512];
        
        if (exit_code == 1) {
            snprintf(error_msg, sizeof(error_msg), 
                "❌ Download failed! This site may require login/premium or the video is unavailable.");
        } else {
            snprintf(error_msg, sizeof(error_msg), 
                "❌ Download failed! (Exit code: %d) Check if URL is valid or site is supported.", exit_code);
        }
        
        gtk_label_set_text(GTK_LABEL(widgets->status_label), error_msg);
    }
    
    if (error) {
        g_error_free(error);
    }
    
    g_object_unref(process);
}

static void on_folder_selected(GObject *source, GAsyncResult *result, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    GFile *file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), result, NULL);
    
    if (file) {
        char *path = g_file_get_path(file);
        gtk_editable_set_text(GTK_EDITABLE(widgets->dir_entry), path);
        g_free(path);
        g_object_unref(file);
    }
}

static void on_browse_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Download Directory");
    
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(widgets->window), NULL, on_folder_selected, widgets);
}

static void on_download_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    
    const char *url = gtk_editable_get_text(GTK_EDITABLE(widgets->url_entry));
    const char *dir = gtk_editable_get_text(GTK_EDITABLE(widgets->dir_entry));
    gboolean mp3_mode = gtk_check_button_get_active(GTK_CHECK_BUTTON(widgets->mp3_check));
    
    if (strlen(url) == 0) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "❌ Please enter a URL");
        return;
    }
    
    gtk_widget_set_sensitive(widgets->download_btn, FALSE);
    gtk_widget_set_visible(widgets->progress, TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->progress), 0.0);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "⏳ Starting download...");
    
    // Build output path
    char output_template[1024];
    if (mp3_mode) {
        snprintf(output_template, sizeof(output_template), "%s/%%(title)s.mp3", dir);
    } else {
        snprintf(output_template, sizeof(output_template), "%s/%%(title)s.%%(ext)s", dir);
    }
    
    // Launch yt-dlp
    GError *error = NULL;
    GSubprocess *process;
    
    if (mp3_mode) {
        // MP3 mode: extract audio only
        process = g_subprocess_new(
            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE,
            &error,
            "yt-dlp",
            "--newline",
            "--no-playlist",
            "-x",
            "--audio-format", "mp3",
            "--audio-quality", "0",
            "-o", output_template,
            url,
            NULL
        );
    } else {
        // Video mode: best quality
        process = g_subprocess_new(
            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE,
            &error,
            "yt-dlp",
            "--newline",
            "--no-playlist",
            "-f", "bestvideo+bestaudio/best",
            "--merge-output-format", "mp4",
            "-o", output_template,
            url,
            NULL
        );
    }
    
    if (error) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "❌ Failed to start yt-dlp");
        gtk_widget_set_sensitive(widgets->download_btn, TRUE);
        gtk_widget_set_visible(widgets->progress, FALSE);
        g_error_free(error);
        return;
    }
    
    // Start reading progress
    widgets->stdout_stream = g_subprocess_get_stdout_pipe(process);
    char buffer[1024];
    g_input_stream_read_async(widgets->stdout_stream, buffer, sizeof(buffer) - 1,
                              G_PRIORITY_DEFAULT, NULL,
                              (GAsyncReadyCallback)read_progress, widgets);
    
    g_subprocess_wait_async(process, NULL, (GAsyncReadyCallback)download_finished, widgets);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *title;
    GtkWidget *url_label;
    GtkWidget *dir_label;
    GtkWidget *download_btn;
    
    AppWidgets *widgets = g_new(AppWidgets, 1);
    
    // Create window
    window = gtk_application_window_new(app);
    widgets->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "DY - Download Anything");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 450);
    
    // Create main container
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_window_set_child(GTK_WINDOW(window), vbox);
    
    // Title
    title = gtk_label_new("DY - Download Anything");
    gtk_widget_add_css_class(title, "title-1");
    gtk_box_append(GTK_BOX(vbox), title);
    
    // URL Entry
    url_label = gtk_label_new("Video URL:");
    gtk_widget_set_halign(url_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), url_label);
    
    widgets->url_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->url_entry), 
        "Paste video URL or webpage URL (YouTube, Twitter, Instagram, TikTok, Reddit, etc.)");
    gtk_box_append(GTK_BOX(vbox), widgets->url_entry);
    
    // Helper text
    GtkWidget *helper = gtk_label_new("💡 Tip: You can paste direct video URLs or post/page URLs - DY will find the video!");
    gtk_widget_set_halign(helper, GTK_ALIGN_START);
    gtk_widget_add_css_class(helper, "dim-label");
    gtk_label_set_wrap(GTK_LABEL(helper), TRUE);
    gtk_box_append(GTK_BOX(vbox), helper);
    
    // Directory Entry
    dir_label = gtk_label_new("Download Directory:");
    gtk_widget_set_halign(dir_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), dir_label);
    
    GtkWidget *dir_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    widgets->dir_entry = gtk_entry_new();
    char default_dir[256];
    snprintf(default_dir, sizeof(default_dir), "%s/Downloads", g_get_home_dir());
    gtk_editable_set_text(GTK_EDITABLE(widgets->dir_entry), default_dir);
    gtk_widget_set_hexpand(widgets->dir_entry, TRUE);
    gtk_box_append(GTK_BOX(dir_hbox), widgets->dir_entry);
    
    GtkWidget *browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(browse_btn, "clicked", G_CALLBACK(on_browse_clicked), widgets);
    gtk_box_append(GTK_BOX(dir_hbox), browse_btn);
    gtk_box_append(GTK_BOX(vbox), dir_hbox);
    
    // MP3 Checkbox
    widgets->mp3_check = gtk_check_button_new_with_label("🎵 Download as MP3 (audio only)");
    gtk_box_append(GTK_BOX(vbox), widgets->mp3_check);
    
    // Progress Bar
    widgets->progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(widgets->progress), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(widgets->progress), "Downloading...");
    gtk_widget_set_visible(widgets->progress, FALSE);
    gtk_box_append(GTK_BOX(vbox), widgets->progress);
    
    // Speed Label
    widgets->speed_label = gtk_label_new("");
    gtk_widget_set_halign(widgets->speed_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(widgets->speed_label, "dim-label");
    gtk_box_append(GTK_BOX(vbox), widgets->speed_label);
    
    // Status Label
    widgets->status_label = gtk_label_new("");
    gtk_widget_set_halign(widgets->status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), widgets->status_label);
    
    // Download Button
    widgets->download_btn = gtk_button_new_with_label("Download");
    gtk_widget_add_css_class(widgets->download_btn, "suggested-action");
    g_signal_connect(widgets->download_btn, "clicked", G_CALLBACK(on_download_clicked), widgets);
    gtk_box_append(GTK_BOX(vbox), widgets->download_btn);
    
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    
    app = gtk_application_new("com.dy.Downloader", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
