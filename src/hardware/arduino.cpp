#include "arduino.h"
#include <QDebug>
#include <QThread>

// =============================================================
//  Constructeur / Destructeur
// =============================================================
Arduino::Arduino(QObject *parent)
    : QObject(parent)
{
    serial = new QSerialPort(this);

    // Connecter le signal readyRead pour la lecture RFID
    connect(serial, &QSerialPort::readyRead,
            this,   &Arduino::onDonneesDisponibles);
}

Arduino::~Arduino()
{
    close_arduino();
}

// =============================================================
//  API bas niveau
// =============================================================
int Arduino::connect_arduino(const QString &portName)
{
    if (serial->isOpen())
        serial->close();

    serial->setPortName(portName);
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite)) {
        qDebug() << "[Arduino] Connexion réussie sur" << portName;
        // Attendre que l'Arduino soit prêt
        QThread::msleep(2000);
        qDebug() << "[Arduino] Arduino prêt";
        return 0;
    }

    qDebug() << "[Arduino] Échec de connexion:" << serial->errorString();
    return 1;
}

int Arduino::close_arduino()
{
    if (serial->isOpen()) {
        serial->close();
        qDebug() << "[Arduino] Port fermé.";
        return 0;
    }
    return 1;
}

QByteArray Arduino::read_from_arduino()
{
    return serial->readAll();
}

void Arduino::write_to_arduino(const QByteArray &data)
{
    if (serial->isOpen() && serial->isWritable()) {
        serial->write(data);
        serial->flush();
        qDebug() << "[Arduino] Envoi:" << data;
    } else {
        qDebug() << "[Arduino] Port non disponible pour écriture.";
    }
}

bool Arduino::isConnected() const
{
    return serial->isOpen();
}

// =============================================================
//  API quai
// =============================================================
bool Arduino::connecter(const QString &portName)
{
    return (connect_arduino(portName) == 0);
}

void Arduino::deconnecter()
{
    close_arduino();
}

bool Arduino::estConnecte() const
{
    return isConnected();
}

QString Arduino::portActuel() const
{
    return serial->portName();
}

// =============================================================
//  Envoi de commandes
// =============================================================
bool Arduino::envoyerCommande(char commande)
{
    if (!serial->isOpen())
        return false;

    QByteArray data;
    data.append(commande);
    qint64 written = serial->write(data);
    serial->flush();
    return (written == 1);
}

bool Arduino::envoyerTexte(const QString &texte)
{
    if (!serial->isOpen())
        return false;

    QByteArray data = (texte + "\n").toUtf8();
    qint64 written = serial->write(data);
    serial->flush();
    QThread::msleep(100);
    return (written == data.size());
}

// =============================================================
//  Détection automatique
// =============================================================
bool Arduino::looksLikeArduino(const QSerialPortInfo &info)
{
    if (info.hasVendorIdentifier()) {
        quint16 vid = info.vendorIdentifier();
        if (vid == 0x2341 || vid == 0x1A86 || vid == 0x0403 || vid == 0x10C4)
            return true;
    }

    QString desc = info.description().toLower();
    QString mfr  = info.manufacturer().toLower();

    QStringList keywords = { "arduino", "ch340", "ch341", "ftdi",
                            "serial port", "usb-serial", "cp210" };
    for (const QString &kw : keywords) {
        if (desc.contains(kw) || mfr.contains(kw))
            return true;
    }

    return false;
}

QString Arduino::detectArduino()
{
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &p : ports) {
        if (looksLikeArduino(p)) {
            qDebug() << "[Arduino] Détecté sur" << p.portName()
                << "—" << p.description();
            return p.portName();
        }
    }
    qDebug() << "[Arduino] Aucun Arduino détecté.";
    return QString();
}

QList<QPair<QString, bool>> Arduino::listAllPorts()
{
    QList<QPair<QString, bool>> result;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &p : ports) {
        result.append({ p.portName(), looksLikeArduino(p) });
    }
    return result;
}

// =============================================================
//  API RFID - Gestion des employés
// =============================================================
void Arduino::onDonneesDisponibles()
{
    qDebug() << "🔌 onDonneesDisponibles() appelé";

    QByteArray data = serial->readAll();
    qDebug() << "📦 Données brutes reçues:" << data.length() << "bytes -" << data;

    m_buffer += QString::fromLatin1(data);
    qDebug() << "📝 Buffer actuel:" << m_buffer;

    while (m_buffer.contains('\n')) {
        int idx       = m_buffer.indexOf('\n');
        QString ligne = m_buffer.left(idx).trimmed();
        m_buffer      = m_buffer.mid(idx + 1);

        if (ligne.isEmpty()) {
            qDebug() << "⚠️ Ligne vide ignorée";
            continue;
        }
        qDebug() << "📥 Arduino →" << ligne;

        // Accepter les deux formats: "CODE:XXXXX" ou juste "XXXXX"
        QString code;
        if (ligne.startsWith("CODE:")) {
            code = ligne.mid(5).trimmed().toUpper();
            qDebug() << "✅ Format CODE: détecté";
        } else if (ligne.length() >= 8 && ligne.length() <= 16) {
            // C'est probablement un code RFID hexadécimal
            code = ligne.trimmed().toUpper();
            qDebug() << "✅ Format direct détecté (longueur:" << ligne.length() << ")";
        } else {
            qDebug() << "❌ Format non reconnu (longueur:" << ligne.length() << ")";
            continue; // Ignorer les autres lignes
        }

        if (!code.isEmpty()) {
            m_dernierCode = code;
            qDebug() << "🔖 Code RFID reçu :" << m_dernierCode;
            qDebug() << "📢 Émission du signal codeRecu";
            emit codeRecu(m_dernierCode);
        }
    }
}

QString Arduino::getDernierCode() const
{
    return m_dernierCode;
}

void Arduino::resetCode()
{
    m_dernierCode.clear();
}

void Arduino::demanderDernierUID()
{
    if (serial->isOpen())
        serial->write("GET\n");
}

int Arduino::lireStatusB(int idEmploye)
{
    QSqlQuery q;
    q.prepare("SELECT STATUSB FROM EMPLOYE WHERE ID_EMPLOYE = :id");
    q.bindValue(":id", idEmploye);
    if (!q.exec() || !q.next()) {
        qDebug() << "❌ lireStatusB erreur:" << q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

int Arduino::traiterBadge(int idEmploye, const QString &rfidCode)
{
    int statusActuel = lireStatusB(idEmploye);
    if (statusActuel == -1) return -1;

    int nouveauStatus = (!CODE_AUTORISE.isEmpty() && rfidCode.toUpper() == CODE_AUTORISE) ? 1 : 0;

    if (nouveauStatus != statusActuel) {
        QSqlQuery upd;
        upd.prepare("UPDATE EMPLOYE SET STATUSB = :s WHERE ID_EMPLOYE = :id");
        upd.bindValue(":s",  nouveauStatus);
        upd.bindValue(":id", idEmploye);
        if (!upd.exec()) {
            qDebug() << "❌ UPDATE STATUSB échoué:" << upd.lastError().text();
            return -1;
        }
        qDebug() << "✅ STATUSB:" << statusActuel << "→" << nouveauStatus;
    } else {
        qDebug() << "ℹ️  STATUSB inchangé:" << statusActuel;
    }

    return nouveauStatus;
}
