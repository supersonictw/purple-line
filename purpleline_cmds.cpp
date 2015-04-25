#include "purpleline.hpp"

void PurpleLine::register_commands() {
    purple_cmd_register(
        "sticker",
        "w",
        PURPLE_CMD_P_PRPL,
        (PurpleCmdFlag)(PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT),
        LINE_PRPL_ID,
        WRAPPER(PurpleLine::cmd_sticker),
        "Sends a sticker. The argument should be of the format VER/PKGID/ID.",
        nullptr);

    purple_cmd_register(
        "history",
        "w",
        PURPLE_CMD_P_PRPL,
        (PurpleCmdFlag)
            (PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS),
        LINE_PRPL_ID,
        WRAPPER(PurpleLine::cmd_history),
        "Shows more chat history. Optional argument specifies number of messages to show.",
        nullptr);

    purple_cmd_register(
        "open",
        "w",
        PURPLE_CMD_P_PRPL,
        (PurpleCmdFlag)(PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT),
        LINE_PRPL_ID,
        WRAPPER(PurpleLine::cmd_open),
        "Opens an attachment (image, audio) by number.",
        nullptr);
}

PurpleCmdRet PurpleLine::cmd_sticker(PurpleConversation *conv,
    const gchar *, gchar **args, gchar **error, void *)
{
    static const char *sticker_fields[] = { "STKVER", "STKPKGID", "STKID" };

    line::Message msg;

    std::stringstream ss(args[0]);
    std::string item;

    int part = 0;
    while (std::getline(ss, item, '/')) {
        if (part == 3) {
            *error = g_strdup("Invalid sticker.");
            return PURPLE_CMD_RET_FAILED;
        }

        msg.contentMetadata[sticker_fields[part]] = item;

        part++;
    }

    if (part != 3) {
        *error = g_strdup("Invalid sticker.");
        return PURPLE_CMD_RET_FAILED;
    }

    msg.contentType = line::ContentType::STICKER;
    msg.from = profile.mid;
    msg.to = purple_conversation_get_name(conv);

    write_message(msg, false);

    send_message(msg);

    return PURPLE_CMD_RET_OK;
}

PurpleCmdRet PurpleLine::cmd_history(PurpleConversation *conv,
    const gchar *, gchar **args, gchar **error, void *)
{
    int count = 10;

    if (args[0]) {
        try {
            count = std::stoi(args[0]);
        } catch (...) {
            *error = g_strdup("Invalid message count.");
            return PURPLE_CMD_RET_FAILED;
        }
    }

    fetch_conversation_history(conv, count, true);

    return PURPLE_CMD_RET_OK;
}

PurpleCmdRet PurpleLine::cmd_open(PurpleConversation *conv,
    const gchar *, gchar **args, gchar **error, void *)
{
    static std::map<line::ContentType::type, std::string> attachment_extensions = {
        { line::ContentType::IMAGE, ".jpg" },
        { line::ContentType::VIDEO, ".mp4" },
        { line::ContentType::AUDIO, ".mp3" },
    };

    std::string token(args[0]);

    Attachment *att = conv_attachment_get(conv, token);
    if (!att) {
        *error = g_strdup("No such attachment.");
        return PURPLE_CMD_RET_FAILED;
    }

    if (att->path != "" && g_file_test(att->path.c_str(), G_FILE_TEST_EXISTS)) {
        purple_notify_uri(conn, att->path.c_str());
        return PURPLE_CMD_RET_OK;
    }

    // Ensure there's nothing funny about the id as we're going to use it as a path element
    try {
        std::stoll(att->id);
    } catch (...) {
        *error = g_strdup("Failed to download attachment.");
        return PURPLE_CMD_RET_FAILED;
    }

    std::string ext = ".jpg";
    if (attachment_extensions.count(att->type))
        ext = attachment_extensions[att->type];

    std::string dir = get_tmp_dir(true);

    gchar *path_p = g_build_filename(
        dir.c_str(),
        (att->id + ext).c_str(),
        nullptr);

    std::string path(path_p);

    g_free(path_p);

    purple_conversation_write(
        conv,
        "",
        "Downloading attachment...",
        (PurpleMessageFlags)PURPLE_MESSAGE_SYSTEM,
        time(NULL));

    std::string url = std::string(LINE_OS_URL) + "os/m/"+ att->id;

    PurpleConversationType ctype = purple_conversation_get_type(conv);
    std::string cname = std::string(purple_conversation_get_name(conv));

    http.request(url, HTTPFlag::AUTH | HTTPFlag::LARGE,
        [this, path, token, ctype, cname]
        (int status, const guchar *data, gsize len)
        {
            if (status == 200 && data && len > 0) {
                g_file_set_contents(path.c_str(), (const char *)data, len, nullptr);

                temp_files.push_back(path);

                PurpleConversation *conv = purple_find_conversation_with_account(
                    ctype, cname.c_str(), acct);

                if (conv) {
                    Attachment *att = conv_attachment_get(conv, token);
                    if (att)
                        att->path = path;
                }

                purple_notify_uri(conn, path.c_str());
            } else {
                notify_error("Failed to download attachment.");
            }
        });

    return PURPLE_CMD_RET_OK;
}
