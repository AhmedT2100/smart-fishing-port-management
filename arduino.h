#ifndef ARDUINO_H
#define ARDUINO_H

#include <QObject>
#include <QString>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QList>
#include <QThread>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

inline const QString CODE_AUTORISE =
    qEnvironmentVariable("FISHING_PORT_AUTHORIZED_RFID").trimmed().toUpper();

class Arduino : public QObject
{
    Q_OBJECT

public:
    explicit Arduino(QObject *parent = nullptr);
    ~Arduino();

    // ── API bas niveau ────────────────────────────────────────
    int connect_arduino(const QString &portName = "COM3");
    int close_arduino();
    QByteArray read_from_arduino();
    void write_to_arduino(const QByteArray &data);
    QSerialPort *getSerial() { return serial; }
    bool isConnected() const;

    // ── API quai ──────────────────────────────────────────────
    bool connecter(const QString &portName);
    void deconnecter();
    bool estConnecte() const;
    QString portActuel() const;

    // ── Envoi ─────────────────────────────────────────────────
    bool envoyerCommande(char commande);
    bool envoyerTexte(const QString &texte);

    // ── Détection automatique ─────────────────────────────────
    static QString detectArduino();
    static QList<QPair<QString, bool>> listAllPorts();

    // ── API RFID ──────────────────────────────────────────────
    QString getDernierCode() const;
    void    resetCode();
    void    demanderDernierUID();  // envoie "GET" à l'Arduino
    int     lireStatusB(int idEmploye);
    int     traiterBadge(int idEmploye, const QString &rfidCode);

signals:
    void codeRecu(const QString &code);

private slots:
    void onDonneesDisponibles();

private:
    QSerialPort *serial;
    QString      m_buffer;
    QString      m_dernierCode;
    static bool looksLikeArduino(const QSerialPortInfo &info);
};

#endif // ARDUINO_H
