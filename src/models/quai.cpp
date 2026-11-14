#include "quai.h"
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QPushButton>
#include <QLinearGradient>
#include <QEnterEvent>
#include <QTimer>
#include <QMessageBox>
#include <climits>

Quai::Quai()
{
    id_quais = 0;
    capacite = 0;
    longueur = 0;
}

Quai::Quai(int id, QString n, int c, double l, QString s)
{
    id_quais = id;
    nom = n;
    capacite = c;
    longueur = l;
    statusq = s;
}

bool Quai::ajouter()
{
    QSqlQuery query;
    query.prepare("INSERT INTO quais (id_quais, nom, capacite, longueur, statusq) "
                  "VALUES (:id, :nom, :capacite, :longueur, :status)");

    query.bindValue(":id", id_quais);
    query.bindValue(":nom", nom);
    query.bindValue(":capacite", capacite);
    query.bindValue(":longueur", longueur);
    query.bindValue(":status", statusq);

    return query.exec();
}

bool Quai::supprimer(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM quais WHERE id_quais = :id");
    query.bindValue(":id", id);

    return query.exec();
}

bool Quai::modifier(int id)
{
    QSqlQuery query;
    query.prepare("UPDATE quais SET nom=:nom, capacite=:capacite, longueur=:longueur, statusq=:status "
                  "WHERE id_quais=:id");

    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":capacite", capacite);
    query.bindValue(":longueur", longueur);
    query.bindValue(":status", statusq);

    return query.exec();
}

QSqlQueryModel* Quai::afficher()
{
    QSqlQueryModel* model = new QSqlQueryModel();
    model->setQuery("SELECT * FROM quais");
    return model;
}

QSqlQueryModel* Quai::chercher(QString colonne, QString valeur)
{
    QSqlQueryModel* model = new QSqlQueryModel();
    QSqlQuery query;

    if (colonne == "id_quais") {
        query.prepare("SELECT * FROM quais WHERE id_quais = :val");
        query.bindValue(":val", valeur.toInt());
    }
    else if (colonne == "capacite") {
        query.prepare("SELECT * FROM quais WHERE capacite = :val");
        query.bindValue(":val", valeur.toInt());
    }
    else if (colonne == "longueur") {
        query.prepare("SELECT * FROM quais WHERE longueur = :val");
        query.bindValue(":val", valeur.toDouble());
    }
    else {
        // nom, statusq  → recherche insensible à la casse avec LIKE
        query.prepare(QString("SELECT * FROM quais WHERE UPPER(%1) LIKE UPPER(:val)").arg(colonne));
        query.bindValue(":val", "%" + valeur + "%");
    }

    query.exec();
    model->setQuery(std::move(query));
    return model;
}

QSqlQueryModel* Quai::trier(QString colonne, QString ordre)
{
    QSqlQueryModel* model = new QSqlQueryModel();
    // colonne et ordre viennent uniquement de notre code → safe
    model->setQuery(QString("SELECT * FROM quais ORDER BY %1 %2").arg(colonne).arg(ordre));
    return model;
}

QMap<QString,int> Quai::getStatutCounts()
{
    QMap<QString,int> counts;

    QSqlQuery q;
    // UPPER() pour fusionner toutes les variantes de casse :
    // "Libre", "libre", "LIBRE"  → une seule entrée
    // "Occupé", "occupé", "occupe", "OCCUPE" → une seule entrée
    if (!q.exec("SELECT UPPER(TRIM(statusq)), COUNT(*) FROM quais GROUP BY UPPER(TRIM(statusq))")) {
        qWarning() << "getStatutCounts error:" << q.lastError().text();
        return counts;
    }
    while (q.next()) {
        QString s = q.value(0).toString().trimmed(); // déjà en MAJUSCULES
        int     n = q.value(1).toInt();
        if (s.isEmpty()) continue;

        // Normalisation vers la forme canonique affichée dans l'UI
        QString canonical;
        if (s == "LIBRE")
            canonical = "Libre";
        else if (s.startsWith("OCCUP"))   // OCCUPE, OCCUPÉ, OCCUPEE…
            canonical = "Occup\u00e9";
        else if (s == "MAINTENANCE" || s == "EN MAINTENANCE")
            canonical = "Maintenance";
        else {
            // Autre statut : mettre la première lettre en majuscule
            canonical = s.toLower();
            if (!canonical.isEmpty())
                canonical[0] = canonical[0].toUpper();
        }
        // += : additionne si plusieurs variantes tombent dans la même catégorie
        counts[canonical] += n;
    }
    // Garantir que les clés de base existent (même à 0) pour l'affichage
    if (!counts.contains("Libre"))    counts["Libre"]    = 0;
    if (!counts.contains("Occup\u00e9")) counts["Occup\u00e9"] = 0;
    return counts;
}

int Quai::prochainId()
{
    QSqlQuery query;
    query.exec("SELECT NVL(MAX(id_quais), 0) + 1 FROM quais");
    if (query.next())
        return query.value(0).toInt();
    return 1; // fallback si table vide
}

// =============================================================
//  Arduino Quai — getStatusQuai
//  Retourne le statusq du quai ou une chaine vide si introuvable
// =============================================================
QString Quai::getStatusQuai(int id)
{
    QSqlQuery q;
    q.prepare("SELECT statusq FROM quais WHERE id_quais = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return q.value(0).toString().trimmed();
    return QString();  // vide = quai introuvable
}

// =============================================================
//  Export Excel  (format CSV – s'ouvre directement dans Excel)
// =============================================================
bool Quai::exporterExcel(const QString &cheminFichier)
{
    QFile fichier(cheminFichier);
    if (!fichier.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&fichier);
    out.setEncoding(QStringConverter::Utf8);

    // En-têtes
    out << "ID Quai;Nom;Capacite;Longueur;Status\n";

    // Données
    QSqlQuery q;
    q.exec("SELECT id_quais, nom, capacite, longueur, statusq FROM quais ORDER BY id_quais");
    while (q.next()) {
        out << q.value(0).toString() << ";"
            << q.value(1).toString() << ";"
            << q.value(2).toString() << ";"
            << q.value(3).toString() << ";"
            << q.value(4).toString() << "\n";
    }

    fichier.close();
    return true;
}

// =============================================================
//  Export PDF  (tableau HTML imprimé via QPrinter)
// =============================================================
bool Quai::exporterPDF(const QString &cheminFichier)
{
    // Construire le tableau HTML
    QString html;
    html += "<html><head><meta charset='utf-8'>"
            "<style>"
            "body { font-family: Arial, sans-serif; font-size: 12pt; }"
            "h2   { color: #0277BD; text-align: center; }"
            "table{ width:100%; border-collapse:collapse; margin-top:16px; }"
            "th   { background:#0288D1; color:white; padding:8px; border:1px solid #90CAF9; }"
            "td   { padding:7px; border:1px solid #B3E5FC; }"
            "tr:nth-child(even){ background:#E1F5FE; }"
            "</style></head><body>"
            "<h2>Liste des Quais – Port de Pêche</h2>"
            "<table>"
            "<tr><th>ID Quai</th><th>Nom</th><th>Capacité</th><th>Longueur (m)</th><th>Status</th></tr>";

    QSqlQuery q;
    q.exec("SELECT id_quais, nom, capacite, longueur, statusq FROM quais ORDER BY id_quais");
    while (q.next()) {
        QString couleur = (q.value(4).toString().toLower() == "libre") ? "#C8E6C9" : "#FFCDD2";
        // Note : on utilise bgcolor HTML (supporté par QTextDocument) plutôt que le style CSS inline sur <tr>
        html += QString("<tr bgcolor='%6'>"
                        "<td align='center'>%1</td><td>%2</td>"
                        "<td align='center'>%3</td><td align='center'>%4</td>"
                        "<td align='center'>%5</td></tr>")
                    .arg(q.value(0).toString())
                    .arg(q.value(1).toString())
                    .arg(q.value(2).toString())
                    .arg(q.value(3).toString())
                    .arg(q.value(4).toString())
                    .arg(couleur);
    }
    html += "</table></body></html>";

    // Impression vers PDF
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(cheminFichier);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Landscape);

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);

    return true;
}

// =============================================================
//  Métier 1 : getNomQuai + getEmplacements
// =============================================================
QString Quai::getNomQuai(int id)
{
    QSqlQuery q;
    q.prepare("SELECT nom FROM quais WHERE id_quais = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QString();
}

QVector<Quai::EmplacementInfo> Quai::getEmplacements(int id)
{
    QVector<EmplacementInfo> liste;
    QSqlQuery q;
    q.prepare("SELECT id_emplacement, numero, etatE, typeP_emplacement "
              "FROM emplacement WHERE id_quais = :id ORDER BY numero");
    q.bindValue(":id", id);
    if (q.exec()) {
        while (q.next()) {
            EmplacementInfo e;
            e.id     = q.value(0).toInt();
            e.numero = q.value(1).toInt();
            e.etat   = q.value(2).toString().trimmed().toLower();
            e.typeP  = q.value(3).toString().trimmed().toLower();
            liste.append(e);
        }
    }
    return liste;
}

// =============================================================
//  Métier 2 : getBateauType
// =============================================================
QString Quai::getBateauType(int carte_grise)
{
    QSqlQuery q;
    q.prepare("SELECT type FROM bateau WHERE numero_carte_grise = :id");
    q.bindValue(":id", carte_grise);
    if (q.exec() && q.next())
        return q.value(0).toString().trimmed().toLower();
    return QString();
}

// =============================================================
//  Tuile d'emplacement (supporte mise en surbrillance Metier 2)
// =============================================================
class EmplacementTile : public QFrame
{
public:
    EmplacementTile(int numero, const QString &etat,
                    bool highlighted = false, QWidget *parent = nullptr)
        : QFrame(parent), m_numero(numero), m_etat(etat),
          m_highlighted(highlighted), m_pulseOn(true), m_timer(nullptr)
    {
        setFixedSize(116, 96);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QString("Emplacement #%1  |  Etat : %2%3")
                   .arg(numero).arg(etat)
                   .arg(highlighted ? "  [Place proposee]" : ""));
        if (m_highlighted) {
            m_timer = new QTimer(this);
            connect(m_timer, &QTimer::timeout, this, [this]() {
                m_pulseOn = !m_pulseOn;
                update();
            });
            m_timer->start(500);
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor bg, border, textColor;
        if (m_etat == "libre") {
            bg = QColor("#4CAF50"); border = QColor("#2E7D32"); textColor = Qt::white;
        } else if (m_etat == "occupe" || m_etat == QString("occup") + "\xc3\xa9") {
            bg = QColor("#E24B4A"); border = QColor("#B71C1C"); textColor = Qt::white;
        } else {
            bg = QColor("#EF9F27"); border = QColor("#E65100"); textColor = Qt::white;
        }

        // Halo pulsant violet si mis en surbrillance
        if (m_highlighted) {
            int hw = m_pulseOn ? 5 : 2;
            QColor halo("#9C27B0");
            halo.setAlpha(m_pulseOn ? 220 : 90);
            p.setPen(QPen(halo, hw));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect().adjusted(hw/2+1, hw/2+1, -(hw/2+1), -(hw/2+1)), 16, 16);
        }

        int m = m_highlighted ? 7 : 3;
        QRectF r = rect().adjusted(m, m, -m, -m);
        QLinearGradient grad(0, 0, 0, height());
        grad.setColorAt(0, bg.lighter(115));
        grad.setColorAt(1, bg);
        p.setBrush(grad);
        p.setPen(QPen(m_highlighted ? QColor("#9C27B0") : border,
                      m_highlighted ? (m_pulseOn ? 3.0 : 1.5) : 2.0));
        p.drawRoundedRect(r, 14, 14);

        // Numero
        QFont f("Segoe UI", 18, QFont::Bold);
        p.setFont(f);
        p.setPen(textColor);
        QRectF numRect = r.adjusted(0, 0, 0, m_highlighted ? -22 : -20);
        p.drawText(numRect, Qt::AlignCenter, QString::number(m_numero));

        // Etiquette bas
        if (m_highlighted) {
            QFont fs("Segoe UI", 7, QFont::Bold);
            p.setFont(fs);
            QRectF tagR(r.left()+4, r.bottom()-20, r.width()-8, 17);
            p.setBrush(QColor(0x4A, 0x14, 0x8C, 180));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(tagR, 4, 4);
            p.setPen(QColor("#F3E5F5"));
            p.drawText(tagR, Qt::AlignCenter, "Propose");
        } else {
            QFont fSmall("Segoe UI", 7);
            p.setFont(fSmall);
            QColor tagBg = border; tagBg.setAlpha(180);
            QRectF tagRect(r.left()+6, r.bottom()-18, r.width()-12, 15);
            p.setBrush(tagBg); p.setPen(Qt::NoPen);
            p.drawRoundedRect(tagRect, 4, 4);
            p.setPen(Qt::white);
            QString ea = m_etat;
            if (!ea.isEmpty()) ea[0] = ea[0].toUpper();
            p.drawText(tagRect, Qt::AlignCenter, ea);
        }
    }

    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *)      override { update(); }

private:
    int     m_numero;
    QString m_etat;
    bool    m_highlighted;
    bool    m_pulseOn;
    QTimer *m_timer;
};

// =============================================================
//  EmplacementDialog – implémentation
// =============================================================
EmplacementDialog::EmplacementDialog(const QString &nomQuai,
                                     int idQuai,
                                     const QVector<Quai::EmplacementInfo> &emplacements,
                                     int highlightId,
                                     const QString &bandeauMsg,
                                     bool bandeauError,
                                     QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Quai \"%1\"  -  ID %2").arg(nomQuai).arg(idQuai));
    setMinimumSize(700, 560);
    setModal(true);

    // Style
    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1A237E, stop:1 #283593);
        }
        QLabel#lblTitre {
            color: white; font-size: 16px; font-weight: bold;
            font-family: "Segoe UI"; padding: 6px;
        }
        QLabel#lblCount {
            color: #90CAF9; font-size: 12px; font-family: "Segoe UI";
        }
        QScrollArea { border: none; background: transparent; }
        QWidget#scrollContents { background: transparent; }
        QFrame#legendFrame {
            background: rgba(255,255,255,0.08);
            border: 1px solid rgba(255,255,255,0.15);
            border-radius: 10px; padding: 6px;
        }
        QLabel#legendItem { color: white; font-size: 12px; font-family: "Segoe UI"; }
        QLabel#lblPropose {
            font-size: 13px; font-weight: bold; font-family: "Segoe UI";
            padding: 10px 16px; border-radius: 10px;
        }
        QPushButton#btnFermer {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #42A5F5, stop:1 #1976D2);
            color: white; border: none; border-radius: 8px;
            padding: 8px 28px; font-weight: bold; font-size: 13px;
        }
        QPushButton#btnFermer:hover { background: #64B5F6; }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(10);

    // En-tete
    QLabel *lblTitre = new QLabel(QString("%1  -  ID : %2").arg(nomQuai).arg(idQuai));
    lblTitre->setObjectName("lblTitre");
    mainLayout->addWidget(lblTitre);

    int total = emplacements.size();
    int libres = 0, occupes = 0, maint = 0;
    for (auto &e : emplacements) {
        if (e.etat == "libre")                              ++libres;
        else if (e.etat == "occupe" || e.etat == "occupé") ++occupes;
        else                                                ++maint;
    }
    QLabel *lblCount = new QLabel(
        QString("Total : %1  |  Libres : %2  |  Occupes : %3  |  Maintenance : %4")
        .arg(total).arg(libres).arg(occupes).arg(maint));
    lblCount->setObjectName("lblCount");
    mainLayout->addWidget(lblCount);

    // Zone scrollable
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget *contents = new QWidget();
    contents->setObjectName("scrollContents");
    QGridLayout *grid = new QGridLayout(contents);
    grid->setSpacing(10);
    grid->setContentsMargins(14, 14, 14, 14);

    const int COLS = 4;
    for (int i = 0; i < emplacements.size(); ++i) {
        const auto &e = emplacements[i];
        bool isHighlighted = (e.id == highlightId);
        EmplacementTile *tile = new EmplacementTile(e.numero, e.etat, isHighlighted, contents);
        grid->addWidget(tile, i / COLS, i % COLS, Qt::AlignCenter);
    }
    int reste = emplacements.size() % COLS;
    if (reste != 0) {
        for (int i = reste; i < COLS; ++i) {
            QWidget *sp = new QWidget(contents);
            sp->setFixedSize(116, 96);
            grid->addWidget(sp, emplacements.size() / COLS, i);
        }
    }
    contents->setLayout(grid);
    scroll->setWidget(contents);
    mainLayout->addWidget(scroll, 1);

    // Legende
    QFrame *legendFrame = new QFrame(this);
    legendFrame->setObjectName("legendFrame");
    QHBoxLayout *legendLayout = new QHBoxLayout(legendFrame);
    legendLayout->setSpacing(20);
    auto makeLegendItem = [&](const QString &color, const QString &text) {
        QLabel *dot = new QLabel("o");
        dot->setStyleSheet(QString("color:%1; font-size:18px; font-weight:bold;").arg(color));
        QLabel *lbl = new QLabel(text);
        lbl->setObjectName("legendItem");
        legendLayout->addWidget(dot);
        legendLayout->addWidget(lbl);
    };
    makeLegendItem("#4CAF50", "Libre");
    makeLegendItem("#E24B4A", "Occupe");
    makeLegendItem("#EF9F27", "Maintenance");
    if (highlightId != -1)
        makeLegendItem("#9C27B0", "Place proposee");
    legendLayout->addStretch();
    mainLayout->addWidget(legendFrame);

    // Bandeau resultat (Metier 2)
    if (!bandeauMsg.isEmpty()) {
        QLabel *lblPropose = new QLabel(bandeauMsg);
        lblPropose->setObjectName("lblPropose");
        lblPropose->setAlignment(Qt::AlignCenter);
        lblPropose->setWordWrap(true);
        if (bandeauError) {
            lblPropose->setStyleSheet(
                "font-size:13px; font-weight:bold; font-family:'Segoe UI';"
                "padding:10px 16px; border-radius:10px;"
                "background:#B71C1C; color:white;");
        } else {
            lblPropose->setStyleSheet(
                "font-size:13px; font-weight:bold; font-family:'Segoe UI';"
                "padding:10px 16px; border-radius:10px;"
                "background:#4A148C; color:#E1BEE7;"
                "border: 2px solid #9C27B0;");
        }
        mainLayout->addWidget(lblPropose);
    }

    // Bouton Fermer
    QPushButton *btnFermer = new QPushButton("Fermer", this);
    btnFermer->setObjectName("btnFermer");
    connect(btnFermer, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(btnFermer);
    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}


// =============================================================
//  Metier 1 & 2 : getAllQuaisInfo
// =============================================================
QVector<Quai::QuaiInfo> Quai::getAllQuaisInfo()
{
    QVector<QuaiInfo> result;
    QSqlQuery q;
    if (!q.exec("SELECT id_quais, nom, capacite FROM quais ORDER BY id_quais")) {
        qWarning() << "getAllQuaisInfo: failed to query quais:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        QuaiInfo info;
        info.id       = q.value(0).toInt();
        info.nom      = q.value(1).toString().trimmed();
        info.capacite = q.value(2).toInt();
        info.total    = 0;
        info.libres   = 0;

        // Compter directement les emplacements libres et le total
        // (séparer "libre" de "occupé" et "maintenance" correctement)
        QSqlQuery q2;
        q2.prepare(
            "SELECT COUNT(*), "
            "SUM(CASE WHEN LOWER(TRIM(etatE)) = 'libre' THEN 1 ELSE 0 END) "
            "FROM emplacement WHERE id_quais = :id");
        q2.bindValue(":id", info.id);
        if (q2.exec() && q2.next()) {
            info.total  = q2.value(0).toInt();
            info.libres = q2.value(1).toInt();   // nb emplacements réellement libres
        }
        result.append(info);
    }
    return result;
}

// =============================================================
//  Metier 2 : occupierPremierEmplacement
// =============================================================
bool Quai::occupierPremierEmplacement(int id_quais)
{
    // Mise a jour du premier emplacement libre du quai
    // Note : on utilise 'Occupé' (avec accent) pour rester cohérent avec
    //        les valeurs écrites par le module Emplacement.
    QSqlQuery q;
    q.prepare(
        "UPDATE emplacement SET etatE = 'Occupé' "
        "WHERE id_emplacement = ("
        "  SELECT MIN(id_emplacement) FROM emplacement "
        "  WHERE id_quais = :id AND LOWER(TRIM(etatE)) = 'libre'"
        ")");
    q.bindValue(":id", id_quais);
    if (!q.exec()) {
        qWarning() << "occupierPremierEmplacement:" << q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() <= 0) return false;
    QSqlDatabase::database().commit();
    return true;
}

// =============================================================
//  QuaiTile — tuile graphique d'un quai (cliquable)
// =============================================================
class QuaiTile : public QFrame
{
public:
    QuaiTile(const Quai::QuaiInfo &info,
             std::function<void(int)> onClick,
             QWidget *parent = nullptr)
        : QFrame(parent)
        , m_id(info.id), m_nom(info.nom)
        , m_total(info.total), m_libres(info.libres)
        , m_clickable(info.libres > 0)
        , m_selected(false), m_blinkOn(true)
        , m_onClick(onClick)
    {
        setFixedSize(140, 110);
        setCursor(m_clickable ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        updateTooltip();
    }

    // Appele par le dialog pour mettre a jour l'etat
    void updateState(int libres, int total) {
        m_libres   = libres;
        m_total    = total;
        m_clickable = (libres > 0);
        setCursor(m_clickable ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        updateTooltip();
        update();
    }

    void setSelected(bool s)  { m_selected = s; update(); }
    void toggleBlink()        { m_blinkOn = !m_blinkOn; update(); }
    int  quaiId() const       { return m_id; }
    bool isClickable() const  { return m_clickable; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Couleur selon le nombre de places libres
        QColor bg, border;
        QString statusText;
        if (m_libres == 0) {
            bg = QColor("#C62828"); border = QColor("#8E0000");
            statusText = "Sature";
        } else if (m_libres == 1) {
            bg = QColor("#F9A825"); border = QColor("#E65100");
            statusText = "1 place";
        } else {
            bg = QColor("#2E7D32"); border = QColor("#1B5E20");
            statusText = QString("%1 places").arg(m_libres);
        }

        // Halo clignotant si selectionne
        if (m_selected && m_blinkOn) {
            p.setPen(QPen(QColor("#FFD600"), 5));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect().adjusted(3,3,-3,-3), 16, 16);
        }

        // Fond
        int m = m_selected ? 8 : 3;
        QRectF r = rect().adjusted(m, m, -m, -m);
        QLinearGradient grad(0, 0, 0, height());
        grad.setColorAt(0, bg.lighter(120));
        grad.setColorAt(1, bg);
        p.setBrush(grad);
        p.setPen(QPen(border, m_selected ? 2.5 : 1.5));
        p.drawRoundedRect(r, 14, 14);

        // Nom du quai
        QFont fNom("Segoe UI", 9, QFont::Bold);
        p.setFont(fNom);
        p.setPen(Qt::white);
        QRectF nomRect = r.adjusted(4, 6, -4, -(r.height()/2));
        p.drawText(nomRect, Qt::AlignCenter | Qt::TextWordWrap, m_nom);

        // ID petit
        QFont fId("Segoe UI", 7);
        p.setFont(fId);
        p.setPen(QColor(255,255,255,160));
        QRectF idRect = r.adjusted(4, r.height()*0.45, -4, -r.height()*0.32);
        p.drawText(idRect, Qt::AlignCenter, QString("ID %1").arg(m_id));

        // Badge etat en bas
        QColor badgeBg = border; badgeBg.setAlpha(200);
        QRectF tagR(r.left()+6, r.bottom()-22, r.width()-12, 18);
        p.setBrush(badgeBg); p.setPen(Qt::NoPen);
        p.drawRoundedRect(tagR, 5, 5);
        QFont fTag("Segoe UI", 8, QFont::Bold);
        p.setFont(fTag);
        p.setPen(Qt::white);
        p.drawText(tagR, Qt::AlignCenter, statusText);

        // Indicateur "clignotant" si selectionne
        if (m_selected) {
            QFont fSel("Segoe UI", 7, QFont::Bold);
            p.setFont(fSel);
            p.setPen(QColor("#FFD600"));
            p.drawText(r.adjusted(0,-r.height()*0.1,0,-r.height()*0.75),
                       Qt::AlignCenter, "PROPOSE");
        }
    }

    void mousePressEvent(QMouseEvent *ev) override {
        if (ev->button() == Qt::LeftButton && m_clickable && m_selected)
            m_onClick(m_id);
    }

    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *)      override { update(); }

private:
    void updateTooltip() {
        setToolTip(QString("Quai : %1\nID : %2\nCapacite : %3 emplacements\nLibres : %4 / %5")
                   .arg(m_nom).arg(m_id).arg(m_total).arg(m_libres).arg(m_total));
    }

    int     m_id;
    QString m_nom;
    int     m_total, m_libres;
    bool    m_clickable, m_selected, m_blinkOn;
    std::function<void(int)> m_onClick;
};

// =============================================================
//  QuaisVisuDialog — Metier 1 : visualisation etat de tous les quais
// =============================================================
QuaisVisuDialog::QuaisVisuDialog(const QVector<Quai::QuaiInfo> &quais,
                                 QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Etat des Quais du Port");
    setMinimumSize(760, 520);
    setModal(true);

    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #0D47A1, stop:1 #1565C0);
        }
        QLabel#lblTitre {
            color: white; font-size: 17px; font-weight: bold;
            font-family: "Segoe UI"; padding: 6px 0;
        }
        QLabel#lblStats {
            color: #90CAF9; font-size: 12px; font-family: "Segoe UI";
        }
        QScrollArea  { border: none; background: transparent; }
        QWidget#sc   { background: transparent; }
        QFrame#legFr {
            background: rgba(255,255,255,0.07);
            border: 1px solid rgba(255,255,255,0.15);
            border-radius: 10px; padding: 6px;
        }
        QLabel#legIt { color: white; font-size: 12px; font-family:"Segoe UI"; }
        QPushButton#btnClose {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #42A5F5, stop:1 #1976D2);
            color: white; border: none; border-radius: 8px;
            padding: 8px 28px; font-weight: bold; font-size: 13px;
        }
        QPushButton#btnClose:hover { background: #64B5F6; }
    )");

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(22, 18, 22, 18);
    main->setSpacing(10);

    // Titre
    QLabel *lblTitre = new QLabel("Visualisation de l'etat des Quais");
    lblTitre->setObjectName("lblTitre");
    main->addWidget(lblTitre);

    // Stats globales
    int nVert = 0, nJaune = 0, nRouge = 0;
    for (const auto &q : quais) {
        if (q.libres == 0)      ++nRouge;
        else if (q.libres == 1) ++nJaune;
        else                    ++nVert;
    }
    QLabel *lblStats = new QLabel(
        QString("Total : %1 quais   |   Disponibles (vert) : %2   |   "
                "Presque pleins (jaune) : %3   |   Satures (rouge) : %4")
        .arg(quais.size()).arg(nVert).arg(nJaune).arg(nRouge));
    lblStats->setObjectName("lblStats");
    main->addWidget(lblStats);

    // Grille de tuiles (lecture seule)
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget *sc = new QWidget(); sc->setObjectName("sc");
    QGridLayout *grid = new QGridLayout(sc);
    grid->setSpacing(14);
    grid->setContentsMargins(16,16,16,16);

    const int COLS = 4;
    for (int i = 0; i < quais.size(); ++i) {
        // Tuile en mode lecture seule (onClick vide = non-cliquable)
        QuaiTile *tile = new QuaiTile(quais[i], [](int){}, sc);
        grid->addWidget(tile, i/COLS, i%COLS, Qt::AlignCenter);
    }
    // Espaces de remplissage
    int reste = quais.size() % COLS;
    if (reste) {
        for (int i = reste; i < COLS; ++i) {
            QWidget *sp = new QWidget(sc); sp->setFixedSize(140,110);
            grid->addWidget(sp, quais.size()/COLS, i);
        }
    }
    sc->setLayout(grid);
    scroll->setWidget(sc);
    main->addWidget(scroll, 1);

    // Legende
    QFrame *legFr = new QFrame(this); legFr->setObjectName("legFr");
    QHBoxLayout *legL = new QHBoxLayout(legFr);
    legL->setSpacing(20);
    auto addLeg = [&](const QString &col, const QString &txt) {
        QLabel *d = new QLabel("o");
        d->setStyleSheet(QString("color:%1;font-size:18px;font-weight:bold;").arg(col));
        QLabel *l = new QLabel(txt); l->setObjectName("legIt");
        legL->addWidget(d); legL->addWidget(l);
    };
    addLeg("#2E7D32", "Disponible (>= 2 places)");
    addLeg("#F9A825", "Presque plein (1 place)");
    addLeg("#C62828", "Sature (0 place)");
    legL->addStretch();
    main->addWidget(legFr);

    // Bouton fermer
    QPushButton *btn = new QPushButton("Fermer", this);
    btn->setObjectName("btnClose");
    connect(btn, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout *bL = new QHBoxLayout();
    bL->addStretch(); bL->addWidget(btn);
    main->addLayout(bL);

    setLayout(main);
}

// =============================================================
//  QuaisAffectDialog — Metier 2 : affectation interactive
// =============================================================

// findBestId : moins de places libres (> 0), tie-break : capacite la plus petite
int QuaisAffectDialog::findBestId() const
{
    int bestId = -1, bestLibres = INT_MAX, bestCap = INT_MAX;
    for (const auto &q : m_quais) {
        if (q.libres <= 0) continue;        // rouge -> ignore
        if (q.libres < bestLibres
            || (q.libres == bestLibres && q.capacite < bestCap)) {
            bestLibres = q.libres;
            bestCap    = q.capacite;
            bestId     = q.id;
        }
    }
    return bestId;
}

void QuaisAffectDialog::updateStatusLabel()
{
    if (m_remaining <= 0)
        m_lblStatus->setText("Tous les bateaux ont ete places !");
    else
        m_lblStatus->setText(
            QString("Bateaux a placer : %1  —  Cliquez sur le quai propose (clignotant en jaune)")
            .arg(m_remaining));
}

void QuaisAffectDialog::refreshHighlight()
{
    // Stopper le timer existant
    if (m_blinkTimer->isActive()) m_blinkTimer->stop();

    // Deselectionner tous
    for (auto *t : m_tiles) t->setSelected(false);

    m_bestId = findBestId();
    if (m_bestId == -1) {
        // Plus de quai disponible
        updateStatusLabel();
        m_btnClose->setEnabled(true);
        return;
    }

    // Selectionner le meilleur
    m_tiles[m_bestId]->setSelected(true);
    m_blinkTimer->start(400);
    updateStatusLabel();
}

void QuaisAffectDialog::onBlink()
{
    if (m_tiles.contains(m_bestId))
        m_tiles[m_bestId]->toggleBlink();
}

void QuaisAffectDialog::onTileClicked(int idQuai)
{
    if (idQuai != m_bestId) return;   // securite

    // Mise a jour base de donnees
    Quai tmpQ;
    if (!tmpQ.occupierPremierEmplacement(idQuai)) {
        QMessageBox::critical(this, "Erreur BD",
            "Impossible de mettre a jour l'emplacement dans la base.");
        return;
    }

    // Mise a jour locale de m_quais
    for (auto &q : m_quais) {
        if (q.id == idQuai) {
            q.libres = qMax(0, q.libres - 1);
            // Mettre a jour la tuile
            QuaiTile *tile = m_tiles[idQuai];
            tile->setSelected(false);
            m_blinkTimer->stop();

            if (q.libres == 0) {
                // Devenu rouge : mise a jour visuelle puis masquer apres 1.2 s
                tile->updateState(0, q.total);
                QTimer::singleShot(1200, tile, [tile]() { tile->hide(); });
            } else {
                tile->updateState(q.libres, q.total);
            }
            break;
        }
    }

    --m_remaining;
    if (m_remaining <= 0) {
        m_blinkTimer->stop();
        updateStatusLabel();
        m_btnClose->setEnabled(true);
    } else {
        refreshHighlight();
    }
}

QuaisAffectDialog::QuaisAffectDialog(int nbBateaux,
                                     QVector<Quai::QuaiInfo> quais,
                                     QWidget *parent)
    : QDialog(parent)
    , m_remaining(nbBateaux)
    , m_quais(quais)
    , m_lblStatus(nullptr)
    , m_blinkTimer(new QTimer(this))
    , m_bestId(-1)
    , m_blinkOn(true)
    , m_btnClose(nullptr)
{
    setWindowTitle("Attribution automatique de places");
    setMinimumSize(780, 560);
    setModal(true);

    connect(m_blinkTimer, &QTimer::timeout, this, &QuaisAffectDialog::onBlink);

    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #1A237E, stop:1 #283593);
        }
        QLabel#lblTitre {
            color: white; font-size: 16px; font-weight: bold;
            font-family: "Segoe UI"; padding: 6px 0;
        }
        QLabel#lblStatus {
            color: #FFD600; font-size: 12px; font-family: "Segoe UI";
            background: rgba(0,0,0,0.25); border-radius: 8px; padding: 6px 12px;
        }
        QScrollArea { border: none; background: transparent; }
        QWidget#sc  { background: transparent; }
        QFrame#legFr {
            background: rgba(255,255,255,0.07);
            border: 1px solid rgba(255,255,255,0.15);
            border-radius: 10px; padding: 6px;
        }
        QLabel#legIt { color: white; font-size: 12px; font-family:"Segoe UI"; }
        QPushButton#btnClose {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #66BB6A, stop:1 #388E3C);
            color: white; border: none; border-radius: 8px;
            padding: 8px 28px; font-weight: bold; font-size: 13px;
        }
        QPushButton#btnClose:disabled { background: #546E7A; color: #90A4AE; }
        QPushButton#btnClose:hover:enabled { background: #81C784; }
    )");

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(22,18,22,18);
    main->setSpacing(10);

    // Titre
    QLabel *lblTitre = new QLabel(
        QString("Attribution de %1 bateau(x) dans le port").arg(nbBateaux));
    lblTitre->setObjectName("lblTitre");
    main->addWidget(lblTitre);

    // Label status
    m_lblStatus = new QLabel(); m_lblStatus->setObjectName("lblStatus");
    main->addWidget(m_lblStatus);

    // Grille de tuiles cliquables
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget *sc = new QWidget(); sc->setObjectName("sc");
    QGridLayout *grid = new QGridLayout(sc);
    grid->setSpacing(14);
    grid->setContentsMargins(16,16,16,16);

    const int COLS = 4;
    int i = 0;
    for (const auto &qi : m_quais) {
        QuaiTile *tile = new QuaiTile(qi,
            [this](int id) { this->onTileClicked(id); },
            sc);
        m_tiles[qi.id] = tile;
        grid->addWidget(tile, i/COLS, i%COLS, Qt::AlignCenter);
        ++i;
    }
    int reste = m_quais.size() % COLS;
    if (reste) {
        for (int j = reste; j < COLS; ++j) {
            QWidget *sp = new QWidget(sc); sp->setFixedSize(140,110);
            grid->addWidget(sp, m_quais.size()/COLS, j);
        }
    }
    sc->setLayout(grid);
    scroll->setWidget(sc);
    main->addWidget(scroll, 1);

    // Legende
    QFrame *legFr = new QFrame(this); legFr->setObjectName("legFr");
    QHBoxLayout *legL = new QHBoxLayout(legFr);
    legL->setSpacing(18);
    auto addLeg = [&](const QString &col, const QString &txt) {
        QLabel *d = new QLabel("o");
        d->setStyleSheet(QString("color:%1;font-size:18px;font-weight:bold;").arg(col));
        QLabel *l = new QLabel(txt); l->setObjectName("legIt");
        legL->addWidget(d); legL->addWidget(l);
    };
    addLeg("#2E7D32", "Disponible");
    addLeg("#F9A825", "Presque plein");
    addLeg("#C62828", "Sature");
    addLeg("#FFD600", "PROPOSE (cliquer)");
    legL->addStretch();
    main->addWidget(legFr);

    // Bouton fermer (desactive jusqu'a la fin)
    m_btnClose = new QPushButton("Terminer", this);
    m_btnClose->setObjectName("btnClose");
    m_btnClose->setEnabled(false);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout *bL = new QHBoxLayout();
    bL->addStretch(); bL->addWidget(m_btnClose);
    main->addLayout(bL);

    setLayout(main);

    // Lancer la premiere mise en surbrillance
    refreshHighlight();
}

// =============================================================
//  StatsDialog – implémentation
// =============================================================
StatsDialog::StatsDialog(QMap<QString,int> counts, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Statistiques des Quais");
    setMinimumSize(560, 520);
    setModal(true);

    // ── Calcul du total ──────────────────────────────────────
    int total = 0;
    for (auto v : counts) total += v;

    // Palette de couleurs automatique selon le statut
    auto couleurPour = [](const QString &statut) -> QColor {
        QString sl = statut.trimmed().toLower();
        if (sl == "libre")                          return QColor("#4CAF50");
        if (sl.startsWith("occup"))                 return QColor("#EF5350");
        if (sl == "maintenance" || sl == "en maintenance") return QColor("#FF9800");
        if (sl == "en mer")                         return QColor("#2196F3");
        if (sl == "en reparation")                  return QColor("#9C27B0");
        // Couleur par défaut pour tout autre statut
        static int idx = 0;
        static QList<QColor> palette = {
            QColor("#00BCD4"), QColor("#8BC34A"), QColor("#FF5722"),
            QColor("#607D8B"), QColor("#795548"), QColor("#E91E63")
        };
        return palette[(idx++) % palette.size()];
    };

    // ── Série camembert ─────────────────────────────────────
    QPieSeries *series = new QPieSeries();
    series->setHoleSize(0.35);

    // Trouver le statut le plus représenté pour l'exploser
    QString maxKey;
    int maxVal = -1;
    for (auto it = counts.begin(); it != counts.end(); ++it)
        if (it.value() > maxVal) { maxVal = it.value(); maxKey = it.key(); }

    for (auto it = counts.begin(); it != counts.end(); ++it) {
        if (it.value() <= 0) continue;
        int pct = (total > 0) ? 100 * it.value() / total : 0;
        QString label = QString("%1  %2 (%3%)")
                            .arg(it.key()).arg(it.value()).arg(pct);
        QPieSlice *sl = series->append(label, it.value());
        sl->setColor(couleurPour(it.key()));
        sl->setBorderColor(Qt::white);
        sl->setBorderWidth(3);
        sl->setLabelVisible(true);
        sl->setLabelFont(QFont("Segoe UI", 10, QFont::Bold));
        if (it.key() == maxKey)
            sl->setExploded(true);
    }

    // ── QChart ───────────────────────────────────────────────
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QString("Répartition des Quais  (Total : %1)").arg(total));
    chart->setTitleFont(QFont("Segoe UI", 13, QFont::Bold));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 10));
    chart->setAnimationOptions(QChart::AllAnimations);
    chart->setBackgroundBrush(QBrush(Qt::transparent));

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet("QChartView { background: #EBF5FB; border-radius: 12px; }");

    // ── Tableau récapitulatif ────────────────────────────────
    // Un bandeau par statut avec sa couleur
    QWidget *resumeWidget = new QWidget();
    resumeWidget->setStyleSheet("background: transparent;");
    QHBoxLayout *resumeLayout = new QHBoxLayout(resumeWidget);
    resumeLayout->setSpacing(10);
    resumeLayout->setContentsMargins(0, 0, 0, 0);

    for (auto it = counts.begin(); it != counts.end(); ++it) {
        QColor col = couleurPour(it.key());
        int    pct = (total > 0) ? 100 * it.value() / total : 0;

        QFrame *card = new QFrame();
        card->setStyleSheet(QString(
            "QFrame { background: %1; border-radius: 10px; }"
        ).arg(col.lighter(170).name()));
        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 8, 10, 8);
        cl->setSpacing(2);

        QLabel *lVal = new QLabel(QString::number(it.value()));
        lVal->setAlignment(Qt::AlignCenter);
        lVal->setStyleSheet(QString("font-size: 22px; font-weight: bold; color: %1; border: none; background: transparent;")
                                .arg(col.darker(150).name()));

        QLabel *lNom = new QLabel(it.key() + QString("  (%1%)").arg(pct));
        lNom->setAlignment(Qt::AlignCenter);
        lNom->setStyleSheet(QString("font-size: 11px; font-weight: bold; color: %1; border: none; background: transparent;")
                                .arg(col.darker(170).name()));

        cl->addWidget(lVal);
        cl->addWidget(lNom);
        resumeLayout->addWidget(card);
    }

    // Carte Total
    QFrame *cardTotal = new QFrame();
    cardTotal->setStyleSheet(
        "QFrame { background: #E3F2FD; border-radius: 10px; border: 2px solid #90CAF9; }");
    QVBoxLayout *ct = new QVBoxLayout(cardTotal);
    ct->setContentsMargins(10, 8, 10, 8);
    ct->setSpacing(2);
    QLabel *lTotalVal = new QLabel(QString::number(total));
    lTotalVal->setAlignment(Qt::AlignCenter);
    lTotalVal->setStyleSheet("font-size: 22px; font-weight: bold; color: #1565C0; border: none; background: transparent;");
    QLabel *lTotalNom = new QLabel("Total  (100%)");
    lTotalNom->setAlignment(Qt::AlignCenter);
    lTotalNom->setStyleSheet("font-size: 11px; font-weight: bold; color: #0D47A1; border: none; background: transparent;");
    ct->addWidget(lTotalVal);
    ct->addWidget(lTotalNom);
    resumeLayout->addWidget(cardTotal);

    // ── Layout principal ─────────────────────────────────────
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(resumeWidget);
    layout->addWidget(chartView, 1);
    setLayout(layout);
}

// =============================================================
//  Méthodes d'intégration  (appelées depuis fishingport.cpp)
// =============================================================

// Charge tous les quais dans un QSqlQueryModel existant
void Quai::loadTable(QSqlQueryModel *model)
{
    if (!model) return;
    // Aliases sans accents ni caractères spéciaux pour compatibilité Oracle
    model->setQuery(
        "SELECT id_quais  AS ID,"
        "       nom       AS Nom,"
        "       capacite  AS Capacite,"
        "       longueur  AS Longueur,"
        "       statusq   AS Status"
        " FROM quais ORDER BY id_quais");
}

// Ouvre le dialog Métier 1 (visualisation des quais)
void Quai::visualiserQuais(QWidget *parent)
{
    QVector<QuaiInfo> infos = getAllQuaisInfo();
    if (infos.isEmpty()) {
        QMessageBox::information(parent, "Visualiser les Quais",
            "Aucun quai trouvé dans la base de données.");
        return;
    }
    QuaisVisuDialog dlg(infos, parent);
    dlg.exec();
}

// Ouvre le dialog Métier 2 (affectation automatique de bateaux)
void Quai::lancerAffectationAutomatique(int nbBateaux, QWidget *parent)
{
    QVector<QuaiInfo> infos = getAllQuaisInfo();
    if (infos.isEmpty()) {
        QMessageBox::information(parent, "Affectation Automatique",
            "Aucun quai trouvé dans la base de données.");
        return;
    }

    // Filtrer les quais saturés (libres == 0) : ils ne doivent pas apparaître
    QVector<QuaiInfo> infosDispo;
    for (const auto &q : infos)
        if (q.libres > 0)
            infosDispo.append(q);

    if (infosDispo.isEmpty()) {
        QMessageBox::information(parent, "Affectation Automatique",
            "Tous les quais sont saturés.\n"
            "Aucune place disponible pour affecter des bateaux.");
        return;
    }

    QuaisAffectDialog dlg(nbBateaux, infosDispo, parent);
    dlg.exec();
}
