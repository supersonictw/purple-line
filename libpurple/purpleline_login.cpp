#include "purpleline.hpp"

#include <sstream>
#include <iomanip>

#include <core.h>

#include <gcrypt.h>

static std::string hex_to_bytes(std::string hex) {
    if (hex.size() % 2 != 0)
        hex = std::string("0") + hex;

    std::string result(hex.size() / 2, '\0');
    for (size_t i = 0; i < result.size(); i++)
        result[i] = std::stoi(hex.substr(i * 2, 2), nullptr, 16);

    return result;
}

static std::string bytes_to_hex(std::string bytes) {
    std::ostringstream ss;

    for (size_t i = 0; i < bytes.size(); i++)
        ss << std::hex << std::setfill('0') << std::setw(2) << (unsigned)(bytes[i] & 0xff);

    return ss.str();
}

void PurpleLine::login_start() {
    purple_connection_set_state(conn, PURPLE_CONNECTING);
    purple_connection_update_progress(conn, "Logging in", 0, 3);

    // The hacks in bitlbee apparently can make this return NULL instead of "" as it should.
    // This is a counter-hack.
    const char *c_auth_token = purple_account_get_string(acct, LINE_ACCOUNT_AUTH_TOKEN, "");
    std::string auth_token = (c_auth_token ? c_auth_token : "");

    if (auth_token != "") {
        // There's a stored authentication token, see if it works

        c_out->send_getLastOpRevision();
        c_out->send([this, auth_token]() {
            int64_t local_rev;

            try {
                local_rev = c_out->recv_getLastOpRevision();
            } catch (line::TalkException &err) {
                if (err.code == line::ErrorCode::AUTHENTICATION_FAILED
                    || err.code == line::ErrorCode::NOT_AUTHORIZED_DEVICE)
                {
                    // Auth token expired, get a new one

                    purple_debug_info("line", "Existing auth token expired.\n");

                    purple_account_remove_setting(acct, LINE_ACCOUNT_AUTH_TOKEN);

                    get_auth_token();
                    return;
                }

                // Unknown error
                throw;
            }

            set_auth_token(auth_token);

            poller.set_local_rev(local_rev);

            // Already got the last op revision, no need to call get_last_op_revision()

            get_profile();
        });
    } else {
        // Get a new auth token

        get_auth_token();
    }
}

void PurpleLine::get_auth_token() {
    purple_debug_info("line", "Logging in with credentials to get new auth token.\n");

    c_out->close();
    c_out->set_path(LINE_LOGIN_PATH);

    c_out->send_getRSAKeyInfo(line::IdentityProvider::LINE);
    c_out->send([this]() {
        line::RSAKey key;
        std::string credentials;

        try {
            c_out->recv_getRSAKeyInfo(key);
        } catch (line::TalkException &err) {
            purple_debug_info(
                "line", "Login error in getRSAKeyInfo: %d %s\n",
                err.code, err.reason.c_str());

            std::string msg = "Could not log in. " + err.reason;

            conn->wants_to_die = TRUE;
            purple_connection_error(conn, msg.c_str());

            return;
        }

        try {
            credentials = get_encrypted_credentials(key);
        } catch (std::exception &ex) {
            purple_debug_info("line", "Login error in get_encrypted_credentials: %s\n", ex.what());

            std::string msg = std::string("Could not log in. ") + ex.what();

            conn->wants_to_die = TRUE;
            purple_connection_error(conn, msg.c_str());

            return;
        }

        std::string certificate(purple_account_get_string(acct, LINE_ACCOUNT_CERTIFICATE, ""));

        std::string ui_name = "purple-line";

        GHashTable *ui_info = purple_core_get_ui_info();
        gpointer ui_name_p = g_hash_table_lookup(ui_info, "name");
        if (ui_name_p)
            ui_name = (char *)ui_name_p;

        c_out->set_path(LINE_LOGINZ_PATH);

        line::LoginRequest req;
        req.type = line::LoginType::ID_CREDENTIAL;
        req.identityProvider = line::IdentityProvider::LINE;
        req.identifier = key.keynm;
        req.password = credentials;
        req.keepLoggedIn = true;
        req.accessLocation = "127.0.0.1";
        req.systemName = ui_name;
        req.certificate = certificate;

        c_out->send_loginZ(req);
        c_out->send([this]() {
            line::LoginResult result;

            try {
                c_out->recv_loginZ(result);
            } catch (line::TalkException &err) {
                purple_debug_info(
                    "line", "Login error in loginZ: %d %s\n",
                    err.code, err.reason.c_str());

                std::string msg = "Could not log in. " + err.reason;

                conn->wants_to_die = TRUE;
                purple_connection_error(conn, msg.c_str());

                return;
            }

            c_out->set_path(LINE_COMMAND_PATH);

            if (result.type == line::LoginResultType::SUCCESS && result.authToken != "")
            {
                set_auth_token(result.authToken);

                get_last_op_revision();
            }
            else if (result.type == line::LoginResultType::REQUIRE_DEVICE_CONFIRM)
            {
                purple_debug_info("line", "Starting PIN verification.\n");

                pin_verifier.verify(result, [this](std::string auth_token, std::string certificate) {
                    (void)auth_token;

                    if (certificate != "") {
                        purple_account_set_string(
                            acct,
                            LINE_ACCOUNT_CERTIFICATE,
                            certificate.c_str());
                    }

                    // For some reason the auth token received from the PIN verification doesn't
                    // work. However the "certificate" does, so just get another auth token.
                    get_auth_token();
                });
            }
            else
            {
                std::stringstream ss("Could not log in. Bad LoginResult type: ");
                ss << result.type;
                std::string msg = ss.str();

                purple_connection_error(conn, msg.c_str());
            }
        });
    });
}

// This may throw.
std::string PurpleLine::get_encrypted_credentials(line::RSAKey &key) {
    if (!gcry_check_version(GCRYPT_VERSION)) {
        std::string err = "libgcrypt version mismatch (compiled: " GCRYPT_VERSION " runtime: ";
        err += gcry_check_version(nullptr);
        err += ")";

        throw std::runtime_error(err);
    }

    if (!gcry_control(GCRYCTL_INITIALIZATION_FINISHED_P)) {
        gcry_control(GCRYCTL_DISABLE_SECMEM, 0);

        gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    }

    std::string username = purple_account_get_username(acct);
    std::string password = purple_account_get_password(acct);

    if (username.size() > 255)
        throw std::runtime_error("Username is too long.");

    if (password.size() > 255)
        throw std::runtime_error("Password is too long.");

    std::ostringstream buf;

    buf << (char)key.sessionKey.size() << key.sessionKey;
    buf << (char)username.size() << username;
    buf << (char)password.size() << password;

    std::string modulus = hex_to_bytes(key.nvalue);
    std::string exponent = hex_to_bytes(key.evalue);

    std::string credentials = buf.str();

    gpg_error_t err;

    gcry_sexp_t public_key, plaintext, ciphertext, result;

    err = gcry_sexp_build(
        &public_key, nullptr,
        "(public-key (rsa (n %b) (e %b)))",
        (int)modulus.size(), modulus.c_str(),
        (int)exponent.size(), exponent.c_str());

    if (err)
        throw std::runtime_error(std::string("ligbcrypt public key error: ") + gpg_strerror(err));

    err = gcry_sexp_build(
        &plaintext, nullptr,
        "(data (flags pkcs1) (value %b))",
        (int)credentials.size(), credentials.c_str());

    if (err) {
        gcry_sexp_release(public_key);
        throw std::runtime_error(std::string("ligbcrypt data error: ") + gpg_strerror(err));
    }

    err = gcry_pk_encrypt(&ciphertext, plaintext, public_key);

    gcry_sexp_release(plaintext);
    gcry_sexp_release(public_key);

    if (err)
        throw std::runtime_error(std::string("libgcrypt encryption error: ") + gpg_strerror(err));

    result = gcry_sexp_find_token(ciphertext, "a", 0);

    gcry_sexp_release(ciphertext);

    if (!result)
        throw std::runtime_error("libgcrypt result token not found");

    size_t result_data_len;
    const char *result_data = gcry_sexp_nth_data(result, 1, &result_data_len);

    if (!result_data) {
        gcry_sexp_release(result);
        throw std::runtime_error("libgcrypt result token value not found");
    }

    std::string result_bytes(result_data, result_data_len);

    gcry_sexp_release(result);

    return bytes_to_hex(result_bytes);
}

void PurpleLine::set_auth_token(std::string auth_token) {
    purple_account_set_string(acct, LINE_ACCOUNT_AUTH_TOKEN, auth_token.c_str());

    // Re-open output client to update persistent headers
    c_out->close();

    c_out->set_auto_reconnect(true);
}

void PurpleLine::get_last_op_revision() {
    c_out->send_getLastOpRevision();
    c_out->send([this]() {
        poller.set_local_rev(c_out->recv_getLastOpRevision());

        get_profile();
    });
}

void PurpleLine::get_profile() {
    c_out->send_getProfile();
    c_out->send([this]() {
        c_out->recv_getProfile(profile);

        profile_contact.mid = profile.mid;
        profile_contact.displayName = profile.displayName;

        // Update display name
        purple_account_set_alias(acct, profile.displayName.c_str());

        purple_connection_set_state(conn, PURPLE_CONNECTED);
        purple_connection_update_progress(conn, "Synchronizing buddy list", 1, 3);

        // Update account icon (not sure if there's a way to tell whether it has changed, maybe
        // pictureStatus?)
        if (profile.picturePath != "") {
            std::string pic_path = profile.picturePath.substr(1) + "/preview";
            //if (icon_path != purple_account_get_string(acct, "icon_path", "")) {
                http.request(LINE_OS_URL + pic_path, HTTPFlag::AUTH,
                    [this](int status, const guchar *data, gsize len)
                {
                    if (status != 200 || !data)
                        return;

                    purple_buddy_icons_set_account_icon(
                        acct,
                        (guchar *)g_memdup(data, len),
                        len);

                    //purple_account_set_string(acct, "icon_path", icon_path.c_str());
                });
            //}
        } else {
            // TODO: Delete icon
        }

        get_contacts();
    });
}

void PurpleLine::get_contacts() {
    c_out->send_getAllContactIds();
    c_out->send([this]() {
        std::vector<std::string> uids;
        c_out->recv_getAllContactIds(uids);

        c_out->send_getContacts(uids);
        c_out->send([this]() {
            std::vector<line::Contact> contacts;
            c_out->recv_getContacts(contacts);

            std::set<PurpleBuddy *> buddies_to_delete = blist_find<PurpleBuddy>();

            for (line::Contact &contact: contacts) {
                if (contact.status == line::ContactStatus::FRIEND)
                    buddies_to_delete.erase(blist_update_buddy(contact));
            }

            for (PurpleBuddy *buddy: buddies_to_delete)
                blist_remove_buddy(purple_buddy_get_name(buddy));

            {
                // Add self as buddy for those lonely debugging conversations
                // TODO: Remove

                line::Contact self;
                self.mid = profile.mid;
                self.displayName = profile.displayName + " [Profile]";
                self.statusMessage = profile.statusMessage;
                self.picturePath = profile.picturePath;

                blist_update_buddy(self);
            }

            get_groups();
        });
    });
}

void PurpleLine::get_groups() {
    c_out->send_getGroupIdsJoined();
    c_out->send([this]() {
        std::vector<std::string> gids;
        c_out->recv_getGroupIdsJoined(gids);

        c_out->send_getGroups(gids);
        c_out->send([this]() {
            std::vector<line::Group> groups;
            c_out->recv_getGroups(groups);

            std::set<PurpleChat *> chats_to_delete = blist_find_chats_by_type(ChatType::GROUP);

            for (line::Group &group: groups)
                chats_to_delete.erase(blist_update_chat(group));

            for (PurpleChat *chat: chats_to_delete)
                purple_blist_remove_chat(chat);

            get_rooms();
        });
    });
}

void PurpleLine::get_rooms() {
    c_out->send_getMessageBoxCompactWrapUpList(1, 65535);
    c_out->send([this]() {
        line::MessageBoxWrapUpList wrap_up_list;
        c_out->recv_getMessageBoxCompactWrapUpList(wrap_up_list);

        std::set<std::string> uids;

        for (line::MessageBoxWrapUp &ent: wrap_up_list.messageBoxWrapUpList) {
            if (ent.messageBox.midType != line::MIDType::ROOM)
                continue;

            for (line::Contact &c: ent.contacts)
                uids.insert(c.mid);
        }

        if (!uids.empty()) {
            // Room contacts don't contain full contact information, so pull separately to get names

            c_out->send_getContacts(std::vector<std::string>(uids.begin(), uids.end()));
            c_out->send([this, wrap_up_list]{
                std::vector<line::Contact> contacts;
                c_out->recv_getContacts(contacts);

                for (line::Contact &c: contacts)
                    this->contacts[c.mid] = c;

                update_rooms(wrap_up_list);
            });
        } else {
            update_rooms(wrap_up_list);
        }
    });
}

void PurpleLine::update_rooms(line::MessageBoxWrapUpList wrap_up_list) {
    std::set<PurpleChat *> chats_to_delete = blist_find_chats_by_type(ChatType::ROOM);

    for (line::MessageBoxWrapUp &ent: wrap_up_list.messageBoxWrapUpList) {
        if (ent.messageBox.midType != line::MIDType::ROOM)
            continue;

        line::Room room;
        room.mid = ent.messageBox.id;
        room.contacts = ent.contacts;

        chats_to_delete.erase(blist_update_chat(room));
    }

    for (PurpleChat *chat: chats_to_delete)
        purple_blist_remove_chat(chat);

    get_group_invites();
}

void PurpleLine::get_group_invites() {
    c_out->send_getGroupIdsInvited();
    c_out->send([this]() {
        std::vector<std::string> gids;
        c_out->recv_getGroupIdsInvited(gids);

        if (gids.size() == 0) {
            login_done();
            return;
        }

        c_out->send_getGroups(gids);
        c_out->send([this]() {
            std::vector<line::Group> groups;
            c_out->recv_getGroups(groups);

            for (line::Group &g: groups)
                handle_group_invite(g, profile_contact, no_contact);

            login_done();
        });
    });
}

void PurpleLine::login_done() {
    poller.start();

    purple_connection_update_progress(conn, "Connected", 2, 3);
}
