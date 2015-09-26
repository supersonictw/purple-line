#include <sstream>
#include <limits>

#include <debug.h>

#include <thrift/transport/TTransportException.h>

#include "thrift_line/TalkService.h"

#include "constants.hpp"
#include "linehttptransport.hpp"

LineHttpTransport::LineHttpTransport(
        PurpleAccount *acct,
        PurpleConnection *conn,
        std::string host,
        uint16_t port,
        bool ls_mode) :
    acct(acct),
    conn(conn),
    host(host),
    port(port),
    ls_mode(ls_mode),
    state(ConnectionState::DISCONNECTED),
    auto_reconnect(false),
    reconnect_timeout_handle(0),
    reconnect_timeout(0),
    ssl(NULL),
    input_handle(0),
    connection_id(0),
    request_written(0),
    keep_alive(false),
    status_code_(0),
    content_length_(0)
{
}

LineHttpTransport::~LineHttpTransport() {
    close();
}

void LineHttpTransport::set_auto_reconnect(bool auto_reconnect) {
    if (state == ConnectionState::DISCONNECTED)
        this->auto_reconnect = auto_reconnect;
}

int LineHttpTransport::status_code() {
    return status_code_;
}

int LineHttpTransport::content_length() {
    return content_length_;
}

void LineHttpTransport::open() {
    if (state != ConnectionState::DISCONNECTED)
        return;

    state = ConnectionState::CONNECTED;

    in_progress = false;

    connection_id++;
    ssl = purple_ssl_connect(
        acct,
        host.c_str(),
        port,
        WRAPPER(LineHttpTransport::ssl_connect),
        WRAPPER_TYPE(LineHttpTransport::ssl_error, end),
        (gpointer)this);
}

void LineHttpTransport::ssl_connect(PurpleSslConnection *, PurpleInputCondition) {
    reconnect_timeout = 0;

    send_next();
}

void LineHttpTransport::ssl_error(PurpleSslConnection *, PurpleSslErrorType err) {
    purple_debug_warning("line", "SSL error: %s\n", purple_ssl_strerror(err));

    ssl = nullptr;

    purple_connection_ssl_error(conn, err);
}

void LineHttpTransport::close() {
    if (state == ConnectionState::DISCONNECTED)
        return;

    state = ConnectionState::DISCONNECTED;

    if (reconnect_timeout_handle) {
        purple_timeout_remove(reconnect_timeout_handle);
        reconnect_timeout_handle = 0;
    }

    if (input_handle) {
        purple_input_remove(input_handle);
        input_handle = 0;
    }

    purple_ssl_close(ssl);
    ssl = NULL;
    connection_id++;

    x_ls = "";

    request_buf.str("");

    response_str = "";
    response_buf.str("");
}

uint32_t LineHttpTransport::read_virt(uint8_t *buf, uint32_t len) {
    return (uint32_t )response_buf.sgetn((char *)buf, len);
}

void LineHttpTransport::write_virt(const uint8_t *buf, uint32_t len) {
    request_buf.sputn((const char *)buf, len);
}

void LineHttpTransport::request(std::string method, std::string path, std::string content_type,
    std::function<void()> callback)
{
    Request req;
    req.method = method;
    req.path = path;
    req.content_type = content_type;
    req.body = request_buf.str();
    req.callback = callback;
    request_queue.push(req);

    request_buf.str("");

    send_next();
}

void LineHttpTransport::send_next() {
    if (state != ConnectionState::CONNECTED) {
        open();
        return;
    }

    if (in_progress || request_queue.empty())
        return;

    keep_alive = ls_mode;
    status_code_ = -1;
    content_length_ = -1;

    Request &next_req = request_queue.front();

    std::ostringstream data;

    data
        << next_req.method << " " << next_req.path << " HTTP/1.1" "\r\n";

    if (ls_mode && x_ls != "") {
        data << "X-LS: " << x_ls << "\r\n";
    } else {
        data
            << "Connection: Keep-Alive\r\n"
            << "Content-Type: " << next_req.content_type << "\r\n"
            << "Host: " << host << ":" << port << "\r\n"
            << "User-Agent: " LINE_USER_AGENT "\r\n"
            << "X-Line-Application: " LINE_APPLICATION "\r\n";

        const char *auth_token = purple_account_get_string(acct, LINE_ACCOUNT_AUTH_TOKEN, "");
        if (auth_token)
            data << "X-Line-Access: " << auth_token << "\r\n";
    }

    if (next_req.method == "POST")
        data << "Content-Length: " << next_req.body.size() << "\r\n";

    data
        << "\r\n"
        << next_req.body;

    request_data = data.str();
    request_written = 0;
    in_progress = true;

    input_handle = purple_input_add(ssl->fd, PURPLE_INPUT_WRITE,
        WRAPPER(LineHttpTransport::ssl_write), (gpointer)this);
    ssl_write(ssl->fd, PURPLE_INPUT_WRITE);
}

int LineHttpTransport::reconnect_timeout_cb() {
    reconnect_timeout = reconnect_timeout ? 10 : 60;

    state = ConnectionState::DISCONNECTED;

    open();

    return FALSE;
}

void LineHttpTransport::write_request() {
    if (request_written < request_data.size()) {
        size_t r = purple_ssl_write(ssl,
            request_data.c_str() + request_written, request_data.size() - request_written);

        request_written += r;

        purple_debug_info("line", "Wrote: %d, %d out of %d!\n",
            (int)r, (int)request_written, (int)request_data.size());
    }
}

void LineHttpTransport::ssl_write(gint, PurpleInputCondition) {
    if (state != ConnectionState::CONNECTED) {
        if (input_handle) {
            purple_input_remove(input_handle);
            input_handle = 0;
        }

        return;
    }

    if (request_written < request_data.size()) {
        request_written += purple_ssl_write(ssl,
            request_data.c_str() + request_written, request_data.size() - request_written);
    }

    if (request_written >= request_data.size()) {
        purple_input_remove(input_handle);

        input_handle = purple_input_add(ssl->fd, PURPLE_INPUT_READ,
            WRAPPER(LineHttpTransport::ssl_read), (gpointer)this);
    }
}

void LineHttpTransport::ssl_read(gint, PurpleInputCondition) {
    if (state != ConnectionState::CONNECTED) {
        purple_input_remove(input_handle);
        input_handle = 0;
        return;
    }

    bool any = false;

    while (true) {
        size_t count = purple_ssl_read(ssl, (void *)buf, BUFFER_SIZE);

        if (count == 0) {
            if (any)
                break;

            purple_debug_info("line", "Connection lost.\n");

            close();

            if (in_progress) {
                if (auto_reconnect) {
                    purple_debug_info("line", "Reconnecting in %ds...\n",
                        reconnect_timeout);

                    state = ConnectionState::RECONNECTING;

                    purple_timeout_add_seconds(
                        reconnect_timeout,
                        WRAPPER(LineHttpTransport::reconnect_timeout_cb),
                        (gpointer)this);
                } else {
                    purple_connection_error(conn, "LINE: Lost connection to server.");
                }
            }

            return;
        }

        if (count == (size_t)-1)
            break;

        any = true;

        response_str.append((const char *)buf, count);

        if (content_length_ < 0)
            try_parse_response_header();

        if (content_length_ >= 0 && response_str.size() >= (size_t)content_length_) {
            purple_input_remove(input_handle);
            input_handle = 0;

            if (status_code_ == 403) {
                // Don't try to reconnect because this usually means the user has logged in from
                // elsewhere.

                // TODO: Check actual reason

                conn->wants_to_die = TRUE;
                purple_connection_error(conn, "Session died.");
                return;
            }

            response_buf.str(response_str.substr(0, content_length_));
            response_str.erase(0, content_length_);

            int connection_id_before = connection_id;

            try {
                request_queue.front().callback();
            } catch (line::TalkException &err) {
                std::string msg = "LINE: TalkException: ";
                msg += err.reason;

                if (err.code == line::ErrorCode::NOT_AUTHORIZED_DEVICE) {
                    purple_account_remove_setting(acct, LINE_ACCOUNT_AUTH_TOKEN);

                    if (err.reason == "AUTHENTICATION_DIVESTED_BY_OTHER_DEVICE") {
                        msg = "LINE: You have been logged out because "
                            "you logged in from another device.";
                    } else if (err.reason == "REVOKE") {
                        msg = "LINE: This device was logged out via the mobile app.";
                    }

                    // Don't try to reconnect so we don't fight over the session with another client

                    conn->wants_to_die = TRUE;
                }

                purple_connection_error(conn, msg.c_str());
                return;
            } catch (apache::thrift::TApplicationException &err) {
                std::string msg = "LINE: Application error: ";
                msg += err.what();

                purple_connection_error(conn, msg.c_str());
                return;
            } catch (apache::thrift::transport::TTransportException &err) {
                std::string msg = "LINE: Transport error: ";
                msg += err.what();

                purple_connection_error(conn, msg.c_str());
                return;
            }

            request_queue.pop();

            in_progress = false;

            if (connection_id != connection_id_before)
                break; // Callback closed connection, don't try to continue reading

            if (!keep_alive) {
                close();
                send_next();
                break;
            }

            send_next();
        }
    }
}

void LineHttpTransport::try_parse_response_header() {
    size_t header_end = response_str.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return;

    if (content_length_ == -1)
        content_length_ = 0;

    std::istringstream stream(response_str.substr(0, header_end));

    stream.ignore(256, ' ');
    stream >> status_code_;
    stream.ignore(256, '\n');

    while (stream) {
        std::string name, value;

        std::getline(stream, name, ':');
        stream.ignore(256, ' ');

        if (name == "Content-Length")
            stream >> content_length_;

        if (name == "X-LS")
            std::getline(stream, x_ls, '\r');

        if (name == "Connection") {
            std::string value;
            std::getline(stream, value, '\r');

            if (value == "Keep-Alive" || value == "Keep-alive" || value == "keep-alive")
                keep_alive = true;
        }

        stream.ignore(256, '\n');
    }

    response_str.erase(0, header_end + 4);
}
