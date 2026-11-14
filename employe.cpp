#include "employe.h"
#include "connexion.h"
#include <QSqlError>
#include <QDebug>

Employe::Employe() {}

Employe::~Employe() {}

bool Employe::ajouter(int id, const QString& nom, const QString& licence,
                      const QString& contact, const QString& role, const QString& mdp)
{
    QSqlQuery query;
    query.prepare("INSERT INTO EMPLOYE(ID_EMPLOYE,NOME,NUM_LICENSE,CONTACT,ROLE,MOT_DE_PASSE) "
                  "VALUES(:id,:nom,:lic,:contact,:role,:mdp)");
    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":lic", licence);
    query.bindValue(":contact", contact);
    query.bindValue(":role", role);
    query.bindValue(":mdp", mdp);

    return query.exec();
}

bool Employe::modifier(int id, const QString& nom, const QString& licence,
                       const QString& contact, const QString& role, const QString& mdp)
{
    QSqlQuery query;
    query.prepare("UPDATE EMPLOYE SET NOME=:nom,NUM_LICENSE=:lic,CONTACT=:contact,"
                  "ROLE=:role,MOT_DE_PASSE=:mdp WHERE ID_EMPLOYE=:id");
    query.bindValue(":nom", nom);
    query.bindValue(":lic", licence);
    query.bindValue(":contact", contact);
    query.bindValue(":role", role);
    query.bindValue(":mdp", mdp);
    query.bindValue(":id", id);

    return query.exec() && query.numRowsAffected() > 0;
}

bool Employe::supprimer(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM EMPLOYE WHERE ID_EMPLOYE=:id");
    query.bindValue(":id", id);
    return query.exec() && query.numRowsAffected() > 0;
}

bool Employe::supprimerParNom(const QString& nom)
{
    QSqlQuery query;
    query.prepare("DELETE FROM EMPLOYE WHERE NOME=:nome");
    query.bindValue(":nome", nom);
    return query.exec() && query.numRowsAffected() > 0;
}

void Employe::rafraichirTable(QSqlQueryModel* model, const QString& orderCol, const QString& orderDir)
{
    QString safeCol = (orderCol == "ROLE") ? "\"ROLE\"" : orderCol;
    QString sql = QString("SELECT ID_EMPLOYE, NOME, NUM_LICENSE, CONTACT, ROLE "
                          "FROM EMPLOYE ORDER BY %1 %2").arg(safeCol, orderDir);
    model->setQuery(sql);

    if (model->lastError().isValid()) {
        qDebug() << "Erreur SQL rafraichirTable:" << model->lastError().text();
        model->setQuery("SELECT ID_EMPLOYE, NOME, NUM_LICENSE, CONTACT, ROLE "
                        "FROM EMPLOYE ORDER BY ID_EMPLOYE ASC");
    }

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Nom");
    model->setHeaderData(2, Qt::Horizontal, "Num Licence");
    model->setHeaderData(3, Qt::Horizontal, "Contact");
    model->setHeaderData(4, Qt::Horizontal, "Rôle");
}

void Employe::rechercher(QSqlQueryModel* model, const QString& texte, const QString& colonne,
                         const QString& orderCol, const QString& orderDir)
{
    QString t = texte;
    t.replace("'", "''");

    QString safeCol = (orderCol == "ROLE") ? "\"ROLE\"" : orderCol;
    QString whereClause;

    if (colonne == "ID employé") {
        // Pour la recherche par ID, essayer d'abord une correspondance exacte
        bool ok;
        int idValue = t.toInt(&ok);
        if (ok) {
            whereClause = QString("ID_EMPLOYE = %1").arg(idValue);
        } else {
            whereClause = QString("CAST(ID_EMPLOYE AS TEXT) LIKE '%%%1%%'").arg(t);
        }
    } else if (colonne == "Nom") {
        whereClause = QString("NOME LIKE '%%%1%%'").arg(t);
    } else if (colonne == "Num_license") {
        whereClause = QString("NUM_LICENSE LIKE '%%%1%%'").arg(t);
    } else if (colonne == "Contact") {
        whereClause = QString("CONTACT LIKE '%%%1%%'").arg(t);
    } else if (colonne == "Role") {
        whereClause = QString("ROLE LIKE '%%%1%%'").arg(t);
    } else {
        whereClause = QString("CAST(ID_EMPLOYE AS TEXT) LIKE '%%%1%%' "
                              "OR NOME LIKE '%%%1%%' "
                              "OR NUM_LICENSE LIKE '%%%1%%' "
                              "OR CONTACT LIKE '%%%1%%' "
                              "OR ROLE LIKE '%%%1%%'").arg(t);
    }

    QString sql = QString("SELECT ID_EMPLOYE, NOME, NUM_LICENSE, CONTACT, ROLE "
                          "FROM EMPLOYE WHERE %1 ORDER BY %2 %3").arg(whereClause, safeCol, orderDir);

    qDebug() << "SQL de recherche:" << sql;
    model->setQuery(sql);

    if (model->lastError().isValid()) {
        qDebug() << "Erreur SQL recherche:" << model->lastError().text();
    } else {
        qDebug() << "Recherche réussie, nombre de lignes:" << model->rowCount();
    }

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Nom");
    model->setHeaderData(2, Qt::Horizontal, "Num Licence");
    model->setHeaderData(3, Qt::Horizontal, "Contact");
    model->setHeaderData(4, Qt::Horizontal, "Rôle");
}

bool Employe::existeId(int id)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM EMPLOYE WHERE ID_EMPLOYE = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

int Employe::getNextId()
{
    QSqlQuery q;
    q.prepare("SELECT NVL(MAX(ID_EMPLOYE), 0) + 1 FROM EMPLOYE");
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

int Employe::getTotalEmployes()
{
    QSqlQuery query;
    query.exec("SELECT COUNT(*) FROM EMPLOYE");
    return query.next() ? query.value(0).toInt() : 0;
}

QMap<QString, int> Employe::getEmployesParRole()
{
    QMap<QString, int> parRole;
    QSqlQuery query;
    query.exec("SELECT ROLE, COUNT(*) FROM EMPLOYE GROUP BY ROLE ORDER BY COUNT(*) DESC");
    while (query.next()) {
        parRole[query.value(0).toString()] = query.value(1).toInt();
    }
    return parRole;
}

QString Employe::getNom(int id)
{
    QSqlQuery query;
    query.prepare("SELECT NOME FROM EMPLOYE WHERE ID_EMPLOYE=:id");
    query.bindValue(":id", id);
    return (query.exec() && query.next()) ? query.value(0).toString() : QString();
}

QSqlQuery Employe::getEmployeDetails(int id)
{
    QSqlQuery query;
    query.prepare("SELECT ID_EMPLOYE, NOME, NUM_LICENSE, CONTACT, ROLE FROM EMPLOYE WHERE ID_EMPLOYE=:id");
    query.bindValue(":id", id);
    query.exec();
    return query;
}

bool Employe::verifierIdentifiant(int id, QString &nom)
{
    QSqlQuery query;
    query.prepare("SELECT NOME FROM EMPLOYE WHERE ID_EMPLOYE = :id");
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        nom = query.value(0).toString();
        return true;
    }
    return false;
}

bool Employe::verifierContact(const QString& nom, const QString& contact)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM EMPLOYE WHERE NOME = :nom AND CONTACT = :contact");
    query.bindValue(":nom", nom);
    query.bindValue(":contact", contact);

    return query.exec() && query.next() && query.value(0).toInt() > 0;
}

bool Employe::reinitialiserMotDePasse(const QString& nom, const QString& nouveauMdp)
{
    QSqlQuery query;
    query.prepare("UPDATE EMPLOYE SET MOT_DE_PASSE = :mdp WHERE NOME = :nom");
    query.bindValue(":mdp", nouveauMdp);
    query.bindValue(":nom", nom);

    return query.exec() && query.numRowsAffected() > 0;
}
// Méthodes pour la séparation téléphone/email
QString Employe::getEmailFromContact(const QString& contact)
{
    QStringList parts = contact.split("|");
    return parts.size() > 1 ? parts[1].trimmed() : "";
}

QString Employe::getPhoneFromContact(const QString& contact)
{
    QStringList parts = contact.split("|");
    return parts.isEmpty() ? "" : parts[0].trimmed();
}

// Méthode pour vérifier si un email existe déjà
bool Employe::emailExists(const QString& email, int excludeId)
{
    QSqlQuery query;
    if (excludeId > 0) {
        query.prepare("SELECT COUNT(*) FROM EMPLOYE WHERE CONTACT LIKE ? AND ID_EMPLOYE != ?");
        query.addBindValue("%|" + email);
        query.addBindValue(excludeId);
    } else {
        query.prepare("SELECT COUNT(*) FROM EMPLOYE WHERE CONTACT LIKE ?");
        query.addBindValue("%|" + email);
    }

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
// IMPLÉMENTATION DES MÉTHODES POUR GESTION DES CONGÉS & ABSENCES
// ═══════════════════════════════════════════════════════════════

bool Employe::ajouterDemandeConge(int idEmploye, const QDate& dateDebut, const QDate& dateFin,
                                  const QString& motif, const QString& type)
{
    // Créer la table si elle n'existe pas
    QSqlQuery createTable;
    createTable.exec(R"(
        CREATE TABLE IF NOT EXISTS DEMANDES_CONGE (
            ID_DEMANDE INTEGER PRIMARY KEY AUTOINCREMENT,
            ID_EMPLOYE INTEGER NOT NULL,
            DATE_DEBUT DATE NOT NULL,
            DATE_FIN DATE NOT NULL,
            NB_JOURS INTEGER NOT NULL,
            TYPE_CONGE TEXT NOT NULL DEFAULT 'Congé',
            MOTIF TEXT,
            STATUT TEXT NOT NULL DEFAULT 'En attente',
            DATE_DEMANDE DATETIME DEFAULT CURRENT_TIMESTAMP,
            DATE_TRAITEMENT DATETIME,
            COMMENTAIRE_RH TEXT,
            FOREIGN KEY (ID_EMPLOYE) REFERENCES EMPLOYE(ID_EMPLOYE)
        )
    )");

    // Calculer le nombre de jours
    int nbJours = dateDebut.daysTo(dateFin) + 1;

    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO DEMANDES_CONGE (ID_EMPLOYE, DATE_DEBUT, DATE_FIN, NB_JOURS, TYPE_CONGE, MOTIF)
        VALUES (:idEmploye, :dateDebut, :dateFin, :nbJours, :type, :motif)
    )");

    query.bindValue(":idEmploye", idEmploye);
    query.bindValue(":dateDebut", dateDebut.toString("yyyy-MM-dd"));
    query.bindValue(":dateFin", dateFin.toString("yyyy-MM-dd"));
    query.bindValue(":nbJours", nbJours);
    query.bindValue(":type", type);
    query.bindValue(":motif", motif);

    return query.exec();
}

bool Employe::validerDemandeConge(int idDemande, bool approuve, const QString& commentaireRH)
{
    QSqlQuery query;
    query.prepare(R"(
        UPDATE DEMANDES_CONGE
        SET STATUT = :statut,
            DATE_TRAITEMENT = CURRENT_TIMESTAMP,
            COMMENTAIRE_RH = :commentaire
        WHERE ID_DEMANDE = :idDemande
    )");

    query.bindValue(":statut", approuve ? "Approuvé" : "Refusé");
    query.bindValue(":commentaire", commentaireRH);
    query.bindValue(":idDemande", idDemande);

    bool success = query.exec();

    // Si approuvé, marquer les présences automatiquement
    if (success && approuve) {
        QSqlQuery getDetails;
        getDetails.prepare("SELECT ID_EMPLOYE, DATE_DEBUT, DATE_FIN FROM DEMANDES_CONGE WHERE ID_DEMANDE = ?");
        getDetails.bindValue(0, idDemande);

        if (getDetails.exec() && getDetails.next()) {
            int idEmploye = getDetails.value(0).toInt();
            QDate dateDebut = QDate::fromString(getDetails.value(1).toString(), "yyyy-MM-dd");
            QDate dateFin = QDate::fromString(getDetails.value(2).toString(), "yyyy-MM-dd");

            // Marquer chaque jour comme congé
            QDate currentDate = dateDebut;
            while (currentDate <= dateFin) {
                marquerPresence(idEmploye, currentDate, "Congé");
                currentDate = currentDate.addDays(1);
            }
        }
    }

    return success;
}

QSqlQueryModel* Employe::getDemandesConge(int idEmploye)
{
    QSqlQueryModel* model = new QSqlQueryModel();
    QString sql =
        "SELECT dc.ID_DEMANDE, e.NOME, dc.DATE_DEBUT, dc.DATE_FIN, "
        "dc.NB_JOURS, dc.TYPE_CONGE, dc.MOTIF, dc.STATUT, "
        "dc.DATE_DEMANDE, dc.COMMENTAIRE_RH "
        "FROM DEMANDES_CONGE dc "
        "JOIN EMPLOYE e ON dc.ID_EMPLOYE = e.ID_EMPLOYE";

    if (idEmploye > 0)
        sql += QString(" WHERE dc.ID_EMPLOYE = %1").arg(idEmploye);

    sql += " ORDER BY dc.DATE_DEMANDE DESC";
    model->setQuery(sql);

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Employé");
    model->setHeaderData(2, Qt::Horizontal, "Date début");
    model->setHeaderData(3, Qt::Horizontal, "Date fin");
    model->setHeaderData(4, Qt::Horizontal, "Nb jours");
    model->setHeaderData(5, Qt::Horizontal, "Type");
    model->setHeaderData(6, Qt::Horizontal, "Motif");
    model->setHeaderData(7, Qt::Horizontal, "Statut");
    model->setHeaderData(8, Qt::Horizontal, "Date demande");
    model->setHeaderData(9, Qt::Horizontal, "Commentaire RH");

    return model;
}
QSqlQueryModel* Employe::getDemandesEnAttente()
{
    QSqlQueryModel* model = new QSqlQueryModel();
    model->setQuery(R"(
        SELECT
            dc.ID_DEMANDE as "ID",
            e.NOME as "Employé",
            dc.DATE_DEBUT as "Date début",
            dc.DATE_FIN as "Date fin",
            dc.NB_JOURS as "Nb jours",
            dc.TYPE_CONGE as "Type",
            dc.MOTIF as "Motif",
            dc.DATE_DEMANDE as "Date demande"
        FROM DEMANDES_CONGE dc
        JOIN EMPLOYE e ON dc.ID_EMPLOYE = e.ID_EMPLOYE
        WHERE dc.STATUT = 'En attente'
        ORDER BY dc.DATE_DEMANDE ASC
    )");
    return model;
}

int Employe::getSoldeConges(int idEmploye, int annee)
{
    if (annee == 0) annee = QDate::currentDate().year();

    // Solde de base : 30 jours par an (modifiable selon vos règles)
    int soldeBase = 30;

    // Soustraire les jours utilisés
    int joursUtilises = getJoursUtilises(idEmploye, annee);

    return soldeBase - joursUtilises;
}

int Employe::getJoursUtilises(int idEmploye, int annee)
{
    if (annee == 0) annee = QDate::currentDate().year();

    QSqlQuery query;
    query.prepare(R"(
        SELECT COALESCE(SUM(NB_JOURS), 0)
        FROM DEMANDES_CONGE
        WHERE ID_EMPLOYE = :idEmploye
        AND STATUT = 'Approuvé'
        AND strftime('%Y', DATE_DEBUT) = :annee
    )");

    query.bindValue(":idEmploye", idEmploye);
    query.bindValue(":annee", QString::number(annee));

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int Employe::getJoursRestants(int idEmploye, int annee)
{
    return getSoldeConges(idEmploye, annee);
}

bool Employe::marquerPresence(int idEmploye, const QDate& date, const QString& statut)
{
    qDebug() << "🟦 marquerPresence appelée: ID=" << idEmploye << "Date=" << date.toString("yyyy-MM-dd") << "Statut=" << statut;

    // Mettre à jour les colonnes DERNIERE_PRESENCE et PRESENCE_CONGE dans la table EMPLOYE
    QSqlQuery query;
    query.prepare(R"(
        UPDATE EMPLOYE
        SET DERNIERE_PRESENCE = TO_DATE(:date, 'YYYY-MM-DD'),
            PRESENCE_CONGE = :statut
        WHERE ID_EMPLOYE = :idEmploye
    )");

    query.bindValue(":idEmploye", idEmploye);
    query.bindValue(":date", date.toString("yyyy-MM-dd"));
    query.bindValue(":statut", statut);

    bool success = query.exec();
    if (!success) {
        qDebug() << "❌ Erreur SQL marquerPresence:" << query.lastError().text();
        qDebug() << "   Requête:" << query.lastQuery();
        return false;
    } else {
        qDebug() << "✅ Présence enregistrée avec succès!";
        // Commit la transaction
        if (!QSqlDatabase::database().commit()) {
            qDebug() << "⚠️ Erreur lors du commit:" << QSqlDatabase::database().lastError().text();
            QSqlDatabase::database().rollback();
            return false;
        }
        return true;
    }
}

QString Employe::getStatutPresence(int idEmploye, const QDate& date)
{
    QSqlQuery query;
    query.prepare("SELECT PRESENCE_CONGE FROM EMPLOYE WHERE ID_EMPLOYE = :idEmploye");
    query.bindValue(":idEmploye", idEmploye);

    if (query.exec() && query.next()) {
        QString statut = query.value(0).toString();
        qDebug() << "✅ Statut trouvé pour ID" << idEmploye << ":" << statut;
        return statut;
    }
    qDebug() << "⚠️ Aucun statut trouvé pour ID" << idEmploye;
    if (!query.lastError().text().isEmpty()) {
        qDebug() << "❌ Erreur SQL getStatutPresence:" << query.lastError().text();
    }
    return ""; // Pas de données
}

QMap<QDate, QString> Employe::getPresencesMois(int idEmploye, int mois, int annee)
{
    QMap<QDate, QString> presences;

    QSqlQuery query;
    query.prepare(R"(
        SELECT DATE_PRESENCE, STATUT
        FROM PRESENCES
        WHERE ID_EMPLOYE = :idEmploye
        AND TO_CHAR(DATE_PRESENCE, 'YYYY') = :annee
        AND TO_CHAR(DATE_PRESENCE, 'MM') = :mois
        ORDER BY DATE_PRESENCE
    )");

    query.bindValue(":idEmploye", idEmploye);
    query.bindValue(":annee", QString::number(annee));
    query.bindValue(":mois", QString("%1").arg(mois, 2, 10, QChar('0')));

    if (query.exec()) {
        while (query.next()) {
            QDate date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
            QString statut = query.value(1).toString();
            presences[date] = statut;
        }
    } else {
        qDebug() << "❌ Erreur SQL getPresencesMois:" << query.lastError().text();
    }

    return presences;
}

QMap<QString, int> Employe::getStatistiquesConges(int annee)
{
    if (annee == 0) annee = QDate::currentDate().year();

    QMap<QString, int> stats;

    QSqlQuery query;
    query.prepare(R"(
        SELECT
            STATUT,
            COUNT(*) as nb_demandes,
            COALESCE(SUM(NB_JOURS), 0) as total_jours
        FROM DEMANDES_CONGE
        WHERE strftime('%Y', DATE_DEBUT) = :annee
        GROUP BY STATUT
    )");

    query.bindValue(":annee", QString::number(annee));

    if (query.exec()) {
        while (query.next()) {
            QString statut = query.value(0).toString();
            int nbDemandes = query.value(1).toInt();
            int totalJours = query.value(2).toInt();

            stats[statut + "_demandes"] = nbDemandes;
            stats[statut + "_jours"] = totalJours;
        }
    }

    return stats;
}
