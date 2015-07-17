#include <sstream>
#include <string.h>

#include <debug.h>

#include "constants.hpp"
#include "httpclient.hpp"

HTTPClient::HTTPClient(PurpleAccount *acct) :
    acct(acct),
    in_flight(0)
{
}

HTTPClient::~HTTPClient() {
    for (Request *r: request_queue) {
        if (r->handle)
            purple_util_fetch_url_cancel(r->handle);
    }
}

void HTTPClient::request(std::string url, HTTPClient::CompleteFunc callback) {
    request(url, HTTPFlag::NONE, callback);
}

void HTTPClient::request(std::string url, HTTPFlag flags, HTTPClient::CompleteFunc callback) {
    request(url, flags, "", "", callback);
}

void HTTPClient::request(std::string url, HTTPFlag flags,
    std::string content_type, std::string body,
    HTTPClient::CompleteFunc callback)
{
    Request *req = new Request();
    req->client = this;
    req->url = url;
    req->content_type = content_type;
    req->body = body;
    req->flags = flags;
    req->callback = callback;
    req->handle = nullptr;

    request_queue.push_back(req);

    execute_next();
}

void HTTPClient::execute_next() {
    while (in_flight < MAX_IN_FLIGHT && request_queue.size() > 0) {
        Request *req = request_queue.front();
        request_queue.pop_front();

        std::stringstream ss;

        char *host, *path;
        int port;

        purple_url_parse(req->url.c_str(), &host, &port, &path, nullptr, nullptr);

        ss
            << (req->body.size() ? "POST" : "GET") << " /" << path << " HTTP/1.1" "\r\n"
            << "Connection: close\r\n"
            << "Host: " << host << ":" << port << "\r\n"
            << "User-Agent: " << LINE_USER_AGENT << "\r\n";

        free(host);
        free(path);

        if (req->flags & HTTPFlag::AUTH) {
            ss
                << "X-Line-Application: " << LINE_APPLICATION << "\r\n"
                << "X-Line-Access: "
                    << purple_account_get_string(acct, LINE_ACCOUNT_AUTH_TOKEN, "") << "\r\n";
        }

        if (req->content_type.size())
            ss << "Content-Type: " << req->content_type << "\r\n";

        if (req->body.size())
            ss << "Content-Length: " << req->body.size() << "\r\n";

        ss
            << "\r\n"
            << req->body;

        in_flight++;

        req->handle = purple_util_fetch_url_request_len_with_account(
            acct,
            req->url.c_str(),
            TRUE,
            LINE_USER_AGENT,
            TRUE,
            ss.str().c_str(),
            TRUE,
            (req->flags & HTTPFlag::LARGE) ? (100 * 1024 * 1024) : -1,
            purple_cb,
            (gpointer)req);
    }
}

void HTTPClient::complete(HTTPClient::Request *req,
    const gchar *url_text, gsize len, const gchar *error_message)
{
    if (!url_text || error_message) {
        purple_debug_error("util", "HTTP error: %s\n", error_message);
        req->callback(-1, nullptr, 0);
    } else {
        int status = 0;
        const guchar *body = nullptr;
        gsize body_len = 0;

        // libpurple guarantees that responses are null terminated even if they're binary, so
        // string functions are safe to use.

        const char *status_end = strstr(url_text, "\r\n"),
            *header_end = strstr(url_text, "\r\n\r\n");

        if (status_end && header_end) {
            std::stringstream ss(std::string(url_text, status_end - url_text));
            ss.ignore(255, ' ');

            ss >> status;

            body = (const guchar *)(header_end + 4);
            body_len = len - (header_end - url_text + 4);
        }

        req->callback(status, body, body_len);
    }

    request_queue.remove(req);
    delete req;

    in_flight--;

    execute_next();
}

void HTTPClient::purple_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
    const gchar *url_text, gsize len, const gchar *error_message)
{
    Request *req = (Request *)user_data;

    req->client->complete(req, url_text, len, error_message);
}
