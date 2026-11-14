#include "connexion.h"
#include <QDebug>
#include <QProcessEnvironment>

Connexion::Connexion() {}

bool Connexion::ouvrirConnexion()
{
    db = QSqlDatabase::addDatabase("QODBC");
    // Configure the database connection using an environment variable.
    const QString odbcConnection = qEnvironmentVariable("FISHING_PORT_ODBC_CONNECTION");
    if (odbcConnection.isEmpty()) {
        qWarning() << "FISHING_PORT_ODBC_CONNECTION is not configured.";
        return false;
    }
    db.setDatabaseName(odbcConnection);

    if (db.open()) {
        qDebug() << "Connexion réussie à la base de données !";
        return true;
    } else {
        qWarning() << "Database connection failed. Check local ODBC configuration.";
        return false;
    }
}

void Connexion::fermerConnexion()
{
    if (db.isOpen()) {
        db.close();
        qDebug() << "Connexion fermée.";
    }
}
