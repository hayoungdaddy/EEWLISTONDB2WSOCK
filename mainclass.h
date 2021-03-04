#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QSqlError>

#include <QtWebSockets/QtWebSockets>

#include "KGEEWLIBS_global.h"
#include <proj_api.h>

#define EEWLISTONDB2WSOCK_VERSION 0.1

class MainClass : public QObject
{
    Q_OBJECT
public:
    explicit MainClass(QString conFile = nullptr, QObject *parent = nullptr);

private:
    _CONFIGURE cfg;

    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;

private slots:
    void onNewConnection();
    void socketDisconnected();
};


class ProcessEEWThread : public QThread
{
    Q_OBJECT
public:
    ProcessEEWThread(QWebSocket *websocket = nullptr, _CONFIGURE *con = nullptr, QWidget *parent = nullptr);
    ~ProcessEEWThread();

public slots:
    void recvTextMessage(QString);

private:
    QWebSocket *pSocket;
    _CONFIGURE cfg;

    QSqlQueryModel *eewinfoModel;
    QSqlQueryModel *eewinfoModel2;

    void openDB();
    void sendBinaryMessage(_BINARY_LARGE_EEWLIST_PACKET);
};

#endif // MAINCLASS_H
