#include "purpleline.hpp"

static std::string get_sticker_id(line::Message &msg) {
    std::map<std::string, std::string> &meta = msg.contentMetadata;

    if (meta.count("STKID") == 0 || meta.count("STKVER") == 0 || meta.count("STKPKGID") == 0)
        return "";

    std::stringstream id;

    id << "[LINE sticker "
        << meta["STKVER"] << "/"
        << meta["STKPKGID"] << "/"
        << meta["STKID"];

    if (meta.count("STKTXT") == 1)
        id << " " << meta["STKTXT"];

    id << "]";

    return id.str();
}

static std::string get_sticker_url(line::Message &msg, bool thumb = false) {
    std::map<std::string, std::string> &meta = msg.contentMetadata;

    int ver;
    std::stringstream ss(meta["STKVER"]);
    ss >> ver;

    std::stringstream url;

    url << LINE_STICKER_URL
        << (ver / 1000000) << "/" << (ver / 1000) << "/" << (ver % 1000) << "/"
        << meta["STKPKGID"] << "/"
        << "PC/stickers/"
        << meta["STKID"];

    if (thumb)
        url << "_key";

    url << ".png";

    return url.str();
}

void PurpleLine::write_message(line::Message &msg, bool replay) {
    std::string text;
    int flags = 0;
    time_t mtime = (time_t)(msg.createdTime / 1000);

    bool sent = (msg.from_ == profile.mid);

    if (std::find(recent_messages.cbegin(), recent_messages.cend(), msg.id)
        != recent_messages.cend())
    {
        // We already processed this message. User is probably talking with himself.
        return;
    }

    // Hack
    if (msg.from_ == msg.to)
        push_recent_message(msg.id);

    PurpleConversation *conv = purple_find_conversation_with_account(
        (msg.toType == line::MIDType::USER ? PURPLE_CONV_TYPE_IM : PURPLE_CONV_TYPE_CHAT),
        ((!sent && msg.toType == line::MIDType::USER) ? msg.from_.c_str() : msg.to.c_str()),
        acct);

    // If this is a new received IM, create the conversation if it doesn't exist
    if (!conv && !sent && msg.toType == line::MIDType::USER)
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, acct, msg.from_.c_str());

    // If this is a new conversation, we're not replaying history and history hasn't been fetched
    // yet, queue the message instead of showing it.
    if (conv && !replay) {
        auto *queue = (std::vector<line::Message> *)
            purple_conversation_get_data(conv, "line-message-queue");

        if (queue) {
            queue->push_back(msg);
            return;
        }
    }

    // Replaying messages from history
    // Unfortunately Pidgin displays messages with this flag with odd formatting and no username.
    // Disable for now.
    //if (replay)
    //    flags |= PURPLE_MESSAGE_NO_LOG;

    switch (msg.contentType) {
        case line::ContentType::NONE: // actually text
        case line::ContentType::LOCATION:
            if (msg.__isset.location) {
                line::Location &loc = msg.location;

                text = markup_escape(loc.title)
                    + " | <a href=\"https://maps.google.com/?q=" + url_encode(loc.address)
                    + "&ll=" + std::to_string(loc.latitude)
                    + "," + std::to_string(loc.longitude)
                    + "\">"
                    + (loc.address.size()
                        ? markup_escape(loc.address)
                        : "(no address)")
                    + "</a>";
            } else {
                text = markup_escape(msg.text);
            }
            break;

        case line::ContentType::STICKER:
            {
                std::string id = get_sticker_id(msg);

                if (id == "")  {
                    text = "<em>[Broken sticker]</em>";

                    purple_debug_warning("line", "Got a broken sticker.\n");
                } else {
                    text = id;

                    if (conv
                        && purple_conv_custom_smiley_add(conv, id.c_str(), "id", id.c_str(), TRUE))
                    {
                        http.request(get_sticker_url(msg),
                            [this, id, conv](int status, const guchar *data, gsize len)
                            {
                                if (status == 200 && data && len > 0) {
                                    purple_conv_custom_smiley_write(
                                        conv,
                                        id.c_str(),
                                        data,
                                        len);
                                } else {
                                    purple_debug_warning(
                                        "line",
                                        "Couldn't download sticker. Status: %d\n",
                                        status);
                                }

                                purple_conv_custom_smiley_close(conv, id.c_str());
                            });
                    }
                }
            }
            break;

        case line::ContentType::IMAGE:
        case line::ContentType::VIDEO: // Videos could really benefit from streaming...
            {
                std::string type_std = line::_ContentType_VALUES_TO_NAMES.at(msg.contentType);

                std::string id = "[LINE " + type_std + " " + msg.id + "]";

                text = id;

                if (conv) {
                    text += " <font color=\"#888888\">/open "
                        + conv_attachment_add(conv, msg.contentType, msg.id)
                        + "</font>";
                }

                if (!conv
                    || !purple_conv_custom_smiley_add(conv, id.c_str(), "id", id.c_str(), TRUE))
                {
                    break;
                }

                if (msg.contentPreview.size() > 0) {
                    purple_conv_custom_smiley_write(
                        conv,
                        id.c_str(),
                        (const guchar *)msg.contentPreview.c_str(),
                        msg.contentPreview.size());

                    purple_conv_custom_smiley_close(conv, id.c_str());
                } else {
                    std::string preview_url = msg.contentMetadata.count("PREVIEW_URL")
                        ? msg.contentMetadata["PREVIEW_URL"]
                        : std::string(LINE_OS_URL) + "os/m/" + msg.id + "/preview";

                    http.request(preview_url, HTTPFlag::AUTH | HTTPFlag::LARGE,
                        [this, id, conv](int status, const guchar *data, gsize len)
                        {
                            if (status == 200 && data && len > 0) {
                                purple_conv_custom_smiley_write(
                                    conv,
                                    id.c_str(),
                                    data,
                                    len);
                            } else {
                                purple_debug_warning(
                                    "line",
                                    "Couldn't download image message. Status: %d\n",
                                    status);
                            }

                            purple_conv_custom_smiley_close(conv, id.c_str());
                        });
                }
            }
            break;

        case line::ContentType::AUDIO:
            {
                text = "[Audio message";

                if (msg.contentMetadata.count("AUDLEN")) {
                    int len = 0;

                    try {
                        len = std::stoi(msg.contentMetadata["AUDLEN"]);
                    } catch(...) { /* ignore */ }

                    if (len > 0) {
                        text += " "
                            + std::to_string(len / 1000)
                            + "."
                            + std::to_string((len % 1000) / 100)
                            + "s";
                    }
                }

                text += "]";

                if (conv) {
                    text += " <font color=\"#888888\">/open "
                        + conv_attachment_add(conv, msg.contentType, msg.id)
                        + "</font>";
                }
            }
            break;

        // TODO: other content types

        default:
            text = "<em>[Unimplemented message type: ";
            text += line::_ContentType_VALUES_TO_NAMES.count(msg.contentType)
                ? line::_ContentType_VALUES_TO_NAMES.at(msg.contentType)
                : "TYPE " + std::to_string(msg.contentType);
            text += "]</em>";

            break;
    }

    if (sent) {
        // Messages sent by user (sync from other devices)

        write_message(conv, msg.from_, text, mtime, flags | PURPLE_MESSAGE_SEND);
    } else {
        // Messages received from other users

        flags |= PURPLE_MESSAGE_RECV;

        if (replay) {
            // Write replayed messages instead of serv_got_* to avoid Pidgin's IM sound

            write_message(conv, msg.from_, text, mtime, flags);
        } else {
            if (msg.toType == line::MIDType::USER) {
                serv_got_im(
                    conn,
                    msg.from_.c_str(),
                    text.c_str(),
                    (PurpleMessageFlags)flags,
                    mtime);
            } else if (msg.toType == line::MIDType::GROUP || msg.toType == line::MIDType::ROOM) {
                serv_got_chat_in(
                    conn,
                    purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)),
                    msg.from_.c_str(),
                    (PurpleMessageFlags)flags,
                    text.c_str(),
                    mtime);
            }
        }
    }
}

void PurpleLine::write_message(PurpleConversation *conv, std::string &from, std::string &text,
    time_t mtime, int flags)
{
    if (!conv)
        return;

    PurpleConversationType type = purple_conversation_get_type(conv);

    if (type == PURPLE_CONV_TYPE_IM) {
        purple_conv_im_write(
            PURPLE_CONV_IM(conv),
            from.c_str(),
            text.c_str(),
            (PurpleMessageFlags)flags,
            mtime);
    } else if (type == PURPLE_CONV_TYPE_CHAT) {
        purple_conv_chat_write(
            PURPLE_CONV_CHAT(conv),
            from.c_str(),
            text.c_str(),
            (PurpleMessageFlags)flags,
            mtime);
    } else {
        purple_debug_warning("line", "write_message for weird conversation type: %d\n", type);
    }
}
