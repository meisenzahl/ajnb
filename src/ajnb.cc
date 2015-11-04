/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <alljoyn/BusAttachment.h>
#include <alljoyn/Init.h>
#include <alljoyn/Session.h>
#include <alljoyn/SessionListener.h>

#include <alljoyn/notification/NotificationService.h>
#include <alljoyn/notification/NotificationReceiver.h>
#include <alljoyn/services_common/AsyncTaskQueue.h>

#include <signal.h>
#include <stdio.h>

#include <sys/stat.h>
#include <curl/curl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnotify/notify.h>

using namespace ajn;
using namespace ajn::services;

static volatile sig_atomic_t s_interrupt = false;

static void CDECL_CALL SigIntHandler(int sig) {
    QCC_UNUSED(sig);
    s_interrupt = true;
}

BusAttachment* bus = NULL;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

class MySessionListener : public SessionListener {
    void SessionLost(SessionId sessionId, SessionLostReason reason) {
        printf("SessionLost sessionId = %u, Reason = %d\n", sessionId, reason);
    }
};

class MyNotificationReceiver : public NotificationReceiver {
    void Receive(Notification const& notification) {
        qcc::String deviceName = qcc::String(notification.getDeviceName());
        qcc::String content = qcc::String(notification.getText()[0].getText());
        qcc::String iconURL = qcc::String(notification.getRichIconUrl());
        size_t index = iconURL.find_last_of("/") + 1;
        qcc::String iconName = iconURL.substr(index);
        qcc::String filename = "/tmp/" + iconName;

        printf("%s: %s %s\n", deviceName.c_str(), content.c_str(), iconURL.c_str());

        struct stat buffer;
        if (!stat(filename.c_str(), &buffer) == 0) {
            CURL *curl;
            FILE *fp;
            curl = curl_easy_init();
            if (curl) {
                fp = fopen(filename.c_str(), "wb");
                curl_easy_setopt(curl, CURLOPT_URL, iconURL.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_perform(curl);

                curl_easy_cleanup(curl);
                fclose(fp);
            }
        }

        notify_init("AllJoyn Notification Bridge");
        NotifyNotification* n = notify_notification_new(deviceName.c_str(), content.c_str(), NULL);

        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(filename.c_str(), NULL);
        if (pixbuf) {
            notify_notification_set_image_from_pixbuf(n, pixbuf);
        }

        notify_notification_show(n, NULL);
        g_object_unref(G_OBJECT(n));
        notify_uninit();
    }

    void Dismiss(const int32_t msgId, const qcc::String appId) {}
};

int CDECL_CALL main(int argc, char** argv)
{
    QCC_UNUSED(argc);
    QCC_UNUSED(argv);

    if (AllJoynInit() != ER_OK) {
        return 1;
    }
#ifdef ROUTER
    if (AllJoynRouterInit() != ER_OK) {
        AllJoynShutdown();
        return 1;
    }
#endif

    /* Install SIGINT handler so Ctrl + C deallocates memory properly */
    signal(SIGINT, SigIntHandler);

    QStatus status;

    bus = new BusAttachment("AllJoyn Notification Bridge", true);

    status = bus->Start();
    if (ER_OK == status) {
        printf("BusAttachment started.\n");
    } else {
        printf("FAILED to start BusAttachment (%s)\n", QCC_StatusText(status));
        exit(1);
    }

    status = bus->Connect();
    if (ER_OK == status) {
        printf("BusAttachment connect succeeded.\n");
    } else {
        printf("FAILED to connect to router node (%s)\n", QCC_StatusText(status));
        exit(1);
    }

    NotificationService* notificationService = NotificationService::getInstance();
    MyNotificationReceiver* myNotifictionReceiver = new MyNotificationReceiver();
    notificationService->initReceive(bus, myNotifictionReceiver);

    /* Perform the service asynchronously until the user signals for an exit. */
    if (ER_OK == status) {
        while (s_interrupt == false) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100 * 1000);
#endif
        }
    }

    delete bus;
#ifdef ROUTER
    AllJoynRouterShutdown();
#endif
    AllJoynShutdown();
    return 0;
}
