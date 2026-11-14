#ifndef EMPLOYE_H
#define EMPLOYE_H

#include <QString>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSortFilterProxyModel>
#include <QMap>
#include <QDate>

class Employe
{
public:
    Employe();
    ~Employe();

    // Méthodes CRUD
    bool ajouter(int id, const QString& nom, const QString& licence,
                 const QString& contact, const QString& role, const QString& mdp);
    bool modifier(int id, const QString& nom, const QString& licence,
                  const QString& contact, const QString& role, const QString& mdp);
    bool supprimer(int id);
    bool supprimerParNom(const QString& nom);
    bool existeId(int id);
    int getNextId();

    // Méthodes de recherche et affichage
    void rechercher(QSqlQueryModel* model, const QString& texte, const QString& colonne,
                    const QString& orderCol, const QString& orderDir);
    void rafraichirTable(QSqlQueryModel* model, const QString& orderCol, const QString& orderDir);

    // Méthodes statistiques
    int getTotalEmployes();
    QMap<QString, int> getEmployesParRole();

    // Getters
    QString getNom(int id);
    QSqlQuery getEmployeDetails(int id);

    // Méthodes pour mot de passe oublié
    bool verifierIdentifiant(int id, QString &nom);
    bool verifierContact(const QString& nom, const QString& contact);
    bool reinitialiserMotDePasse(const QString& nom, const QString& nouveauMdp);

    // Méthodes pour la séparation téléphone/email
    QString getEmailFromContact(const QString& contact);
    QString getPhoneFromContact(const QString& contact);

    // Méthode pour vérifier l'unicité de l'email
    bool emailExists(const QString& email, int excludeId = 0);

    // ═══════════════════════════════════════════════════════════════
    // NOUVELLES MÉTHODES POUR GESTION DES CONGÉS & ABSENCES
    // ═══════════════════════════════════════════════════════════════

    // Gestion des demandes de congé
    bool ajouterDemandeConge(int idEmploye, const QDate& dateDebut, const QDate& dateFin,
                             const QString& motif, const QString& type = "Congé");
    bool validerDemandeConge(int idDemande, bool approuve, const QString& commentaireRH = "");
    QSqlQueryModel* getDemandesConge(int idEmploye = 0); // 0 = toutes les demandes
    QSqlQueryModel* getDemandesEnAttente();

    // Calcul du solde de congés
    int getSoldeConges(int idEmploye, int annee = 0); // 0 = année courante
    int getJoursUtilises(int idEmploye, int annee = 0);
    int getJoursRestants(int idEmploye, int annee = 0);

    // Gestion des présences
    bool marquerPresence(int idEmploye, const QDate& date, const QString& statut); // "Présent", "Absent", "Congé"
    QString getStatutPresence(int idEmploye, const QDate& date);
    QMap<QDate, QString> getPresencesMois(int idEmploye, int mois, int annee);

    // Statistiques
    QMap<QString, int> getStatistiquesConges(int annee = 0);
};

#endif // EMPLOYE_H
