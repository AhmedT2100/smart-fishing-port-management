#ifndef SORTIE_H
#define SORTIE_H

#include <QString>
#include <QDate>
#include <QMap>
#include <QStringList>
#include <QSqlQueryModel>

// ---------------------------------------------------------------------------
// Data structs shared between Model and View
// ---------------------------------------------------------------------------
struct SortieInfo {
    int     id;
    QDate   depart;
    QDate   arrivage;
    QString etat;
};

struct BateauInfo {
    QString numeroCarteGrise;
    QString type;
    QString nomb;
};

// ---------------------------------------------------------------------------
// Sortie — the Model class (handles ALL database operations)
// ---------------------------------------------------------------------------
class Sortie
{
public:
    Sortie();

    // ── CRUD ────────────────────────────────────────────────────────────────
    bool ajouter(int id, const QDate &dateArrivage, const QDate &dateDepart, const QString &etat);
    bool modifier(int id, const QDate &dateArrivage, const QDate &dateDepart, const QString &etat);
    bool supprimer(int id);
    bool existe(int id);

    // ── Queries returning models for QTableView ─────────────────────────────
    QSqlQueryModel* listerTout();
    QSqlQueryModel* rechercherParId(int id);
    QSqlQueryModel* filtrerParEtat(const QString &etat);

    // ── Statistics ──────────────────────────────────────────────────────────
    bool statistiques(QMap<QString, int> &data, int &total);

    // ── Accident detection ─────────────────────────────────────────────────
    QList<SortieInfo> chargerToutesSorties();

    // ── Bateau / AVOIR helpers ─────────────────────────────────────────────
    QList<BateauInfo> chargerListeBateaux();
    QStringList       chargerBateauxSortie(int sortieId);
    void              sauvegarderAvoir(int sortieId, const QStringList &bateaux);

    // ── Error reporting ────────────────────────────────────────────────────
    QString lastError() const;

private:
    QString m_lastError;

    // Shared base query used by listerTout / rechercherParId / filtrerParEtat
    void configureHeaders(QSqlQueryModel *model);
};

#endif // SORTIE_H
