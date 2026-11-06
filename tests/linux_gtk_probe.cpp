// SPDX-License-Identifier: MIT
// GTK scratch editor; Qt Core is used only to serialize observed text as JSON.
#include <gtk/gtk.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

struct State {
    GtkWidget *normal;
    GtkWidget *second;
    GtkWidget *password;
    const char *path;
    GtkWidget *window;
    QString normalPreedit;
};

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    const char *path = argv[1];
    gtk_init(&argc, &argv);
    auto *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "schnellerTyp-e GTK scratch editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 220);
    auto *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), box);
    State state{gtk_entry_new(), gtk_entry_new(), gtk_entry_new(), path, window, {}};
    g_signal_connect(state.normal, "preedit-changed", G_CALLBACK(+[](GtkEntry*, const gchar* text, gpointer data) {
        static_cast<State*>(data)->normalPreedit = QString::fromUtf8(text);
    }), &state);
    gtk_entry_set_visibility(GTK_ENTRY(state.password), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(state.password), GTK_INPUT_PURPOSE_PASSWORD);
    for (auto *field : {state.normal, state.second, state.password})
        gtk_box_pack_start(GTK_BOX(box), field, FALSE, FALSE, 0);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_timeout_add(30, [](gpointer data) -> gboolean {
        auto &s = *static_cast<State *>(data);
        const auto text = [](GtkWidget *widget) {
            return QString::fromUtf8(gtk_entry_get_text(GTK_ENTRY(widget)));
        };
        QJsonObject json{{"normal", text(s.normal)}, {"second", text(s.second)},
                         {"normalPreedit", s.normalPreedit},
                         {"password", text(s.password)},
                         {"focus", gtk_widget_has_focus(s.normal) ? "normal"
                             : gtk_widget_has_focus(s.second) ? "second" : "password"}};
        int x = 0, y = 0;
        gtk_widget_translate_coordinates(s.normal, s.window, 0, 0, &x, &y);
        json.insert(QStringLiteral("normalRect"), QJsonObject{{"x", x}, {"y", y},
                    {"width", gtk_widget_get_allocated_width(s.normal)},
                    {"height", gtk_widget_get_allocated_height(s.normal)}});
        json.insert(QStringLiteral("cursor"), gtk_editable_get_position(GTK_EDITABLE(s.normal)));
        QFile file(QString::fromLocal8Bit(s.path));
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
        return G_SOURCE_CONTINUE;
    }, &state);
    gtk_widget_show_all(window);
    gtk_widget_grab_focus(state.normal);
    gtk_main();
    return 0;
}
