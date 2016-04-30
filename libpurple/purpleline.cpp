#include <algorithm>
#include <functional>
#include <memory>

#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <cmds.h>
#include <connection.h>
#include <conversation.h>
#include <debug.h>
#include <eventloop.h>
#include <notify.h>
#include <request.h>
#include <sslconn.h>
#include <util.h>

#include "purpleline.hpp"
#include "wrapper.hpp"

std::string markup_escape(std::string const &text) {
    gchar *escaped = purple_markup_escape_text(text.c_str(), text.size());
    std::string result(escaped);
    g_free(escaped);

    return result;
}

std::string markup_unescape(std::string const &markup) {
    gchar *unescaped = purple_unescape_html(markup.c_str());
    std::string result(unescaped);
    g_free(unescaped);

    return result;
}

std::string url_encode(std::string const &str) {
    return purple_url_encode(str.c_str());
}

PurpleLine::PurpleLine(PurpleConnection *conn, PurpleAccount *acct) :
    conn(conn),
    acct(acct),
    http(acct),
    os_http(acct, conn, LINE_OS_SERVER, 443, false),
    poller(*this),
    pin_verifier(*this),
    next_purple_id(1)
{
    c_out = boost::make_shared<ThriftClient>(acct, conn, LINE_LOGIN_PATH);
    os_http.set_auto_reconnect(true);
}

PurpleLine::~PurpleLine() {
    c_out->close();
}

const char *PurpleLine::list_icon(PurpleAccount *, PurpleBuddy *) {
    return "line";
}

GList *PurpleLine::status_types(PurpleAccount *) {
    const char *attr_id = "message", *attr_name = "What's Up?";

    GList *types = nullptr;
    PurpleStatusType *t;

    // Friends
    t = purple_status_type_new_with_attrs(
        PURPLE_STATUS_AVAILABLE, nullptr, nullptr,
        TRUE, TRUE, FALSE,
        attr_id, attr_name, purple_value_new(PURPLE_TYPE_STRING),
        nullptr);
    types = g_list_append(types, t);

    // Temporary friends
    t = purple_status_type_new_with_attrs(
        PURPLE_STATUS_UNAVAILABLE, "temporary", "Temporary",
        TRUE, TRUE, FALSE,
        attr_id, attr_name, purple_value_new(PURPLE_TYPE_STRING),
        nullptr);
    types = g_list_append(types, t);

    // Not actually used because LINE doesn't report online status, but this is apparently required.
    t = purple_status_type_new_with_attrs(
        PURPLE_STATUS_OFFLINE, nullptr, nullptr,
        TRUE, TRUE, FALSE,
        attr_id, attr_name, purple_value_new(PURPLE_TYPE_STRING),
        nullptr);
    types = g_list_append(types, t);

    return types;
}

std::string PurpleLine::get_tmp_dir(bool create) {
    std::string name = "line-" + profile.mid;

    for (char c: name) {
        if (!(c == '-' || std::isalpha(c) || std::isdigit(c))) {
            name = "line";
            break;
        }
    }

    gchar *dir_p = g_build_filename(
        g_get_tmp_dir(),
        name.c_str(),
        nullptr);

    if (create)
        g_mkdir_with_parents(dir_p, 0700);

    std::string dir(dir_p);

    g_free(dir_p);

    return dir;
}

char *PurpleLine::status_text(PurpleBuddy *buddy) {
    PurplePresence *presence = purple_buddy_get_presence(buddy);
    PurpleStatus *status = purple_presence_get_active_status(presence);

    const char *msg = purple_status_get_attr_string(status, "message");
    if (msg && msg[0])
        return g_markup_escape_text(msg, -1);

    return nullptr;
}

void PurpleLine::tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full) {
    purple_notify_user_info_add_pair_plaintext(user_info,
        "Name", purple_buddy_get_alias(buddy));

    //purple_notify_user_info_add_pair_plaintext(user_info,
    //    "User ID", purple_buddy_get_name(buddy));

    if (purple_blist_node_get_bool(PURPLE_BLIST_NODE(buddy), "official_account"))
        purple_notify_user_info_add_pair_plaintext(user_info, "Official account", "Yes");

    if (PURPLE_BLIST_NODE_HAS_FLAG(buddy, PURPLE_BLIST_NODE_FLAG_NO_SAVE)) {
        // This breaks?
        //purple_notify_user_info_add_section_break(user_info);

        purple_notify_user_info_add_pair_plaintext(user_info,
            "Temporary",
            "You are currently in a conversation with this contact, "
                "but they are not on your friend list.");
    }
}

void PurpleLine::login(PurpleAccount *acct) {
    PurpleConnection *conn = purple_account_get_connection(acct);

    PurpleLine *plugin = new PurpleLine(conn, acct);
    conn->proto_data = (void *)plugin;

    plugin->connect_signals();

    plugin->login_start();
}

void PurpleLine::close() {
    disconnect_signals();

    if (temp_files.size()) {
        for (std::string &path: temp_files)
            g_unlink(path.c_str());

        g_rmdir(get_tmp_dir().c_str());
    }

    delete this;
}

void PurpleLine::connect_signals() {
    purple_signal_connect(
        purple_blist_get_handle(),
        "blist-node-removed",
        (void *)this,
        PURPLE_CALLBACK(WRAPPER_TYPE(PurpleLine::signal_blist_node_removed, signal)),
        (void *)this);

    purple_signal_connect(
        purple_conversations_get_handle(),
        "conversation-created",
        (void *)this,
        PURPLE_CALLBACK(WRAPPER_TYPE(PurpleLine::signal_conversation_created, signal)),
        (void *)this);

    purple_signal_connect(
        purple_conversations_get_handle(),
        "deleting-conversation",
        (void *)this,
        PURPLE_CALLBACK(WRAPPER_TYPE(PurpleLine::signal_deleting_conversation, signal)),
        (void *)this);
}

void PurpleLine::disconnect_signals() {
    purple_signal_disconnect(
        purple_blist_get_handle(),
        "blist-node-removed",
        (void *)this,
        PURPLE_CALLBACK(WRAPPER_TYPE(PurpleLine::signal_blist_node_removed, signal)));

    purple_signal_disconnect(
        purple_conversations_get_handle(),
        "conversation-created",
        (void *)this,
        PURPLE_CALLBACK(WRAPPER_TYPE(PurpleLine::signal_conversation_created, signal)));


    purple_signal_disconnect(
        purple_conversations_get_handle(),
        "deleting-conversation",
        (void *)this,
        PURPLE_CALLBACK(WRAPPER_TYPE(PurpleLine::signal_deleting_conversation, signal)));
}

std::string PurpleLine::conv_attachment_add(PurpleConversation *conv,
    line::ContentType::type type, std::string id)
{
    auto atts = (std::vector<Attachment> *)purple_conversation_get_data(conv, "line-attachments");
    if (!atts) {
        atts = new std::vector<Attachment>();
        purple_conversation_set_data(conv, "line-attachments", atts);
    }

    atts->emplace_back(type, id);

    return std::to_string(atts->size());
}

PurpleLine::Attachment *PurpleLine::conv_attachment_get(PurpleConversation *conv, std::string token)
{
    int index;

    try {
        index = std::stoi(token);
    } catch (...) {
        return nullptr;
    }

    auto atts = (std::vector<Attachment> *)purple_conversation_get_data(conv, "line-attachments");

    return (atts && index <= (int)atts->size()) ? &(*atts)[index - 1] : nullptr;
}

line::Contact &PurpleLine::get_up_to_date_contact(line::Contact &c) {
    return (contacts.count(c.mid) != 0) ? contacts[c.mid] : c;
}

int PurpleLine::send_message(std::string to, const char *markup) {
    // Parse markup and send message as parts if it contains images

    bool any_sent = false;

    for (const char *p = markup; p && *p; ) {
        const char *start, *end;
        GData *attributes;

        bool img_found = purple_markup_find_tag("IMG", p, &start, &end, &attributes);

        std::string text;

        if (img_found) {
            // Image found but there's text before it, store it

            text = std::string(p, start - p);
            p = end + 1;
        } else {
            // No image found, store the rest of the text

            text = std::string(p);

            // Break the loop

            p = NULL;
        }

        // If the text is not all whitespace, send it as a text message
        if (text.find_first_not_of("\t\n\r ") != std::string::npos)
        {
            line::Message msg;

            msg.contentType = line::ContentType::NONE;
            msg.from_ = profile.mid;
            msg.to = to;
            msg.text = markup_unescape(text);

            send_message(msg);

            any_sent = true;
        }

        if (img_found) {
            int image_id = std::stoi((char *)g_datalist_get_data(&attributes, "id"));
            g_datalist_clear(&attributes);

            std::stringstream ss;
            ss << "(img ID: " << image_id << ")";

            PurpleStoredImage *img = purple_imgstore_find_by_id(image_id);
            if (!img) {
                purple_debug_warning("line", "Tried to send non-existent image: %d\n", image_id);
                continue;
            }

            std::string img_data(
                (const char *)purple_imgstore_get_data(img),
                purple_imgstore_get_size(img));

            line::Message msg;

            msg.contentType = line::ContentType::IMAGE;
            msg.from_ = profile.mid;
            msg.to = to;

            send_message(msg, [this, img_data](line::Message &msg_back) {
                upload_media(msg_back.id, "image", img_data);
            });

            any_sent = true;
        }
    }

    return any_sent ? 1 : 0;
}

void PurpleLine::send_message(
    line::Message &msg,
    std::function<void(line::Message &)> callback)
{
    std::string to(msg.to);

    c_out->send_sendMessage(0, msg);
    c_out->send([this, to, callback]() {
        line::Message msg_back;

        try {
            c_out->recv_sendMessage(msg_back);
        } catch (line::TalkException &err) {
            std::string msg = "Failed to send message: " + err.reason;

            PurpleConversation *conv = purple_find_conversation_with_account(
                PURPLE_CONV_TYPE_ANY,
                to.c_str(),
                acct);

            if (conv) {
                if (err.code == line::ErrorCode::E2EE_RETRY_ENCRYPT) {
                    msg = "Failed to send message: Cannot send with Letter Sealing enabled.";
                    write_e2ee_error(conv);
                }

                purple_conversation_write(
                    conv,
                    "",
                    msg.c_str(),
                    (PurpleMessageFlags)PURPLE_MESSAGE_ERROR,
                    time(NULL));
            } else {
                notify_error(msg);
            }

            return;
        }

        // Kludge >_>
        if (to[0] == 'u')
            push_recent_message(msg_back.id);

        if (callback)
            callback(msg_back);
    });
}

void PurpleLine::upload_media(std::string message_id, std::string type, std::string data) {
    std::string boundary;

    do {
        gchar *random_string = purple_uuid_random();
        boundary = random_string;
        g_free(random_string);
    } while (data.find(boundary) != std::string::npos);

    std::stringstream body;

    body
        << "--" << boundary << "\r\n"
        << "Content-Disposition: form-data; name=\"params\"\r\n"
        << "\r\n"
        << "{"
        << "\"name\":\"media\","
        << "\"oid\":\"" << message_id << "\","
        << "\"size\":\"" << data.size() << "\","
        << "\"type\":\"" << type << "\","
        << "\"ver\":\"1.0\""
        << "}"
        << "\r\n--" << boundary << "\r\n"
        << "Content-Disposition: form-data; name=\"file\"; filename=\"media\"\r\n"
        << "Content-Type: image/jpeg\r\n"
        << "\r\n"
        << data
        << "\r\n--" << boundary << "--\r\n";

    std::string content_type = std::string("multipart/form-data; boundary=") + boundary;

    os_http.write_virt((const uint8_t *)body.str().c_str(), body.tellp());

    os_http.request("POST", "/talk/m/upload.nhn", content_type, [this]() {
        if (os_http.status_code() != 201) {
            purple_debug_warning(
                "line",
                "Couldn't upload message media. Status: %d\n",
                os_http.status_code());
        }
    });
}

void PurpleLine::push_recent_message(std::string id) {
    recent_messages.push_back(id);
    if (recent_messages.size() > 50)
        recent_messages.pop_front();
}

int PurpleLine::send_im(const char *who, const char *message, PurpleMessageFlags flags) {
    return send_message(who, message);
}

void PurpleLine::remove_buddy(PurpleBuddy *buddy, PurpleGroup *) {
    c_out->send_updateContactSetting(
        0,
        purple_buddy_get_name(buddy),
        line::ContactSetting::CONTACT_SETTING_DELETE,
        "true");
    c_out->send([this]{
        try {
            c_out->recv_updateContactSetting();
        } catch (line::TalkException &err) {
            notify_error(std::string("Couldn't delete buddy: ") + err.reason);
        }
    });
}

void PurpleLine::signal_blist_node_removed(PurpleBlistNode *node) {
    if (!(PURPLE_BLIST_NODE_IS_CHAT(node)
        && purple_chat_get_account(PURPLE_CHAT(node)) == acct))
    {
        return;
    }

    GHashTable *components = purple_chat_get_components(PURPLE_CHAT(node));

    char *id_ptr = (char *)g_hash_table_lookup(components, "id");
    if (!id_ptr) {
        purple_debug_warning("line", "Tried to remove a chat with no id.\n");
        return;
    }

    std::string id(id_ptr);

    ChatType type = get_chat_type((char *)g_hash_table_lookup(components, "type"));

    if (type == ChatType::ROOM) {
        c_out->send_leaveRoom(0, id);
        c_out->send([this]{
            try {
                c_out->recv_leaveRoom();
            } catch (line::TalkException &err) {
                notify_error(std::string("Couldn't leave from chat: ") + err.reason);
            }
        });
    } else if (type == ChatType::GROUP) {
        c_out->send_leaveGroup(0, id);
        c_out->send([this]{
            try {
                c_out->recv_leaveGroup();
            } catch (line::TalkException &err) {
                notify_error(std::string("Couldn't leave from group: ") + err.reason);
            }
        });
    } else {
        purple_debug_warning("line", "Tried to remove a chat with no type.\n");
        return;
    }
}

void PurpleLine::signal_conversation_created(PurpleConversation *conv) {
    if (purple_conversation_get_account(conv) != acct)
        return;

    // Start queuing messages while the history is fetched
    purple_conversation_set_data(conv, "line-message-queue", new std::vector<line::Message>());

    fetch_conversation_history(conv, 10, false);
}

void PurpleLine::fetch_conversation_history(PurpleConversation *conv, int count, bool requested) {
    PurpleConversationType type = conv->type;
    std::string name(purple_conversation_get_name(conv));

    int64_t end_seq = -1;

    int64_t *end_seq_p = (int64_t *)purple_conversation_get_data(conv, "line-end-seq");
    if (end_seq_p)
        end_seq = *end_seq_p;

    purple_debug_info("line",
        "Fetching history: end_seq=%" G_GINT64_FORMAT " , count=%d, requested=%d\n",
        end_seq, count, requested);

    if (end_seq != -1)
        c_out->send_getPreviousMessages(name, end_seq - 1, count);
    else
        c_out->send_getRecentMessages(name, count);

    c_out->send([this, requested, type, name, end_seq]() {
        int64_t new_end_seq = end_seq;

        std::vector<line::Message> recent_msgs;

        if (end_seq != -1)
            c_out->recv_getPreviousMessages(recent_msgs);
        else
            c_out->recv_getRecentMessages(recent_msgs);

        PurpleConversation *conv = purple_find_conversation_with_account(type, name.c_str(), acct);
        if (!conv)
            return; // Conversation died while fetching messages

        auto *queue = (std::vector<line::Message> *)
            purple_conversation_get_data(conv, "line-message-queue");

        purple_conversation_set_data(conv, "line-message-queue", nullptr);

        // Find least seq value from messages for future history queries
        for (line::Message &msg: recent_msgs) {
            if (msg.contentMetadata.count("seq")) {
                try {
                    int64_t seq = std::stoll(msg.contentMetadata["seq"]);

                    if (new_end_seq == -1 || seq < new_end_seq)
                        new_end_seq = seq;
                } catch (...) { /* ignore parse error */ }
            }
        }

        if (queue) {
            // If there's a message queue, remove any already-queued messages in the recent message
            // list to prevent them showing up twice.

            recent_msgs.erase(
                std::remove_if(
                    recent_msgs.begin(),
                    recent_msgs.end(),
                    [&queue](line::Message &rm) {
                        auto r = find_if(
                            queue->begin(),
                            queue->end(),
                            [&rm](line::Message &qm) { return qm.id == rm.id; });

                        return (r != queue->end());
                    }),
                recent_msgs.end());
        }

        if (recent_msgs.size()) {
            purple_conversation_write(
                conv,
                "",
                "<strong>Message history</strong>",
                (PurpleMessageFlags)PURPLE_MESSAGE_RAW,
                time(NULL));

            for (auto msgi = recent_msgs.rbegin(); msgi != recent_msgs.rend(); msgi++)
                write_message(*msgi, true);

            purple_conversation_write(
                conv,
                "",
                "<hr>",
                (PurpleMessageFlags)PURPLE_MESSAGE_RAW,
                time(NULL));
        } else {
            if (requested) {
                // If history was requested by the user and there is none, let the user know

                purple_conversation_write(
                    conv,
                    "",
                    "<strong>No more history</strong>",
                    (PurpleMessageFlags)PURPLE_MESSAGE_RAW,
                    time(NULL));
            }
        }

        // If there's a message queue, play it back now
        if (queue) {
            for (line::Message &msg: *queue)
                write_message(msg, false);

            delete queue;
        }

        int64_t *end_seq_p = (int64_t *)purple_conversation_get_data(conv, "line-end-seq");
        if (end_seq_p)
            delete end_seq_p;

        purple_conversation_set_data(conv, "line-end-seq", new int64_t(new_end_seq));

        purple_debug_info("line", "History done: new_end_seq=%" G_GINT64_FORMAT "\n", new_end_seq);
    });
}

void PurpleLine::signal_deleting_conversation(PurpleConversation *conv) {
    if (purple_conversation_get_account(conv) != acct)
        return;

    auto queue = (std::vector<line::Message> *)
        purple_conversation_get_data(conv, "line-message-queue");

    if (queue) {
        purple_conversation_set_data(conv, "line-message-queue", nullptr);
        delete queue;
    }

    int64_t *end_seq_p = (int64_t *)purple_conversation_get_data(conv, "line-end-seq");
    if (end_seq_p) {
        purple_conversation_set_data(conv, "line-end-seq", nullptr);
        delete end_seq_p;
    }

    auto atts = (std::vector<Attachment> *)purple_conversation_get_data(conv, "line-attachments");
    if (atts) {
        purple_conversation_set_data(conv, "line-attachments", nullptr);
        delete atts;
    }
}

void PurpleLine::notify_error(std::string msg) {
    purple_notify_error(
        (void *)conn,
        "LINE error",
        msg.c_str(),
        nullptr);
}
