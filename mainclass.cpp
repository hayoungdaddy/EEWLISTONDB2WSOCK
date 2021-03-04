#include "mainclass.h"

QSqlDatabase qscdDB;
//QSqlQueryModel *eewinfoModel;
//QSqlQueryModel *eewinfoModel2;


MainClass::MainClass(QString configFileName, QObject *parent) : QObject(parent)
{
    cfg = readCFG(configFileName);

    writeLog(cfg.logDir, cfg.processName, "======================================================");
    writeLog(cfg.logDir, cfg.processName, "EEWLISTONDB2WSOCK Started");

    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("EEWLISTONDB2WSOCK"), QWebSocketServer::NonSecureMode,  this);

    if(m_pWebSocketServer->listen(QHostAddress::Any, cfg.websocketPort))
    {
        writeLog(cfg.logDir, cfg.processName, "Listening on port : " + QString::number(cfg.websocketPort));

        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &MainClass::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed,
                this, &QCoreApplication::quit);
    }

    qscdDB = QSqlDatabase::addDatabase("QMYSQL");
    qscdDB.setHostName(cfg.db_ip);
    qscdDB.setDatabaseName(cfg.db_name);
    qscdDB.setUserName(cfg.db_user);
    qscdDB.setPassword(cfg.db_passwd);
}

void MainClass::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::disconnected, this, &MainClass::socketDisconnected);
    m_clients << pSocket;

    ProcessEEWThread *prThread = new ProcessEEWThread(pSocket, &cfg);
    if(!prThread->isRunning())
    {
        prThread->start();
        connect(pSocket, &QWebSocket::disconnected, prThread, &ProcessEEWThread::quit);
        connect(pSocket, &QWebSocket::textMessageReceived, prThread, &ProcessEEWThread::recvTextMessage);
        connect(prThread, &ProcessEEWThread::finished, prThread, &ProcessEEWThread::deleteLater);
    }
}

void MainClass::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());

    if(pClient){
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}



ProcessEEWThread::ProcessEEWThread(QWebSocket *socket, _CONFIGURE *con, QWidget *parent)
{
    pSocket = socket;
    cfg = *con;

    eewinfoModel = new QSqlQueryModel();
    eewinfoModel2 = new QSqlQueryModel();
}

ProcessEEWThread::~ProcessEEWThread()
{
}

void ProcessEEWThread::recvTextMessage(QString message)
{
    if(message.startsWith("Hello"))
        return;

    _BINARY_LARGE_EEWLIST_PACKET mypacket;

    int mode = message.section("_", 0, 0).toInt();
    int nEEWInfos = message.section("_", 1, 1).toInt();
    int nDays = message.section("_", 2, 2).toInt();
    QDateTime sDate = QDateTime::fromString("yyyy-MM-dd", message.section("_", 3, 3));
    QDateTime eDate = QDateTime::fromString("yyyy-MM-dd", message.section("_", 4, 4));

    float sMag = message.section("_", 5, 5).toFloat();
    float eMag = message.section("_", 6, 6).toFloat();

    QString query;
    openDB();

    if(mode == 0)
        query = "SELECT * from EEWINFO WHERE nudmessagetype='NEW' ORDER BY eew_evid DESC LIMIT " + QString::number(nEEWInfos);
    else if(mode == 1)
    {
        QDateTime today = QDateTime::currentDateTimeUtc();
        today = convertKST(today);
        QDateTime ed = today.addDays( - nDays );
        query = "SELECT * FROM EEWINFO WHERE nudmessagetype='NEW' & lddate >= " + today.toString("yyyy-MM-dd") +
                " AND lddate <= " + ed.toString("yyyy-MM-dd") + " ORDER BY eew_evid DESC";
    }
    else if(mode == 2)
    {
        query = "SELECT * FROM EEWINFO WHERE nudmessagetype='NEW' & lddate >= " + sDate.toString("yyyy-MM-dd") +
                " AND lddate <= " + eDate.toString("yyyy-MM-dd") + " ORDER BY eew_evid DESC";

    }

    this->eewinfoModel->setQuery(query);

    QList<_EEWINFO> eewLists;

    for(int i=0;i<this->eewinfoModel->rowCount();i++)
    {
        _EEWINFO eewInfo;
        eewInfo.eew_evid = this->eewinfoModel->record(i).value("eew_evid").toInt();

        if(sMag == 0 && eMag == 0)
            query = "SELECT * FROM EEWINFO WHERE eew_evid = " + this->eewinfoModel->record(i).value("eew_evid").toString();
        else
        {
            query = "SELECT * FROM EEWINFO WHERE magnitude > " + QString::number(sMag, 'f', 1) + " AND magnitude <= " + QString::number(eMag, 'f', 1) +
                    " AND eew_evid = " + this->eewinfoModel->record(i).value("evid").toString();
        }

        this->eewinfoModel2->setQuery(query);

        if(this->eewinfoModel2->rowCount() > 0)
        {
            eewInfo.magnitude = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("magnitude").toDouble();
            eewInfo.latitude = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("latitude").toDouble();
            eewInfo.longitude = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("longitude").toDouble();
            eewInfo.origintime = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("origin_time").toInt();
            eewInfo.depth = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("depth").toDouble();
            eewInfo.lmapX = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("lmapx").toInt();
            eewInfo.lmapY = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("lmapy").toInt();
            eewInfo.smapX = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("smapx").toInt();
            eewInfo.smapY = this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("smapy").toInt();
            strcpy(eewInfo.location, this->eewinfoModel2->record(this->eewinfoModel2->rowCount()-1).value("location").toString().toUtf8().constData());
        }

        eewLists.append(eewInfo);
    }

    qscdDB.close();

    mypacket.numEEW = eewLists.size();

    if(eewLists.size() > MAX_LARGE_NUM_EEW) mypacket.numEEW = MAX_LARGE_NUM_EEW;

    for(int i=0;i<eewLists.size();i++)
    {
        if(i == MAX_LARGE_NUM_EEW)
            break;
        mypacket.eewInfos[i] = eewLists.at(i);
    }

    sendBinaryMessage(mypacket);
}

void ProcessEEWThread::openDB()
{
    if(!qscdDB.isOpen())
    {
        qscdDB.open();
    }
}

void ProcessEEWThread::sendBinaryMessage(_BINARY_LARGE_EEWLIST_PACKET mypacket)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.writeRawData((char*)&mypacket, sizeof(_BINARY_LARGE_EEWLIST_PACKET));

    pSocket->sendBinaryMessage(data);
}


