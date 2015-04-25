#pragma once

#include <string>
#include <deque>

#include <debug.h>
#include <plugin.h>
#include <prpl.h>

class PurpleLine;

class PINVerifier {

    PurpleLine &parent;

    HTTPClient http;

    void *notification;
    guint timeout;

public:

    PINVerifier(PurpleLine &parent);

    void verify(
        line::LoginResult loginResult,
        std::function<void(std::string, std::string)> success);

private:

    int timeout_cb();
    void cancel_cb(int);
    void end();
    void error(std::string error);
};
