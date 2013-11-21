#include "purplehttpclient.hpp"

#include <sstream>
#include <limits>

#include <debug.h>

#include <thrift/transport/TTransportException.h>

PurpleHttpClient::PurpleHttpClient(
        PurpleAccount *acct,
        std::string host,
        uint16_t port,
        std::string path) :
    acct(acct),
    host(host),
    port(port),
    path(path),
    auth_token("x"),
    ssl(NULL),
    connection_id(0),
    first_request(true),
    in_progress(false),
    status_code(-1),
    content_length(-1)
{
}

PurpleHttpClient::~PurpleHttpClient() { }

void PurpleHttpClient::set_path(std::string path) {
    this->path = path;
}

void PurpleHttpClient::set_auth_token(std::string token) {
    this->auth_token = token;
}

static void wrap_ssl_connect(gpointer data, PurpleSslConnection *ssl, PurpleInputCondition cond) {
    ((PurpleHttpClient *)data)->ssl_connect();
}

static void wrap_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType err, gpointer data) {
    ((PurpleHttpClient *)data)->ssl_error(err);
}

void PurpleHttpClient::open() {
    if (ssl)
        return;

    purple_debug_info("line", "Connecting...\n");

    first_request = true;

    connection_id++;
    ssl = purple_ssl_connect(
        acct,
        host.c_str(),
        port,
        wrap_ssl_connect,
        wrap_ssl_error,
        (gpointer)this);
}

void PurpleHttpClient::close() {
    if (!ssl)
        return;

    purple_ssl_close(ssl);
    ssl = NULL;
    connection_id++;

    x_ls = "";

    response_str = "";
}

uint32_t PurpleHttpClient::read_virt(uint8_t *buf, uint32_t len) {
    return (uint32_t )response_buf.sgetn((char *)buf, len);
}

void PurpleHttpClient::write_virt(const uint8_t *buf, uint32_t len) {
    request_buf.sputn((const char *)buf, len);
}

void PurpleHttpClient::send(std::function<void(int)> callback) {
    std::string request_str = request_buf.str();

    std::ostringstream data;

    data
        << "POST " << path << " HTTP/1.1" "\r\n"
        << "Content-Length: " << request_str.size() << "\r\n";

    if (x_ls.size() > 0)
        data << "X-LS: " << x_ls << "\r\n";

    if (first_request) {
        data
            << "Connection: Keep-Alive" "\r\n"
            << "Content-Type: application/x-thrift" "\r\n"
            << "Host: " << host << ":" << port << "\r\n"
            << "Accept: application/x-thrift" "\r\n"
            << "User-Agent: purple-line (LINE for libpurple)" "\r\n"
            << "X-Line-Application: DESKTOPWIN\t3.2.1.83\tWINDOWS\t5.1.2600-XP-x64" "\r\n"
            << "X-Line-Access: " << auth_token << "\r\n";

        first_request = false;
    }

    data
        << "\r\n"
        << request_str;

    Request req;
    req.data = data.str();
    req.callback = callback;
    request_queue.push_back(req);

    request_buf.str("");

    send_next();
}

/*const uin8_t* PurpleHttpClient::borrow_virt(uint8_t *buf, uint32_t *len) {
}

void PurpleHttpClient::consume_virt(uint32_t len) {
}*/

void PurpleHttpClient::send_next() {
    if (!ssl) {
        open();
        return;
    }

    if (in_progress || request_queue.empty())
        return;

    status_code = -1;
    content_length = -1;

    Request &next_req = request_queue.front();

    in_progress = true;
    purple_ssl_write(ssl, next_req.data.c_str(), next_req.data.size());
}

static void wrap_ssl_input(gpointer data, PurpleSslConnection *ssl, PurpleInputCondition cond) {
    ((PurpleHttpClient *)data)->ssl_input(cond);
}

void PurpleHttpClient::ssl_connect() {
    purple_debug_info("line", "Connected.\n");

    purple_ssl_input_add(ssl, wrap_ssl_input, (gpointer)this);

    send_next();
}

void PurpleHttpClient::ssl_input(PurpleInputCondition cond) {
    if (cond != PURPLE_INPUT_READ)
        return;

    bool any = false;

    while (true) {
        size_t count = purple_ssl_read(ssl, (void *)buf, BUFFER_SIZE);

        if (count == 0) {
            if (any)
                break;

            purple_debug_warning("line", "Server closed connection.\n");

            for (Request &r: request_queue)
                r.callback(-1);

            request_queue.clear(); // TODO: Maybe add retry logic

            close();
            break;
        }

        if (count == (size_t)-1)
            break;

        any = true;

        response_str.append((const char *)buf, count);

        try_parse_response_header();

        if (content_length >= 0 && response_str.size() >= (size_t)content_length) {
            response_buf.str(response_str.substr(0, content_length));
            response_str.erase(0, content_length);

            int connection_id_before = connection_id;

            request_queue.front().callback(status_code);
            request_queue.pop_front();

            in_progress = false;

            if (connection_id != connection_id_before)
                break; // Callback closed connection, don't try to continue reading

            send_next();
        }
    }
}

void PurpleHttpClient::ssl_error(PurpleSslErrorType err) {
    purple_debug_warning("line", "SSL error: %s\n", purple_ssl_strerror(err));
}

void PurpleHttpClient::try_parse_response_header() {
    size_t header_end = response_str.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return;

    std::istringstream stream(response_str.substr(0, header_end));

    stream.ignore(256, ' ');
    stream >> status_code;
    stream.ignore(256, '\n');

    while (stream) {
        std::string name, value;

        std::getline(stream, name, ':');
        stream.ignore(256, ' ');

        if (name == "Content-Length")
            stream >> content_length;

        if (name == "X-LS")
            std::getline(stream, x_ls, '\r');

        stream.ignore(256, '\n');
    }

    response_str.erase(0, header_end + 4);
}