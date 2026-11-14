#ifndef QUAI_H
#define QUAI_H

#include <QString>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QMap>
#include <QVector>
#include <QPainter>
#include <QScrollArea>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QPushButton>
#include <QMouseEvent>
#include <functional>
#include <QWidget>

// ── Qt Charts / Widgets pour StatsDialog ─────────────────────
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QChartView>
#include <QPieSeries>
#include <QChart>
#include <QPieSlice>

// ── Export PDF ────────────────────────────────────────────────
#include <QPrinter>
#include <QTextDocument>

// =============================================================
//  Classe Quai  (CRUD + utilitaires)
// =============================================================
class Quai
{
private:
    int id_quais;
    QString nom;
    int capacite;
    double longueur;
    QString statusq;

public:
    Quai();
    Quai(int, QString, int, double, QString);

    // CRUD
    bool ajouter();
    bool supprimer(int);
    bool modifier(int);
    QSqlQueryModel* afficher();
    QSqlQueryModel* chercher(QString colonne, QString valeur);
    QSqlQueryModel* trier(QString colonne, QString ordre = "ASC"); // tri par colonne + ordre
    QMap<QString,int> getStatutCounts();           // {Libre: N, Occupé: M}
    bool exporterExcel(const QString &cheminFichier); // export CSV (Excel)
    bool exporterPDF(const QString &cheminFichier);   // export PDF
    int prochainId();                              // retourne MAX(id_quais)+1
    QString getStatusQuai(int id);                 // retourne statusq pour l'Arduino (module quai)

    // Metier 1 + 2 (anciens)
    struct EmplacementInfo {
        int     id;
        int     numero;
        QString etat;
        QString typeP;
    };
    QString getNomQuai(int id_quais);
    QVector<EmplacementInfo> getEmplacements(int id_quais);
    QString getBateauType(int carte_grise);

    // Metier 1 & 2 (nouveaux) — etat des quais
    struct QuaiInfo {
        int     id;
        QString nom;
        int     capacite;
        int     total;     // nb emplacements total
        int     libres;    // nb emplacements etatE='libre'
    };
    QVector<QuaiInfo> getAllQuaisInfo();          // tous les quais avec leur etat
    bool occupierPremierEmplacement(int id_quais); // marque 1 libre -> occupe (+ commit)

    // Méthodes d'intégration (appelées depuis fishingport.cpp)
    void loadTable(QSqlQueryModel *model);          // charge tous les quais dans le model
    void visualiserQuais(QWidget *parent);          // ouvre QuaisVisuDialog (Métier 1)
    void lancerAffectationAutomatique(int nbBateaux, QWidget *parent); // ouvre QuaisAffectDialog (Métier 2)
};

// =============================================================
//  StatsDialog
// =============================================================
class StatsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StatsDialog(QMap<QString,int> counts, QWidget *parent = nullptr);
};

// =============================================================
//  EmplacementDialog  (conserve pour usage interne)
// =============================================================
class EmplacementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EmplacementDialog(const QString &nomQuai,
                               int idQuai,
                               const QVector<Quai::EmplacementInfo> &emplacements,
                               int highlightId = -1,
                               const QString &bandeauMsg = QString(),
                               bool bandeauError = false,
                               QWidget *parent = nullptr);
};

// Forward declaration de la tuile interne (implementee dans quai.cpp)
class QuaiTile;

// =============================================================
//  QuaisVisuDialog  — Metier 1 : visualisation etat des quais
// =============================================================
class QuaisVisuDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QuaisVisuDialog(const QVector<Quai::QuaiInfo> &quais,
                             QWidget *parent = nullptr);
};

// =============================================================
//  QuaisAffectDialog — Metier 2 : affectation interactive
// =============================================================
class QuaisAffectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QuaisAffectDialog(int nbBateaux,
                               QVector<Quai::QuaiInfo> quais,
                               QWidget *parent = nullptr);

private slots:
    void onBlink();

private:
    void onTileClicked(int idQuai);
    int  findBestId() const;
    void refreshHighlight();
    void updateStatusLabel();

    int                    m_remaining;
    QVector<Quai::QuaiInfo> m_quais;
    QMap<int, QuaiTile*>   m_tiles;
    QLabel                *m_lblStatus;
    QTimer                *m_blinkTimer;
    int                    m_bestId;
    bool                   m_blinkOn;
    QPushButton           *m_btnClose;
};

#endif
