#include "sortie.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlQueryModel>
#include <QDebug>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Sortie::Sortie() {}

// ---------------------------------------------------------------------------
// Last error helper
// ---------------------------------------------------------------------------
QString Sortie::lastError() const { return m_lastError; }

// ---------------------------------------------------------------------------
// Shared header configuration for query models
// ---------------------------------------------------------------------------
void Sortie::configureHeaders(QSqlQueryModel *model)
{
    model->setHeaderData(0, Qt::Horizontal, "ID Sortie");
    model->setHeaderData(1, Qt::Horizontal, "Date Arrivée");
    model->setHeaderData(2, Qt::Horizontal, "Date Depart");
    model->setHeaderData(3, Qt::Horizontal, "Etat");
    model->setHeaderData(4, Qt::Horizontal, "Bateaux");
}

// ═══════════════════════════════════════════════════════════════════════════
//  CRUD
// ═══════════════════════════════════════════════════════════════════════════

bool Sortie::existe(int id)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM SORTIE WHERE SORTIE_ID = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

bool Sortie::ajouter(int id, const QDate &dateArrivage,
                     const QDate &dateDepart, const QString &etat)
{
    m_lastError.clear();
    if (!dateArrivage.isValid() || !dateDepart.isValid()) {
        m_lastError = "Invalid date for sortie";
        return false;
    }
    QSqlQuery q;
    q.prepare("INSERT INTO SORTIE (SORTIE_ID, DATE_ARRIVAGE, DATE_DEPART, ETAT) "
              "VALUES (:id, :da, :dd, :etat)");
    q.bindValue(":id",   id);
    q.bindValue(":da",   dateArrivage);
    q.bindValue(":dd",   dateDepart);
    q.bindValue(":etat", etat);

    if (q.exec()) return true;
    m_lastError = q.lastError().text();
    return false;
}

bool Sortie::modifier(int id, const QDate &dateArrivage,
                      const QDate &dateDepart, const QString &etat)
{
    m_lastError.clear();
    if (!dateArrivage.isValid() || !dateDepart.isValid()) {
        m_lastError = "Invalid date for sortie";
        return false;
    }
    QSqlQuery q;
    q.prepare("UPDATE SORTIE SET "
              "DATE_ARRIVAGE = :da, "
              "DATE_DEPART   = :dd, "
              "ETAT          = :etat "
              "WHERE SORTIE_ID = :id");
    q.bindValue(":da",   dateArrivage);
    q.bindValue(":dd",   dateDepart);
    q.bindValue(":etat", etat);
    q.bindValue(":id",   id);

    if (q.exec()) return true;
    m_lastError = q.lastError().text();
    return false;
}

bool Sortie::supprimer(int id)
{
    QSqlQuery q;
    // First delete child records in AVOIR
    q.prepare("DELETE FROM AVOIR WHERE SORTIE_ID = :id");
    q.bindValue(":id", id);
    q.exec();   // ignore error if none exist

    // Then delete the sortie itself
    q.prepare("DELETE FROM SORTIE WHERE SORTIE_ID = :id");
    q.bindValue(":id", id);
    return q.exec();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Query models for QTableView
// ═══════════════════════════════════════════════════════════════════════════

QSqlQueryModel* Sortie::listerTout()
{
    m_lastError.clear();
    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery(
        "SELECT S.SORTIE_ID, S.DATE_ARRIVAGE, S.DATE_DEPART, S.ETAT, "
        "NVL(LISTAGG(A.NUMERO_CARTE_GRISE, ', ') "
        "WITHIN GROUP (ORDER BY A.NUMERO_CARTE_GRISE), '-') AS BATEAUX "
        "FROM SORTIE S "
        "LEFT JOIN AVOIR A ON S.SORTIE_ID = A.SORTIE_ID "
        "GROUP BY S.SORTIE_ID, S.DATE_ARRIVAGE, S.DATE_DEPART, S.ETAT "
        "ORDER BY S.SORTIE_ID"
        );

    if (model->lastError().isValid())
        m_lastError = model->lastError().text();

    configureHeaders(model);
    return model;
}

QSqlQueryModel* Sortie::rechercherParId(int id)
{
    QString baseQuery =
        "SELECT S.SORTIE_ID, S.DATE_ARRIVAGE, S.DATE_DEPART, S.ETAT, "
        "NVL(LISTAGG(A.NUMERO_CARTE_GRISE, ', ') "
        "WITHIN GROUP (ORDER BY A.NUMERO_CARTE_GRISE), '-') AS BATEAUX "
        "FROM SORTIE S "
        "LEFT JOIN AVOIR A ON S.SORTIE_ID = A.SORTIE_ID "
        "WHERE S.SORTIE_ID = " + QString::number(id) + " "
                                "GROUP BY S.SORTIE_ID, S.DATE_ARRIVAGE, S.DATE_DEPART, S.ETAT "
                                "ORDER BY S.SORTIE_ID";

    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery(baseQuery);
    configureHeaders(model);
    return model;
}

QSqlQueryModel* Sortie::filtrerParEtat(const QString &etat)
{
    QString query =
        "SELECT S.SORTIE_ID, S.DATE_ARRIVAGE, S.DATE_DEPART, S.ETAT, "
        "NVL(LISTAGG(A.NUMERO_CARTE_GRISE, ', ') "
        "WITHIN GROUP (ORDER BY A.NUMERO_CARTE_GRISE), '-') AS BATEAUX "
        "FROM SORTIE S "
        "LEFT JOIN AVOIR A ON S.SORTIE_ID = A.SORTIE_ID "
        "WHERE S.ETAT = '" + etat + "' "
                 "GROUP BY S.SORTIE_ID, S.DATE_ARRIVAGE, S.DATE_DEPART, S.ETAT "
                 "ORDER BY S.SORTIE_ID";

    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery(query);
    configureHeaders(model);
    return model;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Statistics
// ═══════════════════════════════════════════════════════════════════════════

bool Sortie::statistiques(QMap<QString, int> &data, int &total)
{
    QSqlQuery q;
    q.prepare("SELECT ETAT, COUNT(*) FROM SORTIE GROUP BY ETAT");

    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }

    data.clear();
    total = 0;
    while (q.next()) {
        QString etat = q.value(0).toString();
        int count    = q.value(1).toInt();
        data[etat]   = count;
        total       += count;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Accident detection — load all sorties
// ═══════════════════════════════════════════════════════════════════════════

QList<SortieInfo> Sortie::chargerToutesSorties()
{
    QList<SortieInfo> list;
    QSqlQuery q;
    q.prepare("SELECT SORTIE_ID, DATE_ARRIVAGE, DATE_DEPART, ETAT FROM SORTIE");
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return list;
    }

    while (q.next()) {
        SortieInfo s;
        s.id       = q.value(0).toInt();
        s.arrivage = q.value(1).toDate();
        s.depart   = q.value(2).toDate();
        s.etat     = q.value(3).toString();
        if (s.depart.isValid() && s.arrivage.isValid())
            list.append(s);
    }
    return list;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Bateau / AVOIR helpers
// ═══════════════════════════════════════════════════════════════════════════

QList<BateauInfo> Sortie::chargerListeBateaux()
{
    QList<BateauInfo> list;
    QSqlQuery q("SELECT NUMERO_CARTE_GRISE, TYPE, NOMB FROM BATEAU ORDER BY NUMERO_CARTE_GRISE");
    while (q.next()) {
        BateauInfo b;
        b.numeroCarteGrise = q.value(0).toString();
        b.type             = q.value(1).toString();
        b.nomb             = q.value(2).toString();
        list.append(b);
    }
    return list;
}

QStringList Sortie::chargerBateauxSortie(int sortieId)
{
    QStringList result;
    QSqlQuery q;
    q.prepare("SELECT NUMERO_CARTE_GRISE FROM AVOIR WHERE SORTIE_ID = :id");
    q.bindValue(":id", sortieId);
    if (!q.exec()) return result;

    while (q.next())
        result << q.value(0).toString();
    return result;
}

void Sortie::sauvegarderAvoir(int sortieId, const QStringList &bateaux)
{
    // Delete existing AVOIR rows for this sortie
    QSqlQuery del;
    del.prepare("DELETE FROM AVOIR WHERE SORTIE_ID = :id");
    del.bindValue(":id", sortieId);
    del.exec();

    // Insert one row per selected boat
    QSqlQuery ins;
    ins.prepare("INSERT INTO AVOIR (SORTIE_ID, NUMERO_CARTE_GRISE) VALUES (:sid, :cg)");
    for (const QString &cg : bateaux) {
        ins.bindValue(":sid", sortieId);
        ins.bindValue(":cg",  cg);
        if (!ins.exec())
            qDebug() << "AVOIR insert error for" << cg << ":" << ins.lastError().text();
    }
}
