    #ifndef FISHINGPORT_H
#define FISHINGPORT_H

#include <QMainWindow>
#include <QWidget>
#include <QDateTime>
#include <QBoxLayout>
#include <QSqlQueryModel>
#include <QtSql>
#include <QNetworkAccessManager>
#include <QRadioButton>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QComboBox>
#include <QButtonGroup>
#include "bateau.h"
#include "emplacement.h"
#include "quai.h"
#include "sortie.h"
#include "employe.h"
#include "arduino.h"

QT_BEGIN_NAMESPACE
namespace Ui { class fishingport; }
QT_END_NAMESPACE

class fishingport : public QMainWindow
{
    Q_OBJECT

public:
    fishingport(QWidget *parent = nullptr);
    ~fishingport();

    void loadBateaux();
    void loadStatistiques();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void employe_gererConges();
    void employe_gererPresence();
    // ── Bateaux ──────────────────────────────────────────────────────────────
    void on_btnAjouter_clicked();
    void on_btnEnregistrer_clicked();
    void on_btnModify_clicked();
    void on_btnDelete_clicked();
    void on_tableBateaux_clicked(const QModelIndex &index);
    void on_pushButton_2_clicked();
    void on_pushButton_clicked();
    void on_btnConsulter_clicked();
    void on_btnAnnuler_clicked();
    void on_btnNavBateaux_clicked();
    void on_btnRefreshStats_clicked();
    void on_btnActionStats_clicked();
    void on_btnRetour_clicked();
    void applyFilters();
    void on_btnSendChat_clicked();
    void on_btnPrediction_clicked();
    void on_btnLaunchPrediction_clicked();
    void on_btnPredictionRetour_clicked();

    // ── Emplacements ─────────────────────────────────────────────────────────
    void on_btnNavEmplacements_clicked();
    void on_btnNavDashboard_clicked();
    void on_pushButton_14_clicked();
    void on_pushButton_16_clicked();
    void on_pushButton_17_clicked();
    void on_comboBox_11_currentIndexChanged(int index);
    void rechercherEmplacements();
    void trierEmplacements();
    void exporterPDF();
    void exporterExcel();
    void consulterHistorique();
    void rechercherHistorique();
    void afficherFriseDialog(int idEmplacement);
    void lancerRecommandation();
    void afficherModalAttribution(int idEmp, int idBateau,
                                  const QString &numero, const QString &quai,
                                  int score, const QString &nomBateau,
                                  const QString &typeBateau);
    void afficherAnalyseSaisonniere();

    // ── Quais ────────────────────────────────────────────────────────────────
    void on_btnNavQuais_clicked();
    void on_quai_btnAjouter_clicked();
    void on_quai_btnModifier_clicked();
    void on_quai_btnSupprimer_clicked();
    void on_quai_btnStatistiques_clicked();
    void on_quai_btnMetier1_clicked();
    void on_quai_btnMetier2_clicked();
    void on_quai_tableView_clicked(const QModelIndex &index);
    void on_quai_lineEdit_recherche_textChanged(const QString &text);
    void on_quai_comboBox_rechercheCritere_currentIndexChanged(int index);
    void on_quai_comboBox_tri_currentIndexChanged(int index);
    void on_quai_comboBox_export_currentIndexChanged(int index);

    // ── Sorties ──────────────────────────────────────────────────────────────
    void on_btnNavSorties_clicked();

    // ── Arduino Quai ─────────────────────────────────────────────────────────
    void on_btnConnecterArduino_clicked(); // Connecter l'Arduino via port COM
    void on_pushbuttonchoisir_clicked();   // Vérifier le quai et commander l'Arduino

    // ── Arduino Bateau (moniteur PIR — page embarquée dans stackedWidget_2) ───
    void on_btnArduinoMonitor_clicked();   // open the monitor page
    void on_btnArduinoBack_clicked();      // return to bateau list
    void ard_onConnectClicked();           // connect/disconnect Arduino
    void ard_onScanClicked();              // detect Arduino ports
    void ard_onBoatChanged(int index);     // boat selection changed
    void ard_readSerial();                 // readyRead → process MOTION

    void sortie_afficherTable();
    void sortie_clearForm();
    void sortie_onAjouter();
    void sortie_onModifier();
    void sortie_onSupprimer();
    void sortie_onConfirmer();
    void sortie_onAnnuler();
    void sortie_rechercherParId();
    void sortie_trierParEtat();
    void sortie_exporterPDF();
    void sortie_exporterExcel();
    void sortie_afficherStatistiques();
    void sortie_suggestReschedule();
    void sortie_detecterAccident();
    void sortie_applyWeatherSuggestion();
    void sortie_applyAccidentSuggestion();

    // ── Employés ─────────────────────────────────────────────────────────────
    void on_btnNavEmployes_clicked();
    void on_btnNavHardware_clicked();
    void employe_afficherTable();
    void employe_clearForm();
    void employe_onAjouter();
    void employe_onModifier();
    void employe_onSupprimer();
    void employe_onConfirmer();
    void employe_onAnnuler();
    void employe_rechercher();
    void employe_trier();
    void employe_exporterPDF();
    void employe_exporterExcel();
    void employe_exporterCSV();
    void employe_afficherStatistiques();

    // ── Arduino RFID (Vérification d'accès employés) ─────────────────────────
    void on_pushButton_8_clicked();                      // Bouton "Visualiser le status"
    void onBadgeDetecte(const QString &rfidCode);        // Signal Arduino badge détecté
    void showArduinoSmsAuth(int employeId, const QString &rfidCode);  // Authentification SMS - retourne true si succès
    void showAccesRefuse(const QString &rfidCode, int idEmploye);     // Dialogue accès refusé
    void envoyerCodeParEmail(const QString &email, int code, QLabel *statusLabel);

    // ── Authentification & Session ────────────────────────────────────────────
    void showLoginDialog();
    bool authenticateUser(const QString &username, const QString &password);
    void showPasswordResetDialog();
    void onDeconnexion();

private:
    Ui::fishingport *ui;

    // ── Modern shell & dashboard ───────────────────────────────────────────
    void applyTheme();
    void setupShell();
    void setupSidebar();
    void buildDashboardPage();
    void updateDashboard();
    void polishEmplacementsPage();
    void updateResponsiveLayout();
    void setActivePage(QWidget *page, const QString &title,
                       const QString &subtitle = QString());
    void clearLayout(QLayout *layout);
    void polishLegacyUi();
    void applyStatusStyle(QLabel *label, const QString &status);

    // ── Session utilisateur ───────────────────────────────────────────────────
    QString m_connectedUserName;
    QString m_connectedUserRole;
    int     m_connectedUserId = -1;
    QString m_connectionMethod = "";  // "Badge" ou "Mot de passe"
    QDateTime m_connectionTime;       // Heure de connexion

    // ── Badge utilisateur (créé dynamiquement dans la sidebar) ───────────────
    QWidget     *m_badgeWidget    = nullptr;
    QLabel      *m_badgeAvatar    = nullptr;
    QLabel      *m_badgeName      = nullptr;
    QLabel      *m_badgeRole      = nullptr;
    QLabel      *m_badgeId        = nullptr;
    QPushButton *m_btnDeconnexion = nullptr;

    QWidget     *m_badgeHost       = nullptr;
    QHBoxLayout *m_badgeHostLayout = nullptr;

    QWidget     *m_dashboardPage   = nullptr;
    QWidget     *m_mainArea        = nullptr;
    QFrame      *m_headerBar       = nullptr;
    QLabel      *m_headerTitle     = nullptr;
    QLabel      *m_headerSubtitle  = nullptr;
    QLineEdit   *m_globalSearch    = nullptr;
    QButtonGroup *m_navGroup       = nullptr;
    QPushButton *m_btnNavDashboard = nullptr;
    QPushButton *m_btnNavHardware  = nullptr;
    QBoxLayout  *m_emplacementToolsLayout = nullptr;
    QBoxLayout  *m_emplacementContentLayout = nullptr;
    QBoxLayout  *m_emplacementStatsLayout = nullptr;

    QLabel      *m_dashWelcomeName = nullptr;
    QLabel      *m_dashWelcomeRole = nullptr;
    QLabel      *m_dashBoatsTotal  = nullptr;
    QLabel      *m_dashBoatsAtSea  = nullptr;
    QLabel      *m_dashQuaisFree   = nullptr;
    QLabel      *m_dashEmpFree     = nullptr;
    QLabel      *m_dashSortiesPrev = nullptr;
    QLabel      *m_dashEmployesTot = nullptr;
    QLabel      *m_dashRfidStatus  = nullptr;
    QLabel      *m_dashPirStatus   = nullptr;
    QLabel      *m_dashDbStatus    = nullptr;
    QListWidget *m_dashRecentList  = nullptr;

    // ── Droits selon le rôle ─────────────────────────────────────────────────
    void creerBadgeUtilisateur();
    void appliquerDroitsRole();
    void showEmployeePersonalSpace();  // Espace personnel employé

    // Twilio SMS
    bool sendPasswordResetSMS(const QString &phoneNumber, const QString &employeeName, const QString &resetCode);
    QString generateResetCode();

    // ── Bateaux ──────────────────────────────────────────────────────────────
    QSqlQueryModel *model;
    bool m_editMode = false;
    QString m_originalNum;
    QNetworkAccessManager *networkManager;
    void sendToOllama(const QString &prompt);

    // ── Emplacements ─────────────────────────────────────────────────────────
    emplacement Etmp;
    bool m_connexionsBranchees      = false;
    bool m_histConnexionsBranchees  = false;
    bool m_emplacementPagePolished  = false;
    void afficherEmplacements();
    void ajouterBoutonsAction(int ligne);
    void mettreAJourStatistiquesEmp();
    void voirEmplacement(int row);
    void modifierEmplacement(int row);
    void supprimerEmplacement(int row);
    QIcon creerIcone(const QString &type);

    // ── Quais ────────────────────────────────────────────────────────────────
    Quai m_tmpQuai;
    int  m_quaiSelectedId = -1;
    QSqlQueryModel *m_quaiModel = nullptr;
    void chargerQuais();
    void lancerRechercheQuai();
    void clearFormQuai();
    void loadSelectedQuaiRow();

    // ── Sorties ──────────────────────────────────────────────────────────────
    Sortie sortieModel;
    bool   m_sortieModeModification = false;
    QTableView   *sortie_tableView         = nullptr;
    QLabel       *sortie_statusLabel       = nullptr;
    QLineEdit    *sortie_lineEditRecherche  = nullptr;
    QLineEdit    *sortie_lineEditId         = nullptr;
    QLineEdit    *sortie_lineEditArrivee    = nullptr;
    QLineEdit    *sortie_lineEditDepart     = nullptr;
    QRadioButton *sortie_radioConfirme      = nullptr;
    QRadioButton *sortie_radioPrevue        = nullptr;
    QRadioButton *sortie_radioRetardee      = nullptr;
    QRadioButton *sortie_radioAnnulee       = nullptr;
    QRadioButton *sortie_filterRadioConfirme = nullptr;
    QRadioButton *sortie_filterRadioPrevue   = nullptr;
    QRadioButton *sortie_filterRadioRetardee = nullptr;
    QRadioButton *sortie_filterRadioAnnulee  = nullptr;
    QList<QRadioButton*> sortie_etatRadios;
    QListWidget  *sortie_listWidgetBateaux  = nullptr;

    struct AccidentSuggestion {
        int     sortieId    = -1;
        QDate   newDepart;
        QDate   newArrivage;
        QString newEtat;
        QString reason;
        bool    needsChange = false;
    };
    AccidentSuggestion m_currentWeatherSuggestion;
    AccidentSuggestion m_currentAccidentSuggestion;

    void initSortiePage();
    void sortie_ajouterSortie();
    void sortie_modifierSortie();
    QStringList sortie_bateauxSelectionnes() const;
    void sortie_chargerListeBateaux();
    void sortie_preselectionnerBateaux(int sortieId);
    void sortie_sauvegarderAvoir(int sortieId);
    bool sortie_tryGetTargetSortie(SortieInfo &info);
    void sortie_syncFormWithSuggestion(const AccidentSuggestion &suggestion);

    // ── Employés ─────────────────────────────────────────────────────────────
    Employe m_employe;
    QSqlQueryModel *m_employeModel = nullptr;
    bool m_employeModeModification = false;
    void initEmployePage();
    void employe_ajouterEmploye();
    void employe_modifierEmploye();

    // ── Arduino RFID — Variables privées ─────────────────────────────────────
    int     m_arduinoSmsCode = 0;           // Code SMS pour authentification
    int     m_currentEmployeeId = 0;        // ID employé en cours de vérification
    QString m_currentRfidCode;              // Code RFID en cours de traitement
    bool    m_smsAuthSuccess = false;       // Flag pour savoir si l'authentification SMS a réussi

    // ── Arduino Quai ─────────────────────────────────────────────────────────
    Arduino m_arduino;  // objet Arduino pour la connexion série (module quai)

    // ── Arduino Bateau — méthodes privées ────────────────────────────────────
    void initArduinoPage();          // build the page and wire it up
    void ard_addLog(const QString &msg, const QString &color = "#0D47A1");
    void ard_addDbEvent(const QString &msg, const QString &color = "#1B5E20");
    void ard_handleMotion();         // full business logic
    void ard_refreshBoatTable();     // refresh the DB view on the monitor

    // ── Arduino Bateau — membres privés ──────────────────────────────────────
    Arduino        *m_ard            = nullptr;  // serial port wrapper
    int             m_ardCurrentBoat = -1;       // selected boat id
    QString         m_ardBuffer;                 // partial read buffer

    QWidget        *m_ardPage        = nullptr;
    QComboBox      *m_ardComboBoat   = nullptr;
    QLineEdit      *m_ardLePort      = nullptr;
    QPushButton    *m_ardBtnConnect  = nullptr;
    QPushButton    *m_ardBtnScan     = nullptr;
    QLabel         *m_ardLblState    = nullptr;
    QLabel         *m_ardLblDetected = nullptr;
    QLabel         *m_ardLblStatus   = nullptr;
    QTextEdit      *m_ardLogSerial   = nullptr;  // raw serial log
    QTextEdit      *m_ardLogDb       = nullptr;  // DB events log
    QTableView     *m_ardTableView   = nullptr;  // live DB view
    QSqlQueryModel *m_ardModel       = nullptr;
};

#endif // FISHINGPORT_H
