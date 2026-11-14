#include "fishingport.h"
#include "bateau.h"
#include "emplacement.h"
#include "employe.h"
#include "quai.h"
#include "sortie.h"
#include "ui_fishingport.h"
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGridLayout>
#include <QInputDialog>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPrinter>
#include <QPropertyAnimation>
#include <QProgressBar>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <algorithm>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>

namespace {

QString sortieEtatLabel(const QString &etat) {
  if (etat == "confirme")
    return "confirmee";
  if (etat == "prevue")
    return "prevue";
  if (etat == "retardee")
    return "retardee";
  if (etat == "annulee")
    return "annulee";
  return etat;
}

int sortieWeatherScore(int sortieId, const QDate &date) {
  const int seed = sortieId * 17 + date.dayOfYear() * 7 + date.dayOfWeek() * 13;
  int score = (seed % 10) + 1;
  if (date.dayOfWeek() >= 6)
    score = qMax(2, score - 2);
  return qBound(1, score, 10);
}

QString sortieWeatherCondition(int score) {
  if (score >= 8)
    return "Mer calme";
  if (score >= 6)
    return "Vent modere";
  if (score >= 4)
    return "Houle legere";
  if (score >= 2)
    return "Fort vent";
  return "Alerte";
}

void sortieApplyEtatToRadios(const QList<QRadioButton *> &radios,
                             const QString &etat) {
  for (QRadioButton *radio : radios)
    radio->setChecked(false);

  if (etat == "confirme" && radios.size() > 0)
    radios[0]->setChecked(true);
  else if (etat == "prevue" && radios.size() > 1)
    radios[1]->setChecked(true);
  else if (etat == "retardee" && radios.size() > 2)
    radios[2]->setChecked(true);
  else if (etat == "annulee" && radios.size() > 3)
    radios[3]->setChecked(true);
}

} // namespace

fishingport::fishingport(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::fishingport) {
  ui->setupUi(this);

  setupShell();
  setupSidebar();
  buildDashboardPage();

  model = new QSqlQueryModel(this);
  networkManager = new QNetworkAccessManager(this);

  ui->tableBateaux->setModel(model);
  ui->tableBateaux->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->tableBateaux->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
  ui->tableBateaux->setStyleSheet("QTableView::item { text-align: center; }");
  ui->tableBateaux->verticalHeader()->setVisible(false);
  ui->tableBateaux->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tableBateaux->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->tableBateaux->setAlternatingRowColors(true);
  ui->tableBateaux->setShowGrid(false);

  connect(ui->lineEdit, &QLineEdit::textChanged, this,
          &fishingport::applyFilters);
  connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &fishingport::applyFilters);
  connect(ui->comboBoxType, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &fishingport::applyFilters);
  connect(ui->comboTri, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &fishingport::applyFilters);

  loadBateaux();
  ui->stackedWidget_2->setCurrentIndex(0);

  connect(ui->btnNavEmplacements, &QPushButton::clicked, this,
          &fishingport::on_btnNavEmplacements_clicked);
  connect(ui->btnNavEmployes, &QPushButton::clicked, this,
          &fishingport::on_btnNavEmployes_clicked);

  // Quais initialisation
  m_quaiModel = new QSqlQueryModel(this);
  ui->quai_tableView->setModel(m_quaiModel);
  ui->quai_tableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->quai_tableView->verticalHeader()->setVisible(false);
  ui->quai_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->quai_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->quai_tableView->setAlternatingRowColors(true);
  ui->quai_tableView->setShowGrid(false);

  connect(ui->quai_lineEdit_recherche, &QLineEdit::textChanged, this,
          &fishingport::on_quai_lineEdit_recherche_textChanged);
  connect(ui->quai_comboBox_rechercheCritere,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &fishingport::on_quai_comboBox_rechercheCritere_currentIndexChanged);
  connect(ui->quai_comboBox_tri,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &fishingport::on_quai_comboBox_tri_currentIndexChanged);
  connect(ui->quai_comboBox_export,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &fishingport::on_quai_comboBox_export_currentIndexChanged);

  applyTheme();

  initSortiePage();
  initEmployePage();
  initArduinoPage(); // Arduino monitoring page (embedded in stackedWidget_2)
  polishLegacyUi();
  applyTheme();

  // â”€â”€ Connexions pour les boutons CongÃ©s et PrÃ©sence â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  connect(ui->pushButton_3, &QPushButton::clicked, this,
          &fishingport::employe_gererConges);
  connect(ui->pushButton_4, &QPushButton::clicked, this,
          &fishingport::employe_gererPresence);

  // â”€â”€ Arduino RFID â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  connect(&m_arduino, &Arduino::codeRecu, this, &fishingport::onBadgeDetecte);

  QString portDetecte = Arduino::detectArduino();
  if (!portDetecte.isEmpty()) {
    m_arduino.connecter(portDetecte);
    qDebug() << "âœ… Arduino RFID sur" << portDetecte;
  } else {
    for (int i = 1; i <= 20; i++) {
      if (m_arduino.connecter(QString("COM%1").arg(i)))
        break;
    }
  }

  // Afficher la fenÃªtre d'authentification au dÃ©marrage
  // Cacher la fenÃªtre principale pendant le login
  hide();
  showLoginDialog();
  on_btnNavDashboard_clicked();
  show();
}

fishingport::~fishingport() { delete ui; }

// ==================================================================
//  MODERN SHELL + THEME
// ==================================================================
void fishingport::applyTheme() {
  QFile f(":/styles/theme.qss");
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
}

void fishingport::clearLayout(QLayout *layout) {
  if (!layout)
    return;
  while (QLayoutItem *item = layout->takeAt(0)) {
    if (item->layout())
      clearLayout(item->layout());
    if (QWidget *w = item->widget())
      w->setParent(layout->parentWidget());
    delete item;
  }
}

void fishingport::applyStatusStyle(QLabel *label, const QString &status) {
  if (!label)
    return;
  label->setProperty("status", status);
  label->style()->unpolish(label);
  label->style()->polish(label);
}

void fishingport::polishLegacyUi() {
  setMinimumSize(1180, 760);
  resize(1360, 820);

  const QList<QTableView *> tables = findChildren<QTableView *>();
  for (QTableView *table : tables) {
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(38);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setMinimumHeight(40);
    table->setSortingEnabled(false);
    table->setFrameShape(QFrame::NoFrame);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setStyleSheet(QString());
  }

  const QStringList primaryButtons = {
      "btnAjouter", "btnEnregistrer", "quai_btnAjouter", "employe_btnAjouter",
      "sortie_btnAjouter", "sortie_btnConfirmer", "btnConnecterArduino",
      "pushbuttonchoisir"};
  const QStringList dangerButtons = {
      "btnDelete", "quai_btnSupprimer", "employe_btnSupprimer",
      "sortie_btnSupprimer"};

  const QList<QPushButton *> buttons = findChildren<QPushButton *>();
  for (QPushButton *button : buttons) {
    if (button->property("role").toString() == "nav")
      continue;
    button->setCursor(Qt::PointingHandCursor);
    const QString name = button->objectName();
    if (primaryButtons.contains(name))
      button->setProperty("role", "primary");
    else if (dangerButtons.contains(name))
      button->setProperty("role", "danger");
    else if (!name.startsWith("btnNav"))
      button->setProperty("role", "ghost");
  }

  const QList<QLineEdit *> edits = findChildren<QLineEdit *>();
  for (QLineEdit *edit : edits) {
    edit->setMinimumHeight(36);
    edit->setClearButtonEnabled(true);
  }

  const QList<QComboBox *> combos = findChildren<QComboBox *>();
  for (QComboBox *combo : combos)
    combo->setMinimumHeight(36);

  const QStringList surfaceWidgets = {
      "toolbar", "tableArea", "chatWidget", "actionButtons", "formContainer",
      "statsCardsRow", "uiJsonContainer", "widget_historique", "widget_1",
      "widget_3", "pageTitleRow"};
  for (const QString &name : surfaceWidgets) {
    if (QWidget *w = findChild<QWidget *>(name))
      w->setAttribute(Qt::WA_StyledBackground, true);
  }

  if (ui->groupBox_quai_arduino)
    ui->groupBox_quai_arduino->setTitle("Arduino - Controle du quai");
  applyStatusStyle(ui->lblConnexionArduino, "warn");
  applyStatusStyle(ui->lblArduinoStatus, "info");

  const QList<QFrame *> frames = findChildren<QFrame *>();
  for (QFrame *frame : frames) {
    if (frame->objectName().contains("card", Qt::CaseInsensitive) ||
        frame->objectName().contains("panel", Qt::CaseInsensitive) ||
        frame->objectName().startsWith("frame") ||
        frame->objectName().contains("toolbar", Qt::CaseInsensitive)) {
      frame->setProperty("card", true);
    }
  }
}

void fishingport::setupShell() {
  ui->centralwidget->setObjectName("centralwidget");

  QHBoxLayout *root = new QHBoxLayout(ui->centralwidget);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  ui->sidebar->setObjectName("sidebar");
  ui->sidebar->setMinimumWidth(208);
  ui->sidebar->setMaximumWidth(224);
  ui->sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  m_mainArea = new QWidget(ui->centralwidget);
  m_mainArea->setObjectName("mainArea");

  QVBoxLayout *mainLayout = new QVBoxLayout(m_mainArea);
  mainLayout->setContentsMargins(16, 14, 16, 14);
  mainLayout->setSpacing(10);

  m_badgeHost = new QFrame(m_mainArea);
  m_badgeHost->setObjectName("badgeHost");
  m_badgeHost->setProperty("card", true);
  m_badgeHost->setMinimumHeight(58);
  m_badgeHost->setMaximumHeight(66);
  m_badgeHost->setVisible(false);
  m_badgeHostLayout = new QHBoxLayout(m_badgeHost);
  m_badgeHostLayout->setContentsMargins(12, 8, 12, 8);
  m_badgeHostLayout->setSpacing(8);

  m_headerBar = new QFrame(m_mainArea);
  m_headerBar->setObjectName("topBar");
  QHBoxLayout *hdr = new QHBoxLayout(m_headerBar);
  hdr->setContentsMargins(14, 10, 14, 10);
  hdr->setSpacing(10);

  QVBoxLayout *titleCol = new QVBoxLayout();
  titleCol->setContentsMargins(0, 0, 0, 0);
  titleCol->setSpacing(2);

  m_headerTitle = new QLabel("Dashboard");
  m_headerTitle->setObjectName("headerTitle");
  m_headerSubtitle = new QLabel("Overview");
  m_headerSubtitle->setObjectName("headerSubtitle");

  titleCol->addWidget(m_headerTitle);
  titleCol->addWidget(m_headerSubtitle);
  hdr->addLayout(titleCol);
  hdr->addStretch();

  m_globalSearch = new QLineEdit(m_headerBar);
  m_globalSearch->setObjectName("globalSearch");
  m_globalSearch->setPlaceholderText("Rechercher...");
  m_globalSearch->setClearButtonEnabled(true);
  m_globalSearch->setMinimumWidth(220);
  m_globalSearch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  hdr->addWidget(m_globalSearch, 1);

  QToolButton *alertBtn = new QToolButton(m_headerBar);
  alertBtn->setObjectName("headerIconButton");
  alertBtn->setIcon(QIcon(":/icons/alert-triangle.svg"));
  alertBtn->setToolTip("Alertes systeme");
  alertBtn->setCursor(Qt::PointingHandCursor);
  hdr->addWidget(alertBtn);

  QToolButton *settingsBtn = new QToolButton(m_headerBar);
  settingsBtn->setObjectName("headerIconButton");
  settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
  settingsBtn->setToolTip("Parametres");
  settingsBtn->setCursor(Qt::PointingHandCursor);
  hdr->addWidget(settingsBtn);

  mainLayout->addWidget(m_badgeHost);
  mainLayout->addWidget(m_headerBar);

  QFrame *pageContainer = new QFrame(m_mainArea);
  pageContainer->setObjectName("pageContainer");
  QVBoxLayout *pageLayout = new QVBoxLayout(pageContainer);
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->setSpacing(0);
  pageLayout->addWidget(ui->stackedWidget);

  mainLayout->addWidget(pageContainer, 1);

  root->addWidget(ui->sidebar);
  root->addWidget(m_mainArea, 1);
}

void fishingport::setupSidebar() {
  QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(ui->sidebar->layout());
  if (!layout) {
    layout = new QVBoxLayout(ui->sidebar);
  }
  clearLayout(layout);

  layout->setContentsMargins(16, 18, 16, 18);
  layout->setSpacing(4);

  QHBoxLayout *brandRow = new QHBoxLayout();
  brandRow->setContentsMargins(0, 0, 0, 0);
  brandRow->setSpacing(10);

  QLabel *logo = new QLabel(ui->sidebar);
  logo->setFixedSize(34, 34);
  logo->setPixmap(QIcon(":/icons/logo.svg").pixmap(30, 30));
  logo->setAlignment(Qt::AlignCenter);
  brandRow->addWidget(logo);

  QVBoxLayout *brandText = new QVBoxLayout();
  brandText->setContentsMargins(0, 0, 0, 0);
  brandText->setSpacing(1);
  ui->lblMenuTitle->setObjectName("brandTitle");
  ui->lblMenuTitle->setText("Smart Fishing");
  brandText->addWidget(ui->lblMenuTitle);

  QLabel *brandSub = new QLabel("Port Operations");
  brandSub->setObjectName("brandSubtitle");
  brandText->addWidget(brandSub);
  brandRow->addLayout(brandText, 1);
  layout->addLayout(brandRow);
  layout->addSpacing(6);

  QLabel *navTitle = new QLabel("Navigation");
  navTitle->setObjectName("navTitle");
  layout->addWidget(navTitle);

  auto prepareNavButton = [&](QPushButton *btn, const QString &text,
                              const QString &iconPath) {
    btn->setText(text);
    btn->setCheckable(true);
    btn->setProperty("role", "nav");
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(18, 18));
    btn->setCursor(Qt::PointingHandCursor);
  };

  m_btnNavDashboard = new QPushButton(ui->sidebar);
  prepareNavButton(m_btnNavDashboard, "Dashboard", ":/icons/dashboard.svg");
  layout->addWidget(m_btnNavDashboard);

  prepareNavButton(ui->btnNavBateaux, "Bateaux", ":/icons/boat.svg");
  layout->addWidget(ui->btnNavBateaux);

  prepareNavButton(ui->btnNavQuais, "Quais", ":/icons/anchor.svg");
  layout->addWidget(ui->btnNavQuais);

  prepareNavButton(ui->btnNavEmplacements, "Quais / Places",
                   ":/icons/dock.svg");
  layout->addWidget(ui->btnNavEmplacements);

  prepareNavButton(ui->btnNavSorties, "Sorties", ":/icons/route.svg");
  layout->addWidget(ui->btnNavSorties);

  prepareNavButton(ui->btnNavEmployes, "Employes", ":/icons/users.svg");
  layout->addWidget(ui->btnNavEmployes);

  m_btnNavHardware = new QPushButton(ui->sidebar);
  prepareNavButton(m_btnNavHardware, "Arduino / Hardware",
                   ":/icons/hardware.svg");
  layout->addWidget(m_btnNavHardware);

  layout->addStretch();

  QLabel *foot = new QLabel("v2 Console");
  foot->setObjectName("brandSubtitle");
  layout->addWidget(foot);

  m_navGroup = new QButtonGroup(this);
  m_navGroup->setExclusive(true);
  m_navGroup->addButton(m_btnNavDashboard);
  m_navGroup->addButton(ui->btnNavBateaux);
  m_navGroup->addButton(ui->btnNavQuais);
  m_navGroup->addButton(ui->btnNavEmplacements);
  m_navGroup->addButton(ui->btnNavSorties);
  m_navGroup->addButton(ui->btnNavEmployes);
  m_navGroup->addButton(m_btnNavHardware);

  connect(m_btnNavDashboard, &QPushButton::clicked, this,
          &fishingport::on_btnNavDashboard_clicked);
  connect(m_btnNavHardware, &QPushButton::clicked, this,
          &fishingport::on_btnNavHardware_clicked);

  layout->addSpacing(10);
  QLabel *sysTitle = new QLabel("Connexion systeme", ui->sidebar);
  sysTitle->setObjectName("navTitle");
  layout->addWidget(sysTitle);

  auto addSystemCard = [&](const QString &title, const QString &subtitle,
                           const QString &accent) {
    QFrame *card = new QFrame(ui->sidebar);
    card->setProperty("card", "module");
    card->setProperty("accent", accent);
    QVBoxLayout *cl = new QVBoxLayout(card);
    cl->setContentsMargins(10, 8, 10, 8);
    cl->setSpacing(2);
    QLabel *t = new QLabel(title, card);
    t->setProperty("role", "cardTitle");
    QLabel *s = new QLabel(subtitle, card);
    s->setProperty("role", "chip");
    applyStatusStyle(s, "ok");
    cl->addWidget(t);
    cl->addWidget(s);
    layout->addWidget(card);
  };

  addSystemCard("Arduino PIR", "Connecte", "emerald");
  addSystemCard("Arduino RFID", "Connecte", "cyan");
}

void fishingport::buildDashboardPage() {
  if (m_dashboardPage)
    return;

  m_dashboardPage = new QWidget();
  m_dashboardPage->setObjectName("page_dashboard");

  QVBoxLayout *root = new QVBoxLayout(m_dashboardPage);
  root->setContentsMargins(24, 22, 24, 24);
  root->setSpacing(12);

  QFrame *hero = new QFrame();
  hero->setProperty("card", "hero");
  QHBoxLayout *heroL = new QHBoxLayout(hero);
  heroL->setContentsMargins(22, 20, 22, 20);
  heroL->setSpacing(18);

  QVBoxLayout *heroText = new QVBoxLayout();
  heroText->setSpacing(6);
  QLabel *heroKicker = new QLabel("SMART PORT COMMAND CENTER");
  heroKicker->setProperty("role", "eyebrow");
  heroText->addWidget(heroKicker);
  m_dashWelcomeName = new QLabel("Bienvenue");
  m_dashWelcomeName->setProperty("role", "heroTitle");
  m_dashWelcomeRole = new QLabel("Console operationnelle du port");
  m_dashWelcomeRole->setProperty("role", "cardSub");
  heroText->addWidget(m_dashWelcomeName);
  heroText->addWidget(m_dashWelcomeRole);
  heroL->addLayout(heroText);
  heroL->addStretch();

  m_dashDbStatus = new QLabel("Base de donnees: --");
  m_dashDbStatus->setProperty("role", "chip");
  heroL->addWidget(m_dashDbStatus);

  root->addWidget(hero);

  QHBoxLayout *quickActions = new QHBoxLayout();
  quickActions->setSpacing(10);
  auto addQuickAction = [&](const QString &label, const QString &iconPath,
                            const QObject *receiver, const char *slot) {
    QPushButton *button = new QPushButton(label);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(16, 16));
    button->setMinimumHeight(38);
    button->setProperty("role",
                        label == "Arduino / Hardware" ? "primary" : "ghost");
    button->setCursor(Qt::PointingHandCursor);
    connect(button, SIGNAL(clicked()), receiver, slot);
    quickActions->addWidget(button);
  };
  addQuickAction("Bateaux", ":/icons/boat.svg", this, SLOT(on_btnNavBateaux_clicked()));
  addQuickAction("Quais", ":/icons/anchor.svg", this, SLOT(on_btnNavQuais_clicked()));
  addQuickAction("Sorties", ":/icons/route.svg", this, SLOT(on_btnNavSorties_clicked()));
  addQuickAction("Arduino / Hardware", ":/icons/hardware.svg", this,
                 SLOT(on_btnNavHardware_clicked()));
  quickActions->addStretch();
  root->addLayout(quickActions);

  // Metrics grid
  QGridLayout *metrics = new QGridLayout();
  metrics->setHorizontalSpacing(12);
  metrics->setVerticalSpacing(12);

  auto createMetricCard = [&](const QString &title, QLabel **valueOut,
                              const QString &sub, const QString &accent) {
    QFrame *card = new QFrame();
    card->setProperty("card", "module");
    card->setProperty("accent", accent);
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(16, 14, 16, 14);
    l->setSpacing(5);
    QLabel *t = new QLabel(title);
    t->setProperty("role", "cardTitle");
    QLabel *v = new QLabel("--");
    v->setProperty("role", "cardValue");
    QLabel *s = new QLabel(sub);
    s->setProperty("role", "cardSub");
    l->addWidget(t);
    l->addWidget(v);
    l->addWidget(s);
    if (valueOut)
      *valueOut = v;
    return card;
  };

  metrics->addWidget(createMetricCard("Bateaux en flotte", &m_dashBoatsTotal,
                                     "Total enregistres", "indigo"),
                     0, 0);
  metrics->addWidget(createMetricCard("Bateaux en mer", &m_dashBoatsAtSea,
                                     "Actifs en sortie", "cyan"),
                     0, 1);
  metrics->addWidget(createMetricCard("Quais disponibles", &m_dashQuaisFree,
                                     "Statut libre", "emerald"),
                     0, 2);
  metrics->addWidget(createMetricCard("Emplacements libres", &m_dashEmpFree,
                                     "Places ouvertes", "orange"),
                     1, 0);
  metrics->addWidget(createMetricCard("Sorties prevues", &m_dashSortiesPrev,
                                     "Etat prevue", "indigo"),
                     1, 1);
  metrics->addWidget(createMetricCard("Employes", &m_dashEmployesTot,
                                     "Effectif total", "emerald"),
                     1, 2);

  root->addLayout(metrics);

  // Bottom row
  QHBoxLayout *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(12);

  QFrame *hardwareCard = new QFrame();
  hardwareCard->setProperty("card", "module");
  hardwareCard->setProperty("accent", "emerald");
  QVBoxLayout *hwL = new QVBoxLayout(hardwareCard);
  hwL->setContentsMargins(18, 16, 18, 16);
  hwL->setSpacing(10);

  QLabel *hwTitle = new QLabel("Materiel et capteurs");
  hwTitle->setProperty("role", "cardTitle");
  hwL->addWidget(hwTitle);

  auto addHwRow = [&](const QString &label, QLabel **statusOut) {
    QHBoxLayout *row = new QHBoxLayout();
    QLabel *name = new QLabel(label);
    name->setProperty("role", "cardSub");
    QLabel *status = new QLabel("--");
    status->setProperty("role", "chip");
    row->addWidget(name);
    row->addStretch();
    row->addWidget(status);
    if (statusOut)
      *statusOut = status;
    hwL->addLayout(row);
  };

  addHwRow("RFID Employes", &m_dashRfidStatus);
  addHwRow("PIR Bateau", &m_dashPirStatus);
  bottomRow->addWidget(hardwareCard, 1);

  QFrame *activityCard = new QFrame();
  activityCard->setProperty("card", "module");
  activityCard->setProperty("accent", "cyan");
  QVBoxLayout *actL = new QVBoxLayout(activityCard);
  actL->setContentsMargins(18, 16, 18, 16);
  actL->setSpacing(8);

  QLabel *actTitle = new QLabel("Activite recente");
  actTitle->setProperty("role", "cardTitle");
  actL->addWidget(actTitle);

  m_dashRecentList = new QListWidget();
  m_dashRecentList->setMinimumHeight(160);
  actL->addWidget(m_dashRecentList);
  bottomRow->addWidget(activityCard, 1);

  root->addLayout(bottomRow);

  ui->stackedWidget->insertWidget(0, m_dashboardPage);
}

void fishingport::setActivePage(QWidget *page, const QString &title,
                                const QString &subtitle) {
  const bool changingPage = ui->stackedWidget->currentWidget() != page;
  ui->stackedWidget->setCurrentWidget(page);
  if (m_headerTitle)
    m_headerTitle->setText(title);
  if (m_headerSubtitle)
    m_headerSubtitle->setText(subtitle);
  if (m_badgeHost)
    m_badgeHost->setVisible(m_connectedUserId != -1);

  if (!changingPage)
    return;

  auto fadeWidget = [](QWidget *target, qreal from, qreal to, int duration) {
    if (!target)
      return;

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(target);
    effect->setOpacity(from);
    target->setGraphicsEffect(effect);

    QPropertyAnimation *animation =
        new QPropertyAnimation(effect, "opacity", target);
    animation->setDuration(duration);
    animation->setStartValue(from);
    animation->setEndValue(to);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    QObject::connect(animation, &QPropertyAnimation::finished, target,
                     [target, effect]() {
                       if (target->graphicsEffect() == effect)
                         target->setGraphicsEffect(nullptr);
                     });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  };

  fadeWidget(ui->stackedWidget, 0.18, 1.0, 190);
  fadeWidget(m_headerBar, 0.65, 1.0, 150);
}

void fishingport::updateDashboard() {
  if (!m_dashboardPage)
    return;

  const QString displayName =
      m_connectedUserName.isEmpty() ? "Operateur" : m_connectedUserName;
  const QString displayRole =
      m_connectedUserRole.isEmpty() ? "Session active" : m_connectedUserRole;

  if (m_dashWelcomeName)
    m_dashWelcomeName->setText("Bienvenue, " + displayName);
  if (m_dashWelcomeRole)
    m_dashWelcomeRole->setText(displayRole + " - Console operationnelle");

  auto queryCount = [](const QString &sql) -> int {
    QSqlQuery q;
    if (q.exec(sql) && q.next())
      return q.value(0).toInt();
    return -1;
  };

  const int boatsTotal = queryCount("SELECT COUNT(*) FROM BATEAU");
  const int boatsAtSea = queryCount(
      "SELECT COUNT(*) FROM BATEAU WHERE LOWER(STATUSB) = 'en mer'");
  const int quaisFree = queryCount(
      "SELECT COUNT(*) FROM QUAIS WHERE LOWER(STATUSQ) = 'libre'");
  const int empFree = queryCount(
      "SELECT COUNT(*) FROM EMPLACEMENT WHERE LOWER(ETATE) = 'libre'");
  const int sortiesPrev = queryCount(
      "SELECT COUNT(*) FROM SORTIE WHERE LOWER(ETAT) = 'prevue'");
  const int employesTot = queryCount("SELECT COUNT(*) FROM EMPLOYE");

  if (m_dashBoatsTotal)
    m_dashBoatsTotal->setText(boatsTotal >= 0 ? QString::number(boatsTotal)
                                             : "--");
  if (m_dashBoatsAtSea)
    m_dashBoatsAtSea->setText(boatsAtSea >= 0 ? QString::number(boatsAtSea)
                                             : "--");
  if (m_dashQuaisFree)
    m_dashQuaisFree->setText(quaisFree >= 0 ? QString::number(quaisFree)
                                           : "--");
  if (m_dashEmpFree)
    m_dashEmpFree->setText(empFree >= 0 ? QString::number(empFree) : "--");
  if (m_dashSortiesPrev)
    m_dashSortiesPrev->setText(sortiesPrev >= 0 ? QString::number(sortiesPrev)
                                               : "--");
  if (m_dashEmployesTot)
    m_dashEmployesTot->setText(employesTot >= 0 ? QString::number(employesTot)
                                               : "--");

  auto setStatusChip = [](QLabel *label, const QString &text,
                          const char *status) {
    if (!label)
      return;
    label->setText(text);
    label->setProperty("status", status);
    label->style()->unpolish(label);
    label->style()->polish(label);
  };

  const bool dbOk = QSqlDatabase::database().isOpen();
  setStatusChip(m_dashDbStatus, dbOk ? "Base de donnees: connectee"
                                     : "Base de donnees: hors ligne",
               dbOk ? "ok" : "error");

  setStatusChip(m_dashRfidStatus,
               m_arduino.estConnecte() ? "RFID connecte" : "RFID hors ligne",
               m_arduino.estConnecte() ? "ok" : "warn");
  setStatusChip(m_dashPirStatus,
               (m_ard && m_ard->isConnected()) ? "PIR connecte"
                                               : "PIR hors ligne",
               (m_ard && m_ard->isConnected()) ? "ok" : "warn");

  if (m_dashRecentList) {
    m_dashRecentList->clear();
    QSqlQuery q;
    q.prepare(
        "SELECT * FROM (SELECT SORTIE_ID, DATE_DEPART, ETAT FROM SORTIE "
        "ORDER BY SORTIE_ID DESC) WHERE ROWNUM <= 5");
    if (q.exec()) {
      while (q.next()) {
        const int id = q.value(0).toInt();
        const QDate d = q.value(1).toDate();
        const QString etat = q.value(2).toString();
        QString item = QString("Sortie #%1 - %2 - %3")
                           .arg(id)
                           .arg(d.isValid() ? d.toString("dd/MM/yyyy")
                                            : "--")
                           .arg(etat);
        m_dashRecentList->addItem(item);
      }
    }
    if (m_dashRecentList->count() == 0)
      m_dashRecentList->addItem("Aucune activite recente");
  }
}

void fishingport::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  updateResponsiveLayout();
}

void fishingport::updateResponsiveLayout() {
  const int windowWidth = width();
  const bool compact = windowWidth < 1220;
  const bool narrow = windowWidth < 1480;

  if (ui->sidebar) {
    const int sidebarWidth = compact ? 184 : narrow ? 198 : 216;
    ui->sidebar->setMinimumWidth(sidebarWidth);
    ui->sidebar->setMaximumWidth(sidebarWidth);
  }

  if (m_globalSearch) {
    const int searchWidth = compact ? 180 : narrow ? 260 : 360;
    m_globalSearch->setMinimumWidth(searchWidth);
    m_globalSearch->setMaximumWidth(searchWidth + 120);
  }

  if (m_headerSubtitle)
    m_headerSubtitle->setVisible(windowWidth >= 1080);

  if (m_badgeRole)
    m_badgeRole->setVisible(windowWidth >= 1140);
  if (m_badgeId)
    m_badgeId->setVisible(windowWidth >= 980);

  if (m_emplacementToolsLayout) {
    m_emplacementToolsLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                                   : QBoxLayout::LeftToRight);
    m_emplacementToolsLayout->setAlignment(compact ? Qt::AlignLeft
                                                   : Qt::AlignRight);
  }

  if (m_emplacementContentLayout) {
    m_emplacementContentLayout->setDirection(narrow ? QBoxLayout::TopToBottom
                                                    : QBoxLayout::LeftToRight);
  }

  if (ui->groupBox) {
    if (narrow) {
      ui->groupBox->setMinimumWidth(0);
      ui->groupBox->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
      ui->groupBox->setMinimumWidth(460);
      ui->groupBox->setMaximumWidth(560);
    }
  }

  if (ui->groupBox_2)
    ui->groupBox_2->setMinimumHeight(narrow ? 420 : 0);

  if (m_emplacementStatsLayout) {
    m_emplacementStatsLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                                   : QBoxLayout::LeftToRight);
  }
}

void fishingport::polishEmplacementsPage() {
  if (m_emplacementPagePolished)
    return;
  m_emplacementPagePolished = true;

  ui->tabWidget->setDocumentMode(true);
  ui->tabWidget->setUsesScrollButtons(false);
  ui->tabWidget->setTabText(0, "Accueil");
  ui->tabWidget->setTabText(1, "Statistiques historiques");

  ui->groupBox->setTitle("Formulaire Emplacement");
  ui->groupBox->setProperty("accent", "indigo");
  ui->groupBox_2->setTitle("Liste des Emplacements");
  ui->groupBox_2->setProperty("accent", "cyan");
  ui->frame->setProperty("card", "module");

  ui->label_20->setText("Type Emplacement *");
  ui->label_21->setText("Numero *");
  ui->label_22->setText("Quai *");
  ui->label_23->setText("Bateau affecte");
  ui->label_30->setText("Date de debut d'occupation *");
  ui->label_31->setText("Commentaires");
  ui->label_24->setText("Etat *");
  ui->lineEdit_2->setPlaceholderText("Ex: 12");
  ui->textEdit->setPlaceholderText("Ajouter un commentaire...");
  ui->radioButton_8->setText("Libre");
  ui->radioButton_9->setText("Occupe");
  ui->radioButton_10->setText("Maintenance");
  ui->pushButton_14->setText("Ajouter");
  ui->pushButton_16->setText("Annuler");
  ui->pushButton_17->setText("Commentaire IA");
  ui->pushButton_14->setProperty("role", "primary");
  ui->pushButton_16->setProperty("role", "ghost");
  ui->pushButton_17->setProperty("role", "ghost");

  ui->comboBox_9->setMinimumWidth(170);
  ui->lineEdit_3->setMinimumWidth(220);
  ui->comboBox_12->setMinimumWidth(150);
  ui->comboBox_10->setMinimumWidth(150);

  ui->tableWidget_3->verticalHeader()->setVisible(false);
  ui->tableWidget_3->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  ui->tableWidget_3->horizontalHeader()->setMinimumHeight(42);
  ui->tableWidget_3->setAlternatingRowColors(true);
  ui->tableWidget_3->setShowGrid(false);
  ui->tableWidget_3->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tableWidget_3->setSelectionMode(QAbstractItemView::SingleSelection);

  QWidget *tab = ui->tab;
  if (tab->layout())
    delete tab->layout();

  QVBoxLayout *page = new QVBoxLayout(tab);
  page->setContentsMargins(12, 10, 12, 10);
  page->setSpacing(12);

  QBoxLayout *tools = new QBoxLayout(QBoxLayout::LeftToRight);
  tools->setContentsMargins(0, 0, 0, 0);
  tools->setSpacing(10);
  tools->addStretch();
  tools->addWidget(ui->comboBox_9);
  tools->addWidget(ui->lineEdit_3);
  tools->addWidget(ui->comboBox_12);
  tools->addWidget(ui->comboBox_10);
  m_emplacementToolsLayout = tools;
  page->addLayout(tools);

  QBoxLayout *content = new QBoxLayout(QBoxLayout::LeftToRight);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(12);
  ui->groupBox->setMinimumWidth(460);
  ui->groupBox->setMaximumWidth(560);
  content->addWidget(ui->groupBox, 0);
  content->addWidget(ui->groupBox_2, 1);
  m_emplacementContentLayout = content;
  page->addLayout(content, 1);

  ui->frame->setMinimumHeight(96);
  ui->frame->setMaximumHeight(116);
  if (ui->frame->layout())
    delete ui->frame->layout();
  QBoxLayout *statsLayout = new QBoxLayout(QBoxLayout::LeftToRight, ui->frame);
  statsLayout->setContentsMargins(14, 12, 14, 12);
  statsLayout->setSpacing(10);
  m_emplacementStatsLayout = statsLayout;
  page->addWidget(ui->frame);

  const QList<QLabel *> stats = {ui->label_25, ui->label_26, ui->label_27,
                                 ui->label_28};
  for (int i = 0; i < stats.size(); ++i) {
    stats[i]->setProperty("role", "chip");
    stats[i]->setProperty("status", i == 1 ? "ok" : i == 2 ? "error" :
                                      i == 3 ? "warn" : "info");
    stats[i]->setMinimumHeight(44);
    stats[i]->setAlignment(Qt::AlignCenter);
    stats[i]->style()->unpolish(stats[i]);
    stats[i]->style()->polish(stats[i]);
    statsLayout->addWidget(stats[i], 1);
  }

  updateResponsiveLayout();
}

// ==================================================================
//  AUTHENTIFICATION
// ==================================================================
void fishingport::showLoginDialog() {
  QDialog loginDialog(this);
  loginDialog.setWindowTitle("Port Management â€” Connexion");
  loginDialog.setObjectName("authDialog");
  loginDialog.setModal(true);
  loginDialog.setFixedSize(
      QGuiApplication::primaryScreen()->availableGeometry().size());

  QVBoxLayout *rootLayout = new QVBoxLayout(&loginDialog);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  // â”€â”€ Carte blanche centrale â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *card = new QFrame(&loginDialog);
  card->setFixedSize(420, 540);
  card->setObjectName("authCard");
  card->setProperty("card", true);

  QVBoxLayout *cardL = new QVBoxLayout(card);
  cardL->setContentsMargins(38, 28, 38, 28);
  cardL->setSpacing(0);

  // Logo
  QLabel *logoLabel = new QLabel();
  logoLabel->setAlignment(Qt::AlignCenter);
  logoLabel->setPixmap(QIcon(":/icons/logo.svg").pixmap(48, 48));
  cardL->addWidget(logoLabel);
  cardL->addSpacing(8);

  // Titre
    QLabel *titleLabel = new QLabel("Smart Fishing Port");
  titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setObjectName("authTitle");
  cardL->addWidget(titleLabel);
  cardL->addSpacing(4);

  QLabel *subTitle = new QLabel("Connectez-vous pour accÃ©der au systÃ¨me");
  subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setObjectName("authSubtitle");
  cardL->addWidget(subTitle);
  cardL->addSpacing(10);

  // SÃ©parateur
  QFrame *sep = new QFrame();
  sep->setFixedHeight(1);
  sep->setStyleSheet("background: #2E3136;");
  cardL->addWidget(sep);
  cardL->addSpacing(18);

  // â”€â”€ Ã‰TAPE 1 : Champ ID EmployÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *idLabel = new QLabel("ID EmployÃ©");
  idLabel->setProperty("role", "cardTitle");
  cardL->addWidget(idLabel);
  cardL->addSpacing(4);

  QLineEdit *userEdit = new QLineEdit();
  userEdit->setPlaceholderText("Entrez votre ID employÃ©...");
  userEdit->setFixedHeight(46);
  cardL->addWidget(userEdit);
  cardL->addSpacing(20);

  // â”€â”€ Ã‰TAPE 2 : SÃ©lection de la mÃ©thode d'authentification â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *methodLabel = new QLabel("Choisir la mÃ©thode d'authentification");
  methodLabel->setProperty("role", "cardTitle");
  methodLabel->hide();
  cardL->addWidget(methodLabel);
  cardL->addSpacing(8);

  // Conteneur pour les deux boutons
  QWidget *methodContainer = new QWidget();
  methodContainer->hide();
  QHBoxLayout *methodLayout = new QHBoxLayout(methodContainer);
  methodLayout->setContentsMargins(0, 0, 0, 0);
  methodLayout->setSpacing(10);

  // Bouton Mot de passe
  QPushButton *btnPassword = new QPushButton("Mot de passe");
  btnPassword->setFixedHeight(48);
  btnPassword->setProperty("role", "primary");
  methodLayout->addWidget(btnPassword);

  // Bouton Badge RFID
  QPushButton *btnBadge = new QPushButton("Badge RFID");
  btnBadge->setFixedHeight(48);
  btnBadge->setProperty("role", "ghost");
  methodLayout->addWidget(btnBadge);

  cardL->addWidget(methodContainer);
  cardL->addSpacing(20);

  // â”€â”€ Ã‰TAPE 3 : Champ Mot de passe (cachÃ© par dÃ©faut) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *passLabel = new QLabel("Mot de passe");
  passLabel->setProperty("role", "cardTitle");
  passLabel->hide();
  cardL->addWidget(passLabel);
  cardL->addSpacing(4);

  QWidget *passContainer = new QWidget();
  passContainer->hide();
  QHBoxLayout *passHL = new QHBoxLayout(passContainer);
  passHL->setContentsMargins(0, 0, 0, 0);
  passHL->setSpacing(0);

  QLineEdit *passEdit = new QLineEdit();
  passEdit->setEchoMode(QLineEdit::Password);
  passEdit->setPlaceholderText("Entrez votre mot de passe...");
  passEdit->setFixedHeight(46);

  QToolButton *eyeBtn = new QToolButton();
  eyeBtn->setFixedSize(40, 40);
  eyeBtn->setCheckable(true);
  eyeBtn->setIcon(QIcon(":/icons/eye.svg"));
  eyeBtn->setCursor(Qt::PointingHandCursor);
  connect(eyeBtn, &QToolButton::toggled, [passEdit, eyeBtn](bool checked) {
    if (checked) {
      passEdit->setEchoMode(QLineEdit::Normal);
      eyeBtn->setIcon(QIcon(":/icons/eye-off.svg"));
    } else {
      passEdit->setEchoMode(QLineEdit::Password);
      eyeBtn->setIcon(QIcon(":/icons/eye.svg"));
    }
  });

  passHL->addWidget(passEdit);
  passHL->addWidget(eyeBtn);
  cardL->addWidget(passContainer);
  cardL->addSpacing(16);

  // Bouton Se connecter (pour mot de passe)
  QPushButton *btnLogin = new QPushButton("Se connecter");
  btnLogin->setFixedHeight(48);
  btnLogin->setProperty("role", "primary");
  btnLogin->hide();
  cardL->addWidget(btnLogin);
  cardL->addSpacing(10);

  // Lien mot de passe oubliÃ©
  QLabel *forgotLabel =
      new QLabel("<a href='#' style='color:#0288D1; text-decoration:none;'>Mot "
                 "de passe oubliÃ© ?</a>");
  forgotLabel->setAlignment(Qt::AlignCenter);
  forgotLabel->setTextFormat(Qt::RichText);
  forgotLabel->setOpenExternalLinks(false);
  forgotLabel->hide();
  cardL->addWidget(forgotLabel);
  connect(forgotLabel, &QLabel::linkActivated,
          [&loginDialog, this]() { this->showPasswordResetDialog(); });

  // â”€â”€ Ã‰TAPE 4 : Badge RFID (cachÃ© par dÃ©faut) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *badgeLabel = new QLabel("Authentification par Badge RFID");
  badgeLabel->setProperty("role", "cardTitle");
  badgeLabel->hide();
  cardL->addWidget(badgeLabel);
  cardL->addSpacing(8);

  QLabel *badgeInfo = new QLabel("Approchez votre badge RFID du lecteur...");
  badgeInfo->setAlignment(Qt::AlignCenter);
  badgeInfo->setProperty("role", "cardSub");
  badgeInfo->hide();
  cardL->addWidget(badgeInfo);
  cardL->addSpacing(12);

  QLabel *badgeCode = new QLabel("");
  badgeCode->setAlignment(Qt::AlignCenter);
  badgeCode->setStyleSheet("font-size: 13px; font-weight: bold; color: "
                           "#23A559; background: transparent;");
  badgeCode->hide();
  cardL->addWidget(badgeCode);
  cardL->addSpacing(16);

  // Bouton Annuler (pour badge)
  QPushButton *btnCancelBadge = new QPushButton("Annuler");
  btnCancelBadge->setFixedHeight(48);
  btnCancelBadge->setProperty("role", "ghost");
  btnCancelBadge->hide();
  cardL->addWidget(btnCancelBadge);

  cardL->addStretch();

  // Centrer la carte
  QHBoxLayout *hCenter = new QHBoxLayout();
  hCenter->addStretch();
  hCenter->addWidget(card);
  hCenter->addStretch();

  QVBoxLayout *vCenter = new QVBoxLayout();
  vCenter->addStretch();
  vCenter->addLayout(hCenter);
  vCenter->addStretch();

  rootLayout->addLayout(vCenter);

  // â”€â”€ Variables d'Ã©tat â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  bool authenticationInProgress = false;
  QString currentUserId;

  // â”€â”€ Connexions â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  // Afficher les boutons de mÃ©thode aprÃ¨s saisie de l'ID
  connect(userEdit, &QLineEdit::textChanged, [&]() {
    if (!userEdit->text().trimmed().isEmpty()) {
      methodLabel->show();
      methodContainer->show();
    } else {
      methodLabel->hide();
      methodContainer->hide();
      passLabel->hide();
      passContainer->hide();
      btnLogin->hide();
      forgotLabel->hide();
      badgeLabel->hide();
      badgeInfo->hide();
      badgeCode->hide();
      btnCancelBadge->hide();
    }
  });

  // Bouton Mot de passe
  connect(btnPassword, &QPushButton::clicked, [&]() {
    currentUserId = userEdit->text().trimmed();
    methodLabel->hide();
    methodContainer->hide();
    badgeLabel->hide();
    badgeInfo->hide();
    badgeCode->hide();
    btnCancelBadge->hide();
    passLabel->show();
    passContainer->show();
    btnLogin->show();
    forgotLabel->show();
    passEdit->clear();
    passEdit->setFocus();
  });

  // Bouton Badge RFID
  connect(btnBadge, &QPushButton::clicked, [&]() {
    currentUserId = userEdit->text().trimmed();
    methodLabel->hide();
    methodContainer->hide();
    passLabel->hide();
    passContainer->hide();
    btnLogin->hide();
    forgotLabel->hide();
    badgeLabel->show();
    badgeInfo->show();
    badgeCode->show();
    btnCancelBadge->show();
    badgeCode->setText("En attente du badge...");
    authenticationInProgress = true;
  });

  // Bouton Annuler (retour Ã  la sÃ©lection de mÃ©thode)
  connect(btnCancelBadge, &QPushButton::clicked, [&]() {
    authenticationInProgress = false;
    badgeLabel->hide();
    badgeInfo->hide();
    badgeCode->hide();
    btnCancelBadge->hide();
    methodLabel->show();
    methodContainer->show();
  });

  // Authentification par mot de passe
  auto doPasswordLogin = [&]() {
    QString username = userEdit->text().trimmed();
    QString password = passEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
      QMessageBox::warning(&loginDialog, "Erreur",
                           "Veuillez remplir tous les champs.");
      return;
    }
    if (authenticateUser(username, password)) {
      m_connectionMethod = "Mot de passe";
      m_connectionTime = QDateTime::currentDateTime();

      // âœ… Marquer la prÃ©sence automatiquement comme "PrÃ©sent"
      qDebug() << "ðŸ”µ Tentative de marquer prÃ©sence pour ID:"
               << m_connectedUserId;
      bool presenceSuccess = m_employe.marquerPresence(
          m_connectedUserId, QDate::currentDate(), "PrÃ©sent");
      qDebug() << "ðŸŸ¢ RÃ©sultat marquerPresence:" << presenceSuccess;

      loginDialog.accept();
    } else {
      QMessageBox::critical(&loginDialog, "Erreur d'authentification",
                            "ID ou mot de passe incorrect.");
      passEdit->clear();
      passEdit->setFocus();
    }
  };

  connect(btnLogin, &QPushButton::clicked, &loginDialog, doPasswordLogin);
  connect(passEdit, &QLineEdit::returnPressed, &loginDialog, doPasswordLogin);

  // DÃ©connecter les anciennes connexions du signal Arduino
  disconnect(&m_arduino, nullptr, this, nullptr);

  // Authentification par badge RFID
  connect(
      &m_arduino, &Arduino::codeRecu,
      [this, &loginDialog, &authenticationInProgress, &currentUserId,
       badgeCode](const QString &code) {
        if (authenticationInProgress && !currentUserId.isEmpty()) {
          badgeCode->setText(QString("âœ… Badge dÃ©tectÃ©: %1").arg(code));

          qDebug() << "ðŸ” Badge reÃ§u:" << code
                   << "| ID utilisateur:" << currentUserId;

          // VÃ©rifier si le badge correspond au code autorisÃ©
          // The authorized badge is configured in the process environment.
          if (!CODE_AUTORISE.isEmpty() && code.toUpper() == CODE_AUTORISE) {
            qDebug() << "âœ… Badge valide!";

            // Badge valide - rÃ©cupÃ©rer les infos de l'utilisateur
            QSqlQuery query;
            query.prepare("SELECT ID_EMPLOYE, NOME, ROLE, CONTACT FROM EMPLOYE "
                          "WHERE ID_EMPLOYE = :id");
            query.bindValue(":id", currentUserId.toInt());

            qDebug() << "ðŸ“‹ RequÃªte SQL - ID:" << currentUserId.toInt();

            if (query.exec()) {
              qDebug() << "âœ… RequÃªte exÃ©cutÃ©e";
              if (query.next()) {
                qDebug() << "âœ… Utilisateur trouvÃ©!";
                m_connectedUserId = query.value(0).toInt();
                m_connectedUserName = query.value(1).toString();
                m_connectedUserRole = query.value(2).toString();

                qDebug() << "   ID:" << m_connectedUserId
                         << "| Nom:" << m_connectedUserName;

                authenticationInProgress = false;
                m_smsAuthSuccess = false; // RÃ©initialiser le flag

                // Utiliser la fonction showArduinoSmsAuth avec le design
                // complet Cette fonction affichera "AccÃ¨s refusÃ©" si annulÃ©e
                showArduinoSmsAuth(m_connectedUserId, code);

                // Fermer le dialogue de login SEULEMENT si l'authentification
                // SMS a rÃ©ussi
                if (m_smsAuthSuccess) {
                  m_connectionMethod = "Badge RFID";
                  m_connectionTime = QDateTime::currentDateTime();

                  // âœ… Marquer la prÃ©sence automatiquement comme "PrÃ©sent"
                  m_employe.marquerPresence(m_connectedUserId,
                                            QDate::currentDate(), "PrÃ©sent");

                  loginDialog.accept();
                } else {
                  // RÃ©activer l'attente du badge si l'authentification a Ã©chouÃ©
                  badgeCode->setText("En attente du badge...");
                  authenticationInProgress = true;
                }
              } else {
                qDebug() << "âŒ Aucun utilisateur trouvÃ© pour ID:"
                         << currentUserId.toInt();
                badgeCode->setText("âŒ Utilisateur non trouvÃ©");
                QTimer::singleShot(
                    2000, [this, &authenticationInProgress, badgeCode]() {
                      if (authenticationInProgress) {
                        badgeCode->setText("En attente du badge...");
                      }
                    });
              }
            } else {
              qDebug() << "âŒ Erreur SQL:" << query.lastError().text();
              badgeCode->setText("âŒ Erreur base de donnÃ©es");
            }
          } else {
            qDebug() << "âŒ Badge non autorisÃ©:" << code;
            badgeCode->setText("âŒ Badge non autorisÃ©");

            // Afficher le dialogue d'accÃ¨s refusÃ© en rouge
            showAccesRefuse(code, currentUserId.toInt());

            QTimer::singleShot(2000,
                               [this, &authenticationInProgress, badgeCode]() {
                                 if (authenticationInProgress) {
                                   badgeCode->setText("En attente du badge...");
                                 }
                               });
          }
        }
      });

  loginDialog.move(
      QGuiApplication::primaryScreen()->availableGeometry().center() -
      loginDialog.rect().center());

  if (loginDialog.exec() == QDialog::Accepted) {
    // Reconnecter les anciennes connexions du signal Arduino
    connect(&m_arduino, &Arduino::codeRecu, this, &fishingport::onBadgeDetecte);

    creerBadgeUtilisateur();
    appliquerDroitsRole();
    if (m_badgeHost)
      m_badgeHost->setVisible(true);
    // createSessionCard();  // âœ… SupprimÃ© - sera appelÃ© seulement dans Gestion
    // des EmployÃ©s
  } else {
    // Reconnecter les anciennes connexions du signal Arduino
    connect(&m_arduino, &Arduino::codeRecu, this, &fishingport::onBadgeDetecte);

    QApplication::quit();
  }
}

bool fishingport::authenticateUser(const QString &username,
                                   const QString &password) {
  // Authenticate against the database; no hardcoded administrator bypass.
  // Authentification par ID numÃ©rique
  bool isNumeric = false;
  int userId = username.toInt(&isNumeric);

  QSqlQuery query;
  if (isNumeric) {
    query.prepare("SELECT ID_EMPLOYE, NOME, ROLE FROM EMPLOYE "
                  "WHERE ID_EMPLOYE = :id AND MOT_DE_PASSE = :mdp");
    query.bindValue(":id", userId);
    query.bindValue(":mdp", password);
  } else {
    query.prepare("SELECT ID_EMPLOYE, NOME, ROLE FROM EMPLOYE "
                  "WHERE NOME = :nom AND MOT_DE_PASSE = :mdp");
    query.bindValue(":nom", username);
    query.bindValue(":mdp", password);
  }

  if (query.exec() && query.next()) {
    m_connectedUserId = query.value(0).toInt();
    m_connectedUserName = query.value(1).toString();
    m_connectedUserRole = query.value(2).toString();
    return true;
  }
  return false;
}

// ==================================================================
//  BADGE UTILISATEUR (sidebar, en haut)
// ==================================================================
void fishingport::creerBadgeUtilisateur() {
  if (!m_badgeHost || !m_badgeHostLayout)
    return;

  clearLayout(m_badgeHostLayout);
  m_badgeWidget = m_badgeHost;
  m_badgeWidget->setProperty("card", true);
  m_badgeWidget->setVisible(true);

  // Couleur selon le rÃ´le
  QString avatarColor = "#FF8A65";
  if (m_connectedUserRole == "Administration")
    avatarColor = "#E53935";
  else if (m_connectedUserRole == "Responsable RH")
    avatarColor = "#FF8A65";
  else if (m_connectedUserRole == "EmployÃ©")
    avatarColor = "#43A047";

  QString initiale = m_connectedUserName.isEmpty()
                         ? "?"
                         : QString(m_connectedUserName[0]).toUpper();

  QHBoxLayout *mainRow = m_badgeHostLayout;
  mainRow->setContentsMargins(12, 8, 12, 8);
  mainRow->setSpacing(10);

  // â”€â”€ Point vert connectÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *dot = new QWidget();
  dot->setFixedSize(10, 10);
  dot->setStyleSheet("background: #23A559; border-radius: 5px; border: none;");
  mainRow->addWidget(dot, 0, Qt::AlignVCenter);

  // â”€â”€ Avatar rond â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  m_badgeAvatar = new QLabel(initiale);
  m_badgeAvatar->setFixedSize(42, 42);
  m_badgeAvatar->setAlignment(Qt::AlignCenter);
  m_badgeAvatar->setStyleSheet(
      QString("QLabel { background: %1; border-radius: 21px; color: white; "
              "font-size: 18px; font-weight: bold; border: none; }")
          .arg(avatarColor));
  mainRow->addWidget(m_badgeAvatar, 0, Qt::AlignVCenter);

  // â”€â”€ Nom + ID Â· ConnectÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QVBoxLayout *infoCol = new QVBoxLayout();
  infoCol->setSpacing(2);
  infoCol->setContentsMargins(0, 0, 0, 0);

  m_badgeName = new QLabel(m_connectedUserName);
    m_badgeName->setStyleSheet(
      "QLabel { font-size: 13px; font-weight: bold; color: #F2F4F7; "
      "background: transparent; border: none; }");

  QHBoxLayout *subRow = new QHBoxLayout();
  subRow->setSpacing(3);
  subRow->setContentsMargins(0, 0, 0, 0);

  m_badgeId = new QLabel(QString("ID %1").arg(m_connectedUserId));
  m_badgeId->setStyleSheet("QLabel { font-size: 10px; color: #9AA3AE; "
                           "background: transparent; border: none; }");
  QLabel *dotSep = new QLabel("Â·");
  dotSep->setStyleSheet("QLabel { font-size: 10px; color: #9AA3AE; background: "
                        "transparent; border: none; }");
  QLabel *connLbl = new QLabel("ConnectÃ©");
  connLbl->setStyleSheet("QLabel { font-size: 10px; color: #23A559; "
                         "background: transparent; border: none; }");

  subRow->addWidget(m_badgeId);
  subRow->addWidget(dotSep);
  subRow->addWidget(connLbl);
  subRow->addStretch();

  infoCol->addWidget(m_badgeName);
  infoCol->addLayout(subRow);
  mainRow->addLayout(infoCol);
  mainRow->addStretch();

  // â”€â”€ Badge rÃ´le colorÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  m_badgeRole = new QLabel(m_connectedUserRole);
    m_badgeRole->setStyleSheet(
      QString(
        "QLabel { background: transparent; color: %1; border: 2px solid %1; "
        "border-radius: 10px; padding: 3px 10px; font-size: 10px; "
        "font-weight: bold; }")
        .arg(avatarColor));
  mainRow->addWidget(m_badgeRole, 0, Qt::AlignVCenter);

  mainRow->addSpacing(6);

  // â”€â”€ Bouton DÃ©connexion â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  m_btnDeconnexion = new QPushButton("Deconnexion");
  m_btnDeconnexion->setFixedHeight(32);
  m_btnDeconnexion->setProperty("role", "danger");
  connect(m_btnDeconnexion, &QPushButton::clicked, this,
          &fishingport::onDeconnexion);
  mainRow->addWidget(m_btnDeconnexion, 0, Qt::AlignVCenter);
  m_badgeHost->setVisible(true);
  // badge cachÃ© par dÃ©faut, affichÃ© seulement sur page employÃ©s
}

void fishingport::onDeconnexion() {
  int rep = QMessageBox::question(this, "DÃ©connexion",
                                  "Voulez-vous vous dÃ©connecter ?",
                                  QMessageBox::Yes | QMessageBox::No);
  if (rep != QMessageBox::Yes)
    return;

  // RÃ©initialiser la session
  m_connectedUserName.clear();
  m_connectedUserRole.clear();
  m_connectedUserId = -1;

  if (m_badgeHostLayout)
    clearLayout(m_badgeHostLayout);
  if (m_badgeHost)
    m_badgeHost->hide();
  m_badgeWidget = nullptr;

  // Retourner Ã  l'Ã©cran de login
  showLoginDialog();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  CRÃ‰ER LA CARTE DE SESSION EMPLOYÃ‰
void fishingport::showPasswordResetDialog() {
  QDialog resetDialog(this);
  resetDialog.setWindowTitle("RÃ©initialisation du mot de passe");
  resetDialog.setObjectName("authDialog");
  resetDialog.setModal(true);
  resetDialog.setFixedSize(400, 300);

  QVBoxLayout *layout = new QVBoxLayout(&resetDialog);
  layout->setContentsMargins(30, 30, 30, 30);
  layout->setSpacing(20);

  // Titre
  QLabel *titleLabel = new QLabel("RÃ©initialisation du mot de passe");
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setObjectName("authTitle");
  layout->addWidget(titleLabel);

  // Instructions
  QLabel *infoLabel = new QLabel("Entrez votre ID employÃ© pour recevoir les "
                                 "instructions de rÃ©initialisation.");
  infoLabel->setAlignment(Qt::AlignCenter);
  infoLabel->setObjectName("authSubtitle");
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  // Champ ID employÃ©
  QLabel *idLabel = new QLabel("ID EmployÃ©:");
  idLabel->setProperty("role", "cardTitle");
  layout->addWidget(idLabel);

  QLineEdit *idEdit = new QLineEdit();
  idEdit->setPlaceholderText("Entrez votre ID employÃ©...");
  idEdit->setFixedHeight(40);
  layout->addWidget(idEdit);

  layout->addStretch();

  // Boutons
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->setSpacing(10);

  QPushButton *btnEnvoyer = new QPushButton("Envoyer");
  QPushButton *btnAnnuler = new QPushButton("Annuler");
  btnEnvoyer->setProperty("role", "primary");
  btnAnnuler->setProperty("role", "ghost");

  buttonLayout->addWidget(btnAnnuler);
  buttonLayout->addWidget(btnEnvoyer);
  layout->addLayout(buttonLayout);

  // Connexions
  connect(btnEnvoyer, &QPushButton::clicked, [&]() {
    QString idText = idEdit->text().trimmed();
    if (idText.isEmpty()) {
      QMessageBox::warning(&resetDialog, "Erreur",
                           "Veuillez entrer votre ID employÃ©.");
      return;
    }

    // VÃ©rifier si l'ID existe dans la base de donnÃ©es
    QSqlQuery query;
    query.prepare("SELECT NOME, CONTACT FROM EMPLOYE WHERE ID_EMPLOYE = ?");
    query.addBindValue(idText.toInt());

    if (query.exec() && query.next()) {
      QString nom = query.value(0).toString();
      QString contactField = query.value(1).toString();

      // Extraire uniquement le numÃ©ro de tÃ©lÃ©phone du champ CONTACT (qui
      // contient tÃ©lÃ©phone|email)
      QString phoneNumber = contactField.split("|").first().trimmed();
      // Supprimer tous les espaces du numÃ©ro (Twilio n'accepte pas les espaces)
      phoneNumber.remove(' ');
      // Normaliser : si le numÃ©ro ne commence pas par +216, l'ajouter
      if (!phoneNumber.startsWith("+")) {
        if (phoneNumber.startsWith("216"))
          phoneNumber = "+" + phoneNumber;
        else
          phoneNumber = "+216" + phoneNumber;
      }
      qDebug() << "Champ CONTACT original:" << contactField;
      qDebug() << "NumÃ©ro de tÃ©lÃ©phone extrait:" << phoneNumber;

      // GÃ©nÃ©rer un code de rÃ©initialisation
      QString resetCode = generateResetCode();

      // Tenter l'envoi SMS via Twilio (non bloquant)
      bool smsSent = sendPasswordResetSMS(phoneNumber, nom, resetCode);

      if (smsSent) {
        QMessageBox::information(
            &resetDialog, "SMS envoyÃ©",
            QString("Un SMS avec le code de rÃ©initialisation a Ã©tÃ© envoyÃ© au "
                    "%1.\n\n"
                    "Veuillez consulter vos messages.")
                .arg(phoneNumber));

        // â”€â”€ Ã‰tape 2 : Saisie du code de vÃ©rification â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        QDialog codeDialog(&resetDialog);
        codeDialog.setWindowTitle("VÃ©rification du code");
        codeDialog.setModal(true);
        codeDialog.setFixedSize(380, 220);
        codeDialog.setStyleSheet(
            "QDialog { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 #87CEEB, stop:0.5 #5BB8D4, stop:1 #29B6F6); }"
            "QLabel { border:none; background:transparent; color:white; }"
            "QLineEdit { background:white; color:#1A237E; border:2px solid "
            "#81D4FA; "
            "border-radius:8px; padding:8px 12px; font-size:13px; }"
            "QPushButton { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 #1565C0,stop:1 #0288D1); color:white; border-radius:8px; "
            "font-size:12px; font-weight:bold; border:none; padding:8px 16px; "
            "}");

        QVBoxLayout *codeL = new QVBoxLayout(&codeDialog);
        codeL->setContentsMargins(30, 25, 30, 25);
        codeL->setSpacing(12);

        QLabel *codeTitleL = new QLabel("Entrez le code reÃ§u par SMS :");
        codeTitleL->setStyleSheet(
            "font-size:13px; font-weight:bold; color:white;");
        codeL->addWidget(codeTitleL);

        QLineEdit *codeEdit = new QLineEdit();
        codeEdit->setPlaceholderText("Code Ã  6 chiffres...");
        codeEdit->setFixedHeight(42);
        codeEdit->setMaxLength(6);
        codeL->addWidget(codeEdit);

        QLabel *codeErrL = new QLabel("");
        codeErrL->setStyleSheet("color:#FFCDD2; font-size:11px;");
        codeL->addWidget(codeErrL);

        codeL->addStretch();

        QHBoxLayout *codeBtnL = new QHBoxLayout();
        QPushButton *btnValiderCode = new QPushButton("Valider");
        QPushButton *btnAnnulerCode = new QPushButton("Annuler");
        codeBtnL->addWidget(btnAnnulerCode);
        codeBtnL->addWidget(btnValiderCode);
        codeL->addLayout(codeBtnL);

        connect(btnAnnulerCode, &QPushButton::clicked, &codeDialog,
                &QDialog::reject);
        connect(btnValiderCode, &QPushButton::clicked, [&]() {
          if (codeEdit->text().trimmed() == resetCode) {
            codeDialog.accept();
          } else {
            codeErrL->setText("Code incorrect. Veuillez rÃ©essayer.");
            codeEdit->clear();
            codeEdit->setFocus();
          }
        });
        connect(codeEdit, &QLineEdit::returnPressed, btnValiderCode,
                &QPushButton::click);

        codeDialog.move(
            QGuiApplication::primaryScreen()->availableGeometry().center() -
            codeDialog.rect().center());

        if (codeDialog.exec() != QDialog::Accepted) {
          return; // annulÃ©
        }

        // â”€â”€ Ã‰tape 3 : Nouveau mot de passe â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        QDialog mdpDialog(&resetDialog);
        mdpDialog.setWindowTitle("Nouveau mot de passe");
        mdpDialog.setModal(true);
        mdpDialog.setFixedSize(380, 260);
        mdpDialog.setStyleSheet(codeDialog.styleSheet());

        QVBoxLayout *mdpL = new QVBoxLayout(&mdpDialog);
        mdpL->setContentsMargins(30, 25, 30, 25);
        mdpL->setSpacing(12);

        QLabel *mdpTitleL = new QLabel("Choisissez un nouveau mot de passe :");
        mdpTitleL->setStyleSheet(
            "font-size:13px; font-weight:bold; color:white;");
        mdpL->addWidget(mdpTitleL);

        QLineEdit *mdpEdit = new QLineEdit();
        mdpEdit->setPlaceholderText("Nouveau mot de passe...");
        mdpEdit->setEchoMode(QLineEdit::Password);
        mdpEdit->setFixedHeight(42);
        mdpL->addWidget(mdpEdit);

        QLineEdit *mdpConfirmEdit = new QLineEdit();
        mdpConfirmEdit->setPlaceholderText("Confirmer le mot de passe...");
        mdpConfirmEdit->setEchoMode(QLineEdit::Password);
        mdpConfirmEdit->setFixedHeight(42);
        mdpL->addWidget(mdpConfirmEdit);

        QLabel *mdpErrL = new QLabel("");
        mdpErrL->setStyleSheet("color:#FFCDD2; font-size:11px;");
        mdpL->addWidget(mdpErrL);

        mdpL->addStretch();

        QHBoxLayout *mdpBtnL = new QHBoxLayout();
        QPushButton *btnSaveMdp = new QPushButton("Enregistrer");
        QPushButton *btnAnnulerMdp = new QPushButton("Annuler");
        mdpBtnL->addWidget(btnAnnulerMdp);
        mdpBtnL->addWidget(btnSaveMdp);
        mdpL->addLayout(mdpBtnL);

        connect(btnAnnulerMdp, &QPushButton::clicked, &mdpDialog,
                &QDialog::reject);
        connect(btnSaveMdp, &QPushButton::clicked, [&]() {
          QString newMdp = mdpEdit->text();
          QString confirmMdp = mdpConfirmEdit->text();
          if (newMdp.isEmpty()) {
            mdpErrL->setText("Le mot de passe ne peut pas Ãªtre vide.");
            return;
          }
          if (newMdp != confirmMdp) {
            mdpErrL->setText("Les mots de passe ne correspondent pas.");
            mdpConfirmEdit->clear();
            mdpConfirmEdit->setFocus();
            return;
          }
          // Mettre Ã  jour le mot de passe en BD
          QSqlQuery updateQuery;
          updateQuery.prepare(
              "UPDATE EMPLOYE SET MOT_DE_PASSE = :mdp WHERE ID_EMPLOYE = :id");
          updateQuery.bindValue(":mdp", newMdp);
          updateQuery.bindValue(":id", idText.toInt());
          if (updateQuery.exec()) {
            mdpDialog.accept();
          } else {
            mdpErrL->setText("Erreur lors de la mise Ã  jour. RÃ©essayez.");
          }
        });
        connect(mdpConfirmEdit, &QLineEdit::returnPressed, btnSaveMdp,
                &QPushButton::click);

        mdpDialog.move(
            QGuiApplication::primaryScreen()->availableGeometry().center() -
            mdpDialog.rect().center());

        if (mdpDialog.exec() == QDialog::Accepted) {
          QMessageBox::information(
              &resetDialog, "SuccÃ¨s",
              "Votre mot de passe a Ã©tÃ© rÃ©initialisÃ© avec succÃ¨s.\nVous pouvez "
              "maintenant vous connecter.");
          resetDialog.accept();
        }

      } else {
        QMessageBox::critical(
            &resetDialog, "Erreur d'envoi",
            QString("Impossible d'envoyer le SMS au numÃ©ro : %1\n\n"
                    "VÃ©rifiez que le numÃ©ro est au format international (ex: "
                    "+213XXXXXXXXX).\n"
                    "Consultez les logs Qt pour plus de dÃ©tails.")
                .arg(phoneNumber));
      }
    } else {
      QMessageBox::critical(
          &resetDialog, "Erreur",
          "ID employÃ© non trouvÃ©. Veuillez vÃ©rifier votre ID.");
      idEdit->clear();
      idEdit->setFocus();
    }
  });

  connect(btnAnnuler, &QPushButton::clicked, &resetDialog, &QDialog::reject);
  connect(idEdit, &QLineEdit::returnPressed, btnEnvoyer,
          [&]() { btnEnvoyer->click(); });

  // Centrer et afficher
  resetDialog.move(
      QGuiApplication::primaryScreen()->availableGeometry().center() -
      resetDialog.rect().center());
  resetDialog.exec();
}

QString fishingport::generateResetCode() {
  // GÃ©nÃ©rer un code alÃ©atoire de 6 chiffres
  int code = QRandomGenerator::global()->bounded(100000, 999999);
  return QString::number(code);
}

bool fishingport::sendPasswordResetSMS(const QString &phoneNumber,
                                       const QString &employeeName,
                                       const QString &resetCode) {
  qDebug() << "Tentative d'envoi SMS Ã :" << phoneNumber << "pour"
           << employeeName;

  // Set credentials in the local environment; never commit live credentials.
  QString accountSid = qEnvironmentVariable("TWILIO_ACCOUNT_SID");
  QString authToken = qEnvironmentVariable("TWILIO_AUTH_TOKEN");
  QString twilioNumber = qEnvironmentVariable("TWILIO_FROM_NUMBER");
  if (accountSid.isEmpty() || authToken.isEmpty() || twilioNumber.isEmpty()) {
    qWarning() << "Twilio environment variables are missing.";
    return false;
  }

  // Le numÃ©ro est dÃ©jÃ  normalisÃ© avec l'indicatif pays avant l'appel
  QString normalizedPhone = phoneNumber.trimmed();
  if (!normalizedPhone.startsWith("+")) {
    normalizedPhone = "+" + normalizedPhone;
  }
  qDebug() << "NumÃ©ro normalisÃ©:" << normalizedPhone;

  // URL de l'API Twilio
  QString url =
      QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json")
          .arg(accountSid);

  // CrÃ©er le gestionnaire de rÃ©seau
  QNetworkAccessManager *manager = new QNetworkAccessManager(this);

  // PrÃ©parer la requÃªte
  QNetworkRequest request;
  request.setUrl(QUrl(url));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");

  // Authentification Basic
  QString credentials = accountSid + ":" + authToken;
  QByteArray authData = credentials.toUtf8().toBase64();
  request.setRawHeader("Authorization", "Basic " + authData);

  // PrÃ©parer les donnÃ©es du message (URL-encodage obligatoire pour Twilio)
  QString message = QString("Port Management - Code de reinitialisation: %1. "
                            "Ce code expire dans 15 minutes.")
                        .arg(resetCode);

  QByteArray postData;
  postData.append("To=" + QUrl::toPercentEncoding(normalizedPhone) + "&");
  postData.append("From=" + QUrl::toPercentEncoding(twilioNumber) + "&");
  postData.append("Body=" + QUrl::toPercentEncoding(message));

  // Envoyer la requÃªte
  QNetworkReply *reply = manager->post(request, postData);

  // Attendre la rÃ©ponse (synchrone pour la dÃ©mo)
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  // VÃ©rifier la rÃ©ponse
  bool success = false;
  if (reply->error() == QNetworkReply::NoError) {
    QByteArray response = reply->readAll();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);

    if (jsonDoc.isObject()) {
      QJsonObject jsonObj = jsonDoc.object();

      if (jsonObj.contains("sid")) {
        success = true;
        qDebug() << "SMS envoyÃ© avec succÃ¨s - SID:"
                 << jsonObj["sid"].toString();
      } else if (jsonObj.contains("message")) {
        qDebug() << "Erreur Twilio:" << jsonObj["message"].toString();
        if (jsonObj.contains("code")) {
          qDebug() << "Code erreur:" << jsonObj["code"].toString();
        }
      }
    } else {
    }
  } else {
    qDebug() << "Erreur rÃ©seau:" << reply->errorString();
    qDebug()
        << "Code HTTP:"
        << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray errorResponse = reply->readAll();
  }

  reply->deleteLater();
  manager->deleteLater();

  return success;
}

// ==================================================================
//  DROITS PAR RÃ”LE
// ==================================================================
void fishingport::appliquerDroitsRole() {
  // RÃ´les :
  //   "Administration"  â†’ Ajouter + Modifier + Supprimer
  //   "Responsable RH"  â†’ Ajouter + Modifier  (pas Supprimer)
  //   autres / "EmployÃ©"â†’ Consulter seulement (pas Ajouter, Modifier,
  //   Supprimer)

  bool peutAjouter = false;
  bool peutModifier = false;
  bool peutSupprimer = false;

  QString role = m_connectedUserRole;

  if (role == "Administration" || role == "admin") {
    peutAjouter = true;
    peutModifier = true;
    peutSupprimer = true;
  } else if (role == "Responsable RH") {
    peutAjouter = true;
    peutModifier = true;
    peutSupprimer = false;
  } else {
    // EmployÃ© ou autre : lecture seule
    peutAjouter = false;
    peutModifier = false;
    peutSupprimer = false;
  }

  // Appliquer sur les boutons de la page EmployÃ©s
  ui->employe_btnAjouter->setVisible(peutAjouter);
  ui->employe_btnModifier->setVisible(peutModifier);
  ui->employe_btnSupprimer->setVisible(peutSupprimer);

  // Griser les champs du formulaire si pas de droits d'Ã©criture
  bool readonly = !peutAjouter && !peutModifier;
  ui->employe_lineEdit_nom->setReadOnly(readonly);
  ui->employe_lineEdit_licence->setReadOnly(readonly);
  ui->employe_lineEdit_contact->setReadOnly(readonly);
  ui->employe_lineEdit_email->setReadOnly(readonly);
  ui->employe_lineEdit_mdp->setReadOnly(readonly);
  ui->employe_comboBox_role->setEnabled(!readonly);
}

// ==================================================================
//  BATEAUX
// ==================================================================
void fishingport::loadBateaux() {
  Bateau::loadTable(model);
  ui->tableBateaux->setModel(model);
  ui->tableBateaux->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->tableBateaux->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
  ui->tableBateaux->verticalHeader()->setVisible(false);

  int enMer = Bateau::countStatus("En mer");
  int aQuai = Bateau::countStatus("A quai");
  int maint = Bateau::countStatus("En maintenance");
  int total = enMer + aQuai + maint;

  ui->lblStatEnMer->setText("En mer : " + QString::number(enMer));
  ui->lblStatAQuai->setText("A quai : " + QString::number(aQuai));
  ui->lblStatMaintenance->setText("En maintenance : " + QString::number(maint));
  ui->lblStatTotal->setText("Total : " + QString::number(total));
}

void fishingport::applyFilters() {
  QString search = ui->lineEdit->text().trimmed();
  QString status = ui->comboBox->currentText();
  QString type = ui->comboBoxType->currentText();
  int triIndex = ui->comboTri->currentIndex();

  Bateau::filterTable(model, search, status, type, triIndex);

  ui->tableBateaux->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->tableBateaux->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
  ui->tableBateaux->verticalHeader()->setVisible(false);
}

void fishingport::loadStatistiques() {
  int enMer = Bateau::countStatus("En mer");
  int aQuai = Bateau::countStatus("A quai");
  int maint = Bateau::countStatus("En maintenance");
  int total = enMer + aQuai + maint;

  ui->lblCardMerCount->setText(QString::number(enMer));
  ui->lblCardQuaiCount->setText(QString::number(aQuai));
  ui->lblCardMaintCount->setText(QString::number(maint));
  ui->lblCardTotalCount->setText(QString::number(total));

  ui->lblStatEnMer->setText("En mer : " + QString::number(enMer));
  ui->lblStatAQuai->setText("A quai : " + QString::number(aQuai));
  ui->lblStatMaintenance->setText("En maintenance : " + QString::number(maint));
  ui->lblStatTotal->setText("Total : " + QString::number(total));

  // Modern QtCharts visualization
  ui->tableStats->hide();
  ui->chartLabel->hide();

  QChartView *chartView = ui->pageStatistiques->findChild<QChartView*>("myChartView");
  if (!chartView) {
      chartView = new QChartView(ui->pageStatistiques);
      chartView->setObjectName("myChartView");
      chartView->setGeometry(ui->chartLabel->geometry());
      chartView->setRenderHint(QPainter::Antialiasing);
      chartView->show();
  }

  QPieSeries *series = new QPieSeries();

  QPieSlice *sliceMer = series->append("En mer", enMer);
  sliceMer->setBrush(QColor(0xFF48CAE4));
  sliceMer->setLabelVisible(true);

  QPieSlice *sliceQuai = series->append("Ã€ quai", aQuai);
  sliceQuai->setBrush(QColor(0xFF2DC653));
  sliceQuai->setLabelVisible(true);

  QPieSlice *sliceMaint = series->append("Maintenance", maint);
  sliceMaint->setBrush(QColor(0xFFF4A825));
  sliceMaint->setLabelVisible(true);

  // Explode the largest slice
  int maxVal = qMax(qMax(enMer, aQuai), maint);
  if (enMer == maxVal) sliceMer->setExploded();
  else if (aQuai == maxVal) sliceQuai->setExploded();
  else if (maint == maxVal) sliceMaint->setExploded();

  QChart *chart = new QChart();
  chart->addSeries(series);
  chart->setTitle("RÃ©partition de la Flotte");
  chart->setAnimationOptions(QChart::AllAnimations);
  chart->legend()->setAlignment(Qt::AlignRight);
  chart->legend()->setFont(QFont("Arial", 10, QFont::Bold));

  chartView->setChart(chart);
}

void fishingport::on_pushButton_clicked() {
  QString path = QFileDialog::getSaveFileName(
      this, "Exporter CSV", "bateaux.csv", "CSV Files (*.csv)");
  if (path.isEmpty())
    return;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::critical(this, "Erreur", "Impossible d'ouvrir le fichier.");
    return;
  }
  QTextStream out(&file);
  out << "\xEF\xBB\xBF";
  out << "Carte Grise;Nom;Statut;Date Fabrication;Type\n";
  int rows = model->rowCount();
  int cols = model->columnCount();
  for (int r = 0; r < rows; ++r) {
    QStringList fields;
    for (int c = 0; c < cols; ++c) {
      QString val = model->data(model->index(r, c)).toString();
      val.replace("\"", "\"\"");
      if (val.contains(';') || val.contains('"') || val.contains('\n'))
        val = "\"" + val + "\"";
      fields << val;
    }
    out << fields.join(';') << "\n";
  }
  file.close();
  QMessageBox::information(
      this, "Export CSV",
      QString("Fichier exportÃ© avec succÃ¨s !\n%1 enregistrement(s).")
          .arg(rows));
}

void fishingport::on_btnNavDashboard_clicked() {
  ui->groupBox_employe_form->hide();
  if (m_btnNavDashboard)
    m_btnNavDashboard->setChecked(true);
  setActivePage(m_dashboardPage, "Dashboard", "Vue operationnelle");
  updateDashboard();
}

void fishingport::on_btnNavBateaux_clicked() {
  ui->groupBox_employe_form->hide();
  ui->btnNavBateaux->setChecked(true);
  setActivePage(ui->page, "Bateaux", "Flotte et etats");
  loadBateaux();
  ui->stackedWidget_2->setCurrentIndex(0);
}

void fishingport::on_btnRefreshStats_clicked() { loadStatistiques(); }

void fishingport::on_pushButton_2_clicked() {
  ui->lineEdit->clear();
  ui->comboBox->setCurrentIndex(0);
  ui->comboBoxType->setCurrentIndex(0);
  ui->comboTri->setCurrentIndex(0);
  loadBateaux();
}

void fishingport::on_btnAjouter_clicked() {
  m_editMode = false;
  ui->lineEditNum->clear();
  ui->lineEditNom->clear();
  ui->comboStatus->setCurrentIndex(0);
  ui->comboType->setCurrentIndex(0);
  ui->dateEdit->setDate(QDate::currentDate());
  ui->stackedWidget_2->setCurrentIndex(1);
}

void fishingport::on_tableBateaux_clicked(const QModelIndex &index) {
  int row = index.row();
  m_originalNum = model->data(model->index(row, 0)).toString();
  ui->lineEditNum->setText(m_originalNum);
  ui->lineEditNom->setText(model->data(model->index(row, 1)).toString());
  ui->comboStatus->setCurrentText(model->data(model->index(row, 2)).toString());
  ui->dateEdit->setDate(model->data(model->index(row, 3)).toDate());
  ui->comboType->setCurrentText(model->data(model->index(row, 4)).toString());
}

void fishingport::on_btnEnregistrer_clicked() {
  QString num = ui->lineEditNum->text().trimmed();
  QString nom = ui->lineEditNom->text().trimmed();
  QDate date = ui->dateEdit->date();

  QRegularExpression re("^\\d{8}$");
  if (!re.match(num).hasMatch()) {
    QMessageBox::warning(
        this, "Erreur de saisie",
        "Le numÃ©ro de carte grise doit contenir exactement 8 chiffres.");
    return;
  }
  if (nom.isEmpty()) {
    QMessageBox::warning(this, "Erreur de saisie",
                         "Le nom du bateau ne peut pas Ãªtre vide.");
    return;
  }
  if (date > QDate::currentDate()) {
    QMessageBox::warning(
        this, "Erreur de saisie",
        "La date de fabrication ne peut pas Ãªtre dans le futur.");
    return;
  }

  Bateau b(num, nom, ui->comboStatus->currentText(), date,
           ui->comboType->currentText());
  if (m_editMode) {
    // In edit mode, num == m_originalNum (field is read-only), so pass it as
    // oldNum
    if (!b.modifier(m_originalNum)) {
      QMessageBox::critical(this, "Erreur",
                            "Erreur lors de la modification.");
      return;
    }
    QMessageBox::information(this, "Mis Ã  jour",
                             "Bateau mis Ã  jour avec succÃ¨s.");
    m_editMode = false;
    m_originalNum.clear();
    // Restore lineEditNum to editable state
    ui->lineEditNum->setReadOnly(false);
    ui->lineEditNum->setStyleSheet("");
  } else {
    if (Bateau::existe(num)) {
      QMessageBox::warning(this, "Erreur de saisie", "Ce bateau existe dÃ©jÃ .");
      return;
    }
    if (!b.ajouter()) {
      QMessageBox::critical(this, "Erreur", "Erreur lors de l'ajout.");
      return;
    }
    QMessageBox::information(this, "SuccÃ¨s", "Bateau ajoutÃ© avec succÃ¨s.");
  }
  loadBateaux();
  ui->stackedWidget_2->setCurrentIndex(0);
}

void fishingport::on_btnModify_clicked() {
  if (ui->lineEditNum->text().isEmpty()) {
    QMessageBox::warning(
        this, "Attention",
        "Veuillez sÃ©lectionner un bateau dans le tableau d'abord.");
    return;
  }
  m_originalNum = ui->lineEditNum->text();
  m_editMode = true;
  // Lock the primary key field â€” changing it would break FK constraints in
  // avoir/controler
  ui->lineEditNum->setReadOnly(true);
  ui->lineEditNum->setStyleSheet(
      "QLineEdit { background-color: #15161A; border: 1px dashed #3A3D45; "
      "color: #9AA3AE; border-radius:7px; padding:8px 10px; }");
  ui->stackedWidget_2->setCurrentIndex(1);
}

void fishingport::on_btnDelete_clicked() {
  if (ui->lineEditNum->text().isEmpty()) {
    QMessageBox::warning(
        this, "Attention",
        "Veuillez sÃ©lectionner un bateau dans le tableau d'abord.");
    return;
  }
  if (!Bateau::supprimer(ui->lineEditNum->text())) {
    QMessageBox::critical(this, "Erreur", "Erreur lors de la suppression.");
    return;
  }
  QMessageBox::information(this, "SupprimÃ©", "Bateau supprimÃ© avec succÃ¨s.");
  ui->lineEditNum->clear();
  loadBateaux();
}

void fishingport::on_btnConsulter_clicked() {
  if (ui->lineEditNum->text().isEmpty()) {
    QMessageBox::warning(
        this, "Consulter",
        "Veuillez sÃ©lectionner un bateau dans le tableau d'abord.");
    return;
  }
  QString details = "<b>DÃ©tails du Bateau</b><br><br>"
                    "<table cellspacing='6'>"
                    "<tr><td><b>Carte Grise :</b></td><td>" +
                    ui->lineEditNum->text() +
                    "</td></tr>"
                    "<tr><td><b>Nom :</b></td><td>" +
                    ui->lineEditNom->text() +
                    "</td></tr>"
                    "<tr><td><b>Statut :</b></td><td>" +
                    ui->comboStatus->currentText() +
                    "</td></tr>"
                    "<tr><td><b>Type :</b></td><td>" +
                    ui->comboType->currentText() +
                    "</td></tr>"
                    "<tr><td><b>Date Fabrication :</b></td><td>" +
                    ui->dateEdit->date().toString("dd/MM/yyyy") +
                    "</td></tr>"
                    "</table>";
  QMessageBox msg(this);
  msg.setWindowTitle("Consulter â€” Fiche Bateau");
  msg.setTextFormat(Qt::RichText);
  msg.setText(details);
  msg.setIcon(QMessageBox::Information);
  msg.exec();
}

void fishingport::on_btnAnnuler_clicked() {
  m_editMode = false;
  m_originalNum.clear();
  // Restore lineEditNum to editable state
  ui->lineEditNum->setReadOnly(false);
  ui->lineEditNum->setStyleSheet("");
  ui->stackedWidget_2->setCurrentIndex(0);
}

void fishingport::on_btnActionStats_clicked() {
  loadStatistiques();
  ui->stackedWidget_2->setCurrentIndex(2);
  if (m_headerSubtitle)
    m_headerSubtitle->setText("Bateaux â€¢ Statistiques");
}

void fishingport::on_btnRetour_clicked() { on_btnNavBateaux_clicked(); }

void fishingport::on_btnSendChat_clicked() {
  QString question = ui->txtChatInput->text().trimmed();
  if (question.isEmpty())
    return;
  ui->txtChatHistory->append("<b>Vous:</b> " + question);
  ui->txtChatInput->clear();
  sendToOllama(question);
}

void fishingport::sendToOllama(const QString &question) {
  QString context = "Voici la base de donnÃ©es actuelle des bateaux du port:\n";
  QSqlQuery query(
      "SELECT numero_carte_grise, nomB, statusB, id_emplacement FROM bateau");
  while (query.next()) {
    context +=
        "- Bateau '" + query.value("nomB").toString() +
        "' (Carte Grise: " + query.value("numero_carte_grise").toString() +
        "), Statut: " + query.value("statusB").toString() +
        ", Emplacement ID: " + query.value("id_emplacement").toString() + "\n";
  }
  QString systemPrompt = "Tu es l'assistant intelligent du Port de PÃªche. "
                         "Tu dois rÃ©pondre briÃ¨vement et de maniÃ¨re concise "
                         "aux questions de l'utilisateur "
                         "en te basant UNIQUEMENT sur ces donnÃ©es en temps "
                         "rÃ©el. Ne pose pas de questions toi-mÃªme:\n\n" +
                         context;
  QJsonObject json;
  json["model"] = "mistral";
  json["system"] = systemPrompt;
  json["prompt"] = question;
  json["stream"] = true;
  QJsonDocument doc(json);
  QByteArray data = doc.toJson();
  QUrl url("http://localhost:11434/api/generate");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  ui->txtChatHistory->append("<b>Assistant:</b> ");
  QNetworkReply *reply = networkManager->post(request, data);
  connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
    while (reply->canReadLine()) {
      QByteArray line = reply->readLine().trimmed();
      if (line.isEmpty())
        continue;
      QJsonDocument streamDoc = QJsonDocument::fromJson(line);
      QJsonObject streamObj = streamDoc.object();
      if (streamObj.contains("response")) {
        QString chunk = streamObj["response"].toString();
        QTextCursor cursor = ui->txtChatHistory->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->txtChatHistory->setTextCursor(cursor);
        ui->txtChatHistory->insertPlainText(chunk);
      }
    }
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (reply->error() != QNetworkReply::NoError)
      ui->txtChatHistory->append("<br><b style='color:red;'>Erreur:</b> "
                                 "Impossible de joindre Ollama.");
    ui->txtChatHistory->append("<br>");
    reply->deleteLater();
  });
}

void fishingport::on_btnPrediction_clicked() {
  if (ui->lineEditNum->text().isEmpty()) {
    QMessageBox::warning(
        this, "PrÃ©diction IA",
        "Veuillez sÃ©lectionner un bateau dans le tableau d'abord.");
    return;
  }
  QString num = ui->lineEditNum->text().trimmed();
  QString nom = ui->lineEditNom->text().trimmed();
  QString type = ui->comboType->currentText();
  QDate dateFab = ui->dateEdit->date();
  int nbSorties = Bateau::countSorties(num);
  int ageYears = dateFab.daysTo(QDate::currentDate()) / 365;
  int ageMonths = (dateFab.daysTo(QDate::currentDate()) % 365) / 30;
  ui->lblPredBoatName->setText("Nom : " + nom);
  ui->lblPredBoatType->setText("Type : " + type);
  ui->lblPredBoatDate->setText("Date de fabrication : " +
                               dateFab.toString("dd/MM/yyyy"));
  ui->lblPredBoatAge->setText(
      QString("Ã‚ge : %1 ans et %2 mois").arg(ageYears).arg(ageMonths));
  ui->lblPredBoatSorties->setText("Nombre de sorties : " +
                                  QString::number(nbSorties));
  ui->txtPredictionResult->clear();
  ui->stackedWidget_2->setCurrentIndex(3);
}

void fishingport::on_btnLaunchPrediction_clicked() {
  QString nom = ui->lblPredBoatName->text().mid(6);
  QString type = ui->lblPredBoatType->text().mid(7);
  QString dateFab = ui->lblPredBoatDate->text().mid(23);
  QString age = ui->lblPredBoatAge->text().mid(6);
  QString sorties = ui->lblPredBoatSorties->text().mid(21);
  ui->txtPredictionResult->clear();
  ui->lblPredJsonStatus->setText("Ã‰tat GÃ©nÃ©ral: En cours d'analyse...");
  ui->lblPredJsonYears->setText("-- ans");
  ui->progressBarHealth->setValue(0);
  ui->txtPredictionResult->append("<b>ðŸ”® Analyse en cours...</b><br>");

  QString prompt =
      "Tu es un expert maritime. Voici les informations d'un bateau :\n"
      "- Nom : " +
      nom + "\n- Type : " + type + "\n- Date de fabrication : " + dateFab +
      "\n- Ã‚ge actuel : " + age +
      "\n- Nombre total de sorties en mer : " + sorties +
      "\n\nRÃ©ponds UNIQUEMENT avec un objet JSON strict :\n"
      "{ \"score_sante\": (0-100), \"statut\": (\"En bon Ã©tat\"|\"Ã€ "
      "surveiller\"|\"Fin de vie\"), "
      "\"annees_restantes\": (entier), \"raisonnement\": (chaÃ®ne en franÃ§ais) "
      "}";

  QJsonObject json;
  json["model"] = "mistral";
  json["prompt"] = prompt;
  json["stream"] = false;
  json["format"] = "json";
  QJsonDocument doc(json);
  QNetworkRequest request(QUrl("http://localhost:11434/api/generate"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  QString *buf = new QString();
  QNetworkReply *reply = networkManager->post(request, doc.toJson());
  connect(reply, &QNetworkReply::readyRead, this,
          [reply, buf]() { *buf += reply->readAll(); });
  connect(reply, &QNetworkReply::finished, this, [this, reply, buf]() {
    if (reply->error() != QNetworkReply::NoError) {
      ui->txtPredictionResult->setText(
          "<b style='color:red;'>Erreur:</b> Impossible de joindre Ollama.");
    } else {
      QJsonObject resp = QJsonDocument::fromJson(buf->toUtf8()).object();
      QJsonDocument inner =
          QJsonDocument::fromJson(resp["response"].toString().toUtf8());
      if (inner.isObject()) {
        QJsonObject ai = inner.object();
        int health = ai["score_sante"].toInt();
        QString stat = ai["statut"].toString();
        int years = ai["annees_restantes"].toInt();
        QString reason = ai["raisonnement"].toString();
        ui->progressBarHealth->setValue(health);
        QString chunk = health > 66   ? "#2DC653"
                        : health > 33 ? "#F4A825"
                                      : "#E63946";
        ui->progressBarHealth->setStyleSheet(
            QString("QProgressBar { border-radius:6px; background:#0A1628; "
                    "color:white; font-weight:bold; }"
                    "QProgressBar::chunk { border-radius:5px; background:%1; }")
                .arg(chunk));
        ui->lblPredJsonStatus->setText("Ã‰tat GÃ©nÃ©ral: " + stat);
        ui->lblPredJsonYears->setText(QString::number(years) + " ans");
        ui->txtPredictionResult->setText(reason);
      }
    }
    delete buf;
    reply->deleteLater();
  });
}

void fishingport::on_btnPredictionRetour_clicked() {
  on_btnNavBateaux_clicked();
}

// ==================================================================
//  EMPLACEMENTS
// ==================================================================
void fishingport::on_btnNavEmplacements_clicked() {
  ui->groupBox_employe_form->hide();
  ui->btnNavEmplacements->setChecked(true);
  setActivePage(ui->page_2, "Quais / Places", "Gestion des emplacements du port");
  polishEmplacementsPage();

  Etmp.chargerQuais(ui->comboBox_7);
  Etmp.chargerBateaux(ui->comboBox_8);
  Etmp.chargerBateaux(ui->comboBox_16);
  afficherEmplacements();

  if (!m_histConnexionsBranchees) {
    m_histConnexionsBranchees = true;
    connect(ui->pushButton_19, &QPushButton::clicked, this,
            &fishingport::consulterHistorique);
    connect(ui->lineEdit_5, &QLineEdit::textChanged, this,
            &fishingport::rechercherHistorique);
    connect(ui->comboBox_14,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &fishingport::consulterHistorique);
    connect(ui->comboBox_15,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &fishingport::consulterHistorique);
    connect(ui->comboBox_16,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &fishingport::lancerRecommandation);
    connect(ui->pushButton_20, &QPushButton::clicked, this,
            &fishingport::afficherAnalyseSaisonniere);
  }

  Etmp.afficherRecommandation(ui->widget_1, 0, nullptr);
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  LANCER RECOMMANDATION
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::lancerRecommandation() {
  int idBateau = ui->comboBox_16->currentData().toInt();
  Etmp.afficherRecommandation(
      ui->widget_1, idBateau,
      [this](int idEmp, int idBat, const QString &num, const QString &quai,
             int score, const QString &nomB, const QString &typeB) {
        afficherModalAttribution(idEmp, idBat, num, quai, score, nomB, typeB);
      });
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  ICONES
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
QIcon fishingport::creerIcone(const QString &type) {
  QPixmap pixmap(32, 32);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  if (type == "voir") {
    painter.setPen(QPen(Qt::white, 2.5));
    painter.drawEllipse(4, 10, 24, 12);
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(10, 11, 12, 10);
    painter.setBrush(QColor(30, 80, 30));
    painter.drawEllipse(13, 13, 6, 6);
  } else if (type == "modifier") {
    painter.setPen(
        QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPolygonF p;
    p << QPointF(6, 22) << QPointF(10, 26) << QPointF(26, 10) << QPointF(22, 6);
    painter.drawPolygon(p);
  } else if (type == "supprimer") {
    painter.setPen(
        QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawArc(11, 3, 10, 6, 0, 180 * 16);
    painter.drawRoundedRect(5, 7, 22, 3, 2, 2);
  }
  painter.end();
  return QIcon(pixmap);
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  AJOUTER
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::on_pushButton_14_clicked() {
  QString etat = "";
  if (ui->radioButton_8->isChecked())
    etat = "Libre";
  else if (ui->radioButton_9->isChecked())
    etat = "OccupÃ©";
  else if (ui->radioButton_10->isChecked())
    etat = "Maintenance";
  if (ui->lineEdit_2->text().trimmed().isEmpty()) {
    QMessageBox::warning(this, "Champ manquant", "Veuillez saisir le NumÃ©ro !");
    return;
  }
  if (etat.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant",
                         "Veuillez sÃ©lectionner un Ã‰tat !");
    return;
  }
  Etmp.setNumero(ui->lineEdit_2->text().trimmed());
  Etmp.setType(ui->comboBox_13->currentText());
  Etmp.setIdQuais(ui->comboBox_7->currentData().toInt());
  Etmp.setDateDebut(ui->dateEdit_2->date());
  Etmp.setCommentaire(ui->textEdit->toPlainText().trimmed());
  Etmp.setIdBateau(ui->comboBox_8->currentData().toInt());
  Etmp.setEtat(etat);
  if (Etmp.ajouter()) {
    afficherEmplacements();
    ui->lineEdit_2->clear();
    ui->comboBox_13->setCurrentIndex(0);
    ui->comboBox_7->setCurrentIndex(0);
    ui->comboBox_8->setCurrentIndex(0);
    ui->textEdit->clear();
    ui->radioButton_8->setChecked(true);
  }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  ANNULER
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::on_pushButton_16_clicked() {
  ui->lineEdit_2->clear();
  ui->comboBox_13->setCurrentIndex(0);
  ui->comboBox_7->setCurrentIndex(0);
  ui->comboBox_8->setCurrentIndex(0);
  ui->dateEdit_2->setDate(QDate::currentDate());
  ui->textEdit->clear();
  ui->radioButton_8->setChecked(true);
  ui->lineEdit_2->setProperty("idEmplacement", 0);
  QMessageBox::information(this, "AnnulÃ©", "Formulaire rÃ©initialisÃ© !");
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  COMMENTAIRE AUTO
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::on_pushButton_17_clicked() {
  QString type = ui->comboBox_13->currentText();
  QString etat = "";
  if (ui->radioButton_8->isChecked())
    etat = "Libre";
  else if (ui->radioButton_9->isChecked())
    etat = "OccupÃ©";
  else if (ui->radioButton_10->isChecked())
    etat = "Maintenance";
  if (type == "Aucun type" || type.isEmpty()) {
    QMessageBox::warning(this, "Attention", "Veuillez sÃ©lectionner un Type !");
    return;
  }
  if (etat.isEmpty()) {
    QMessageBox::warning(this, "Attention", "Veuillez sÃ©lectionner un Ã‰tat !");
    return;
  }
  ui->textEdit->setPlainText(
      Etmp.genererCommentaire(type, etat, ui->dateEdit_2->date()));
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  AFFICHER EMPLACEMENTS
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::afficherEmplacements() {
  if (!Etmp.afficher(ui->tableWidget_3))
    return;
  for (int ligne = 0; ligne < ui->tableWidget_3->rowCount(); ligne++) {
    ajouterBoutonsAction(ligne);
    ui->tableWidget_3->setRowHeight(ligne, 50);
    if (QTableWidgetItem *etatItem = ui->tableWidget_3->item(ligne, 4)) {
      const QString etat = etatItem->text().toLower();
      if (etat.contains("libre")) {
        etatItem->setForeground(QColor("#23A559"));
        etatItem->setBackground(QColor("#12261D"));
      } else if (etat.contains("maintenance")) {
        etatItem->setForeground(QColor("#F0B232"));
        etatItem->setBackground(QColor("#2A2111"));
      } else if (etat.contains("occup")) {
        etatItem->setForeground(QColor("#FF8F8F"));
        etatItem->setBackground(QColor("#2B191B"));
      }
      etatItem->setTextAlignment(Qt::AlignCenter);
    }
  }
  if (!m_connexionsBranchees) {
    m_connexionsBranchees = true;
    connect(ui->lineEdit_2, &QLineEdit::textChanged, this,
            [this](const QString &texte) {
              bool ok;
              texte.trimmed().toInt(&ok);
              if (texte.isEmpty()) {
                ui->label_3->setText("");
                ui->label_3->setStyleSheet("");
              } else if (!ok || texte.trimmed().toInt() <= 0) {
                ui->label_3->setText("Le numÃ©ro doit Ãªtre un entier positif !");
                ui->label_3->setStyleSheet(
                    "color: #F44336; font-size: 11px; font-weight: bold;");
              } else {
                ui->label_3->setText("NumÃ©ro valide");
                ui->label_3->setStyleSheet(
                    "color: #4CAF50; font-size: 11px; font-weight: bold;");
              }
            });
    connect(ui->dateEdit_2, &QDateEdit::dateChanged, this,
            [this](const QDate &date) {
              QDate auj = QDate::currentDate();
              int ans = date.daysTo(auj) / 365;
              if (date > auj) {
                ui->label_4->setText(
                    "La date ne peut pas Ãªtre dans le futur !");
                ui->label_4->setStyleSheet(
                    "color: #F44336; font-size: 11px; font-weight: bold;");
              } else if (ans > 50) {
                ui->label_4->setText("Date trop ancienne !");
                ui->label_4->setStyleSheet(
                    "color: #F44336; font-size: 11px; font-weight: bold;");
              } else {
                ui->label_4->setText("Date valide (" + QString::number(ans) +
                                     " an(s))");
                ui->label_4->setStyleSheet(
                    "color: #4CAF50; font-size: 11px; font-weight: bold;");
              }
            });
    connect(ui->lineEdit_3, &QLineEdit::textChanged, this,
            &fishingport::rechercherEmplacements);
    connect(ui->comboBox_9, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &fishingport::rechercherEmplacements);
    connect(ui->comboBox_10,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &fishingport::trierEmplacements);
    connect(ui->comboBox_12,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
              if (index == 1)
                exporterPDF();
              else if (index == 2)
                exporterExcel();
              ui->comboBox_12->setCurrentIndex(0);
            });
  }
  mettreAJourStatistiquesEmp();
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  BOUTONS ACTION
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::ajouterBoutonsAction(int ligne) {
  QWidget *w = new QWidget();
  QHBoxLayout *l = new QHBoxLayout(w);
  l->setContentsMargins(4, 2, 4, 2);
  l->setSpacing(5);
  auto makeBtn = [](const QString &emoji, const QString &color,
                    const QString &hover, const QString &pressed,
                    const QString &tip) -> QPushButton * {
    QPushButton *btn = new QPushButton(emoji);
    btn->setFixedSize(36, 36);
    btn->setToolTip(tip);
    btn->setFont(QFont("Segoe UI Emoji", 14));
    btn->setStyleSheet(
        QString("QPushButton{background-color:%1;border-radius:8px;border:1px "
                "solid %2;color:white;font-size:16px;padding:0px;}"
                "QPushButton:hover{background-color:%2;} "
                "QPushButton:pressed{background-color:%3;}")
            .arg(color, hover, pressed));
    return btn;
  };
  QPushButton *btnVoir =
      makeBtn("\U0001F441", "#4CAF50", "#388E3C", "#2E7D32", "Voir");
  QPushButton *btnMod =
      makeBtn("\u270F", "#FF9800", "#F57C00", "#E65100", "Modifier");
  QPushButton *btnSup =
      makeBtn("\U0001F5D1", "#F44336", "#C62828", "#B71C1C", "Supprimer");
  connect(btnVoir, &QPushButton::clicked, this,
          [this, ligne]() { voirEmplacement(ligne); });
  connect(btnMod, &QPushButton::clicked, this,
          [this, ligne]() { modifierEmplacement(ligne); });
  connect(btnSup, &QPushButton::clicked, this,
          [this, ligne]() { supprimerEmplacement(ligne); });
  l->addWidget(btnVoir);
  l->addWidget(btnMod);
  l->addWidget(btnSup);
  l->addStretch();
  ui->tableWidget_3->setCellWidget(ligne, 7, w);
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  VOIR
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::voirEmplacement(int row) {
  auto cell = [&](int col) -> QString {
    return ui->tableWidget_3->item(row, col)
               ? ui->tableWidget_3->item(row, col)->text()
               : "";
  };

  QString message = "â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”\n"
                    "DÃ‰TAILS EMPLACEMENT\n"
                    "â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”\n"
                    "ID         : " +
                    cell(0) +
                    "\n"
                    "Type       : " +
                    cell(1) +
                    "\n"
                    "NumÃ©ro     : " +
                    cell(2) +
                    "\n"
                    "Quai       : " +
                    cell(3) +
                    "\n"
                    "Ã‰tat       : " +
                    cell(4) +
                    "\n"
                    "Bateau     : " +
                    cell(5) +
                    "\n"
                    "Date DÃ©but : " +
                    cell(6) +
                    "\n"
                    "â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”â”";

  QMessageBox::information(this, "DÃ©tails Emplacement", message);
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  MODIFIER
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::modifierEmplacement(int row) {
  auto cell = [&](int col) -> QString {
    return ui->tableWidget_3->item(row, col)
               ? ui->tableWidget_3->item(row, col)->text()
               : "";
  };
  ui->lineEdit_2->setText(cell(2));
  int idx = ui->comboBox_13->findText(cell(1));
  if (idx >= 0)
    ui->comboBox_13->setCurrentIndex(idx);
  int idxQ = ui->comboBox_7->findText(cell(3));
  if (idxQ >= 0)
    ui->comboBox_7->setCurrentIndex(idxQ);
  QString etat = cell(4);
  if (etat == "Libre")
    ui->radioButton_8->setChecked(true);
  else if (etat == "OccupÃ©")
    ui->radioButton_9->setChecked(true);
  else if (etat == "Maintenance")
    ui->radioButton_10->setChecked(true);
  QDate qdate = QDate::fromString(cell(6), "dd/MM/yyyy");
  if (qdate.isValid())
    ui->dateEdit_2->setDate(qdate);
  ui->lineEdit_2->setProperty("idEmplacement", cell(0).toInt());
  QMessageBox::information(this, "Modifier",
                           "Modifiez les champs puis cliquez sur Confirmer !");
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  SUPPRIMER
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::supprimerEmplacement(int row) {
  QString id = ui->tableWidget_3->item(row, 0)->text();
  QString numero = ui->tableWidget_3->item(row, 2)->text();
  if (QMessageBox::question(this, "Confirmation",
                            "Supprimer l'emplacement NumÃ©ro : " + numero + " ?",
                            QMessageBox::Yes | QMessageBox::No) ==
      QMessageBox::Yes) {
    Etmp.setId(id.toInt());
    if (Etmp.supprimer())
      afficherEmplacements();
  }
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  RECHERCHER,TRIER,EXPORTER PDF,EXPORTER EXCEL,STATISTIQUES LABELS,CONSULTER
//  HISTORIQUE,RECHERCHER HISTORIQUE
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::rechercherEmplacements() {
  Etmp.rechercher(ui->tableWidget_3, ui->lineEdit_3->text(),
                  ui->comboBox_9->currentText());
  mettreAJourStatistiquesEmp();
}
void fishingport::trierEmplacements() {
  Etmp.trier(ui->tableWidget_3, ui->comboBox_10->currentText());
}
void fishingport::exporterPDF() { Etmp.exporterPDF(ui->tableWidget_3, this); }
void fishingport::exporterExcel() {
  Etmp.exporterExcel(ui->tableWidget_3, this);
}
void fishingport::mettreAJourStatistiquesEmp() {
  Etmp.mettreAJourStatistiques(ui->tableWidget_3, ui->label_25, ui->label_26,
                               ui->label_27, ui->label_28);
  auto normalizeStat = [](QLabel *label, const QString &title) {
    if (!label)
      return;
    const QString original = label->text();
    QRegularExpressionMatch m = QRegularExpression("(\\d+)").match(original);
    const QString value = m.hasMatch() ? m.captured(1) : "--";
    label->setText(title + "\n" + value);
  };
  normalizeStat(ui->label_25, "Total");
  normalizeStat(ui->label_26, "Libres");
  normalizeStat(ui->label_27, "Occupes");
  normalizeStat(ui->label_28, "Maintenance");
}
void fishingport::consulterHistorique() {
  Etmp.afficherHistorique(ui->widget_historique, ui->comboBox_14->currentText(),
                          ui->comboBox_15->currentText(),
                          ui->lineEdit_5->text().trimmed());
}
void fishingport::rechercherHistorique() { consulterHistorique(); }
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  FRISE DIALOG
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::afficherFriseDialog(int idEmplacement) {
  QSqlQuery q;
  q.prepare("SELECT e.numero, e.etatE, e.date_debut_occ, q.nom, b.nomB "
            "FROM emplacement e "
            "LEFT JOIN quais q ON e.id_quais = q.id_quais "
            "LEFT JOIN bateau b ON b.id_emplacement = e.id_emplacement "
            "WHERE e.id_emplacement = :id");
  q.bindValue(":id", idEmplacement);

  if (!q.exec() || !q.next()) {
    QMessageBox::warning(this, "Erreur", "Emplacement introuvable !");
    return;
  }

  QString numero = q.value(0).toString();
  QString etat = q.value(1).toString();
  QDate debut = q.value(2).toDate();
  QString quai = q.value(3).toString();
  QString bateau = q.value(4).toString();
  if (bateau.isEmpty())
    bateau = "Aucun";

  int jours = debut.daysTo(QDate::currentDate());
  int mois = jours / 30;

  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Frise â€” Emplacement " + numero);
  dlg->resize(520, 300);
  dlg->setStyleSheet("QDialog { background: #F0F4F8; }"
                     "QLabel  { border: none; background: transparent; }"
                     "QFrame  { border: none; }");

  QVBoxLayout *layout = new QVBoxLayout(dlg);
  layout->setContentsMargins(20, 20, 20, 20);
  layout->setSpacing(12);

  QLabel *titre =
      new QLabel(QString("Emp. %1   â€”   Quai : %2   â€”   Bateau : %3")
                     .arg(numero, quai.isEmpty() ? "â€”" : quai, bateau));
  titre->setStyleSheet(
      "font-weight: bold; font-size: 14px; color: #0D47A1;"
      "padding: 12px 16px; background: #E3F2FD; border-radius: 10px;");
  titre->setAlignment(Qt::AlignCenter);
  layout->addWidget(titre);

  QLabel *info =
      new QLabel(QString("Ã‰tat actuel : %1   â€”   depuis %2 jour(s)  (%3 mois)")
                     .arg(etat.isEmpty() ? "Inconnu" : etat)
                     .arg(jours)
                     .arg(mois));
  info->setStyleSheet("font-size: 12px; color: #546E7A; padding: 4px 8px;");
  info->setAlignment(Qt::AlignCenter);
  layout->addWidget(info);

  QString couleur = "#90CAF9";
  if (etat == "Libre")
    couleur = "#97C459";
  else if (etat == "OccupÃ©")
    couleur = "#E24B4A";
  else if (etat == "Maintenance")
    couleur = "#EF9F27";

  QProgressBar *barre = new QProgressBar();
  barre->setRange(0, 100);
  barre->setValue(100);
  barre->setTextVisible(false);
  barre->setFixedHeight(30);
  barre->setStyleSheet(
      QString("QProgressBar { border-radius: 10px; background: #ECEFF1; "
              "border: none; }"
              "QProgressBar::chunk { border-radius: 10px; background: %1; }")
          .arg(couleur));
  layout->addWidget(barre);

  QHBoxLayout *leg = new QHBoxLayout();
  auto addLeg = [&](const QString &col, const QString &txt) {
    QLabel *dot = new QLabel("â—");
    dot->setStyleSheet(QString("color: %1; font-size: 16px;").arg(col));
    dot->setFixedWidth(20);
    QLabel *lbl = new QLabel(txt);
    lbl->setStyleSheet("font-size: 12px; color: #546E7A;");
    leg->addWidget(dot);
    leg->addWidget(lbl);
    leg->addSpacing(10);
  };
  addLeg("#97C459", "Libre");
  addLeg("#E24B4A", "OccupÃ©");
  addLeg("#EF9F27", "Maintenance");
  leg->addStretch();
  layout->addLayout(leg);

  if (etat == "Libre" && jours > 30) {
    QLabel *alerte = new QLabel(
        QString("Libre depuis %1 jours â€” mise en valeur conseillÃ©e !")
            .arg(jours));
    alerte->setStyleSheet(
        "background: #E3F2FD; color: #0D47A1; border-radius: 8px;"
        "padding: 10px 14px; font-size: 12px; font-weight: bold;");
    alerte->setAlignment(Qt::AlignCenter);
    layout->addWidget(alerte);
  } else if (etat == "Maintenance" && jours > 14) {
    QLabel *alerte = new QLabel(
        QString("En maintenance depuis %1 jours â€” inspection requise !")
            .arg(jours));
    alerte->setStyleSheet(
        "background: #FFF8E1; color: #E65100; border-radius: 8px;"
        "padding: 10px 14px; font-size: 12px; font-weight: bold;");
    alerte->setAlignment(Qt::AlignCenter);
    layout->addWidget(alerte);
  } else if (etat == "OccupÃ©" && jours > 365) {
    QLabel *alerte = new QLabel(
        QString(
            "OccupÃ© depuis plus d'un an (%1 jours) â€” inspection recommandÃ©e !")
            .arg(jours));
    alerte->setStyleSheet(
        "background: #FFCDD2; color: #C62828; border-radius: 8px;"
        "padding: 10px 14px; font-size: 12px; font-weight: bold;");
    alerte->setAlignment(Qt::AlignCenter);
    layout->addWidget(alerte);
  }

  layout->addStretch();

  QPushButton *btnFermer = new QPushButton("Fermer");
  btnFermer->setStyleSheet(
      "QPushButton { background: #0288D1; color: white; border-radius: 8px;"
      "padding: 9px 28px; font-weight: bold; font-size: 13px; border: none; }"
      "QPushButton:hover { background: #0277BD; }");
  connect(btnFermer, &QPushButton::clicked, dlg, &QDialog::close);

  QHBoxLayout *btnRow = new QHBoxLayout();
  btnRow->addStretch();
  btnRow->addWidget(btnFermer);
  layout->addLayout(btnRow);

  dlg->exec();
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  STATISTIQUES GRAPHIQUES
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::on_comboBox_11_currentIndexChanged(int /*index*/) {
  QString critere = ui->comboBox_11->currentText();
  if (critere.isEmpty() || critere == "voir par:")
    return;

  int total = 0, libres = 0, occupes = 0, maintenance = 0;
  QMap<QString, int> parType, parQuai;
  QMap<QString, int> libresParQuai, occupesParQuai, maintenanceParQuai;

  for (int i = 0; i < ui->tableWidget_3->rowCount(); i++) {
    if (ui->tableWidget_3->isRowHidden(i))
      continue;
    total++;
    QString etat = ui->tableWidget_3->item(i, 4)
                       ? ui->tableWidget_3->item(i, 4)->text()
                       : "";
    QString type = ui->tableWidget_3->item(i, 1)
                       ? ui->tableWidget_3->item(i, 1)->text()
                       : "?";
    QString quai = ui->tableWidget_3->item(i, 3)
                       ? ui->tableWidget_3->item(i, 3)->text()
                       : "?";

    if (etat == "Libre")
      libres++;
    else if (etat == "OccupÃ©")
      occupes++;
    else if (etat == "Maintenance")
      maintenance++;

    if (!type.isEmpty())
      parType[type]++;
    if (!quai.isEmpty()) {
      parQuai[quai]++;
      if (etat == "Libre")
        libresParQuai[quai]++;
      else if (etat == "OccupÃ©")
        occupesParQuai[quai]++;
      else if (etat == "Maintenance")
        maintenanceParQuai[quai]++;
    }
  }

  // â”€â”€ Dialog â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Statistiques â€” " + critere);
  dlg->resize(820, 700);
  dlg->setStyleSheet("QDialog { background: #F4F7FB; }"
                     "QLabel  { border: none; background: transparent; }"
                     "QFrame  { border: none; }");

  QVBoxLayout *root = new QVBoxLayout(dlg);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(16);

  // â”€â”€ Header gradient â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *headerFrame = new QFrame();
  headerFrame->setFixedHeight(70);
  headerFrame->setStyleSheet(
      "QFrame { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
      "stop:0 #1565C0, stop:0.5 #0288D1, stop:1 #00BCD4);"
      "border-radius: 14px; }");
  QHBoxLayout *headerL = new QHBoxLayout(headerFrame);
  headerL->setContentsMargins(20, 0, 20, 0);

  QLabel *headerIcon = new QLabel("ðŸ“Š");
  headerIcon->setStyleSheet("font-size: 26px;");
  QLabel *headerTitle = new QLabel("Statistiques â€” " + critere.toUpper());
  headerTitle->setStyleSheet(
      "font-size: 18px; font-weight: bold; color: white; letter-spacing: 1px;");
  QLabel *headerSub =
      new QLabel(QString::number(total) + " emplacements analysÃ©s");
  headerSub->setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.8);");

  QVBoxLayout *headerText = new QVBoxLayout();
  headerText->setSpacing(2);
  headerText->addWidget(headerTitle);
  headerText->addWidget(headerSub);

  headerL->addWidget(headerIcon);
  headerL->addSpacing(10);
  headerL->addLayout(headerText);
  headerL->addStretch();
  root->addWidget(headerFrame);

  // â”€â”€ KPI Cards â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  int tauxOcc = (total > 0) ? occupes * 100 / total : 0;
  int tauxLib = (total > 0) ? libres * 100 / total : 0;
  int tauxMnt = (total > 0) ? maintenance * 100 / total : 0;

  QHBoxLayout *kpiRow = new QHBoxLayout();
  kpiRow->setSpacing(12);

  struct KPI {
    QString nom, valeur, icon, couleur, bg, border;
  };
  QList<KPI> kpis = {
      {"Total", QString::number(total), "âš“", "#1565C0", "#E3F2FD", "#90CAF9"},
      {"Libres",
       QString::number(libres) + "\n" + QString::number(tauxLib) + "%", "ðŸŸ¢",
       "#2E7D32", "#E8F5E9", "#A5D6A7"},
      {"OccupÃ©s",
       QString::number(occupes) + "\n" + QString::number(tauxOcc) + "%", "ðŸ”´",
       "#C62828", "#FFEBEE", "#EF9A9A"},
      {"Maintenance",
       QString::number(maintenance) + "\n" + QString::number(tauxMnt) + "%",
       "ðŸ”§", "#E65100", "#FFF3E0", "#FFCC80"}};

  for (const KPI &k : kpis) {
    QFrame *card = new QFrame();
    card->setFixedHeight(100);
    card->setStyleSheet(
        QString("QFrame { background: %1; border: 2px solid %2; "
                "border-radius: 16px; }")
            .arg(k.bg, k.border));

    QVBoxLayout *cl = new QVBoxLayout(card);
    cl->setContentsMargins(12, 8, 12, 8);
    cl->setSpacing(3);
    cl->setAlignment(Qt::AlignCenter);

    QLabel *iconLbl = new QLabel(k.icon);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet("font-size: 20px;");

    // Valeur principale (nombre)
    QString parts = k.valeur;
    QStringList sp = parts.split("\n");
    QLabel *valLbl = new QLabel(sp[0]);
    valLbl->setAlignment(Qt::AlignCenter);
    valLbl->setStyleSheet(
        QString("font-size: 22px; font-weight: bold; color: %1;")
            .arg(k.couleur));

    QLabel *nomLbl = new QLabel(k.nom + (sp.size() > 1 ? "  " + sp[1] : ""));
    nomLbl->setAlignment(Qt::AlignCenter);
    nomLbl->setStyleSheet(
        QString("font-size: 11px; font-weight: bold; color: %1;")
            .arg(k.couleur));

    cl->addWidget(iconLbl);
    cl->addWidget(valLbl);
    cl->addWidget(nomLbl);
    kpiRow->addWidget(card);
  }
  root->addLayout(kpiRow);

  // â”€â”€ Barre occupation segmentÃ©e â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *tauxFrame = new QFrame();
  tauxFrame->setStyleSheet("QFrame { background: white; border-radius: 12px; "
                           "border: 1px solid #E0E0E0; }");
  QVBoxLayout *tauxL = new QVBoxLayout(tauxFrame);
  tauxL->setContentsMargins(16, 12, 16, 12);
  tauxL->setSpacing(8);

  QHBoxLayout *tauxHeader = new QHBoxLayout();
  QLabel *tauxTitle = new QLabel("Taux d'occupation global");
  tauxTitle->setStyleSheet(
      "font-size: 13px; font-weight: bold; color: #1565C0;");
  QLabel *tauxPct = new QLabel(QString::number(tauxOcc) + "%");
  tauxPct->setStyleSheet("font-size: 20px; font-weight: bold; color: #C62828;");
  tauxHeader->addWidget(tauxTitle);
  tauxHeader->addStretch();
  tauxHeader->addWidget(tauxPct);
  tauxL->addLayout(tauxHeader);

  // Barre segmentÃ©e
  QWidget *barWidget = new QWidget();
  barWidget->setFixedHeight(24);
  barWidget->setStyleSheet("background: #ECEFF1; border-radius: 12px;");
  QHBoxLayout *barL = new QHBoxLayout(barWidget);
  barL->setContentsMargins(0, 0, 0, 0);
  barL->setSpacing(1);

  auto addSeg = [&](int pct, const QString &color, const QString &label,
                    bool isFirst, bool isLast) {
    if (pct <= 0)
      return;
    QFrame *seg = new QFrame();
    seg->setFixedHeight(24);
    int w = qMax(8, pct * 7);
    seg->setFixedWidth(w);
    QString radius = "border-radius: 0px;";
    if (isFirst && isLast)
      radius = "border-radius: 12px;";
    else if (isFirst)
      radius = "border-radius: 12px 0 0 12px;";
    else if (isLast)
      radius = "border-radius: 0 12px 12px 0;";
    seg->setStyleSheet(QString("QFrame { background: %1; %2 border: none; }")
                           .arg(color, radius));
    seg->setToolTip(label + " : " + QString::number(pct) + "%");
    barL->addWidget(seg);
  };

  bool hasLib = tauxLib > 0, hasOcc = tauxOcc > 0, hasMnt = tauxMnt > 0;
  addSeg(tauxLib, "#4CAF50", "Libres", hasLib, !hasOcc && !hasMnt);
  addSeg(tauxOcc, "#F44336", "OccupÃ©s", !hasLib, !hasMnt);
  addSeg(tauxMnt, "#FF9800", "Maintenance", !hasLib && !hasOcc, true);
  barL->addStretch();
  tauxL->addWidget(barWidget);

  // LÃ©gende
  QHBoxLayout *legL = new QHBoxLayout();
  struct Leg {
    QString col, txt;
    int pct;
  };
  for (const Leg &lg : QList<Leg>{{"#4CAF50", "Libres", tauxLib},
                                  {"#F44336", "OccupÃ©s", tauxOcc},
                                  {"#FF9800", "Maintenance", tauxMnt}}) {
    QFrame *dot = new QFrame();
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(
        QString("QFrame { background: %1; border-radius: 5px; border: none; }")
            .arg(lg.col));
    QLabel *lbl = new QLabel(lg.txt + "  " + QString::number(lg.pct) + "%");
    lbl->setStyleSheet("font-size: 11px; color: #546E7A;");
    legL->addWidget(dot);
    legL->addWidget(lbl);
    legL->addSpacing(16);
  }
  legL->addStretch();
  tauxL->addLayout(legL);
  root->addWidget(tauxFrame);

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  GRAPHIQUES SELON CRITÃˆRE
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

  if (critere == "Ã©tat") {
    // â”€â”€ Donut amÃ©liorÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QPieSeries *pie = new QPieSeries();
    pie->setHoleSize(0.45);
    pie->setPieSize(0.75);

    struct Slice {
      QString lbl;
      int val;
      QColor col;
    };
    for (const Slice &s :
         QList<Slice>{{"Libres", libres, QColor(76, 175, 80)},
                      {"OccupÃ©s", occupes, QColor(244, 67, 54)},
                      {"Maintenance", maintenance, QColor(255, 152, 0)}}) {
      if (s.val <= 0)
        continue;
      QPieSlice *sl = pie->append(QString("%1\n%2  (%3%)")
                                      .arg(s.lbl)
                                      .arg(s.val)
                                      .arg(total > 0 ? s.val * 100 / total : 0),
                                  s.val);
      sl->setColor(s.col);
      sl->setBorderColor(Qt::white);
      sl->setBorderWidth(4);
      sl->setLabelVisible(true);
      sl->setLabelFont(QFont("Segoe UI", 11, QFont::Bold));
      sl->setExploded(true);
      sl->setExplodeDistanceFactor(0.04f);
    }

    QChart *chart = new QChart();
    chart->addSeries(pie);
    chart->setTitle("RÃ©partition des Ã©tats");
    chart->setTitleFont(QFont("Segoe UI", 14, QFont::Bold));
    chart->setBackgroundBrush(Qt::transparent);
    chart->setAnimationOptions(QChart::AllAnimations);
    chart->setAnimationDuration(800);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 10));

    QChartView *cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setMinimumHeight(300);
    cv->setStyleSheet("QChartView { background: white; border-radius: 14px; "
                      "border: 1px solid #E0E0E0; }");
    root->addWidget(cv);
  }

  else if (critere == "type") {
    // â”€â”€ Une barre par type â€” couleur unique par type â”€â”€â”€â”€â”€â”€
    // Palette de couleurs distinctes
    QList<QColor> palette = {
        QColor(33, 150, 243), // bleu    Commerce
        QColor(76, 175, 80),  // vert    Plaisance
        QColor(255, 152, 0),  // orange  Plaisance lÃ©gÃ¨re
        QColor(156, 39, 176), // violet  Plaisance lourde
        QColor(244, 67, 54),  // rouge   PÃªche
        QColor(0, 188, 212),  // cyan    PÃªche artisanale
        QColor(255, 87, 34),  // deep orange  Mixte
        QColor(233, 30, 99),  // rose    SpÃ©cial
        QColor(96, 125, 139), // gris    Maintenance
        QColor(255, 235, 59), // jaune   Urgence
    };

    // (pour que chaque type apparaisse sur sa propre catÃ©gorie)
    QStringList categories;
    QList<int> valeurs;
    QList<QColor> couleurs;

    int ci = 0;
    for (auto it = parType.begin(); it != parType.end(); ++it, ++ci) {
      categories << it.key();
      valeurs << it.value();
      couleurs << palette[ci % palette.size()];
    }

    // CrÃ©er une QBarSeries avec UN QBarSet par type
    // mais une seule catÃ©gorie par set â†’ chaque barre est indÃ©pendante
    QBarSeries *barSeries = new QBarSeries();
    for (int i = 0; i < categories.size(); i++) {
      QBarSet *set = new QBarSet(categories[i]);
      set->setColor(couleurs[i]);
      set->setLabelColor(Qt::white);
      // Ajouter 0 pour toutes les catÃ©gories sauf la sienne
      for (int j = 0; j < categories.size(); j++)
        *set << (j == i ? valeurs[i] : 0);
      barSeries->append(set);
    }
    barSeries->setLabelsVisible(true);
    barSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);

    QBarCategoryAxis *axX = new QBarCategoryAxis();
    axX->append(categories);
    axX->setLabelsFont(QFont("Segoe UI", 9));

    QValueAxis *axY = new QValueAxis();
    axY->setLabelFormat("%d");
    axY->setTickCount(6);
    axY->setTitleText("Nombre d'emplacements");
    axY->setTitleFont(QFont("Segoe UI", 9));

    QChart *chart = new QChart();
    chart->addSeries(barSeries);
    chart->setTitle("Emplacements par type");
    chart->setTitleFont(QFont("Segoe UI", 14, QFont::Bold));
    chart->setBackgroundBrush(Qt::transparent);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationDuration(600);
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    barSeries->attachAxis(axX);
    barSeries->attachAxis(axY);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 9));

    QChartView *cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setMinimumHeight(300);
    cv->setStyleSheet("QChartView { background: white; border-radius: 14px; "
                      "border: 1px solid #E0E0E0; }");
    root->addWidget(cv);
  }

  else if (critere == "quai") {
    // â”€â”€ Barres groupÃ©es par quai â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QBarSet *setLibres = new QBarSet("Libres");
    QBarSet *setOccupes = new QBarSet("OccupÃ©s");
    QBarSet *setMaintenance = new QBarSet("Maintenance");

    setLibres->setColor(QColor(76, 175, 80));
    setLibres->setBorderColor(QColor(56, 142, 60));
    setOccupes->setColor(QColor(244, 67, 54));
    setOccupes->setBorderColor(QColor(198, 40, 40));
    setMaintenance->setColor(QColor(255, 152, 0));
    setMaintenance->setBorderColor(QColor(230, 81, 0));

    QStringList quais;
    for (auto it = parQuai.begin(); it != parQuai.end(); ++it) {
      quais << it.key();
      *setLibres << libresParQuai.value(it.key(), 0);
      *setOccupes << occupesParQuai.value(it.key(), 0);
      *setMaintenance << maintenanceParQuai.value(it.key(), 0);
    }

    QBarSeries *barSeries = new QBarSeries();
    barSeries->append(setLibres);
    barSeries->append(setOccupes);
    barSeries->append(setMaintenance);
    barSeries->setLabelsVisible(true);
    barSeries->setLabelsPosition(QAbstractBarSeries::LabelsInsideEnd);

    QBarCategoryAxis *axX = new QBarCategoryAxis();
    axX->append(quais);
    axX->setLabelsFont(QFont("Segoe UI", 9));

    QValueAxis *axY = new QValueAxis();
    axY->setLabelFormat("%d");
    axY->setTickCount(6);
    axY->setTitleText("Nombre d'emplacements");
    axY->setTitleFont(QFont("Segoe UI", 9));

    QChart *chart = new QChart();
    chart->addSeries(barSeries);
    chart->setTitle("RÃ©partition par quai");
    chart->setTitleFont(QFont("Segoe UI", 14, QFont::Bold));
    chart->setBackgroundBrush(Qt::transparent);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationDuration(600);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 10));
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    barSeries->attachAxis(axX);
    barSeries->attachAxis(axY);

    QChartView *cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setMinimumHeight(300);
    cv->setStyleSheet("QChartView { background: white; border-radius: 14px; "
                      "border: 1px solid #E0E0E0; }");
    root->addWidget(cv);
  }

  else if (critere == "num") {

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }");
    scroll->setMinimumHeight(300);

    QWidget *inner = new QWidget();
    inner->setStyleSheet("background: transparent;");
    QVBoxLayout *ll = new QVBoxLayout(inner);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(7);
    scroll->setWidget(inner);

    for (int i = 0; i < ui->tableWidget_3->rowCount(); i++) {
      if (ui->tableWidget_3->isRowHidden(i))
        continue;

      QString num = ui->tableWidget_3->item(i, 2)
                        ? ui->tableWidget_3->item(i, 2)->text()
                        : "-";
      QString type = ui->tableWidget_3->item(i, 1)
                         ? ui->tableWidget_3->item(i, 1)->text()
                         : "-";
      QString quai = ui->tableWidget_3->item(i, 3)
                         ? ui->tableWidget_3->item(i, 3)->text()
                         : "-";
      QString etat = ui->tableWidget_3->item(i, 4)
                         ? ui->tableWidget_3->item(i, 4)->text()
                         : "-";
      QString date = ui->tableWidget_3->item(i, 6)
                         ? ui->tableWidget_3->item(i, 6)->text()
                         : "-";

      QString bColor = (etat == "Libre")    ? "#4CAF50"
                       : (etat == "OccupÃ©") ? "#F44336"
                                            : "#FF9800";
      QString bgLine = (etat == "Libre")    ? "#F1F8E9"
                       : (etat == "OccupÃ©") ? "#FFF3F3"
                                            : "#FFFDE7";

      QFrame *row = new QFrame();
      row->setStyleSheet(QString("QFrame { background: %1; border-radius: "
                                 "10px; border-left: 4px solid %2; }")
                             .arg(bgLine, bColor));

      QHBoxLayout *rl = new QHBoxLayout(row);
      rl->setContentsMargins(10, 7, 10, 7);
      rl->setSpacing(12);

      QLabel *numLbl = new QLabel(QString("NÂ° %1").arg(num));
      numLbl->setStyleSheet(
          QString("font-weight: bold; color: %1; font-size: 13px;")
              .arg(bColor));

      QLabel *infoLbl =
          new QLabel(QString("ðŸ· %1   âš“ %2   ðŸ“… %3").arg(type, quai, date));
      infoLbl->setStyleSheet("color: #546E7A; font-size: 11px;");

      QLabel *etatBadge = new QLabel(etat);
      etatBadge->setFixedHeight(24);
      etatBadge->setAlignment(Qt::AlignCenter);
      etatBadge->setStyleSheet(
          QString("background: %1; color: white; border-radius: 8px;"
                  "padding: 0 10px; font-weight: bold; font-size: 11px;")
              .arg(bColor));

      rl->addWidget(numLbl);
      rl->addWidget(infoLbl, 1);
      rl->addWidget(etatBadge);
      ll->addWidget(row);
    }
    ll->addStretch();
    root->addWidget(scroll);
  }

  // â”€â”€ Bouton Fermer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *btnRow = new QHBoxLayout();
  btnRow->addStretch();
  QPushButton *btnFermer = new QPushButton("  Fermer  ");
  btnFermer->setFixedHeight(40);
  btnFermer->setStyleSheet(
      "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
      "stop:0 #1565C0, stop:1 #0288D1); color: white; border-radius: 10px;"
      "padding: 0 30px; font-weight: bold; font-size: 13px; border: none; }"
      "QPushButton:hover { background: #0D47A1; }");
  connect(btnFermer, &QPushButton::clicked, dlg, &QDialog::close);
  btnRow->addWidget(btnFermer);
  root->addLayout(btnRow);

  ui->comboBox_11->blockSignals(true);
  ui->comboBox_11->setCurrentIndex(0);
  ui->comboBox_11->blockSignals(false);

  dlg->exec();
}
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  MODAL ATTRIBUTION
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::afficherModalAttribution(int idEmp, int idBateau,
                                           const QString &numero,
                                           const QString &quai, int score,
                                           const QString &nomBateau,
                                           const QString &typeBateau) {
  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Confirmation d'attribution");
  dlg->setFixedWidth(420);
  dlg->setStyleSheet("QDialog { background: #FAFAFA; }"
                     "QLabel  { border: none; background: transparent; }"
                     "QFrame  { border: none; }");

  QVBoxLayout *root = new QVBoxLayout(dlg);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  QFrame *headerFrame = new QFrame();
  headerFrame->setStyleSheet("QFrame { background: #0288D1; }");
  QHBoxLayout *headerL = new QHBoxLayout(headerFrame);
  headerL->setContentsMargins(18, 14, 18, 14);

  QLabel *headerTitle = new QLabel("Confirmation d'attribution");
  headerTitle->setStyleSheet(
      "font-size: 15px; font-weight: bold; color: white;");
  headerL->addWidget(headerTitle);
  headerL->addStretch();

  QPushButton *btnX = new QPushButton("âœ•");
  btnX->setFixedSize(28, 28);
  btnX->setStyleSheet(
      "QPushButton { background: transparent; color: white; border: none; "
      "font-size: 14px; font-weight: bold; }"
      "QPushButton:hover { background: rgba(255,255,255,0.2); border-radius: "
      "14px; }");
  connect(btnX, &QPushButton::clicked, dlg, &QDialog::reject);
  headerL->addWidget(btnX);
  root->addWidget(headerFrame);

  QWidget *body = new QWidget();
  QVBoxLayout *bodyL = new QVBoxLayout(body);
  bodyL->setContentsMargins(20, 16, 20, 16);
  bodyL->setSpacing(12);

  auto makeSection = [](const QString &titre) -> QLabel * {
    QLabel *l = new QLabel(titre.toUpper());
    l->setStyleSheet("font-size: 10px; font-weight: bold; color: #90A4AE; "
                     "letter-spacing: 1px;");
    return l;
  };

  auto makeRow = [](const QString &cle, const QString &val,
                    bool bold = false) -> QHBoxLayout * {
    QHBoxLayout *row = new QHBoxLayout();
    QLabel *k = new QLabel(cle);
    k->setStyleSheet("font-size: 13px; color: #78909C;");
    QLabel *v = new QLabel(val);
    v->setStyleSheet(QString("font-size: 13px; color: #263238; %1")
                         .arg(bold ? "font-weight: bold;" : ""));
    row->addWidget(k);
    row->addStretch();
    row->addWidget(v);
    return row;
  };

  bodyL->addWidget(makeSection("Fiche bateau"));
  QFrame *sep0 = new QFrame();
  sep0->setFixedHeight(1);
  sep0->setStyleSheet("background: #ECEFF1;");
  bodyL->addWidget(sep0);
  bodyL->addLayout(makeRow("Nom", nomBateau, true));
  bodyL->addLayout(makeRow("Type", typeBateau.isEmpty() ? "â€”" : typeBateau));
  bodyL->addLayout(makeRow("Quai habituel", quai.isEmpty() ? "â€”" : quai));

  QHBoxLayout *etatRow = new QHBoxLayout();
  QLabel *etatK = new QLabel("Ã‰tat");
  etatK->setStyleSheet("font-size: 13px; color: #78909C;");
  QLabel *etatV = new QLabel("Actif");
  etatV->setStyleSheet(
      "font-size: 11px; font-weight: bold; color: #2E7D32; "
      "background: #E8F5E9; border-radius: 10px; padding: 2px 12px;");
  etatRow->addWidget(etatK);
  etatRow->addStretch();
  etatRow->addWidget(etatV);
  bodyL->addLayout(etatRow);

  QFrame *sep1 = new QFrame();
  sep1->setFixedHeight(1);
  sep1->setStyleSheet("background: #ECEFF1; margin: 4px 0;");
  bodyL->addWidget(sep1);

  bodyL->addWidget(makeSection("Emplacement attribuÃ©"));
  QFrame *sep2 = new QFrame();
  sep2->setFixedHeight(1);
  sep2->setStyleSheet("background: #ECEFF1;");
  bodyL->addWidget(sep2);

  QFrame *scoreBox = new QFrame();
  scoreBox->setStyleSheet(
      "QFrame { background: #E8F5E9; border-radius: 8px; }");
  QHBoxLayout *scoreL = new QHBoxLayout(scoreBox);
  scoreL->setContentsMargins(12, 8, 12, 8);
  QLabel *scoreTxt = new QLabel("Score de compatibilitÃ©");
  scoreTxt->setStyleSheet(
      "font-size: 12px; color: #2E7D32; font-weight: bold;");
  QLabel *scoreVal = new QLabel(QString::number(score) + "%");
  scoreVal->setStyleSheet(
      "font-size: 18px; font-weight: bold; color: #2E7D32;");
  scoreL->addWidget(scoreTxt);
  scoreL->addStretch();
  scoreL->addWidget(scoreVal);
  bodyL->addWidget(scoreBox);

  bodyL->addLayout(makeRow("NumÃ©ro", "Emp. " + numero, true));
  bodyL->addLayout(makeRow("Quai", quai.isEmpty() ? "â€”" : quai));

  QHBoxLayout *nEtatRow = new QHBoxLayout();
  QLabel *nEtatK = new QLabel("Nouvel Ã©tat");
  nEtatK->setStyleSheet("font-size: 13px; color: #78909C;");
  QLabel *nEtatV = new QLabel("OccupÃ©");
  nEtatV->setStyleSheet(
      "font-size: 11px; font-weight: bold; color: #B71C1C; "
      "background: #FFEBEE; border-radius: 10px; padding: 2px 12px;");
  nEtatRow->addWidget(nEtatK);
  nEtatRow->addStretch();
  nEtatRow->addWidget(nEtatV);
  bodyL->addLayout(nEtatRow);

  QFrame *sep3 = new QFrame();
  sep3->setFixedHeight(1);
  sep3->setStyleSheet("background: #ECEFF1; margin: 4px 0;");
  bodyL->addWidget(sep3);

  QLabel *note = new QLabel("AprÃ¨s confirmation, l'Ã©tat passe Ã  <b>OccupÃ©</b> "
                            "et un log est enregistrÃ© dans l'historique.");
  note->setWordWrap(true);
  note->setTextFormat(Qt::RichText);
  note->setStyleSheet("font-size: 11px; color: #90A4AE;");
  bodyL->addWidget(note);
  root->addWidget(body);

  QFrame *footer = new QFrame();
  footer->setStyleSheet(
      "QFrame { background: #FAFAFA; border-top: 1px solid #ECEFF1; }");
  QHBoxLayout *footerL = new QHBoxLayout(footer);
  footerL->setContentsMargins(20, 12, 20, 12);
  footerL->setSpacing(10);

  QPushButton *btnAnnuler = new QPushButton("Annuler");
  btnAnnuler->setFixedHeight(38);
  btnAnnuler->setStyleSheet(
      "QPushButton { background: white; color: #546E7A; "
      "border: 1px solid #B0BEC5; border-radius: 8px; "
      "font-size: 13px; font-weight: bold; padding: 0 20px; }"
      "QPushButton:hover { background: #ECEFF1; }");
  connect(btnAnnuler, &QPushButton::clicked, dlg, &QDialog::reject);

  QPushButton *btnConfirmer = new QPushButton("Confirmer");
  btnConfirmer->setFixedHeight(38);
  btnConfirmer->setStyleSheet(
      "QPushButton { background: #263238; color: white; "
      "border-radius: 8px; font-size: 13px; font-weight: bold; "
      "padding: 0 20px; border: none; }"
      "QPushButton:hover { background: #37474F; }");

  connect(btnConfirmer, &QPushButton::clicked, dlg, [=]() {
    if (Etmp.attribuerEmplacement(idEmp, idBateau)) {
      dlg->accept();
      afficherEmplacements();
      lancerRecommandation();
      QMessageBox::information(
          this, "SuccÃ¨s",
          QString("Emplacement NÂ°%1 attribuÃ© avec succÃ¨s !\nBateau : %2")
              .arg(numero, nomBateau));
    }
  });

  footerL->addWidget(btnAnnuler);
  footerL->addWidget(btnConfirmer);
  root->addWidget(footer);

  dlg->exec();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  afficherAnalyseSaisonniere
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void fishingport::afficherAnalyseSaisonniere() {
  Etmp.afficherAnalyseSaisonniere(this);
}

// ==================================================================
//  QUAIS
// ==================================================================
void fishingport::on_btnNavQuais_clicked() {
  ui->groupBox_employe_form->hide();
  ui->btnNavQuais->setChecked(true);
  setActivePage(ui->page_quai, "Quais", "Gestion des quais");
  chargerQuais();
}

void fishingport::clearFormQuai() {
  ui->quai_lineEdit_id->clear();
  ui->quai_comboBox_nom->setCurrentIndex(0);
  ui->quai_lineEdit_capacite->clear();
  ui->quai_lineEdit_longueur->clear();
  ui->quai_radioLibre->setChecked(true);
  m_quaiSelectedId = -1;
}

void fishingport::chargerQuais() {
  m_tmpQuai.loadTable(m_quaiModel);
  ui->quai_tableView->setModel(m_quaiModel);
  ui->quai_tableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->quai_tableView->verticalHeader()->setVisible(false);
}

void fishingport::lancerRechercheQuai() {
  QString text = ui->quai_lineEdit_recherche->text().trimmed();
  int crit = ui->quai_comboBox_rechercheCritere->currentIndex(),
      tri = ui->quai_comboBox_tri->currentIndex();
  if (text.isEmpty()) {
    QSqlQueryModel *m = m_tmpQuai.trier((tri == 1)   ? "capacite"
                                        : (tri == 2) ? "longueur"
                                                     : "id_quais",
                                        "ASC");
    ui->quai_tableView->setModel(m);
    ui->quai_tableView->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    ui->quai_tableView->verticalHeader()->setVisible(false);
    return;
  }
  QString col;
  switch (crit) {
  case 1:
    col = "id_quais";
    break;
  case 2:
    col = "nom";
    break;
  case 3:
    col = "capacite";
    break;
  case 4:
    col = "longueur";
    break;
  case 5:
    col = "statusq";
    break;
  default:
    col = "nom";
  }
  QSqlQueryModel *m = m_tmpQuai.chercher(col, text);
  ui->quai_tableView->setModel(m);
  ui->quai_tableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->quai_tableView->verticalHeader()->setVisible(false);
}

void fishingport::loadSelectedQuaiRow() {
  QModelIndexList sel = ui->quai_tableView->selectionModel()->selectedRows();
  if (sel.isEmpty())
    return;
  int row = sel.first().row();
  m_quaiSelectedId = m_quaiModel->data(m_quaiModel->index(row, 0)).toInt();
  ui->quai_lineEdit_id->setText(QString::number(m_quaiSelectedId));
  QString nom = m_quaiModel->data(m_quaiModel->index(row, 1)).toString();
  int cap = m_quaiModel->data(m_quaiModel->index(row, 2)).toInt();
  double lon = m_quaiModel->data(m_quaiModel->index(row, 3)).toDouble();
  QString status = m_quaiModel->data(m_quaiModel->index(row, 4)).toString();
  int idx = ui->quai_comboBox_nom->findText(nom);
  if (idx >= 0)
    ui->quai_comboBox_nom->setCurrentIndex(idx);
  ui->quai_lineEdit_capacite->setText(QString::number(cap));
  ui->quai_lineEdit_longueur->setText(QString::number(lon, 'f', 2));
  if (status.trimmed().toLower() == "libre")
    ui->quai_radioLibre->setChecked(true);
  else
    ui->quai_radioOccupe->setChecked(true);
}

void fishingport::on_quai_tableView_clicked(const QModelIndex &) {
  loadSelectedQuaiRow();
}

void fishingport::on_quai_btnAjouter_clicked() {
  ui->quai_btnAjouter->setEnabled(false);
  QString nom = ui->quai_comboBox_nom->currentText().trimmed();
  QString capStr = ui->quai_lineEdit_capacite->text().trimmed();
  QString lonStr =
      ui->quai_lineEdit_longueur->text().trimmed().replace(',', '.');
  if (capStr.isEmpty() || lonStr.isEmpty()) {
    QMessageBox::warning(this, "Champs manquants",
                         "Veuillez remplir la CapacitÃ© et la Longueur.");
    ui->quai_btnAjouter->setEnabled(true);
    return;
  }
  bool capOk = false, lonOk = false;
  int cap = capStr.toInt(&capOk);
  double lon = lonStr.toDouble(&lonOk);
  if (!capOk || !lonOk) {
    QMessageBox::warning(
        this, "Valeur invalide",
        "CapacitÃ© doit Ãªtre un entier et Longueur un dÃ©cimal.");
    ui->quai_btnAjouter->setEnabled(true);
    return;
  }
  QString stat = ui->quai_radioLibre->isChecked() ? "Libre" : "OccupÃ©";
  int id = m_tmpQuai.prochainId();
  Quai q(id, nom, cap, lon, stat);
  if (q.ajouter()) {
    QSqlDatabase::database().commit();
    clearFormQuai();
    chargerQuais();
    QMessageBox::information(
        this, "SuccÃ¨s", QString("Quai '%1' ajoutÃ© ! (ID:%2)").arg(nom).arg(id));
  } else
    QMessageBox::critical(
        this, "Erreur",
        "Ajout Ã©chouÃ© !\n" +
            QSqlDatabase::database().lastError().text().left(250));
  ui->quai_btnAjouter->setEnabled(true);
}

void fishingport::on_quai_btnModifier_clicked() {
  if (m_quaiSelectedId < 0) {
    QMessageBox::warning(this, "Attention",
                         "Veuillez sÃ©lectionner un quai d'abord.");
    return;
  }
  QString nom = ui->quai_comboBox_nom->currentText().trimmed();
  QString capStr = ui->quai_lineEdit_capacite->text().trimmed(),
          lonStr =
              ui->quai_lineEdit_longueur->text().trimmed().replace(',', '.');
  bool capOk = false, lonOk = false;
  int cap = capStr.toInt(&capOk);
  double lon = lonStr.toDouble(&lonOk);
  QString stat = ui->quai_radioLibre->isChecked() ? "Libre" : "OccupÃ©";
  if (capStr.isEmpty() || lonStr.isEmpty() || !capOk || !lonOk) {
    QMessageBox::warning(this, "Valeur invalide",
                         "CapacitÃ© (entier) et Longueur (dÃ©cimal) requis.");
    return;
  }
  Quai q(m_quaiSelectedId, nom, cap, lon, stat);
  if (q.modifier(m_quaiSelectedId)) {
    QSqlDatabase::database().commit();
    clearFormQuai();
    chargerQuais();
    QMessageBox::information(this, "SuccÃ¨s", "Quai modifiÃ© !");
  } else
    QMessageBox::critical(
        this, "Erreur",
        "Modification Ã©chouÃ©e !\n" +
            QSqlDatabase::database().lastError().text().left(250));
}

void fishingport::on_quai_btnSupprimer_clicked() {
  if (m_quaiSelectedId < 0) {
    QMessageBox::warning(this, "Attention",
                         "Veuillez sÃ©lectionner un quai d'abord.");
    return;
  }
  if (QMessageBox::question(
          this, "Confirmation",
          QString("Supprimer le quai ID %1 ?").arg(m_quaiSelectedId),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;
  Quai q(m_quaiSelectedId, "", 0, 0, "");
  if (!q.supprimer(m_quaiSelectedId)) {
    QMessageBox::critical(this, "Erreur", "Erreur lors de la suppression.");
    return;
  }
  QSqlDatabase::database().commit();
  QMessageBox::information(this, "SuccÃ¨s", "Quai supprimÃ©.");
  clearFormQuai();
  chargerQuais();
}

void fishingport::on_quai_btnStatistiques_clicked() {
  QMap<QString, int> counts = m_tmpQuai.getStatutCounts();
  StatsDialog dlg(counts, this);
  dlg.exec();
}

void fishingport::on_quai_lineEdit_recherche_textChanged(const QString &) {
  lancerRechercheQuai();
}
void fishingport::on_quai_comboBox_rechercheCritere_currentIndexChanged(int) {
  lancerRechercheQuai();
}
void fishingport::on_quai_comboBox_tri_currentIndexChanged(int) {
  lancerRechercheQuai();
}

void fishingport::on_quai_comboBox_export_currentIndexChanged(int index) {
  if (index == 0)
    return;
  QString format = ui->quai_comboBox_export->currentText();
  ui->quai_comboBox_export->setCurrentIndex(0);
  if (format == "Excel") {
    QString path = QFileDialog::getSaveFileName(
        this, "Exporter Quais (CSV)", "quais.csv", "CSV Files (*.csv)");
    if (path.isEmpty())
      return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::critical(this, "Erreur", "Impossible d'ouvrir.");
      return;
    }
    QTextStream out(&file);
    out << "\xEF\xBB\xBF" << "ID;Nom;CapacitÃ©;Longueur;Status\n";
    for (int r = 0; r < m_quaiModel->rowCount(); ++r) {
      for (int c = 0; c < m_quaiModel->columnCount(); ++c) {
        if (c)
          out << ";";
        out << m_quaiModel->data(m_quaiModel->index(r, c)).toString();
      }
      out << "\n";
    }
    file.close();
    QMessageBox::information(
        this, "Export CSV",
        QString("%1 quai(s) exportÃ©(s).").arg(m_quaiModel->rowCount()));
  } else if (format == "PDF")
    QMessageBox::information(
        this, "Export PDF",
        "Export PDF des quais disponible via QtPrintSupport.");
}

void fishingport::on_quai_btnMetier1_clicked() {
  m_tmpQuai.visualiserQuais(this);
}
void fishingport::on_quai_btnMetier2_clicked() {
  bool ok;
  int nb = ui->quai_lineEdit_nbBateaux->text().trimmed().toInt(&ok);
  if (!ok || nb <= 0) {
    QMessageBox::warning(this, "Saisie invalide",
                         "Nombre de bateaux valide requis (entier > 0).");
    return;
  }
  m_tmpQuai.lancerAffectationAutomatique(nb, this);
  chargerQuais();
}

// ==================================================================
//  SORTIES
// ==================================================================
void fishingport::initSortiePage() {
  sortie_tableView = ui->sortie_tableView;
  sortie_lineEditRecherche = ui->sortie_lineEditRecherche;
  sortie_lineEditId = ui->sortie_lineEditId;
  sortie_lineEditArrivee = ui->sortie_lineEditArrivee;
  sortie_lineEditDepart = ui->sortie_lineEditDepart;
  sortie_radioConfirme = ui->sortie_radioConfirme;
  sortie_radioPrevue = ui->sortie_radioPrevue;
  sortie_radioRetardee = ui->sortie_radioRetardee;
  sortie_radioAnnulee = ui->sortie_radioAnnulee;
  sortie_filterRadioConfirme = ui->sortie_formRadioConfirme;
  sortie_filterRadioPrevue = ui->sortie_formRadioPrevue;
  sortie_filterRadioRetardee = ui->sortie_formRadioRetardee;
  sortie_filterRadioAnnulee = ui->sortie_formRadioAnnulee;
  sortie_etatRadios = {sortie_radioConfirme, sortie_radioPrevue,
                       sortie_radioRetardee, sortie_radioAnnulee};
  sortie_listWidgetBateaux = ui->sortie_listWidgetBateaux;
  sortie_tableView->horizontalHeader()->setStretchLastSection(true);
  sortie_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  sortie_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  sortie_listWidgetBateaux->setSelectionMode(QAbstractItemView::MultiSelection);
  connect(ui->sortie_btnAjouter, &QPushButton::clicked, this,
          &fishingport::sortie_onAjouter);
  connect(ui->sortie_btnModifier, &QPushButton::clicked, this,
          &fishingport::sortie_onModifier);
  connect(ui->sortie_btnSupprimer, &QPushButton::clicked, this,
          &fishingport::sortie_onSupprimer);
  connect(ui->sortie_btnExportPDF, &QPushButton::clicked, this,
          &fishingport::sortie_exporterPDF);
  connect(ui->sortie_btnExportExcel, &QPushButton::clicked, this,
          &fishingport::sortie_exporterExcel);
  connect(ui->sortie_btnStats, &QPushButton::clicked, this,
          &fishingport::sortie_afficherStatistiques);
  connect(ui->sortie_btnMeteo, &QPushButton::clicked, this,
          &fishingport::sortie_suggestReschedule);
  connect(ui->sortie_btnAccident, &QPushButton::clicked, this,
          &fishingport::sortie_detecterAccident);
  connect(ui->pushButtonApply_2, &QPushButton::clicked, this,
          &fishingport::sortie_applyWeatherSuggestion);
  connect(ui->pushButtonAccidentApply_2, &QPushButton::clicked, this,
          &fishingport::sortie_applyAccidentSuggestion);
  connect(ui->listWidgetConflicts_2, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *current, QListWidgetItem *) {
            if (!current) {
              m_currentAccidentSuggestion = AccidentSuggestion();
              ui->labelAccidentSuggestion_2->setText(
                  "Selectionnez un conflit");
              ui->pushButtonAccidentApply_2->setEnabled(false);
              return;
            }

            m_currentAccidentSuggestion.sortieId =
                current->data(Qt::UserRole).toInt();
            m_currentAccidentSuggestion.newDepart =
                current->data(Qt::UserRole + 1).toDate();
            m_currentAccidentSuggestion.newArrivage =
                current->data(Qt::UserRole + 2).toDate();
            m_currentAccidentSuggestion.newEtat =
                current->data(Qt::UserRole + 3).toString();
            m_currentAccidentSuggestion.reason =
                current->data(Qt::UserRole + 4).toString();
            m_currentAccidentSuggestion.needsChange =
                current->data(Qt::UserRole + 5).toBool();

            ui->labelAccidentSuggestion_2->setText(
                QString("Sortie %1 : %2\nDepart suggere : %3\nArrivage suggere : %4\nEtat suggere : %5")
                    .arg(m_currentAccidentSuggestion.sortieId)
                    .arg(m_currentAccidentSuggestion.reason)
                    .arg(m_currentAccidentSuggestion.newDepart.toString(
                        "dd-MM-yyyy"))
                    .arg(m_currentAccidentSuggestion.newArrivage.toString(
                        "dd-MM-yyyy"))
                    .arg(sortieEtatLabel(
                        m_currentAccidentSuggestion.newEtat)));
            ui->pushButtonAccidentApply_2->setEnabled(
                m_currentAccidentSuggestion.needsChange);
          });
  connect(ui->sortie_btnRecherche, &QPushButton::clicked, this,
          &fishingport::sortie_rechercherParId);
  connect(sortie_lineEditRecherche, &QLineEdit::returnPressed, this,
          &fishingport::sortie_rechercherParId);
  connect(sortie_filterRadioConfirme, &QRadioButton::clicked, this,
          &fishingport::sortie_trierParEtat);
  connect(sortie_filterRadioPrevue, &QRadioButton::clicked, this,
          &fishingport::sortie_trierParEtat);
  connect(sortie_filterRadioRetardee, &QRadioButton::clicked, this,
          &fishingport::sortie_trierParEtat);
  connect(sortie_filterRadioAnnulee, &QRadioButton::clicked, this,
          &fishingport::sortie_trierParEtat);
  connect(ui->sortie_btnConfirmer, &QPushButton::clicked, this,
          &fishingport::sortie_onConfirmer);
  connect(ui->sortie_btnAnnuler, &QPushButton::clicked, this,
          &fishingport::sortie_onAnnuler);
  ui->pushButtonApply_2->setEnabled(false);
  ui->pushButtonAccidentApply_2->setEnabled(false);
}

void fishingport::on_btnNavSorties_clicked() {
  ui->groupBox_employe_form->hide();
  ui->btnNavSorties->setChecked(true);
  setActivePage(ui->page_4, "Sorties", "Suivi et planification");
  sortie_afficherTable();
  sortie_chargerListeBateaux();
}

void fishingport::sortie_afficherTable() {
  QSqlQueryModel *qm = sortieModel.listerTout();
  if (!sortieModel.lastError().isEmpty()) {
    QMessageBox::critical(this, "Erreur",
                          "Impossible de charger la table :\n" +
                              sortieModel.lastError());
    return;
  }
  sortie_tableView->setModel(qm);
}

void fishingport::sortie_clearForm() {
  m_sortieModeModification = false;
  sortie_lineEditId->clear();
  sortie_lineEditId->setReadOnly(false);
  sortie_lineEditArrivee->clear();
  sortie_lineEditDepart->clear();
  for (auto rb : sortie_etatRadios)
    rb->setChecked(false);
  sortie_listWidgetBateaux->clearSelection();
}

void fishingport::sortie_onAjouter() {
  sortie_clearForm();
  ui->sortie_stackedWidget->setCurrentIndex(0);
}

void fishingport::sortie_onModifier() {
  QModelIndex idx = sortie_tableView->currentIndex();
  if (!idx.isValid()) {
    QMessageBox::warning(this, "SÃ©lection requise",
                         "Veuillez sÃ©lectionner une ligne Ã  modifier.");
    return;
  }
  m_sortieModeModification = true;
  int row = idx.row();
  QAbstractItemModel *m = sortie_tableView->model();
  sortie_lineEditId->setText(m->data(m->index(row, 0)).toString());
  sortie_lineEditId->setReadOnly(true);
  sortie_lineEditArrivee->setText(
      m->data(m->index(row, 1)).toDate().toString("dd-MM-yyyy"));
  sortie_lineEditDepart->setText(
      m->data(m->index(row, 2)).toDate().toString("dd-MM-yyyy"));
  QString etat = m->data(m->index(row, 3)).toString();
  if (etat == "confirme")
    sortie_etatRadios[0]->setChecked(true);
  else if (etat == "prevue")
    sortie_etatRadios[1]->setChecked(true);
  else if (etat == "retardee")
    sortie_etatRadios[2]->setChecked(true);
  else if (etat == "annulee")
    sortie_etatRadios[3]->setChecked(true);
  sortie_preselectionnerBateaux(m->data(m->index(row, 0)).toInt());
  ui->sortie_stackedWidget->setCurrentIndex(0);
}

void fishingport::sortie_onSupprimer() {
  QModelIndex idx = sortie_tableView->currentIndex();
  if (!idx.isValid()) {
    QMessageBox::warning(this, "SÃ©lection requise",
                         "Veuillez sÃ©lectionner une ligne Ã  supprimer.");
    return;
  }
  int id = sortie_tableView->model()
               ->data(sortie_tableView->model()->index(idx.row(), 0))
               .toInt();
  if (QMessageBox::question(this, "Confirmation",
                            "Supprimer la sortie ID " + QString::number(id) +
                                " ?") == QMessageBox::Yes) {
    if (sortieModel.supprimer(id)) {
      sortie_afficherTable();
      QMessageBox::information(this, "SuccÃ¨s", "Sortie supprimÃ©e.");
    } else
      QMessageBox::critical(
          this, "Erreur", "Suppression Ã©chouÃ©e :\n" + sortieModel.lastError());
  }
}

void fishingport::sortie_onConfirmer() {
  if (m_sortieModeModification)
    sortie_modifierSortie();
  else
    sortie_ajouterSortie();
}
void fishingport::sortie_onAnnuler() {
  ui->sortie_stackedWidget->setCurrentIndex(1);
  sortie_clearForm();
}

void fishingport::sortie_rechercherParId() {
  QString id = sortie_lineEditRecherche->text().trimmed();
  if (id.isEmpty()) {
    sortie_afficherTable();
    return;
  }

  bool ok = false;
  const int idValue = id.toInt(&ok);
  if (!ok || idValue <= 0) {
    QMessageBox::warning(this, "Recherche invalide",
                         "Veuillez saisir un ID sortie numerique valide.");
    return;
  }

  QSqlQueryModel *qm = sortieModel.rechercherParId(idValue);
  sortie_tableView->setModel(qm);
}

void fishingport::sortie_trierParEtat() {
  QRadioButton *clicked = qobject_cast<QRadioButton *>(sender());
  static QRadioButton *lastChecked = nullptr;
  if (lastChecked == clicked) {
    clicked->setChecked(false);
    lastChecked = nullptr;
    sortie_afficherTable();
    return;
  }
  lastChecked = clicked;
  QString etat;
  if (sortie_filterRadioConfirme->isChecked())
    etat = "confirme";
  else if (sortie_filterRadioPrevue->isChecked())
    etat = "prevue";
  else if (sortie_filterRadioRetardee->isChecked())
    etat = "retardee";
  else if (sortie_filterRadioAnnulee->isChecked())
    etat = "annulee";
  if (etat.isEmpty()) {
    sortie_afficherTable();
    return;
  }
  sortie_tableView->setModel(sortieModel.filtrerParEtat(etat));
}

void fishingport::sortie_exporterPDF() {
  QString fp = QFileDialog::getSaveFileName(this, "Exporter en PDF",
                                            "sorties.pdf", "PDF Files (*.pdf)");
  if (fp.isEmpty())
    return;
  QMessageBox::information(this, "Export PDF", "PDF exportÃ© : " + fp);
}
void fishingport::sortie_exporterExcel() {
  QString fp = QFileDialog::getSaveFileName(
      this, "Exporter en Excel", "sorties.xml", "Excel Files (*.xml)");
  if (fp.isEmpty())
    return;
  QMessageBox::information(this, "Export Excel", "Excel exportÃ© : " + fp);
}

void fishingport::sortie_afficherStatistiques() {
  QMap<QString, int> data;
  int total = 0;
  if (!sortieModel.statistiques(data, total)) {
    QMessageBox::critical(this, "Erreur", sortieModel.lastError());
    return;
  }
  if (data.isEmpty()) {
    QMessageBox::information(this, "Statistiques", "Aucune donnÃ©e disponible.");
    return;
  }
  QDialog dlg(this);
  dlg.setWindowTitle("Statistiques des Sorties");
  dlg.resize(600, 450);
  QVBoxLayout *layout = new QVBoxLayout(&dlg);
  QLabel *title = new QLabel(QString("Total: %1 sortie(s)").arg(total));
  title->setAlignment(Qt::AlignCenter);
  layout->addWidget(title);
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  layout->addWidget(buttons);
  dlg.exec();
}

bool fishingport::sortie_tryGetTargetSortie(SortieInfo &info) {
  QList<SortieInfo> sorties = sortieModel.chargerToutesSorties();
  if (sorties.isEmpty())
    return false;

  int targetId = -1;
  if (!sortie_lineEditId->text().trimmed().isEmpty())
    targetId = sortie_lineEditId->text().trimmed().toInt();

  QModelIndex currentIndex = sortie_tableView->currentIndex();
  if (targetId <= 0 && currentIndex.isValid()) {
    targetId = sortie_tableView->model()
                   ->data(sortie_tableView->model()->index(currentIndex.row(), 0))
                   .toInt();
  }

  if (targetId > 0) {
    for (const SortieInfo &sortie : sorties) {
      if (sortie.id == targetId) {
        info = sortie;
        return true;
      }
    }
  }

  std::sort(sorties.begin(), sorties.end(),
            [](const SortieInfo &a, const SortieInfo &b) {
              if (a.depart == b.depart)
                return a.id < b.id;
              return a.depart < b.depart;
            });

  const QDate today = QDate::currentDate();
  for (const SortieInfo &sortie : sorties) {
    if (sortie.depart >= today) {
      info = sortie;
      return true;
    }
  }

  info = sorties.first();
  return true;
}

void fishingport::sortie_suggestReschedule() {
  SortieInfo target;
  if (!sortie_tryGetTargetSortie(target)) {
    QMessageBox::information(this, "Suggestion meteo",
                             "Aucune sortie exploitable n'a ete trouvee.");
    return;
  }

  struct ForecastDay {
    QDate date;
    int score = 0;
    QString condition;
  };

  QList<ForecastDay> forecast;
  for (int offset = 0; offset < 3; ++offset) {
    ForecastDay day;
    day.date = target.depart.addDays(offset);
    day.score = sortieWeatherScore(target.id, day.date);
    day.condition = sortieWeatherCondition(day.score);
    forecast.append(day);
  }

  auto writeForecast = [](QLabel *cond, QLabel *date, QLabel *score,
                          const ForecastDay &day) {
    cond->setText(day.condition);
    date->setText(day.date.toString("dd/MM"));
    score->setText(QString("%1/10").arg(day.score));
  };

  writeForecast(ui->labelD1Cond_2, ui->labelD1Date_2, ui->labelD1Score_2,
                forecast.value(0));
  writeForecast(ui->labelD2Cond_2, ui->labelD_2, ui->labelD2Score_2,
                forecast.value(1));
  writeForecast(ui->labelD3Cond_2, ui->label_29, ui->labelD3Score_2,
                forecast.value(2));

  ForecastDay bestDay = forecast.first();
  for (const ForecastDay &day : forecast) {
    if (day.score > bestDay.score)
      bestDay = day;
  }

  m_currentWeatherSuggestion = AccidentSuggestion();
  m_currentWeatherSuggestion.sortieId = target.id;
  m_currentWeatherSuggestion.newDepart = target.depart;
  m_currentWeatherSuggestion.newArrivage = target.arrivage;
  m_currentWeatherSuggestion.newEtat = target.etat;

  const int tripDuration = qMax(1, target.depart.daysTo(target.arrivage));
  if (bestDay.date != target.depart && bestDay.score >= forecast.first().score) {
    m_currentWeatherSuggestion.newDepart = bestDay.date;
    m_currentWeatherSuggestion.newArrivage = bestDay.date.addDays(tripDuration);
    m_currentWeatherSuggestion.newEtat = "retardee";
    m_currentWeatherSuggestion.needsChange = true;
    m_currentWeatherSuggestion.reason =
        QString("Conditions plus favorables le %1 (%2, score %3/10).")
            .arg(bestDay.date.toString("dd-MM-yyyy"))
            .arg(bestDay.condition)
            .arg(bestDay.score);
  } else if (forecast.first().score <= 3) {
    const QDate safeDate = target.depart.addDays(1);
    m_currentWeatherSuggestion.newDepart = safeDate;
    m_currentWeatherSuggestion.newArrivage = safeDate.addDays(tripDuration);
    m_currentWeatherSuggestion.newEtat = "retardee";
    m_currentWeatherSuggestion.needsChange = true;
    m_currentWeatherSuggestion.reason =
        QString("La fenetre actuelle est risquee (%1, score %2/10). "
                "Un decalage de 24h est recommande.")
            .arg(forecast.first().condition)
            .arg(forecast.first().score);
  } else {
    m_currentWeatherSuggestion.reason =
        QString("La sortie %1 peut etre maintenue le %2. Conditions estimees : %3 (%4/10).")
            .arg(target.id)
            .arg(target.depart.toString("dd-MM-yyyy"))
            .arg(forecast.first().condition)
            .arg(forecast.first().score);
  }

  if (m_currentWeatherSuggestion.needsChange) {
    ui->label_32->setText(
        QString("Sortie %1\n%2\nNouveau depart : %3\nNouvel arrivage : %4")
            .arg(target.id)
            .arg(m_currentWeatherSuggestion.reason)
            .arg(m_currentWeatherSuggestion.newDepart.toString("dd-MM-yyyy"))
            .arg(m_currentWeatherSuggestion.newArrivage.toString("dd-MM-yyyy")));
  } else {
    ui->label_32->setText(m_currentWeatherSuggestion.reason);
  }

  ui->pushButtonApply_2->setEnabled(m_currentWeatherSuggestion.needsChange);
}

void fishingport::sortie_detecterAccident() {
  ui->listWidgetConflicts_2->clear();
  ui->pushButtonAccidentApply_2->setEnabled(false);
  m_currentAccidentSuggestion = AccidentSuggestion();

  QList<SortieInfo> sorties = sortieModel.chargerToutesSorties();
  if (sorties.isEmpty()) {
    ui->labelAccidentTitle_2->setText("Aucune sortie a analyser");
    ui->labelAccidentSuggestion_2->setText("Ajoutez ou selectionnez une sortie.");
    return;
  }

  QHash<int, QStringList> boatsBySortie;
  for (const SortieInfo &sortie : sorties)
    boatsBySortie.insert(sortie.id, sortieModel.chargerBateauxSortie(sortie.id));

  int issueCount = 0;

  for (const SortieInfo &sortie : sorties) {
    if (sortie.arrivage <= sortie.depart) {
      QListWidgetItem *item = new QListWidgetItem(
          QString("Sortie %1 : chronologie invalide (%2 -> %3)")
              .arg(sortie.id)
              .arg(sortie.depart.toString("dd-MM-yyyy"))
              .arg(sortie.arrivage.toString("dd-MM-yyyy")));
      item->setData(Qt::UserRole, sortie.id);
      item->setData(Qt::UserRole + 1, sortie.depart);
      item->setData(Qt::UserRole + 2, sortie.depart.addDays(1));
      item->setData(Qt::UserRole + 3, QString("retardee"));
      item->setData(Qt::UserRole + 4,
                    "L'arrivage est anterieur ou egal au depart.");
      item->setData(Qt::UserRole + 5, true);
      ui->listWidgetConflicts_2->addItem(item);
      ++issueCount;
    }
  }

  for (int i = 0; i < sorties.size(); ++i) {
    for (int j = i + 1; j < sorties.size(); ++j) {
      const SortieInfo &a = sorties[i];
      const SortieInfo &b = sorties[j];
      if (a.arrivage < b.depart || b.arrivage < a.depart)
        continue;

      QStringList overlap;
      const QStringList boatsA = boatsBySortie.value(a.id);
      const QStringList boatsB = boatsBySortie.value(b.id);
      for (const QString &boat : boatsA) {
        if (boatsB.contains(boat) && !overlap.contains(boat))
          overlap << boat;
      }
      if (overlap.isEmpty())
        continue;

      const SortieInfo &toAdjust = (a.depart <= b.depart) ? b : a;
      const SortieInfo &blocking = (&toAdjust == &b) ? a : b;
      const int duration = qMax(1, toAdjust.depart.daysTo(toAdjust.arrivage));
      const QDate newDepart = blocking.arrivage.addDays(1);
      const QDate newArrivage = newDepart.addDays(duration);

      QListWidgetItem *item = new QListWidgetItem(
          QString("Chevauchement bateaux : sorties %1 et %2 (%3)")
              .arg(a.id)
              .arg(b.id)
              .arg(overlap.join(", ")));
      item->setData(Qt::UserRole, toAdjust.id);
      item->setData(Qt::UserRole + 1, newDepart);
      item->setData(Qt::UserRole + 2, newArrivage);
      item->setData(Qt::UserRole + 3, QString("retardee"));
      item->setData(Qt::UserRole + 4,
                    QString("Conflit de planification avec les bateaux : %1.")
                        .arg(overlap.join(", ")));
      item->setData(Qt::UserRole + 5, true);
      ui->listWidgetConflicts_2->addItem(item);
      ++issueCount;
    }
  }

  if (issueCount == 0) {
    ui->labelAccidentTitle_2->setText("Aucun accident detecte");
    ui->labelAccidentSuggestion_2->setText(
        "Aucun conflit critique n'a ete trouve dans les sorties.");
    return;
  }

  ui->labelAccidentTitle_2->setText(
      QString("%1 conflit(s) detecte(s)").arg(issueCount));
  ui->listWidgetConflicts_2->setCurrentRow(0);
}

void fishingport::sortie_ajouterSortie() {
  QString id = sortie_lineEditId->text().trimmed(),
          arrivee = sortie_lineEditArrivee->text().trimmed(),
          depart = sortie_lineEditDepart->text().trimmed(), etat;
  if (sortie_radioConfirme->isChecked())
    etat = "confirme";
  else if (sortie_radioPrevue->isChecked())
    etat = "prevue";
  else if (sortie_radioRetardee->isChecked())
    etat = "retardee";
  else if (sortie_radioAnnulee->isChecked())
    etat = "annulee";
  if (id.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant", "ID Sortie obligatoire.");
    return;
  }
  bool ok;
  int idInt = id.toInt(&ok);
  if (!ok || idInt <= 0) {
    QMessageBox::warning(this, "ID invalide",
                         "ID doit Ãªtre un entier positif.");
    return;
  }
  if (sortieModel.existe(idInt)) {
    QMessageBox::warning(this, "ID existant",
                         "Une sortie avec cet ID existe dÃ©jÃ .");
    return;
  }
  if (depart.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant", "Date dÃ©part obligatoire.");
    return;
  }
  QDate qD = QDate::fromString(depart, "dd-MM-yyyy");
  if (!qD.isValid()) {
    QMessageBox::warning(this, "Date invalide", "Format DD-MM-YYYY requis.");
    return;
  }
  if (arrivee.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant", "Date arrivÃ©e obligatoire.");
    return;
  }
  QDate qA = QDate::fromString(arrivee, "dd-MM-yyyy");
  if (!qA.isValid()) {
    QMessageBox::warning(this, "Date invalide", "Format DD-MM-YYYY requis.");
    return;
  }
  if (qA <= qD) {
    QMessageBox::warning(this, "Dates incohÃ©rentes",
                         "Date arrivÃ©e doit Ãªtre aprÃ¨s date dÃ©part.");
    return;
  }
  if (etat.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant",
                         "Veuillez sÃ©lectionner un Ã©tat.");
    return;
  }
  if (sortie_bateauxSelectionnes().isEmpty()) {
    QMessageBox::warning(this, "Champ manquant",
                         "Veuillez sÃ©lectionner au moins un bateau.");
    return;
  }
  if (sortieModel.ajouter(idInt, qA, qD, etat)) {
    sortie_sauvegarderAvoir(idInt);
    QMessageBox::information(this, "SuccÃ¨s", "Sortie ajoutÃ©e.");
    sortie_clearForm();
    sortie_afficherTable();
    ui->sortie_stackedWidget->setCurrentIndex(1);
  } else
    QMessageBox::critical(this, "Erreur",
                          "Ã‰chec de l'ajout :\n" + sortieModel.lastError());
}

void fishingport::sortie_modifierSortie() {
  QString id = sortie_lineEditId->text().trimmed(),
          arrivee = sortie_lineEditArrivee->text().trimmed(),
          depart = sortie_lineEditDepart->text().trimmed(), etat;
  if (sortie_radioConfirme->isChecked())
    etat = "confirme";
  else if (sortie_radioPrevue->isChecked())
    etat = "prevue";
  else if (sortie_radioRetardee->isChecked())
    etat = "retardee";
  else if (sortie_radioAnnulee->isChecked())
    etat = "annulee";
  if (id.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "ID manquant.");
    return;
  }
  int idInt = id.toInt();
  if (depart.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant", "Date dÃ©part obligatoire.");
    return;
  }
  QDate qD = QDate::fromString(depart, "dd-MM-yyyy");
  if (!qD.isValid()) {
    QMessageBox::warning(this, "Date invalide", "Format DD-MM-YYYY requis.");
    return;
  }
  if (arrivee.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant", "Date arrivÃ©e obligatoire.");
    return;
  }
  QDate qA = QDate::fromString(arrivee, "dd-MM-yyyy");
  if (!qA.isValid()) {
    QMessageBox::warning(this, "Date invalide", "Format DD-MM-YYYY requis.");
    return;
  }
  if (qA <= qD) {
    QMessageBox::warning(this, "Dates incohÃ©rentes",
                         "Date arrivÃ©e doit Ãªtre aprÃ¨s date dÃ©part.");
    return;
  }
  if (etat.isEmpty()) {
    QMessageBox::warning(this, "Champ manquant",
                         "Veuillez sÃ©lectionner un Ã©tat.");
    return;
  }
  if (sortie_bateauxSelectionnes().isEmpty()) {
    QMessageBox::warning(this, "Champ manquant",
                         "Veuillez sÃ©lectionner au moins un bateau.");
    return;
  }
  if (sortieModel.modifier(idInt, qA, qD, etat)) {
    sortie_sauvegarderAvoir(idInt);
    QMessageBox::information(this, "SuccÃ¨s", "Sortie modifiÃ©e.");
    sortie_clearForm();
    sortie_afficherTable();
    ui->sortie_stackedWidget->setCurrentIndex(1);
  } else
    QMessageBox::critical(this, "Erreur",
                          "Ã‰chec de la modification :\n" +
                              sortieModel.lastError());
}

void fishingport::sortie_syncFormWithSuggestion(
    const AccidentSuggestion &suggestion) {
  if (suggestion.sortieId <= 0)
    return;

  if (sortie_lineEditId->text().trimmed() == QString::number(suggestion.sortieId)) {
    sortie_lineEditDepart->setText(suggestion.newDepart.toString("dd-MM-yyyy"));
    sortie_lineEditArrivee->setText(
        suggestion.newArrivage.toString("dd-MM-yyyy"));
    sortieApplyEtatToRadios(sortie_etatRadios, suggestion.newEtat);
  }

  QAbstractItemModel *model = sortie_tableView->model();
  if (!model)
    return;

  for (int row = 0; row < model->rowCount(); ++row) {
    if (model->data(model->index(row, 0)).toInt() == suggestion.sortieId) {
      sortie_tableView->selectRow(row);
      break;
    }
  }
}

void fishingport::sortie_applyWeatherSuggestion() {
  if (!m_currentWeatherSuggestion.needsChange ||
      m_currentWeatherSuggestion.sortieId <= 0) {
    QMessageBox::information(this, "Suggestion meteo",
                             "Aucun ajustement meteo a appliquer.");
    return;
  }

    if (!sortieModel.modifier(
      m_currentWeatherSuggestion.sortieId,
      m_currentWeatherSuggestion.newArrivage,
      m_currentWeatherSuggestion.newDepart,
      m_currentWeatherSuggestion.newEtat)) {
    QMessageBox::critical(this, "Erreur",
                          "Impossible d'appliquer la suggestion meteo :\n" +
                              sortieModel.lastError());
    return;
  }

  sortie_afficherTable();
  sortie_syncFormWithSuggestion(m_currentWeatherSuggestion);
  ui->label_32->setText("Suggestion appliquee avec succes.");
  ui->pushButtonApply_2->setEnabled(false);
  m_currentWeatherSuggestion.needsChange = false;
}

void fishingport::sortie_applyAccidentSuggestion() {
  if (!m_currentAccidentSuggestion.needsChange ||
      m_currentAccidentSuggestion.sortieId <= 0) {
    QMessageBox::information(this, "Detection d'accident",
                             "Selectionnez un conflit avec une action a appliquer.");
    return;
  }

    if (!sortieModel.modifier(
      m_currentAccidentSuggestion.sortieId,
      m_currentAccidentSuggestion.newArrivage,
      m_currentAccidentSuggestion.newDepart,
      m_currentAccidentSuggestion.newEtat)) {
    QMessageBox::critical(this, "Erreur",
                          "Impossible d'appliquer la suggestion :\n" +
                              sortieModel.lastError());
    return;
  }

  sortie_afficherTable();
  sortie_syncFormWithSuggestion(m_currentAccidentSuggestion);
  ui->labelAccidentSuggestion_2->setText("Suggestion appliquee avec succes.");
  ui->pushButtonAccidentApply_2->setEnabled(false);
  m_currentAccidentSuggestion.needsChange = false;
  if (QListWidgetItem *item = ui->listWidgetConflicts_2->currentItem())
    item->setForeground(QColor("#23A559"));
}

void fishingport::sortie_chargerListeBateaux() {
  sortie_listWidgetBateaux->clear();
  for (const BateauInfo &b : sortieModel.chargerListeBateaux()) {
    QListWidgetItem *item = new QListWidgetItem(
        QString("%1 | %2 | %3").arg(b.numeroCarteGrise, b.type, b.nomb));
    item->setData(Qt::UserRole, b.numeroCarteGrise);
    sortie_listWidgetBateaux->addItem(item);
  }
}

void fishingport::sortie_preselectionnerBateaux(int sortieId) {
  QStringList linked = sortieModel.chargerBateauxSortie(sortieId);
  for (int i = 0; i < sortie_listWidgetBateaux->count(); ++i) {
    QListWidgetItem *item = sortie_listWidgetBateaux->item(i);
    if (linked.contains(item->data(Qt::UserRole).toString()))
      item->setSelected(true);
  }
}

QStringList fishingport::sortie_bateauxSelectionnes() const {
  QStringList res;
  for (auto *item : sortie_listWidgetBateaux->selectedItems())
    res << item->data(Qt::UserRole).toString();
  return res;
}

void fishingport::sortie_sauvegarderAvoir(int sortieId) {
  sortieModel.sauvegarderAvoir(sortieId, sortie_bateauxSelectionnes());
}

// ==================================================================
//  EMPLOYÃ‰S
// ==================================================================
void fishingport::initEmployePage() {
  m_employeModel = new QSqlQueryModel(this);
  ui->tableView->setModel(m_employeModel);
  ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  ui->tableView->verticalHeader()->setVisible(false);
  ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->tableView->setAlternatingRowColors(true);
  ui->tableView->setShowGrid(false);

  connect(ui->tableView, &QTableView::clicked, this,
          [this](const QModelIndex &index) {
            int row = index.row();
            int id =
                m_employeModel->data(m_employeModel->index(row, 0)).toInt();
            QString nom =
                m_employeModel->data(m_employeModel->index(row, 1)).toString();
            QString lic =
                m_employeModel->data(m_employeModel->index(row, 2)).toString();
            QString cont =
                m_employeModel->data(m_employeModel->index(row, 3)).toString();
            QString role =
                m_employeModel->data(m_employeModel->index(row, 4)).toString();
            QSqlQuery q;
            q.prepare("SELECT MOT_DE_PASSE FROM EMPLOYE WHERE ID_EMPLOYE=:id");
            q.bindValue(":id", id);
            QString mdp = "";
            if (q.exec() && q.next())
              mdp = q.value(0).toString();
            // SÃ©parer tÃ©lÃ©phone et email (stockÃ©s comme "telephone|email" dans
            // CONTACT)
            QStringList contactParts = cont.split("|");
            QString telephone = contactParts.value(0).trimmed();
            QString email = contactParts.value(1).trimmed();
            ui->employe_lineEdit_id->setText(QString::number(id));
            ui->employe_lineEdit_id->setReadOnly(false);
            ui->employe_lineEdit_nom->setText(nom);
            ui->employe_lineEdit_licence->setText(lic);
            ui->employe_lineEdit_contact->setText(telephone);
            ui->employe_lineEdit_email->setText(email);
            ui->employe_comboBox_role->setCurrentText(role);
            ui->employe_lineEdit_mdp->setText(mdp);
            m_employeModeModification = true;
          });

  connect(ui->btnNavEmployes, &QPushButton::clicked, this,
          &fishingport::on_btnNavEmployes_clicked);
  connect(ui->employe_btnAjouter, &QPushButton::clicked, this,
          &fishingport::employe_onAjouter);
  connect(ui->employe_btnModifier, &QPushButton::clicked, this,
          &fishingport::employe_onModifier);
  connect(ui->employe_btnSupprimer, &QPushButton::clicked, this,
          &fishingport::employe_onSupprimer);
  connect(ui->employe_btnSearch, &QPushButton::clicked, this,
          &fishingport::employe_rechercher);
  connect(ui->employe_btnSort, &QPushButton::clicked, this,
          &fishingport::employe_trier);
  connect(ui->employe_btnRefresh, &QPushButton::clicked, this,
          &fishingport::employe_afficherTable);

  // â”€â”€ Consulter â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  connect(ui->employe_btnConsulter, &QPushButton::clicked, this, [this]() {
    QModelIndexList sel = ui->tableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) {
      QMessageBox::warning(this, "Consulter",
                           "Veuillez sÃ©lectionner un employÃ©.");
      return;
    }
    int row = sel.first().row();
    int id = m_employeModel->data(m_employeModel->index(row, 0)).toInt();
    QString nom =
        m_employeModel->data(m_employeModel->index(row, 1)).toString();
    QString lic =
        m_employeModel->data(m_employeModel->index(row, 2)).toString();
    QString cont =
        m_employeModel->data(m_employeModel->index(row, 3)).toString();
    QString role =
        m_employeModel->data(m_employeModel->index(row, 4)).toString();

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Fiche EmployÃ©");
    dlg->setFixedSize(420, 480);
    dlg->setStyleSheet("QDialog{background:#F0F8FF;}QLabel{border:none;"
                       "background:transparent;}");
    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *header = new QFrame();
    header->setFixedHeight(110);
    header->setStyleSheet("QFrame{background:qlineargradient(x1:0,y1:0,x2:0,y2:"
                          "1,stop:0 #29B6F6,stop:1 #0288D1);border:none;}");
    QVBoxLayout *hL = new QVBoxLayout(header);
    hL->setContentsMargins(0, 12, 0, 12);
    hL->setSpacing(4);
    hL->setAlignment(Qt::AlignCenter);
    QLabel *avatarLbl = new QLabel("ðŸ‘¤");
    avatarLbl->setFixedSize(54, 54);
    avatarLbl->setAlignment(Qt::AlignCenter);
    avatarLbl->setStyleSheet(
        "QLabel{background:rgba(255,255,255,0.25);border-radius:27px;font-size:"
        "28px;color:white;border:2px solid rgba(255,255,255,0.5);}");
    QLabel *roleBadge = new QLabel(role);
    roleBadge->setAlignment(Qt::AlignCenter);
    roleBadge->setStyleSheet(
        "QLabel{background:#FF8A65;color:white;border-radius:10px;padding:3px "
        "14px;font-size:11px;font-weight:bold;}");
    QLabel *titleLbl = new QLabel("Fiche EmployÃ©");
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet("font-size:14px;font-weight:bold;color:white;");
    QLabel *subLbl = new QLabel("Informations complÃ¨tes du compte");
    subLbl->setAlignment(Qt::AlignCenter);
    subLbl->setStyleSheet("font-size:10px;color:rgba(255,255,255,0.85);");
    hL->addWidget(avatarLbl, 0, Qt::AlignCenter);
    hL->addWidget(titleLbl);
    hL->addWidget(subLbl);
    hL->addWidget(roleBadge, 0, Qt::AlignCenter);
    root->addWidget(header);

    QWidget *body = new QWidget();
    body->setStyleSheet("background:white;");
    QVBoxLayout *bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(24, 20, 24, 20);
    bodyL->setSpacing(10);

    auto makeInfoRow = [&](const QString &icon, const QString &labelTxt,
                           const QString &value) {
      QFrame *rw = new QFrame();
      rw->setStyleSheet("QFrame{background:#F8FBFF;border-radius:10px;border:"
                        "1px solid #E3F2FD;}");
      QHBoxLayout *rl = new QHBoxLayout(rw);
      rl->setContentsMargins(14, 10, 14, 10);
      rl->setSpacing(12);
      QLabel *iLbl = new QLabel(icon);
      iLbl->setFixedSize(28, 28);
      iLbl->setAlignment(Qt::AlignCenter);
      iLbl->setStyleSheet("font-size:16px;background:transparent;border:none;");
      QVBoxLayout *tL = new QVBoxLayout();
      tL->setSpacing(1);
      QLabel *lbl = new QLabel(labelTxt);
      lbl->setStyleSheet("font-size:9px;color:#90A4AE;font-weight:bold;letter-"
                         "spacing:1px;background:transparent;border:none;");
      QLabel *val = new QLabel(value);
      val->setStyleSheet("font-size:13px;color:#1A237E;font-weight:600;"
                         "background:transparent;border:none;");
      tL->addWidget(lbl);
      tL->addWidget(val);
      rl->addWidget(iLbl);
      rl->addLayout(tL);
      rl->addStretch();
      bodyL->addWidget(rw);
    };
    makeInfoRow("ðŸªª", "ID EMPLOYÃ‰", QString::number(id));
    makeInfoRow("ðŸ‘¤", "NOM", nom);
    makeInfoRow("ðŸ“‹", "NUMÃ‰RO DE LICENCE", lic);
    makeInfoRow("ðŸ“ž", "CONTACT", cont);
    bodyL->addStretch();
    QPushButton *btnF = new QPushButton("âœ•  Fermer");
    btnF->setFixedHeight(44);
    btnF->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 "
        "#0288D1,stop:1 "
        "#29B6F6);color:white;border-radius:10px;font-size:13px;font-weight:"
        "bold;border:none;}QPushButton:hover{background:#0277BD;}");
    connect(btnF, &QPushButton::clicked, dlg, &QDialog::close);
    bodyL->addWidget(btnF);
    root->addWidget(body);
    dlg->exec();
  });

  // â”€â”€ Statistiques â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  connect(ui->pushButton_11, &QPushButton::clicked, this,
          &fishingport::employe_afficherStatistiques);

  // â”€â”€ Exporter â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  connect(ui->pushButton_10, &QPushButton::clicked, this, [this]() {
    QString format = ui->comboBox_3->currentText();
    if (format.isEmpty()) {
      QMessageBox::warning(this, "Erreur",
                           "Veuillez sÃ©lectionner un format d'export");
      return;
    }
    if (format == "PDF")
      employe_exporterPDF();
    else if (format == "Excel")
      employe_exporterExcel();
    else if (format == "CSV")
      employe_exporterCSV();
  });

  // Validateurs
  ui->employe_lineEdit_nom->setValidator(new QRegularExpressionValidator(
      QRegularExpression("^[a-zA-Z\\s]*$"), this));
  connect(ui->employe_lineEdit_nom, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (!text.isEmpty()) {
              QRegularExpression r("^[a-zA-Z\\s]+$");
              ui->employe_lineEdit_nom->setStyleSheet(
                  r.match(text).hasMatch()
                      ? "QLineEdit{border:2px solid green;}"
                      : "QLineEdit{border:2px solid red;}");
            } else
              ui->employe_lineEdit_nom->setStyleSheet("");
          });
  connect(ui->employe_lineEdit_licence, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (!text.isEmpty()) {
              QRegularExpression r("^LIC\\d*$");
              ui->employe_lineEdit_licence->setStyleSheet(
                  r.match(text).hasMatch()
                      ? "QLineEdit{border:2px solid green;}"
                      : "QLineEdit{border:2px solid red;}");
            } else
              ui->employe_lineEdit_licence->setStyleSheet("");
          });
  ui->employe_lineEdit_contact->setValidator(
      new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), this));
  connect(ui->employe_lineEdit_contact, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (!text.isEmpty()) {
              QString d = text;
              d.remove(QRegularExpression("[^0-9]"));
              ui->employe_lineEdit_contact->setStyleSheet(
                  d.length() >= 8 ? "QLineEdit{border:2px solid green;}"
                                  : "QLineEdit{border:2px solid red;}");
            } else
              ui->employe_lineEdit_contact->setStyleSheet("");
          });
  connect(ui->employe_lineEdit_email, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (!text.isEmpty()) {
              QRegularExpression r(
                  "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
              ui->employe_lineEdit_email->setStyleSheet(
                  r.match(text).hasMatch()
                      ? "QLineEdit{border:2px solid green;}"
                      : "QLineEdit{border:2px solid orange;}");
            } else
              ui->employe_lineEdit_email->setStyleSheet(
                  "QLineEdit{border:2px solid orange;}");
          });
}

void fishingport::on_btnNavEmployes_clicked() {
  ui->btnNavEmployes->setChecked(true);
  setActivePage(ui->page_employes, "Employes", "Gestion RH et acces");
  ui->groupBox_employe_form->show();
  employe_afficherTable();
  appliquerDroitsRole();
}

void fishingport::on_btnNavHardware_clicked() {
  ui->groupBox_employe_form->hide();
  if (m_btnNavHardware)
    m_btnNavHardware->setChecked(true);
  setActivePage(ui->page, "Arduino / Hardware", "Monitoring capteurs");
  int idx = ui->stackedWidget_2->indexOf(m_ardPage);
  if (idx >= 0)
    ui->stackedWidget_2->setCurrentIndex(idx);
}

void fishingport::employe_afficherTable() {
  employe_clearForm();
  m_employe.rafraichirTable(m_employeModel, "ID_EMPLOYE", "ASC");
}

void fishingport::employe_clearForm() {
  ui->employe_lineEdit_id->clear();
  ui->employe_lineEdit_id->setReadOnly(true);
  ui->employe_lineEdit_id->setPlaceholderText("ID sera gÃ©nÃ©rÃ© automatiquement");
  ui->employe_lineEdit_nom->clear();
  ui->employe_lineEdit_nom->setStyleSheet("");
  ui->employe_lineEdit_licence->clear();
  ui->employe_lineEdit_licence->setStyleSheet("");
  ui->employe_lineEdit_contact->clear();
  ui->employe_lineEdit_contact->setStyleSheet("");
  ui->employe_lineEdit_email->clear();
  ui->employe_lineEdit_email->setStyleSheet("");
  ui->employe_comboBox_role->setCurrentIndex(0);
  ui->employe_lineEdit_mdp->clear();
  m_employeModeModification = false;
}

void fishingport::employe_onAjouter() {
  QString nom = ui->employe_lineEdit_nom->text().trimmed(),
          licence = ui->employe_lineEdit_licence->text().trimmed();
  QString contact = ui->employe_lineEdit_contact->text().trimmed(),
          email = ui->employe_lineEdit_email->text().trimmed();
  QString role = ui->employe_comboBox_role->currentText(),
          mdp = ui->employe_lineEdit_mdp->text().trimmed();
  if (nom.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Nom obligatoire");
    return;
  }
  QRegularExpression nameR("^[a-zA-Z\\s]+$");
  if (!nameR.match(nom).hasMatch()) {
    QMessageBox::warning(this, "Erreur",
                         "Le nom ne doit contenir que des lettres");
    return;
  }
  if (licence.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "NumÃ©ro de licence obligatoire");
    return;
  }
  QRegularExpression licR("^LIC\\d+$");
  if (!licR.match(licence).hasMatch()) {
    QMessageBox::warning(this, "Erreur",
                         "La licence doit commencer par LIC suivi de chiffres");
    return;
  }
  if (contact.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Contact obligatoire");
    return;
  }
  QString pd = contact;
  pd.remove(QRegularExpression("[^0-9]"));
  if (pd.length() < 8) {
    QMessageBox::warning(this, "Erreur", "TÃ©lÃ©phone : 8 chiffres minimum");
    return;
  }
  if (email.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Email obligatoire");
    return;
  }
  QRegularExpression emailR(
      "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
  if (!emailR.match(email).hasMatch()) {
    QMessageBox::warning(this, "Erreur", "Email invalide");
    return;
  }
  if (role.isEmpty() || role == "SÃ©lÃ©ctionner") {
    QMessageBox::warning(this, "Erreur", "RÃ´le obligatoire");
    return;
  }
  if (mdp.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Mot de passe obligatoire");
    return;
  }
  int newId = m_employe.getNextId();
  if (newId == -1) {
    QMessageBox::critical(this, "Erreur", "Impossible de gÃ©nÃ©rer un ID");
    return;
  }
  if (m_employe.ajouter(newId, nom, licence, contact, role, mdp)) {
    QMessageBox::information(this, "SuccÃ¨s",
                             QString("EmployÃ© ajoutÃ© (ID:%1)").arg(newId));
    employe_clearForm();
    employe_afficherTable();
  } else
    QMessageBox::critical(this, "Erreur", "Ã‰chec de l'ajout");
}

void fishingport::employe_onModifier() {
  QString idText = ui->employe_lineEdit_id->text().trimmed(),
          nom = ui->employe_lineEdit_nom->text().trimmed();
  QString licence = ui->employe_lineEdit_licence->text().trimmed(),
          contact = ui->employe_lineEdit_contact->text().trimmed();
  QString email = ui->employe_lineEdit_email->text().trimmed(),
          role = ui->employe_comboBox_role->currentText();
  QString mdp = ui->employe_lineEdit_mdp->text().trimmed();
  if (idText.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "ID employÃ© obligatoire");
    return;
  }
  bool ok;
  int id = idText.toInt(&ok);
  if (!ok || id <= 0) {
    QMessageBox::warning(this, "Erreur", "ID doit Ãªtre un entier positif");
    return;
  }
  if (!m_employe.existeId(id)) {
    QMessageBox::warning(this, "Erreur", "Cet employÃ© n'existe pas");
    return;
  }
  if (nom.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Nom obligatoire");
    return;
  }
  QRegularExpression nameR("^[a-zA-Z\\s]+$");
  if (!nameR.match(nom).hasMatch()) {
    QMessageBox::warning(this, "Erreur",
                         "Le nom ne doit contenir que des lettres");
    return;
  }
  if (licence.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Licence obligatoire");
    return;
  }
  QRegularExpression licR("^LIC\\d+$");
  if (!licR.match(licence).hasMatch()) {
    QMessageBox::warning(this, "Erreur", "La licence doit commencer par LIC");
    return;
  }
  if (contact.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Contact obligatoire");
    return;
  }
  QString pd = contact;
  pd.remove(QRegularExpression("[^0-9]"));
  if (pd.length() < 8) {
    QMessageBox::warning(this, "Erreur", "TÃ©lÃ©phone : 8 chiffres minimum");
    return;
  }
  if (email.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Email obligatoire");
    return;
  }
  QRegularExpression emailR(
      "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
  if (!emailR.match(email).hasMatch()) {
    QMessageBox::warning(this, "Erreur", "Email invalide");
    return;
  }
  if (role.isEmpty() || role == "SÃ©lÃ©ctionner") {
    QMessageBox::warning(this, "Erreur", "RÃ´le obligatoire");
    return;
  }
  if (mdp.isEmpty()) {
    QMessageBox::warning(this, "Erreur", "Mot de passe obligatoire");
    return;
  }
  // Reconstruire le champ CONTACT avec le sÃ©parateur |
  QString contactFull = contact + "|" + email;
  if (m_employe.modifier(id, nom, licence, contactFull, role, mdp)) {
    QMessageBox::information(this, "SuccÃ¨s", "EmployÃ© modifiÃ©");
    employe_clearForm();
    employe_afficherTable();
  } else
    QMessageBox::critical(this, "Erreur", "Ã‰chec de la modification");
}

void fishingport::employe_onSupprimer() {
  QModelIndexList sel = ui->tableView->selectionModel()->selectedRows();
  if (sel.isEmpty()) {
    QMessageBox::warning(this, "Erreur",
                         "Veuillez sÃ©lectionner un employÃ© Ã  supprimer");
    return;
  }
  int row = sel.first().row();
  int id = m_employeModel->data(m_employeModel->index(row, 0)).toInt();
  QString nom = m_employeModel->data(m_employeModel->index(row, 1)).toString();
  if (QMessageBox::question(
          this, "Confirmation",
          QString("Supprimer l'employÃ© %1 (ID:%2) ?").arg(nom).arg(id),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    if (m_employe.supprimer(id)) {
      QMessageBox::information(this, "SuccÃ¨s", "EmployÃ© supprimÃ©");
      employe_clearForm();
      employe_afficherTable();
    } else
      QMessageBox::critical(this, "Erreur", "Ã‰chec de la suppression");
  }
}

void fishingport::employe_onConfirmer() {
  if (m_employeModeModification)
    employe_modifierEmploye();
  else
    employe_ajouterEmploye();
}
void fishingport::employe_onAnnuler() { employe_clearForm(); }

void fishingport::employe_rechercher() {
  QString searchText = ui->employe_lineEdit_search->text().trimmed();
  QString searchCol = ui->employe_comboBox_searchBy->currentText();
  QString orderCol = ui->employe_comboBox_searchType->currentText();
  QString orderDir =
      ui->employe_comboBox_order->currentText() == "Croissant" ? "ASC" : "DESC";
  if (searchCol == "Recherche par:")
    searchCol = "";
  if (orderCol == "ID employÃ©")
    orderCol = "ID_EMPLOYE";
  else if (orderCol == "Nom")
    orderCol = "NOME";
  else if (orderCol == "NumÃ©ro de licence")
    orderCol = "NUM_LICENSE";
  else if (orderCol == "Contact")
    orderCol = "CONTACT";
  else
    orderCol = "ID_EMPLOYE";
  if (searchText.isEmpty())
    m_employe.rafraichirTable(m_employeModel, orderCol, orderDir);
  else
    m_employe.rechercher(m_employeModel, searchText, searchCol, orderCol,
                         orderDir);
}

void fishingport::employe_trier() {
  QString orderCol = ui->employe_comboBox_searchType->currentText();
  QString orderDir =
      ui->employe_comboBox_order->currentText() == "Croissant" ? "ASC" : "DESC";
  if (orderCol == "ID employÃ©")
    orderCol = "ID_EMPLOYE";
  else if (orderCol == "Nom")
    orderCol = "NOME";
  else if (orderCol == "NumÃ©ro de licence")
    orderCol = "NUM_LICENSE";
  else if (orderCol == "Contact")
    orderCol = "CONTACT";
  else
    orderCol = "ID_EMPLOYE";
  m_employe.rafraichirTable(m_employeModel, orderCol, orderDir);
}

void fishingport::employe_exporterPDF() {
  QMessageBox::information(this, "Export PDF",
                           "FonctionnalitÃ© d'export PDF Ã  implÃ©menter");
}
void fishingport::employe_exporterExcel() {
  QMessageBox::information(this, "Export Excel",
                           "FonctionnalitÃ© d'export Excel Ã  implÃ©menter");
}
void fishingport::employe_exporterCSV() {
  QMessageBox::information(this, "Export CSV",
                           "FonctionnalitÃ© d'export CSV Ã  implÃ©menter");
}

// â”€â”€ Statistiques EmployÃ©s â€” identiques Ã  la capture (camembert + barres + KPI
// + rÃ©cap) â”€â”€
void fishingport::employe_afficherStatistiques() {
  int total = m_employe.getTotalEmployes();
  QMap<QString, int> parRole = m_employe.getEmployesParRole();

  if (parRole.isEmpty()) {
    QMessageBox::information(this, "Statistiques", "Aucune donnÃ©e disponible.");
    return;
  }

  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Statistiques des EmployÃ©s");
  dlg->resize(720, 580);
  dlg->setStyleSheet("QDialog{background:#F0F8FF;}QLabel{border:none;"
                     "background:transparent;}QFrame{border:none;}");

  QVBoxLayout *root = new QVBoxLayout(dlg);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(16);

  // â”€â”€ Header â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *hdrFrame = new QFrame();
  hdrFrame->setFixedHeight(60);
  hdrFrame->setStyleSheet(
      "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 "
      "#0288D1,stop:1 #29B6F6);border-radius:12px;}");
  QHBoxLayout *hL = new QHBoxLayout(hdrFrame);
  hL->setContentsMargins(20, 0, 20, 0);
  QLabel *hIcon = new QLabel("ðŸ“Š");
  hIcon->setStyleSheet("font-size:24px;");
  QLabel *hTitle = new QLabel("Statistiques des EmployÃ©s");
  hTitle->setStyleSheet("font-size:17px;font-weight:bold;color:white;");
  QLabel *hTotal = new QLabel(QString("ðŸ‘¥ Total employÃ©s : %1").arg(total));
  hTotal->setStyleSheet(
      "font-size:12px;color:rgba(255,255,255,0.9);font-weight:bold;");
  hL->addWidget(hIcon);
  hL->addSpacing(8);
  hL->addWidget(hTitle);
  hL->addStretch();
  hL->addWidget(hTotal);
  root->addWidget(hdrFrame);

  // â”€â”€ Calcul du rÃ´le dominant â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QString roleDominant;
  int roleDominantCount = 0, rolesDistincts = parRole.size();
  for (auto it = parRole.begin(); it != parRole.end(); ++it) {
    if (it.value() > roleDominantCount) {
      roleDominantCount = it.value();
      roleDominant = it.key();
    }
  }
  int partDominant = (total > 0) ? roleDominantCount * 100 / total : 0;
  Q_UNUSED(partDominant);

  // â”€â”€ KPI Cards â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *kpiRow = new QHBoxLayout();
  kpiRow->setSpacing(10);
  struct KPI {
    QString icon, title, value, bg, border, color;
  };
  QList<KPI> kpis = {
      {"ðŸ‘¥", "Total employÃ©s", QString::number(total), "#E3F2FD", "#90CAF9",
       "#0D47A1"},
      {"ðŸ†", "RÃ´le dominant", roleDominant, "#FFF8E1", "#FFD54F", "#E65100"},
      {"ðŸ“Œ", "RÃ´les distincts", QString::number(rolesDistincts), "#E8F5E9",
       "#A5D6A7", "#2E7D32"}};
  for (const KPI &k : kpis) {
    QFrame *card = new QFrame();
    card->setStyleSheet(
        QString("QFrame{background:%1;border:2px solid %2;border-radius:14px;}")
            .arg(k.bg, k.border));
    QVBoxLayout *cl = new QVBoxLayout(card);
    cl->setContentsMargins(14, 12, 14, 12);
    cl->setSpacing(4);
    cl->setAlignment(Qt::AlignCenter);
    QLabel *iconL = new QLabel(k.icon);
    iconL->setAlignment(Qt::AlignCenter);
    iconL->setStyleSheet("font-size:22px;");
    QLabel *valL = new QLabel(k.value);
    valL->setAlignment(Qt::AlignCenter);
    valL->setWordWrap(true);
    valL->setStyleSheet(
        QString("font-size:18px;font-weight:bold;color:%1;").arg(k.color));
    QLabel *titL = new QLabel(k.title);
    titL->setAlignment(Qt::AlignCenter);
    titL->setWordWrap(true);
    titL->setStyleSheet(
        QString("font-size:10px;color:%1;font-weight:bold;").arg(k.color));
    cl->addWidget(iconL);
    cl->addWidget(valL);
    cl->addWidget(titL);
    kpiRow->addWidget(card);
  }
  root->addLayout(kpiRow);

  // â”€â”€ Graphiques cÃ´te Ã  cÃ´te â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *chartsRow = new QHBoxLayout();
  chartsRow->setSpacing(12);
  QList<QColor> palette = {QColor(33, 150, 243), QColor(0, 188, 212),
                           QColor(76, 175, 80), QColor(255, 152, 0),
                           QColor(156, 39, 176)};
  QStringList roles = parRole.keys();

  // Camembert
  {
    QFrame *pf = new QFrame();
    pf->setStyleSheet("QFrame{background:white;border-radius:14px;border:1px "
                      "solid #E3F2FD;}");
    QVBoxLayout *pfl = new QVBoxLayout(pf);
    pfl->setContentsMargins(10, 10, 10, 10);
    QLabel *pTitle = new QLabel("ðŸ¥§  RÃ©partition par rÃ´le");
    pTitle->setStyleSheet(
        "font-size:12px;font-weight:bold;color:#0288D1;padding:4px "
        "8px;background:#E3F2FD;border-radius:8px;");
    pTitle->setAlignment(Qt::AlignCenter);
    pfl->addWidget(pTitle);
    QPieSeries *pie = new QPieSeries();
    pie->setHoleSize(0.35);
    int ci = 0;
    for (const QString &r : roles) {
      int val = parRole[r];
      int pct = (total > 0) ? val * 100 / total : 0;
      QPieSlice *sl = pie->append(QString("%1\n%2%").arg(r).arg(pct), val);
      sl->setColor(palette[ci % palette.size()]);
      sl->setBorderColor(Qt::white);
      sl->setBorderWidth(3);
      sl->setLabelVisible(true);
      sl->setLabelFont(QFont("Segoe UI", 9, QFont::Bold));
      sl->setExploded(true);
      sl->setExplodeDistanceFactor(0.04f);
      ci++;
    }
    QChart *chart = new QChart();
    chart->addSeries(pie);
    chart->setBackgroundBrush(Qt::transparent);
    chart->setAnimationOptions(QChart::AllAnimations);
    chart->setAnimationDuration(600);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 9));
    chart->setMargins(QMargins(0, 0, 0, 0));
    QChartView *cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setMinimumHeight(230);
    cv->setStyleSheet("QChartView{background:transparent;border:none;}");
    pfl->addWidget(cv);
    chartsRow->addWidget(pf);
  }

  // Barres
  {
    QFrame *bf = new QFrame();
    bf->setStyleSheet("QFrame{background:white;border-radius:14px;border:1px "
                      "solid #FFF8E1;}");
    QVBoxLayout *bfl = new QVBoxLayout(bf);
    bfl->setContentsMargins(10, 10, 10, 10);
    QLabel *bTitle = new QLabel("ðŸ“Š  Nombre d'employÃ©s par rÃ´le");
    bTitle->setStyleSheet(
        "font-size:12px;font-weight:bold;color:#E65100;padding:4px "
        "8px;background:#FFF8E1;border-radius:8px;");
    bTitle->setAlignment(Qt::AlignCenter);
    bfl->addWidget(bTitle);
    QBarSeries *barSeries = new QBarSeries();
    QStringList categories;
    int ci = 0;
    for (const QString &r : roles) {
      int val = parRole[r];
      QBarSet *set = new QBarSet(r.left(12));
      set->setColor(palette[ci % palette.size()]);
      set->setBorderColor(palette[ci % palette.size()].darker(120));
      for (int j = 0; j < roles.size(); j++)
        *set << (j == ci ? val : 0);
      barSeries->append(set);
      categories << r.left(10);
      ci++;
    }
    barSeries->setLabelsVisible(true);
    barSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
    QBarCategoryAxis *axX = new QBarCategoryAxis();
    axX->append(categories);
    axX->setLabelsFont(QFont("Segoe UI", 8));
    QValueAxis *axY = new QValueAxis();
    axY->setLabelFormat("%d");
    axY->setTickCount(qMax(total, 4) + 1);
    axY->setRange(0, qMax(total, 4));
    QChart *chart = new QChart();
    chart->addSeries(barSeries);
    chart->setBackgroundBrush(Qt::transparent);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationDuration(600);
    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);
    barSeries->attachAxis(axX);
    barSeries->attachAxis(axY);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 9));
    chart->setMargins(QMargins(0, 0, 0, 0));
    QChartView *cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setMinimumHeight(230);
    cv->setStyleSheet("QChartView{background:transparent;border:none;}");
    bfl->addWidget(cv);
    chartsRow->addWidget(bf);
  }
  root->addLayout(chartsRow);

  // â”€â”€ RÃ©capitulatif badges â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *recapFrame = new QFrame();
  recapFrame->setStyleSheet(
      "QFrame{background:white;border-radius:10px;border:1px solid #E3F2FD;}");
  QHBoxLayout *recapL = new QHBoxLayout(recapFrame);
  recapL->setContentsMargins(14, 10, 14, 10);
  recapL->setSpacing(10);
  QLabel *recapTitle = new QLabel("RÃ©capitulatif :");
  recapTitle->setStyleSheet("font-size:11px;font-weight:bold;color:#0288D1;");
  recapL->addWidget(recapTitle);
  int ci = 0;
  for (const QString &r : roles) {
    QFrame *badge = new QFrame();
    QColor c = palette[ci % palette.size()];
    badge->setStyleSheet(
        QString("QFrame{background:%1;border-radius:8px;}").arg(c.name()));
    QHBoxLayout *bl = new QHBoxLayout(badge);
    bl->setContentsMargins(8, 4, 8, 4);
    QLabel *bl2 = new QLabel(QString("%1 : %2").arg(r).arg(parRole[r]));
    bl2->setStyleSheet("font-size:11px;font-weight:bold;color:white;");
    bl->addWidget(bl2);
    recapL->addWidget(badge);
    ci++;
  }
  recapL->addStretch();
  root->addWidget(recapFrame);

  // â”€â”€ Bouton Fermer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QPushButton *btnF = new QPushButton("âœ•  Fermer");
  btnF->setFixedHeight(42);
  btnF->setStyleSheet(
      "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 "
      "#0288D1,stop:1 "
      "#29B6F6);color:white;border-radius:10px;font-size:13px;font-weight:bold;"
      "border:none;}QPushButton:hover{background:#0277BD;}");
  connect(btnF, &QPushButton::clicked, dlg, &QDialog::close);
  root->addWidget(btnF);

  dlg->exec();
}

void fishingport::employe_ajouterEmploye() { /* placeholder */ }
void fishingport::employe_modifierEmploye() { /* placeholder */ }

// =========================================================================
//  ARDUINO BATEAU monitor page embedded in stackedWidget_2
// =========================================================================

void fishingport::initArduinoPage() {
  m_ard = new Arduino(this);
  m_ardModel = new QSqlQueryModel(this);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ 1. Add "Â­Æ’Ã¶Ã® Arduino" button to the existing actionButtons panel
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  QPushButton *btnMon = new QPushButton("Arduino PIR");
  btnMon->setObjectName("btnArduinoMonitor");
  btnMon->setToolTip("Ouvrir le moniteur Arduino PIR");
  btnMon->setIcon(QIcon(":/icons/hardware.svg"));
  btnMon->setProperty("role", "primary");
  connect(btnMon, &QPushButton::clicked, this,
          &fishingport::on_btnArduinoMonitor_clicked);

  // Insert into actionButtons layout
  QWidget *ab = ui->centralwidget->findChild<QWidget *>("actionButtons");
  if (ab && ab->layout())
    ab->layout()->addWidget(btnMon);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ 2. Build the monitor page widget
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  m_ardPage = new QWidget();

  QVBoxLayout *root = new QVBoxLayout(m_ardPage);
  root->setContentsMargins(20, 16, 20, 16);
  root->setSpacing(12);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ Header bar
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  QHBoxLayout *hdrRow = new QHBoxLayout();
  QPushButton *btnBack = new QPushButton("Retour");
  btnBack->setObjectName("btnArduinoBack");
  btnBack->setFixedWidth(110);
  btnBack->setProperty("role", "ghost");
  connect(btnBack, &QPushButton::clicked, this,
          &fishingport::on_btnArduinoBack_clicked);
  hdrRow->addWidget(btnBack);

  QLabel *title = new QLabel("Moniteur Arduino PIR - Port de Peche");
  title->setProperty("role", "heroTitle");
  hdrRow->addWidget(title);
  hdrRow->addStretch();

  m_ardLblState = new QLabel("Deconnecte");
  m_ardLblState->setProperty("role", "chip");
  applyStatusStyle(m_ardLblState, "warn");
  hdrRow->addWidget(m_ardLblState);
  root->addLayout(hdrRow);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ Control bar
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  QFrame *ctrlCard = new QFrame();
  ctrlCard->setProperty("card", "module");
  QHBoxLayout *ctrl = new QHBoxLayout(ctrlCard);
  ctrl->setContentsMargins(14, 10, 14, 10);
  ctrl->setSpacing(10);

  ctrl->addWidget(new QLabel("Port :"));
  m_ardLePort = new QLineEdit("COM3");
  m_ardLePort->setFixedWidth(70);
  ctrl->addWidget(m_ardLePort);

  m_ardBtnScan = new QPushButton("Detecter");
  m_ardBtnScan->setIcon(QIcon(":/icons/search.svg"));
  m_ardBtnScan->setProperty("role", "ghost");
  connect(m_ardBtnScan, &QPushButton::clicked, this,
          &fishingport::ard_onScanClicked);
  ctrl->addWidget(m_ardBtnScan);

  m_ardBtnConnect = new QPushButton("Connecter");
  m_ardBtnConnect->setIcon(QIcon(":/icons/hardware.svg"));
  m_ardBtnConnect->setProperty("role", "primary");
  connect(m_ardBtnConnect, &QPushButton::clicked, this,
          &fishingport::ard_onConnectClicked);
  ctrl->addWidget(m_ardBtnConnect);

  ctrl->addSpacing(16);
  ctrl->addWidget(new QLabel("Bateau :"));
  m_ardComboBoat = new QComboBox();
  m_ardComboBoat->setMinimumWidth(200);
  m_ardComboBoat->addItem("-- Selectionner un bateau --", -1);
  QSqlQuery bq;
  bq.prepare("SELECT NUMERO_CARTE_GRISE, nomB FROM BATEAU ORDER BY nomB");
  if (bq.exec()) {
    while (bq.next()) {
      m_ardComboBoat->addItem(QString("%1 - %2")
                                  .arg(bq.value(0).toInt())
                                  .arg(bq.value(1).toString()),
                              bq.value(0).toInt());
    }
  }
  connect(m_ardComboBoat, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &fishingport::ard_onBoatChanged);
  ctrl->addWidget(m_ardComboBoat);
  ctrl->addStretch();

  m_ardLblStatus = new QLabel("En attente...");
  m_ardLblStatus->setProperty("role", "chip");
  applyStatusStyle(m_ardLblStatus, "info");
  ctrl->addWidget(m_ardLblStatus);

  root->addWidget(ctrlCard);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ Detection hint
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  m_ardLblDetected =
      new QLabel("Scannez les ports disponibles pour detecter la carte Arduino.");
  m_ardLblDetected->setWordWrap(true);
  m_ardLblDetected->setProperty("role", "chip");
  applyStatusStyle(m_ardLblDetected, "info");
  root->addWidget(m_ardLblDetected);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ Body: two columns
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  QHBoxLayout *body = new QHBoxLayout();
  body->setSpacing(12);

  // Left column: logs
  QVBoxLayout *leftCol = new QVBoxLayout();
  leftCol->setSpacing(8);

  // Serial log
  QLabel *lblSerial = new QLabel("Journal serie (Arduino -> Qt)");
  lblSerial->setProperty("role", "cardTitle");
  leftCol->addWidget(lblSerial);

  m_ardLogSerial = new QTextEdit();
  m_ardLogSerial->setReadOnly(true);
  m_ardLogSerial->setMinimumHeight(200);
  m_ardLogSerial->setStyleSheet(
      "background:#0D1117;color:#58A6FF;font-family:Consolas,monospace;"
      "font-size:12px;border-radius:10px;border:2px solid "
      "#1F6FEB;padding:8px;");
  m_ardLogSerial->setPlaceholderText("En attente de donnees serie...");
  leftCol->addWidget(m_ardLogSerial);

  // DB events log
  QLabel *lblDb = new QLabel("Evenements base de donnees");
  lblDb->setProperty("role", "cardTitle");
  leftCol->addWidget(lblDb);

  m_ardLogDb = new QTextEdit();
  m_ardLogDb->setReadOnly(true);
  m_ardLogDb->setMinimumHeight(160);
  m_ardLogDb->setStyleSheet(
      "background:#101A14;color:#23A559;font-family:Consolas,monospace;"
      "font-size:12px;border-radius:8px;border:1px solid "
      "#245C3D;padding:8px;");
  m_ardLogDb->setPlaceholderText("Aucun evenement DB encore...");
  leftCol->addWidget(m_ardLogDb);

  QPushButton *btnClearLogs = new QPushButton("Effacer les journaux");
  btnClearLogs->setProperty("role", "ghost");
  connect(btnClearLogs, &QPushButton::clicked, this, [this]() {
    m_ardLogSerial->clear();
    m_ardLogDb->clear();
  });
  leftCol->addWidget(btnClearLogs);

  body->addLayout(leftCol, 1);

  // Right column: live DB table
  QVBoxLayout *rightCol = new QVBoxLayout();
  rightCol->setSpacing(8);

  QHBoxLayout *tblHdr = new QHBoxLayout();
  QLabel *lblTbl =
      new QLabel("Vue live - bateau / sortie / date_motion");
  lblTbl->setProperty("role", "cardTitle");
  tblHdr->addWidget(lblTbl);
  tblHdr->addStretch();
  QPushButton *btnRefresh = new QPushButton("Actualiser");
  btnRefresh->setFixedWidth(110);
  btnRefresh->setIcon(QIcon(":/icons/refresh.svg"));
  btnRefresh->setProperty("role", "ghost");
  connect(btnRefresh, &QPushButton::clicked, this,
          &fishingport::ard_refreshBoatTable);
  tblHdr->addWidget(btnRefresh);
  rightCol->addLayout(tblHdr);

  m_ardTableView = new QTableView();
  m_ardTableView->setModel(m_ardModel);
  m_ardTableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  m_ardTableView->verticalHeader()->setVisible(false);
  m_ardTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_ardTableView->setAlternatingRowColors(true);
  m_ardTableView->setShowGrid(false);
  rightCol->addWidget(m_ardTableView);

  body->addLayout(rightCol, 1);
  root->addLayout(body);

  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡ Add the new page to stackedWidget_2
  // Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡Ã”Ã¶Ã‡
  ui->stackedWidget_2->addWidget(m_ardPage);

  // Pre-load the table
  ard_refreshBoatTable();
}

// --------------------------------------------------------------------------
void fishingport::on_btnArduinoMonitor_clicked() {
  // Navigate to the last page of stackedWidget_2 (the monitor page)
  int idx = ui->stackedWidget_2->indexOf(m_ardPage);
  if (idx >= 0)
    ui->stackedWidget_2->setCurrentIndex(idx);
}

void fishingport::on_btnArduinoBack_clicked() {
  ui->stackedWidget_2->setCurrentIndex(0); // pageListe
}

// --------------------------------------------------------------------------
//  Scan ports
// --------------------------------------------------------------------------
void fishingport::ard_onScanClicked() {
  auto ports = Arduino::listAllPorts();
  if (ports.isEmpty()) {
    m_ardLblDetected->setText("Aucun port serie detecte.");
    applyStatusStyle(m_ardLblDetected, "error");
    return;
  }
  QString best;
  QStringList lines;
  for (const auto &p : std::as_const(ports)) {
    if (p.second) {
      lines << QString("  OK  %1  Arduino").arg(p.first);
      if (best.isEmpty())
        best = p.first;
    } else {
      lines << QString("  --  %1").arg(p.first);
    }
  }
  if (!best.isEmpty()) {
    m_ardLePort->setText(best);
    m_ardLblDetected->setText(
        QString("Ports :\n%1\nArduino detecte sur %2")
            .arg(lines.join("\n"), best));
    applyStatusStyle(m_ardLblDetected, "ok");
  } else {
    m_ardLblDetected->setText(QString("Ports :\n%1\nAucun Arduino reconnu.")
                                  .arg(lines.join("\n")));
    applyStatusStyle(m_ardLblDetected, "warn");
  }
}

// --------------------------------------------------------------------------
//  Connect / disconnect
// --------------------------------------------------------------------------
void fishingport::ard_onConnectClicked() {
  if (m_ard->isConnected()) {
    m_ard->close_arduino();
    m_ardLblState->setText("Deconnecte");
    applyStatusStyle(m_ardLblState, "warn");
    m_ardBtnConnect->setText("Connecter");
    m_ardLblStatus->setText("Deconnecte.");
    applyStatusStyle(m_ardLblStatus, "warn");
    ard_addLog("Arduino deconnecte", "#888888");
  } else {
    QString port = m_ardLePort->text().trimmed();
    if (port.isEmpty())
      port = "COM3";

    // Disconnect the main application's Arduino instance if it's holding the port
    if (m_arduino.estConnecte()) {
      m_arduino.close_arduino();
    }

    if (m_ard->connect_arduino(port) == 0) {
      // Disconnect Arduino's built-in RFID handler so it doesn't consume
      // the PIR serial data before ard_readSerial() gets to read it.
      disconnect(m_ard->getSerial(), &QSerialPort::readyRead, m_ard, nullptr);
      connect(m_ard->getSerial(), &QSerialPort::readyRead, this,
              &fishingport::ard_readSerial);
      m_ardLblState->setText(QString("Connecte - %1").arg(port));
      applyStatusStyle(m_ardLblState, "ok");
      m_ardBtnConnect->setText("Deconnecter");
      m_ardLblStatus->setText("En attente de signal PIR...");
      applyStatusStyle(m_ardLblStatus, "info");
      ard_addLog(QString("Connecte sur %1 @ 9600 baud").arg(port), "#23A559");
    } else {
      QMessageBox::critical(
          this, "Erreur Arduino",
          QString("Impossible d'ouvrir %1.\nVÃ©rifiez le branchement USB.")
              .arg(port));
    }
  }
}

// --------------------------------------------------------------------------
//  Boat selected
// --------------------------------------------------------------------------
void fishingport::ard_onBoatChanged(int index) {
  m_ardCurrentBoat = m_ardComboBoat->itemData(index).toInt();
  if (m_ardCurrentBoat != -1) {
    m_ardLblStatus->setText(
        QString("Bateau #%1 selectionne.").arg(m_ardCurrentBoat));
    ard_addLog(
        QString("Bateau selectionne : ID %1").arg(m_ardCurrentBoat),
        "#4EC5F1");
    ard_refreshBoatTable();
  }
}

// --------------------------------------------------------------------------
//  readyRead slot
// --------------------------------------------------------------------------
void fishingport::ard_readSerial() {
  m_ardBuffer += QString::fromUtf8(m_ard->read_from_arduino());
  while (m_ardBuffer.contains('\n')) {
    int idx = m_ardBuffer.indexOf('\n');
    QString line = m_ardBuffer.left(idx).trimmed();
    m_ardBuffer = m_ardBuffer.mid(idx + 1);
    if (line.isEmpty())
      continue;

    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    ard_addLog(QString("[%1]  Arduino Ã”Ã¥Ã† Qt :  %2").arg(ts, line));

    if (line.contains("motion", Qt::CaseInsensitive) &&
        !line.contains("no", Qt::CaseInsensitive))
      ard_handleMotion();
  }
}

// --------------------------------------------------------------------------
//  Helpers: append to log panels
// --------------------------------------------------------------------------
void fishingport::ard_addLog(const QString &msg, const QString &color) {
  if (!m_ardLogSerial)
    return;
  m_ardLogSerial->append(QString("<span style='color:%1;'>%2</span>")
                             .arg(color, msg.toHtmlEscaped()));
  m_ardLogSerial->verticalScrollBar()->setValue(
      m_ardLogSerial->verticalScrollBar()->maximum());
}

void fishingport::ard_addDbEvent(const QString &msg, const QString &color) {
  if (!m_ardLogDb)
    return;
  QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
  m_ardLogDb->append(QString("<span style='color:%1;'>[%2]  %3</span>")
                         .arg(color, ts, msg.toHtmlEscaped()));
  m_ardLogDb->verticalScrollBar()->setValue(
      m_ardLogDb->verticalScrollBar()->maximum());
}

// --------------------------------------------------------------------------
//  Refresh the live table
// --------------------------------------------------------------------------
void fishingport::ard_refreshBoatTable() {
  if (!m_ardModel)
    return;
  m_ardModel->setQuery(
      "SELECT b.NUMERO_CARTE_GRISE AS \"ID\", "
      "       b.nomB                AS \"Bateau\", "
      "       TO_CHAR(b.DATE_MOTION,'DD/MM/YYYY HH24:MI') AS \"Date Motion\", "
      "       TO_CHAR(s.DATE_DEPART,'DD/MM/YYYY')          AS \"Date "
      "Dâ”œÂ®part\", "
      "       s.ETAT                AS \"â”œÃ«tat sortie\" "
      "FROM   bateau b "
      "LEFT JOIN avoir a  ON b.NUMERO_CARTE_GRISE = a.NUMERO_CARTE_GRISE "
      "LEFT JOIN sortie s ON a.SORTIE_ID = s.SORTIE_ID "
      "ORDER BY b.NUMERO_CARTE_GRISE");
}

// --------------------------------------------------------------------------
//  Business logic
// --------------------------------------------------------------------------
void fishingport::ard_handleMotion() {
  if (m_ardCurrentBoat == -1) {
    m_ardLblStatus->setText("Ã”ÃœÃ¡ MOTION Ã”Ã‡Ã¶ aucun bateau sâ”œÂ®lectionnâ”œÂ® !");
    m_ardLblStatus->setStyleSheet(
        "font-size:12px;font-weight:700;color:#E65100;"
        "background:#FFF3E0;border-radius:6px;padding:4px 10px;");
    ard_addDbEvent("MOTION reâ”œÂºu Ã”Ã‡Ã¶ aucun bateau sâ”œÂ®lectionnâ”œÂ®", "#E65100");
    return;
  }

  // Step 1: UPDATE date_motion
  {
    QSqlQuery q;
    q.prepare("UPDATE BATEAU SET DATE_MOTION = SYSDATE "
              "WHERE NUMERO_CARTE_GRISE = :id");
    q.bindValue(":id", m_ardCurrentBoat);
    if (!q.exec()) {
      ard_addDbEvent(
          QString("Ã”Ã˜Ã® UPDATE â”œÂ®chouâ”œÂ® : %1").arg(q.lastError().text()),
          "#C62828");
      return;
    }
    ard_addDbEvent(
        QString("Ã”Â£Ã¶ date_motion mise â”œÃ¡ jour (ID %1)").arg(m_ardCurrentBoat));
  }

  // Step 2: Fetch dates
  QDate dateMotion, dateDepart;
  bool hasDepart = false;
  {
    QSqlQuery q;
    q.prepare("SELECT date_motion, date_depart FROM ("
              "  SELECT b.date_motion, s.date_depart "
              "  FROM bateau b "
              "  JOIN avoir a  ON b.numero_carte_grise = a.numero_carte_grise "
              "  JOIN sortie s ON a.sortie_id = s.sortie_id "
              "  WHERE b.numero_carte_grise = :id "
              "  ORDER BY s.sortie_id DESC"
              ") WHERE ROWNUM <= 1");
    q.bindValue(":id", m_ardCurrentBoat);
    if (!q.exec()) {
      ard_addDbEvent(
          QString("Ã”Ã˜Ã® SELECT â”œÂ®chouâ”œÂ® : %1").arg(q.lastError().text()),
          "#C62828");
      return;
    }
    if (q.next()) {
      if (!q.value(0).isNull())
        dateMotion = q.value(0).toDate();
      if (!q.value(1).isNull()) {
        dateDepart = q.value(1).toDate();
        hasDepart = true;
      }
    }
  }
  ard_addDbEvent(
      QString("date_motion=%1  |  date_depart=%2")
          .arg(dateMotion.toString("dd/MM/yyyy"),
               hasDepart ? dateDepart.toString("dd/MM/yyyy") : "NULL"));

  // Step 3: NULL guard
  if (!hasDepart) {
    m_ardLblStatus->setText("Ã”Ã¤â•£ Aucune sortie programmâ”œÂ®e.");
    ard_addDbEvent("date_depart NULL Ã”Ã¥Ã† aucune action.", "#888888");
    ard_refreshBoatTable();
    return;
  }

  // Step 4: Compare
  if (dateMotion == dateDepart) {
    m_ard->write_to_arduino("OK\n");
    m_ardLblStatus->setText("Ã”Â£Ã   OK Ã”Ã‡Ã¶ Date confirmâ”œÂ®e !");
    m_ardLblStatus->setStyleSheet(
        "font-size:12px;font-weight:700;color:#1B5E20;"
        "background:#C8E6C9;border-radius:6px;padding:4px 10px;");
    ard_addLog("Ã”Ã¥Ã‰ Qt Ã”Ã¥Ã† Arduino : OK", "#2E7D32");
    ard_addDbEvent("Ã”Â£Ã  Râ”œÂ®sultat : OK (dates identiques)", "#1B5E20");
  } else {
    m_ard->write_to_arduino("ALERT\n");
    m_ardLblStatus->setText("Â­Æ’ÃœÂ¿  ALERT Ã”Ã‡Ã¶ Date incorrecte !");
    m_ardLblStatus->setStyleSheet(
        "font-size:12px;font-weight:700;color:#C62828;"
        "background:#FFCDD2;border-radius:6px;padding:4px 10px;");
    ard_addLog("Ã”Ã¥Ã‰ Qt Ã”Ã¥Ã† Arduino : ALERT", "#C62828");
    ard_addDbEvent("Â­Æ’ÃœÂ¿ Râ”œÂ®sultat : ALERT (dates diffâ”œÂ®rentes)", "#C62828");
  }

  ard_refreshBoatTable();
}

// ============================================================
// ARDUINO QUAI â€” Slot 1 : Connexion au port COM
// ============================================================
void fishingport::on_btnConnecterArduino_clicked() {
  QString port = ui->lineEditPortArduino->text().trimmed();

  if (port.isEmpty()) {
    ui->lblConnexionArduino->setText("âš ï¸ Saisissez un port (ex: COM3)");
    ui->lblConnexionArduino->setStyleSheet("color:#E65100; font-weight:bold;");
    return;
  }

  if (m_arduino.estConnecte())
    m_arduino.deconnecter();

  if (m_arduino.connecter(port)) {
    ui->lblConnexionArduino->setText(QString("ðŸŸ¢ ConnectÃ© sur %1").arg(port));
    ui->lblConnexionArduino->setStyleSheet("color:#2E7D32; font-weight:bold;");
    ui->lblArduinoStatus->setText(
        "âœ… Arduino connectÃ©. Saisissez l'ID du quai Ã  vÃ©rifier puis cliquez "
        "sur \"VÃ©rifier quai\".");
    ui->lblArduinoStatus->setStyleSheet(
        "color:#1A237E; font-size:13px; padding:6px;");
  } else {
    ui->lblConnexionArduino->setText(QString("ðŸ”´ Ã‰chec sur %1").arg(port));
    ui->lblConnexionArduino->setStyleSheet("color:#B71C1C; font-weight:bold;");
    ui->lblArduinoStatus->setText("âŒ Impossible d'ouvrir le port " + port +
                                  ".\n    VÃ©rifiez que l'Arduino est branchÃ© "
                                  "et que le port est correct.");
    ui->lblArduinoStatus->setStyleSheet(
        "color:#B71C1C; font-size:13px; padding:6px;");
  }
}

// ============================================================
// ARDUINO QUAI â€” Slot 2 : VÃ©rifier quai + commander Arduino
// ============================================================
void fishingport::on_pushbuttonchoisir_clicked() {
  // 1. Lire l'ID du quai saisi
  bool ok;
  int idQuai = ui->lineEditchoisir->text().trimmed().toInt(&ok);

  if (!ok || idQuai <= 0) {
    ui->lblArduinoStatus->setText(
        "âš ï¸ Veuillez saisir un identifiant de quai valide (entier > 0).");
    ui->lblArduinoStatus->setStyleSheet(
        "color:#E65100; font-size:13px; padding:6px;");
    return;
  }

  // 2. VÃ©rifier que l'Arduino est connectÃ©
  if (!m_arduino.estConnecte()) {
    ui->lblArduinoStatus->setText("âŒ Arduino non connectÃ©.\n"
                                  "    â†’ Saisissez le port COM (ex: COM3) et "
                                  "cliquez sur \"Connecter\" d'abord.");
    ui->lblArduinoStatus->setStyleSheet(
        "color:#B71C1C; font-size:13px; padding:6px;");
    return;
  }

  // 3. Lire le statusq depuis Oracle via la methode SQL de Quai
  //    (aucun QSqlQuery ici â€” SQL entiÃ¨rement dans quai.cpp)
  QString statusq = m_tmpQuai.getStatusQuai(idQuai);

  // 4. Traiter le resultat et envoyer la commande a l'Arduino
  QString resultat;

  if (statusq.isEmpty()) {
    resultat =
        QString("Quai ID %1 introuvable dans la base de donnees.").arg(idQuai);
    ui->lblArduinoStatus->setText(resultat);
    ui->lblArduinoStatus->setStyleSheet(
        "color:#E65100; background:#FFF3E0; border:2px solid #FF9800;"
        "border-radius:8px; font-size:13px; padding:8px;");
    return;
  }

  if (statusq.toLower() == "libre") {
    // Quai LIBRE : LED verte + servo 90 degres
    if (m_arduino.envoyerCommande('1'))
      resultat =
          QString("Quai ID %1 : LIBRE\n"
                  "     LED verte allumee  |  Barriere OUVERTE  (servo 90)")
              .arg(idQuai);
    else
      resultat = "Quai LIBRE mais echec envoi Arduino.";

    ui->lblArduinoStatus->setText(resultat);
    ui->lblArduinoStatus->setStyleSheet(
        "color:#1B5E20; background:#E8F5E9; border:2px solid #4CAF50;"
        "border-radius:8px; font-size:13px; font-weight:bold; padding:8px;");

  } else {
    // Quai OCCUPE : LED rouge + servo 0 degre
    if (m_arduino.envoyerCommande('0'))
      resultat =
          QString("Quai ID %1 : OCCUPE\n"
                  "     LED rouge allumee  |  Barriere FERMEE   (servo 0)")
              .arg(idQuai);
    else
      resultat = "Quai OCCUPE mais echec envoi Arduino.";

    ui->lblArduinoStatus->setText(resultat);
    ui->lblArduinoStatus->setStyleSheet(
        "color:#B71C1C; background:#FFEBEE; border:2px solid #EF5350;"
        "border-radius:8px; font-size:13px; font-weight:bold; padding:8px;");
  }
}

// ==================================================================
//  ARDUINO RFID - DÃ©tection de badge
// ==================================================================
void fishingport::onBadgeDetecte(const QString &rfidCode) {
  qDebug() << "ðŸŽ¯ Badge dÃ©tectÃ©:" << rfidCode;

  bool compat = (!CODE_AUTORISE.isEmpty() && rfidCode.toUpper() == CODE_AUTORISE);

  // RÃ©cupÃ©rer l'ID depuis le champ
  QString idStr = ui->lineEdit_2->text().trimmed();
  bool ok;
  int idEmploye = idStr.toInt(&ok);

  // Pas d'ID valide â†’ afficher juste le rÃ©sultat badge sans BDD
  if (!ok || idEmploye <= 0) {
    QMessageBox::information(
        this, "Badge dÃ©tectÃ©",
        QString("%1 Code : %2\n%3\n\nSaisissez l'ID employÃ© pour mettre Ã  jour "
                "la BDD.")
            .arg(compat ? "âœ…" : "âŒ", rfidCode,
                 compat ? "ðŸ”“ AccÃ¨s autorisÃ©." : "ðŸ”’ AccÃ¨s refusÃ©."));
    return;
  }

  // ðŸ”¥ VÃ‰RIFIER QUE L'ID SAISI CORRESPOND Ã€ L'UTILISATEUR CONNECTÃ‰
  if (idEmploye != m_connectedUserId) {
    QMessageBox::warning(
        this, "AccÃ¨s refusÃ©",
        QString("Vous Ãªtes connectÃ© en tant qu'ID %1.\n"
                "Vous ne pouvez accÃ©der que avec votre propre ID.\n\n"
                "ID saisi : %2")
            .arg(m_connectedUserId)
            .arg(idEmploye));
    return;
  }

  // Badge compatible â†’ DÃ©clencher authentification SMS
  if (compat) {
    showArduinoSmsAuth(idEmploye, rfidCode);
  } else {
    showAccesRefuse(rfidCode, idEmploye);
    loadBateaux();
  }

  m_arduino.resetCode();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  ACCÃˆS REFUSÃ‰ â€” DIALOGUE REDESSINÃ‰
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::showAccesRefuse(const QString &rfidCode, int idEmploye) {
  int s = m_arduino.traiterBadge(idEmploye, rfidCode);
  if (s == -1) {
    QMessageBox::critical(this, "Erreur",
                          QString("EmployÃ© ID %1 introuvable.").arg(idEmploye));
    return;
  }

  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("AccÃ¨s refusÃ©");
  dlg->setFixedWidth(400);
  dlg->setModal(true);
  dlg->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

  dlg->setStyleSheet(
      "QDialog { background: #FFFFFF; } QLabel { background: transparent; }");

  QVBoxLayout *rootLayout = new QVBoxLayout(dlg);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  // â”€â”€ HEADER ROUGE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *header = new QWidget(dlg);
  header->setFixedHeight(76);
  header->setStyleSheet("background: #C62828;");

  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(20, 0, 20, 0);
  headerLayout->setSpacing(14);

  QLabel *iconCircle = new QLabel(header);
  iconCircle->setFixedSize(40, 40);
  iconCircle->setAlignment(Qt::AlignCenter);
  iconCircle->setText("ðŸ”’");
  iconCircle->setStyleSheet("QLabel { background: rgba(255,255,255,0.18); "
                            "border-radius: 20px; font-size: 18px; }");

  QWidget *headerTextWidget = new QWidget(header);
  headerTextWidget->setStyleSheet("background: transparent;");
  QVBoxLayout *headerTextLayout = new QVBoxLayout(headerTextWidget);
  headerTextLayout->setContentsMargins(0, 0, 0, 0);
  headerTextLayout->setSpacing(2);

  QLabel *headerTitle = new QLabel("AccÃ¨s refusÃ©", header);
  headerTitle->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: "
                             "bold; background: transparent;");

  QLabel *headerSub = new QLabel("Badge non autorisÃ© dÃ©tectÃ©", header);
  headerSub->setStyleSheet("color: rgba(255,255,255,0.75); font-size: 12px; "
                           "background: transparent;");

  headerTextLayout->addWidget(headerTitle);
  headerTextLayout->addWidget(headerSub);

  headerLayout->addWidget(iconCircle);
  headerLayout->addWidget(headerTextWidget);
  headerLayout->addStretch();
  rootLayout->addWidget(header);

  // â”€â”€ CORPS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *body = new QWidget(dlg);
  body->setStyleSheet("background: #FFFFFF;");

  QVBoxLayout *bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(24, 20, 24, 24);
  bodyLayout->setSpacing(10);

  // Helper : carte info
  auto makeInfoRow = [&](const QString &dotColor, const QString &labelText,
                         const QString &valueText) -> QWidget * {
    QWidget *row = new QWidget(body);
    row->setStyleSheet("QWidget { background: #F8F9FA; border: 1px solid "
                       "#E0E0E0; border-radius: 8px; }");
    row->setFixedHeight(52);

    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(14, 0, 14, 0);
    rl->setSpacing(10);

    QLabel *dot = new QLabel(row);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QString("QLabel { background: %1; border-radius: 4px; }")
                           .arg(dotColor));

    QWidget *textBlock = new QWidget(row);
    textBlock->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout *tl = new QVBoxLayout(textBlock);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(1);

    QLabel *lbl = new QLabel(labelText.toUpper(), row);
    lbl->setStyleSheet("font-size: 10px; color: #9E9E9E; font-weight: bold; "
                       "background: transparent;");

    QLabel *val = new QLabel(valueText, row);
    val->setStyleSheet("font-size: 13px; color: #B71C1C; font-weight: bold; "
                       "background: transparent;");

    tl->addWidget(lbl);
    tl->addWidget(val);
    rl->addWidget(dot);
    rl->addWidget(textBlock);
    rl->addStretch();
    return row;
  };

  // Carte : code badge
  bodyLayout->addWidget(makeInfoRow("#C62828", "Code badge", rfidCode));

  // Carte : statut
  bodyLayout->addWidget(
      makeInfoRow("#E53935", "Statut RFID", "STATUSB = 0 â€” AccÃ¨s refusÃ©"));

  // Carte : BDD
  QWidget *bddRow = new QWidget(body);
  bddRow->setStyleSheet("QWidget { background: #FFF8E1; border: 1px solid "
                        "#FFE082; border-radius: 8px; }");
  bddRow->setFixedHeight(40);
  QHBoxLayout *brl = new QHBoxLayout(bddRow);
  brl->setContentsMargins(14, 0, 14, 0);
  brl->setSpacing(10);
  QLabel *bddDot = new QLabel(bddRow);
  bddDot->setFixedSize(8, 8);
  bddDot->setStyleSheet("QLabel { background: #F9A825; border-radius: 4px; }");
  QLabel *bddVal = new QLabel("Base de donnÃ©es mise Ã  jour", bddRow);
  bddVal->setStyleSheet("font-size: 12px; color: #F57F17; font-weight: bold; "
                        "background: transparent;");
  brl->addWidget(bddDot);
  brl->addWidget(bddVal);
  brl->addStretch();
  bodyLayout->addWidget(bddRow);

  // â”€â”€ SÃ©parateur â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  bodyLayout->addSpacing(6);
  QFrame *sep = new QFrame(body);
  sep->setFrameShape(QFrame::HLine);
  sep->setStyleSheet("background: #EEEEEE; max-height: 1px; border: none;");
  bodyLayout->addWidget(sep);
  bodyLayout->addSpacing(6);

  // â”€â”€ Bouton OK â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QPushButton *btnOk = new QPushButton("Fermer", body);
  btnOk->setFixedHeight(42);
  btnOk->setCursor(Qt::PointingHandCursor);
  btnOk->setStyleSheet(R"(
        QPushButton {
            background: #C62828;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background: #B71C1C; }
        QPushButton:pressed { background: #9A1515; }
    )");
  bodyLayout->addWidget(btnOk);

  rootLayout->addWidget(body);

  connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);

  dlg->adjustSize();
  dlg->exec();
  dlg->deleteLater();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  ARDUINO - Visualiser STATUSB (pushButton_8)
//  Ce bouton sert UNIQUEMENT pour la saisie manuelle si port indisponible
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::on_pushButton_8_clicked() {
  qDebug() << "ðŸ”˜ Bouton 'Visualiser le status' cliquÃ©";
  QString idStr = ui->lineEdit_2->text().trimmed();
  if (idStr.isEmpty()) {
    QMessageBox::warning(this, "Champ vide",
                         "Veuillez saisir l'ID de l'employÃ©.");
    return;
  }
  bool ok;
  int idEmploye = idStr.toInt(&ok);
  if (!ok || idEmploye <= 0) {
    QMessageBox::warning(this, "ID invalide",
                         "L'ID employÃ© doit Ãªtre un entier positif.");
    return;
  }

  // Code dÃ©jÃ  reÃ§u â†’ traiter directement
  QString rfidCode = m_arduino.getDernierCode();
  if (!rfidCode.isEmpty()) {
    int s = m_arduino.traiterBadge(idEmploye, rfidCode);
    if (s == -1) {
      QMessageBox::critical(this, "Erreur", "EmployÃ© introuvable.");
      return;
    }
    bool compat = (!CODE_AUTORISE.isEmpty() && rfidCode == CODE_AUTORISE);
    QMessageBox::information(
        this, "RÃ©sultat RFID",
        QString("%1 Code badge : %2\n%3\n\n%4\n\n(BDD mise Ã  jour)")
            .arg(compat ? "âœ…" : "âŒ", rfidCode,
                 compat ? "ðŸ”“ Compatible." : "ðŸ”’ NON compatible.",
                 s == 1 ? "ðŸŸ¢ STATUSB = 1 (AccÃ¨s autorisÃ©)"
                        : "ðŸ”´ STATUSB = 0 (AccÃ¨s refusÃ©)"));
    m_arduino.resetCode();
    loadBateaux();
    return;
  }

  // Port non connectÃ© â†’ tenter reconnexion
  if (!m_arduino.estConnecte()) {
    for (int i = 1; i <= 20; i++) {
      if (m_arduino.connecter(QString("COM%1").arg(i)))
        break;
    }
  }

  // Aucun code en mÃ©moire â†’ attendre le badge
  qDebug() << "â³ En attente du badge RFID...";
  QMessageBox::information(
      this, "En attente",
      "â³ Veuillez passer votre badge sur le lecteur RFID.\n\n"
      "Le rÃ©sultat s'affichera automatiquement.");
  // Le slot onBadgeDetecte() sera appelÃ© automatiquement
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  AUTHENTIFICATION SMS ARDUINO â€” VERSION REDESSINÃ‰E
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::showArduinoSmsAuth(int employeId, const QString &rfidCode) {
  // â”€â”€ RÃ©cupÃ©rer les infos employÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QSqlQuery q;
  q.prepare("SELECT NOME, CONTACT FROM EMPLOYE WHERE ID_EMPLOYE = :id");
  q.bindValue(":id", employeId);
  if (!q.exec() || !q.next()) {
    QMessageBox::critical(this, "Erreur",
                          QString("EmployÃ© ID %1 introuvable.\nErreur: %2")
                              .arg(employeId)
                              .arg(q.lastError().text()));
    return;
  }

  QString nomEmploye = q.value(0).toString();
  QString contact = q.value(1).toString();
  QString telephone =
      contact.contains("|") ? contact.split("|").first() : contact;

  if (telephone.length() != 8) {
    QMessageBox::critical(
        this, "Erreur",
        QString("NumÃ©ro de tÃ©lÃ©phone invalide : '%1' (doit faire 8 chiffres)")
            .arg(telephone));
    return;
  }

  m_currentEmployeeId = employeId;
  m_currentRfidCode = rfidCode;

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  DIALOGUE PRINCIPAL
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Authentification sÃ©curisÃ©e");
  dlg->setFixedWidth(420);
  dlg->setModal(true);
  dlg->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

  // Style global du dialogue
  dlg->setStyleSheet(R"(
        QDialog {
            background: #FFFFFF;
        }
        QLabel {
            background: transparent;
        }
        QLineEdit {
            background: #FFFFFF;
            border: 1px solid #BDBDBD;
            border-radius: 8px;
            padding: 10px 14px;
            font-size: 22px;
            font-weight: bold;
            letter-spacing: 10px;
            color: #1A237E;
        }
        QLineEdit:focus {
            border: 1.5px solid #1565C0;
        }
        QLineEdit:disabled {
            background: #F5F5F5;
            color: #9E9E9E;
            border-color: #E0E0E0;
        }
    )");

  QVBoxLayout *rootLayout = new QVBoxLayout(dlg);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  HEADER BLEU
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  QWidget *header = new QWidget(dlg);
  header->setFixedHeight(76);
  header->setStyleSheet("background: #1565C0; border-radius: 0px;");

  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(20, 0, 20, 0);
  headerLayout->setSpacing(14);

  // IcÃ´ne cadenas dans un cercle
  QLabel *iconCircle = new QLabel(header);
  iconCircle->setFixedSize(40, 40);
  iconCircle->setAlignment(Qt::AlignCenter);
  iconCircle->setText("ðŸ”");
  iconCircle->setStyleSheet(R"(
        QLabel {
            background: rgba(255,255,255,0.18);
            border-radius: 20px;
            font-size: 18px;
        }
    )");

  QWidget *headerTextWidget = new QWidget(header);
  headerTextWidget->setStyleSheet("background: transparent;");
  QVBoxLayout *headerTextLayout = new QVBoxLayout(headerTextWidget);
  headerTextLayout->setContentsMargins(0, 0, 0, 0);
  headerTextLayout->setSpacing(2);

  QLabel *headerTitle = new QLabel("Authentification sÃ©curisÃ©e", header);
  headerTitle->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: "
                             "bold; background: transparent;");

  QLabel *headerSub = new QLabel("VÃ©rification en deux Ã©tapes", header);
  headerSub->setStyleSheet("color: rgba(255,255,255,0.75); font-size: 12px; "
                           "background: transparent;");

  headerTextLayout->addWidget(headerTitle);
  headerTextLayout->addWidget(headerSub);

  headerLayout->addWidget(iconCircle);
  headerLayout->addWidget(headerTextWidget);
  headerLayout->addStretch();

  rootLayout->addWidget(header);

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  CORPS DU DIALOGUE
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  QWidget *body = new QWidget(dlg);
  body->setStyleSheet("background: #FFFFFF;");

  QVBoxLayout *bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(24, 20, 24, 24);
  bodyLayout->setSpacing(10);

  // â”€â”€ Info : EmployÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  auto makeInfoRow = [&](const QString &dotColor, const QString &labelText,
                         const QString &valueText) -> QWidget * {
    QWidget *row = new QWidget(body);
    row->setStyleSheet(R"(
            QWidget {
                background: #F8F9FA;
                border: 1px solid #E0E0E0;
                border-radius: 8px;
            }
        )");
    row->setFixedHeight(52);

    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(14, 0, 14, 0);
    rl->setSpacing(10);

    QLabel *dot = new QLabel(row);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QString("QLabel { background: %1; border-radius: 4px; }")
                           .arg(dotColor));

    QWidget *textBlock = new QWidget(row);
    textBlock->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout *tl = new QVBoxLayout(textBlock);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(1);

    QLabel *lbl = new QLabel(labelText.toUpper(), row);
    lbl->setStyleSheet("font-size: 10px; color: #9E9E9E; font-weight: bold; "
                       "background: transparent;");

    QLabel *val = new QLabel(valueText, row);
    val->setStyleSheet("font-size: 13px; color: #1A237E; font-weight: bold; "
                       "background: transparent;");

    tl->addWidget(lbl);
    tl->addWidget(val);

    rl->addWidget(dot);
    rl->addWidget(textBlock);
    rl->addStretch();
    return row;
  };

  bodyLayout->addWidget(
      makeInfoRow("#1565C0", "EmployÃ©",
                  QString("%1 â€” ID %2").arg(nomEmploye).arg(employeId)));
  bodyLayout->addWidget(makeInfoRow("#2E7D32", "Badge RFID",
                                    QString("AutorisÃ© â€” %1").arg(rfidCode)));

  // â”€â”€ Barre de progression â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  bodyLayout->addSpacing(6);

  QProgressBar *stepBar = new QProgressBar(body);
  stepBar->setRange(0, 2);
  stepBar->setValue(1);
  stepBar->setTextVisible(false);
  stepBar->setFixedHeight(4);
  stepBar->setStyleSheet(R"(
        QProgressBar {
            background: #E0E0E0;
            border-radius: 2px;
            border: none;
        }
        QProgressBar::chunk {
            background: #1565C0;
            border-radius: 2px;
        }
    )");
  bodyLayout->addWidget(stepBar);

  QLabel *stepLabel = new QLabel("Ã‰tape 2 sur 2 â€” VÃ©rification SMS", body);
  stepLabel->setStyleSheet(
      "font-size: 11px; color: #9E9E9E; background: transparent;");
  bodyLayout->addWidget(stepLabel);

  // â”€â”€ SÃ©parateur â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  bodyLayout->addSpacing(4);
  QFrame *sep = new QFrame(body);
  sep->setFrameShape(QFrame::HLine);
  sep->setStyleSheet("background: #EEEEEE; max-height: 1px; border: none;");
  bodyLayout->addWidget(sep);
  bodyLayout->addSpacing(4);

  // â”€â”€ Label section envoi â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *sectionSend = new QLabel("Envoi du code", body);
  sectionSend->setStyleSheet("font-size: 12px; font-weight: bold; color: "
                             "#616161; background: transparent;");
  bodyLayout->addWidget(sectionSend);

  // â”€â”€ Bouton Envoyer SMS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QPushButton *btnSms = new QPushButton("  Envoyer le code par email", body);
  btnSms->setFixedHeight(44);
  btnSms->setCursor(Qt::PointingHandCursor);
  btnSms->setStyleSheet(R"(
        QPushButton {
            background: #1565C0;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 13px;
            font-weight: bold;
            padding: 0 20px;
        }
        QPushButton:hover { background: #0D47A1; }
        QPushButton:pressed { background: #0A3880; }
        QPushButton:disabled {
            background: #BDBDBD;
            color: #EEEEEE;
        }
    )");
  bodyLayout->addWidget(btnSms);

  // â”€â”€ Statut SMS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *smsStatus = new QLabel(
      "Un code Ã  6 chiffres sera envoyÃ© au numÃ©ro enregistrÃ©.", body);
  smsStatus->setAlignment(Qt::AlignCenter);
  smsStatus->setWordWrap(true);
  smsStatus->setFixedHeight(34);
  smsStatus->setStyleSheet(
      "font-size: 11px; color: #9E9E9E; background: transparent;");
  bodyLayout->addWidget(smsStatus);

  // â”€â”€ SÃ©parateur â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *sep2 = new QFrame(body);
  sep2->setFrameShape(QFrame::HLine);
  sep2->setStyleSheet("background: #EEEEEE; max-height: 1px; border: none;");
  bodyLayout->addWidget(sep2);
  bodyLayout->addSpacing(4);

  // â”€â”€ Label section code â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *sectionCode = new QLabel("Code de vÃ©rification", body);
  sectionCode->setStyleSheet("font-size: 12px; font-weight: bold; color: "
                             "#616161; background: transparent;");
  bodyLayout->addWidget(sectionCode);

  // â”€â”€ Champ code â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLineEdit *codeEdit = new QLineEdit(body);
  codeEdit->setPlaceholderText("______");
  codeEdit->setMaxLength(6);
  codeEdit->setAlignment(Qt::AlignCenter);
  codeEdit->setFixedHeight(52);
  codeEdit->setEnabled(false);
  codeEdit->setValidator(new QIntValidator(100000, 999999, dlg));
  bodyLayout->addWidget(codeEdit);

  // â”€â”€ Boutons VÃ©rifier / Annuler â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *actionRow = new QHBoxLayout();
  actionRow->setSpacing(10);

  QPushButton *btnVerifier = new QPushButton("VÃ©rifier", body);
  btnVerifier->setFixedHeight(42);
  btnVerifier->setEnabled(false);
  btnVerifier->setCursor(Qt::PointingHandCursor);
  btnVerifier->setStyleSheet(R"(
        QPushButton {
            background: #1565C0;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background: #0D47A1; }
        QPushButton:pressed { background: #0A3880; }
        QPushButton:disabled {
            background: #BDBDBD;
            color: #EEEEEE;
        }
    )");

  QPushButton *btnAnnuler = new QPushButton("Annuler", body);
  btnAnnuler->setFixedHeight(42);
  btnAnnuler->setCursor(Qt::PointingHandCursor);
  btnAnnuler->setStyleSheet(R"(
        QPushButton {
            background: #F5F5F5;
            color: #616161;
            border: 1px solid #E0E0E0;
            border-radius: 8px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background: #EEEEEE; }
        QPushButton:pressed { background: #E0E0E0; }
    )");

  actionRow->addWidget(btnVerifier);
  actionRow->addWidget(btnAnnuler);
  bodyLayout->addLayout(actionRow);

  rootLayout->addWidget(body);

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  CONNEXIONS
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

  // -- Bouton Envoyer SMS --
  connect(btnSms, &QPushButton::clicked, [=]() mutable {
    m_arduinoSmsCode = QRandomGenerator::global()->bounded(100000, 999999);

    btnSms->setEnabled(false);
    smsStatus->setStyleSheet(
        "font-size: 11px; color: #1565C0; background: transparent;");
    smsStatus->setText("ðŸ“§ Envoi du code par email...");

    // Envoyer le code par email
    envoyerCodeParEmail(qEnvironmentVariable("FISHING_PORT_ALERT_EMAIL"), m_arduinoSmsCode,
                        smsStatus);

    // Attendre que l'email soit envoyÃ©
    QTimer *checkTimer = new QTimer(this);
    int *attempts = new int(0);

    connect(checkTimer, &QTimer::timeout, [=]() mutable {
      (*attempts)++;

      // VÃ©rifier si l'email a Ã©tÃ© envoyÃ© (le label change)
      if (smsStatus->text().contains("âœ…") || *attempts > 60) {
        checkTimer->stop();

        if (smsStatus->text().contains("âœ…")) {
          // Email envoyÃ© avec succÃ¨s
          codeEdit->setEnabled(true);
          btnVerifier->setEnabled(true);
          codeEdit->setFocus();

          // Avancer la barre de progression
          stepBar->setValue(2);
          stepBar->setStyleSheet(R"(
                        QProgressBar {
                            background: #E0E0E0;
                            border-radius: 2px;
                            border: none;
                        }
                        QProgressBar::chunk {
                            background: #2E7D32;
                            border-radius: 2px;
                        }
                    )");
          stepLabel->setText("Code envoyÃ© par email â€” entrez le code reÃ§u");

        } else {
          // Erreur - Email obligatoire
          smsStatus->setStyleSheet(
              "font-size: 11px; color: #E53935; background: transparent;");
          smsStatus->setText("âŒ Erreur lors de l'envoi de l'email");
          btnSms->setEnabled(true);
        }

        delete checkTimer;
        delete attempts;
      }
    });

    checkTimer->start(500); // VÃ©rifier toutes les 500ms
  });

  // -- Bouton VÃ©rifier --
  connect(btnVerifier, &QPushButton::clicked, [=]() {
    QString saisi = codeEdit->text().trimmed();

    if (saisi.isEmpty()) {
      codeEdit->setStyleSheet(R"(
                QLineEdit {
                    border: 1.5px solid #E53935;
                    background: #FFEBEE;
                    border-radius: 8px;
                    padding: 10px 14px;
                    font-size: 22px;
                    font-weight: bold;
                    letter-spacing: 10px;
                    color: #E53935;
                }
            )");
      smsStatus->setStyleSheet(
          "font-size: 11px; color: #E53935; background: transparent;");
      smsStatus->setText("Veuillez entrer le code reÃ§u par email.");
      return;
    }

    if (saisi.toInt() != m_arduinoSmsCode) {
      codeEdit->setStyleSheet(R"(
                QLineEdit {
                    border: 1.5px solid #E53935;
                    background: #FFEBEE;
                    border-radius: 8px;
                    padding: 10px 14px;
                    font-size: 22px;
                    font-weight: bold;
                    letter-spacing: 10px;
                    color: #E53935;
                }
            )");
      smsStatus->setStyleSheet(
          "font-size: 11px; color: #E53935; background: transparent;");
      smsStatus->setText("Code incorrect. VÃ©rifiez votre SMS.");
      codeEdit->clear();
      codeEdit->setFocus();
      return;
    }

    // â”€â”€ Code correct : accÃ¨s autorisÃ© â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_smsAuthSuccess = true; // âœ… Marquer l'authentification comme rÃ©ussie
    dlg->accept();

    int s = m_arduino.traiterBadge(m_currentEmployeeId, m_currentRfidCode);
    if (s != -1) {
      // ðŸ”¥ ENREGISTRER LA PRÃ‰SENCE AUTOMATIQUEMENT
      // Utiliser la colonne PRESENCE_CONGE pour enregistrer la prÃ©sence
      QString empId = QString::number(m_currentEmployeeId);

      // Format: DATE_DEBUT|DATE_FIN|MOTIF|STATUT|DATE_DEMANDE
      // Pour la prÃ©sence: utiliser DATE_PRESENCE|DATE_PRESENCE|Badge
      // RFID|PrÃ©sent|DATE_AUJOURD'HUI
      QString presenceData =
          QString("%1|%1|Badge RFID autorisÃ©|PrÃ©sent|%2")
              .arg(QDate::currentDate().toString("yyyy-MM-dd"))
              .arg(QDate::currentDate().toString("yyyy-MM-dd"));

      // RÃ©cupÃ©rer les donnÃ©es existantes
      QSqlQuery qRead;
      qRead.prepare(
          "SELECT PRESENCE_CONGE FROM EMPLOYE WHERE ID_EMPLOYE = :id");
      qRead.bindValue(":id", empId);

      QString existingData = "";
      if (qRead.exec() && qRead.next()) {
        existingData = qRead.value(0).toString();
      }

      // Ajouter la prÃ©sence aux donnÃ©es existantes
      QString allData = existingData.isEmpty()
                            ? presenceData
                            : existingData + ";" + presenceData;

      QSqlQuery qPresence;
      qPresence.prepare(
          "UPDATE EMPLOYE SET PRESENCE_CONGE = :data WHERE ID_EMPLOYE = :id");
      qPresence.bindValue(":data", allData);
      qPresence.bindValue(":id", empId);

      if (qPresence.exec()) {
        qDebug() << "âœ… PrÃ©sence enregistrÃ©e pour l'employÃ©" << empId;
      } else {
        qDebug() << "âŒ Erreur enregistrement prÃ©sence:"
                 << qPresence.lastError().text();
      }

      // BoÃ®te de succÃ¨s stylisÃ©e
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("AccÃ¨s autorisÃ©");
      msgBox.setIcon(QMessageBox::NoIcon);
      msgBox.setText(
          QString("<b style='font-size:15px; color:#1565C0;'>Authentification "
                  "rÃ©ussie</b>"
                  "<br><br>"
                  "<table style='font-size:13px; color:#37474F;'>"
                  "<tr><td>EmployÃ© :</td><td><b>%1</b></td></tr>"
                  "<tr><td>Badge :</td><td><b>%2</b></td></tr>"
                  "<tr><td>Statut :</td><td><b style='color:#2E7D32;'>AccÃ¨s "
                  "autorisÃ© (STATUSB = 1)</b></td></tr>"
                  "<tr><td>PrÃ©sence :</td><td><b "
                  "style='color:#2E7D32;'>EnregistrÃ©e âœ“</b></td></tr>"
                  "</table>")
              .arg(nomEmploye, m_currentRfidCode));
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.exec();
      loadBateaux();
    }

    dlg->deleteLater();
  });

  // -- Connexion codeEdit : reset style Ã  la saisie --
  connect(codeEdit, &QLineEdit::textChanged, [=]() {
    codeEdit->setStyleSheet(R"(
            QLineEdit {
                background: #FFFFFF;
                border: 1px solid #BDBDBD;
                border-radius: 8px;
                padding: 10px 14px;
                font-size: 22px;
                font-weight: bold;
                letter-spacing: 10px;
                color: #1A237E;
            }
            QLineEdit:focus {
                border: 1.5px solid #1565C0;
            }
        )");
    smsStatus->setStyleSheet(
        "font-size: 11px; color: #2E7D32; background: transparent;");
    smsStatus->setText("Code envoyÃ© avec succÃ¨s.");
  });

  // -- Bouton Annuler --
  connect(btnAnnuler, &QPushButton::clicked, [=]() {
    dlg->reject();
    QMessageBox::information(this, "AnnulÃ©",
                             "Authentification annulÃ©e.\nAccÃ¨s refusÃ©.");
  });

  dlg->adjustSize();
  int result = dlg->exec();
  dlg->deleteLater();

  // Si l'utilisateur a annulÃ©, afficher accÃ¨s refusÃ©
  if (result != QDialog::Accepted) {
    showAccesRefuse(rfidCode, employeId);
  }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  ENVOYER CODE PAR EMAIL
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void fishingport::envoyerCodeParEmail(const QString &email, int code,
                                      QLabel *statusLabel) {
  const QString serviceId = qEnvironmentVariable("EMAILJS_SERVICE_ID");
  const QString templateId = qEnvironmentVariable("EMAILJS_TEMPLATE_ID");
  const QString publicKey = qEnvironmentVariable("EMAILJS_PUBLIC_KEY");
  if (serviceId.isEmpty() || templateId.isEmpty() || publicKey.isEmpty() ||
      email.trimmed().isEmpty()) {
    if (statusLabel)
      statusLabel->setText("Email configuration is missing.");
    qWarning() << "EmailJS environment configuration or recipient is missing.";
    return;
  }

  QJsonObject templateParams;
  templateParams["email"] = email; // â† {{email}} dans "To Email"
  templateParams["passcode"] =
      QString::number(code); // â† {{passcode}} dans le body
  templateParams["time"] =
      QDateTime::currentDateTime().addSecs(5 * 60).toString(
          "hh:mm"); // â† {{time}} = heure expiration

  QJsonObject body;
  body["service_id"] = serviceId;
  body["template_id"] = templateId;
  body["user_id"] = publicKey;
  body["template_params"] = templateParams;

  QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);


  QNetworkRequest request(QUrl("https://api.emailjs.com/api/v1.0/email/send"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setRawHeader("Origin", "http://localhost");

  if (statusLabel)
    statusLabel->setText("ðŸ“§ Envoi en cours...");

  QNetworkAccessManager *nam = new QNetworkAccessManager(this);
  QNetworkReply *reply = nam->post(request, postData);

  connect(reply, &QNetworkReply::finished, this, [=]() {
    QByteArray resp = reply->readAll();
    int http =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "ðŸ“¡ HTTP:" << http << "| RÃ©ponse:" << resp;
    reply->deleteLater();
    nam->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
      if (statusLabel)
        statusLabel->setText(QString("âœ… Code envoyÃ© Ã  %1").arg(email));
    } else {
      if (statusLabel)
        statusLabel->setText(
            QString("âŒ Erreur HTTP %1 : %2").arg(http).arg(QString(resp)));
    }
  });
}
void fishingport::showEmployeePersonalSpace() {
  // RÃ©cupÃ©rer les infos complÃ¨tes
  QSqlQuery q;
  q.prepare("SELECT NOME, ROLE, CONTACT FROM EMPLOYE WHERE ID_EMPLOYE = :id");
  q.bindValue(":id", m_connectedUserId);
  if (!q.exec() || !q.next())
    return;

  QString nomEmploye = q.value(0).toString();
  QString roleEmploye = q.value(1).toString();
  QString contact = q.value(2).toString();

  // Initiales
  QStringList parts = nomEmploye.split(" ");
  QString initials;
  for (const QString &p : parts)
    if (!p.isEmpty())
      initials += p[0];
  initials = initials.toUpper();

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  WIDGET PRINCIPAL
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  QWidget *spaceWidget = new QWidget(this);
  spaceWidget->setStyleSheet(
      "QWidget { background: #E3F2FD; border-radius: 12px; }");

  QVBoxLayout *mainLayout = new QVBoxLayout(spaceWidget);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(14);

  // â”€â”€ HEADER "Mon espace" â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *headerBar = new QWidget(spaceWidget);
  headerBar->setStyleSheet("background: transparent;");
  QHBoxLayout *headerBarL = new QHBoxLayout(headerBar);
  headerBarL->setContentsMargins(0, 0, 0, 0);

  QPushButton *btnMonEspace = new QPushButton("ðŸ  Mon espace", headerBar);
  btnMonEspace->setFixedHeight(36);
  btnMonEspace->setStyleSheet(R"(
        QPushButton {
            background: #00897B;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 12px;
            font-weight: bold;
            padding: 0 16px;
        }
    )");
  headerBarL->addWidget(btnMonEspace);
  headerBarL->addStretch();

  QLabel *bonjourLbl = new QLabel(
      QString("Bonjour, %1 ðŸ‘‹").arg(nomEmploye.split(" ").first()), headerBar);
  bonjourLbl->setStyleSheet("font-size: 13px; font-weight: bold; color: "
                            "#0D47A1; background: transparent;");
  headerBarL->addWidget(bonjourLbl);
  mainLayout->addWidget(headerBar);

  // â”€â”€ CARTE IDENTITÃ‰ â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *idCard = new QWidget(spaceWidget);
  idCard->setStyleSheet("QWidget { background: white; border-radius: 10px; "
                        "border: 1px solid #E0E0E0; }");
  QHBoxLayout *idL = new QHBoxLayout(idCard);
  idL->setContentsMargins(14, 12, 14, 12);
  idL->setSpacing(14);

  // Avatar
  QLabel *avatar = new QLabel(initials, idCard);
  avatar->setFixedSize(56, 56);
  avatar->setAlignment(Qt::AlignCenter);
  avatar->setStyleSheet(R"(
        QLabel {
            background: #2196F3;
            border-radius: 28px;
            color: white;
            font-size: 20px;
            font-weight: bold;
        }
    )");

  QVBoxLayout *idTextL = new QVBoxLayout();
  idTextL->setSpacing(3);

  QLabel *nameLbl = new QLabel(nomEmploye, idCard);
  nameLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #1A237E; "
                         "background: transparent;");

  QLabel *roleLbl = new QLabel(roleEmploye, idCard);
  roleLbl->setStyleSheet(
      "font-size: 11px; color: #607D8B; background: transparent;");

  // Badge mÃ©thode connexion
  QPushButton *methodBadge =
      new QPushButton("ðŸ”— ConnectÃ© par " + m_connectionMethod, idCard);
  methodBadge->setFixedHeight(22);
  methodBadge->setStyleSheet(R"(
        QPushButton {
            background: #E3F2FD;
            color: #1565C0;
            border: 1px solid #90CAF9;
            border-radius: 10px;
            font-size: 10px;
            font-weight: bold;
            padding: 0 10px;
        }
    )");

  idTextL->addWidget(nameLbl);
  idTextL->addWidget(roleLbl);
  idTextL->addWidget(methodBadge);
  idL->addWidget(avatar);
  idL->addLayout(idTextL);
  idL->addStretch();
  mainLayout->addWidget(idCard);

  // â”€â”€ KPI : Heures / CongÃ©s â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *kpiRow = new QHBoxLayout();
  kpiRow->setSpacing(10);

  auto makeKpiCard = [&](const QString &title, const QString &value,
                         const QString &sub, const QString &subColor,
                         const QString &bg) -> QWidget * {
    QWidget *card = new QWidget(spaceWidget);
    card->setStyleSheet(
        QString("QWidget { background: %1; border-radius: 10px; "
                "border: 1px solid #E0E0E0; }")
            .arg(bg));
    QVBoxLayout *cl = new QVBoxLayout(card);
    cl->setContentsMargins(12, 10, 12, 10);
    cl->setSpacing(2);

    QLabel *tl = new QLabel(title, card);
    tl->setStyleSheet(
        "font-size: 10px; color: #78909C; background: transparent;");

    QLabel *vl = new QLabel(value, card);
    vl->setStyleSheet("font-size: 22px; font-weight: bold; color: #1A237E; "
                      "background: transparent;");

    QLabel *sl = new QLabel(sub, card);
    sl->setStyleSheet(
        QString("font-size: 10px; color: %1; font-weight: bold; "
                "background: #FFFFFF; border-radius: 8px; padding: 1px 8px;")
            .arg(subColor));

    cl->addWidget(tl);
    cl->addWidget(vl);
    cl->addWidget(sl);
    return card;
  };

  kpiRow->addWidget(
      makeKpiCard("Heures ce mois", "142h", "+2h sup.", "#2E7D32", "white"));
  kpiRow->addWidget(makeKpiCard("CongÃ©s restants", "8 jours", "1 en attente",
                                "#E65100", "white"));
  mainLayout->addLayout(kpiRow);

  // â”€â”€ Prochain service / Statut accÃ¨s â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *infoRow2 = new QHBoxLayout();
  infoRow2->setSpacing(10);

  auto makeInfoCard = [&](const QString &title, const QString &val1,
                          const QString &val2, const QString &bg) -> QWidget * {
    QWidget *card = new QWidget(spaceWidget);
    card->setStyleSheet(
        QString("QWidget { background: %1; border-radius: 10px; "
                "border: 1px solid #E0E0E0; }")
            .arg(bg));
    QVBoxLayout *cl = new QVBoxLayout(card);
    cl->setContentsMargins(12, 10, 12, 10);
    cl->setSpacing(2);

    QLabel *tl = new QLabel(title, card);
    tl->setStyleSheet(
        "font-size: 10px; color: #78909C; background: transparent;");

    QLabel *vl = new QLabel(val1, card);
    vl->setStyleSheet("font-size: 15px; font-weight: bold; color: #1A237E; "
                      "background: transparent;");

    QLabel *sl = new QLabel(val2, card);
    sl->setStyleSheet(
        "font-size: 11px; color: #78909C; background: transparent;");

    cl->addWidget(tl);
    cl->addWidget(vl);
    cl->addWidget(sl);
    return card;
  };

  infoRow2->addWidget(
      makeInfoCard("Prochain service", "Demain", "06:00 â€” 14:00", "white"));

  // Statut accÃ¨s
  QWidget *statutCard = new QWidget(spaceWidget);
  statutCard->setStyleSheet("QWidget { background: white; border-radius: 10px; "
                            "border: 1px solid #E0E0E0; }");
  QVBoxLayout *scl = new QVBoxLayout(statutCard);
  scl->setContentsMargins(12, 10, 12, 10);
  scl->setSpacing(2);
  QLabel *stTitle = new QLabel("Statut accÃ¨s", statutCard);
  stTitle->setStyleSheet(
      "font-size: 10px; color: #78909C; background: transparent;");
  QLabel *stVal = new QLabel("âœ” Actif", statutCard);
  stVal->setStyleSheet("font-size: 15px; font-weight: bold; color: #2E7D32; "
                       "background: transparent;");
  QLabel *stSub = new QLabel("Badge valide", statutCard);
  stSub->setStyleSheet(
      "font-size: 11px; color: #78909C; background: transparent;");
  scl->addWidget(stTitle);
  scl->addWidget(stVal);
  scl->addWidget(stSub);

  infoRow2->addWidget(statutCard);
  mainLayout->addLayout(infoRow2);

  // â”€â”€ MES ACTIONS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QLabel *actionsTitle = new QLabel("MES ACTIONS", spaceWidget);
  actionsTitle->setStyleSheet(
      "font-size: 10px; font-weight: bold; color: #78909C; "
      "letter-spacing: 2px; background: transparent;");
  mainLayout->addWidget(actionsTitle);

  struct Action {
    QString icon, title, subtitle;
  };
  QList<Action> actions = {
      {"ðŸ“…", "Demander un congÃ©", "Soumettre une demande de congÃ©"},
      {"ðŸ“‹", "Mon planning", "Voir mes horaires et shifts"},
      {"ðŸ’°", "Ma fiche de paie", "Consulter mes bulletins de salaire"},
      {"ðŸ”‘", "Changer mon mot de passe", "Modifier mes identifiants"}};

  for (const Action &a : actions) {
    QWidget *row = new QWidget(spaceWidget);
    row->setStyleSheet(R"(
            QWidget {
                background: white;
                border-radius: 10px;
                border: 1px solid #E8E8E8;
            }
            QWidget:hover { background: #F0F8FF; }
        )");
    row->setFixedHeight(52);
    row->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(14, 0, 14, 0);
    rl->setSpacing(12);

    QLabel *iconLbl = new QLabel(a.icon, row);
    iconLbl->setFixedSize(32, 32);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(
        "font-size: 16px; background: #F0F8FF; border-radius: 8px;");

    QVBoxLayout *tl = new QVBoxLayout();
    tl->setSpacing(1);
    QLabel *tit = new QLabel(a.title, row);
    tit->setStyleSheet("font-size: 13px; font-weight: bold; color: #1A237E; "
                       "background: transparent;");
    QLabel *sub = new QLabel(a.subtitle, row);
    sub->setStyleSheet(
        "font-size: 10px; color: #90A4AE; background: transparent;");
    tl->addWidget(tit);
    tl->addWidget(sub);

    QLabel *arrow = new QLabel("â€º", row);
    arrow->setStyleSheet(
        "font-size: 18px; color: #90A4AE; background: transparent;");

    rl->addWidget(iconLbl);
    rl->addLayout(tl);
    rl->addStretch();
    rl->addWidget(arrow);
    mainLayout->addWidget(row);
  }

  mainLayout->addStretch();

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  //  PLACER DANS L'INTERFACE (remplacer groupBox_5)
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  QGroupBox *verificationBox = this->findChild<QGroupBox *>("groupBox_5");
  if (verificationBox) {
    QWidget *parent = verificationBox->parentWidget();
    if (parent) {
      QLayout *layout = parent->layout();
      if (layout) {
        int index = layout->indexOf(verificationBox);
        if (index >= 0) {
          layout->removeWidget(verificationBox);
          verificationBox->hide();
          if (QBoxLayout *boxLayout = qobject_cast<QBoxLayout *>(layout))
            boxLayout->insertWidget(index, spaceWidget);
          else
            layout->addWidget(spaceWidget);
        }
      }
    }
  }

  qDebug() << "âœ… Espace personnel crÃ©Ã© pour" << nomEmploye;
}
// ==================================================================
//  GESTION DES CONGÃ‰S
// ==================================================================
void fishingport::employe_gererConges() {
  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Gestion des CongÃ©s");
  dlg->resize(800, 600);
  dlg->setStyleSheet("QDialog{background:#F0F8FF;}QLabel{border:none;"
                     "background:transparent;}QFrame{border:none;}");

  QVBoxLayout *root = new QVBoxLayout(dlg);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(12);

  // â”€â”€ Header â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *hdr = new QFrame();
  hdr->setFixedHeight(60);
  hdr->setStyleSheet("QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                     "stop:0 #0288D1,stop:1 #29B6F6);border-radius:12px;}");
  QHBoxLayout *hL = new QHBoxLayout(hdr);
  hL->setContentsMargins(20, 0, 20, 0);
  QLabel *hIcon = new QLabel("ðŸ–");
  hIcon->setStyleSheet("font-size:22px;");
  QLabel *hTitle = new QLabel("Gestion des CongÃ©s");
  hTitle->setStyleSheet("font-size:16px;font-weight:bold;color:white;");
  hL->addWidget(hIcon);
  hL->addSpacing(8);
  hL->addWidget(hTitle);
  hL->addStretch();
  root->addWidget(hdr);

  // â”€â”€ Tabs â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QTabWidget *tabs = new QTabWidget(dlg);
  tabs->setStyleSheet(
      "QTabWidget::pane{background:white;border-radius:10px;border:2px solid "
      "#B3E5FC;}"
      "QTabBar::tab{background:#E3F2FD;color:#0D47A1;padding:8px "
      "20px;font-weight:bold;border-radius:6px 6px 0 0;margin-right:4px;}"
      "QTabBar::tab:selected{background:#0288D1;color:white;}");

  // â”€â”€ Tab 1 : Nouvelle demande â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *tabNouv = new QWidget();
  QVBoxLayout *nouvL = new QVBoxLayout(tabNouv);
  nouvL->setContentsMargins(16, 16, 16, 16);
  nouvL->setSpacing(10);

  QLabel *lblInfo = new QLabel("Soumettre une nouvelle demande de congÃ©");
  lblInfo->setStyleSheet("font-size:13px;font-weight:bold;color:#0288D1;");
  nouvL->addWidget(lblInfo);

  // Solde
  int solde = m_employe.getJoursRestants(m_connectedUserId);
  QLabel *lblSolde =
      new QLabel(QString("ðŸ’³ Solde disponible : %1 jours").arg(solde));
  lblSolde->setStyleSheet(
      "font-size:12px;font-weight:bold;color:#2E7D32;background:#E8F5E9;border-"
      "radius:8px;padding:6px 12px;");
  nouvL->addWidget(lblSolde);

  // Formulaire
  QFormLayout *form = new QFormLayout();
  form->setSpacing(10);

  auto makeLE = [](const QString &ph) {
    QLineEdit *le = new QLineEdit();
    le->setPlaceholderText(ph);
    le->setFixedHeight(36);
    le->setStyleSheet(
        "QLineEdit{background:white;border:2px solid "
        "#81D4FA;border-radius:8px;padding:6px 10px;font-size:12px;}");
    return le;
  };

  QDateEdit *dateDebut = new QDateEdit(QDate::currentDate());
  QDateEdit *dateFin = new QDateEdit(QDate::currentDate().addDays(1));
  for (QDateEdit *de : {dateDebut, dateFin}) {
    de->setCalendarPopup(true);
    de->setFixedHeight(36);
    de->setStyleSheet(
        "QDateEdit{background:white;border:2px solid "
        "#81D4FA;border-radius:8px;padding:6px 10px;font-size:12px;}");
  }

  QComboBox *comboType = new QComboBox();
  comboType->addItems({"CongÃ© annuel", "CongÃ© maladie", "CongÃ© exceptionnel",
                       "CongÃ© sans solde"});
  comboType->setFixedHeight(36);
  comboType->setStyleSheet(
      "QComboBox{background:white;border:2px solid "
      "#81D4FA;border-radius:8px;padding:6px 10px;font-size:12px;}");

  QTextEdit *motifEdit = new QTextEdit();
  motifEdit->setPlaceholderText("Motif de la demande...");
  motifEdit->setFixedHeight(70);
  motifEdit->setStyleSheet(
      "QTextEdit{background:white;border:2px solid "
      "#81D4FA;border-radius:8px;padding:6px;font-size:12px;}");

  QLabel *lblNbJours = new QLabel("Nombre de jours : 1");
  lblNbJours->setStyleSheet(
      "font-size:12px;font-weight:bold;color:#E65100;background:#FFF3E0;border-"
      "radius:6px;padding:4px 10px;");

  auto updateJours = [=]() {
    int nb = dateDebut->date().daysTo(dateFin->date()) + 1;
    lblNbJours->setText(QString("Nombre de jours : %1").arg(nb > 0 ? nb : 0));
  };
  connect(dateDebut, &QDateEdit::dateChanged, updateJours);
  connect(dateFin, &QDateEdit::dateChanged, updateJours);

  form->addRow("Date dÃ©but :", dateDebut);
  form->addRow("Date fin :", dateFin);
  form->addRow("", lblNbJours);
  form->addRow("Type :", comboType);
  form->addRow("Motif :", motifEdit);
  nouvL->addLayout(form);

  QPushButton *btnSoumettre = new QPushButton("ðŸ“¨ Soumettre la demande");
  btnSoumettre->setFixedHeight(42);
  btnSoumettre->setStyleSheet("QPushButton{background:qlineargradient(x1:0,y1:"
                              "0,x2:1,y2:0,stop:0 #0288D1,stop:1 #29B6F6);"
                              "color:white;border:none;border-radius:10px;font-"
                              "size:13px;font-weight:bold;}"
                              "QPushButton:hover{background:#0277BD;}");
  nouvL->addWidget(btnSoumettre);
  nouvL->addStretch();

  connect(btnSoumettre, &QPushButton::clicked, [=]() {
    QDate dd = dateDebut->date(), df = dateFin->date();
    if (df <= dd) {
      QMessageBox::warning(dlg, "Erreur",
                           "La date de fin doit Ãªtre aprÃ¨s la date de dÃ©but.");
      return;
    }
    QString motif = motifEdit->toPlainText().trimmed();
    if (motif.isEmpty()) {
      QMessageBox::warning(dlg, "Erreur", "Veuillez saisir un motif.");
      return;
    }

    // RÃ©cupÃ©rer MAX ID
    QSqlQuery maxQ;
    maxQ.exec("SELECT NVL(MAX(ID_DEMANDE), 0) + 1 FROM DEMANDES_CONGE");
    int nextId = maxQ.next() ? maxQ.value(0).toInt() : 1;

    // INSERT direct sans prepare
    QString sql = QString("INSERT INTO DEMANDES_CONGE "
                          "(ID_DEMANDE, ID_EMPLOYE, DATE_DEBUT, DATE_FIN, "
                          "NB_JOURS, TYPE_CONGE, MOTIF, STATUT, DATE_DEMANDE) "
                          "VALUES (%1, %2, TO_DATE('%3','YYYY-MM-DD'), "
                          "TO_DATE('%4','YYYY-MM-DD'), "
                          "%5, '%6', '%7', 'En attente', SYSDATE)")
                      .arg(nextId)
                      .arg(m_connectedUserId)
                      .arg(dd.toString("yyyy-MM-dd"))
                      .arg(df.toString("yyyy-MM-dd"))
                      .arg(dd.daysTo(df) + 1)
                      .arg(comboType->currentText())
                      .arg(motif);

    QSqlQuery query;
    if (query.exec(sql)) {
      QMessageBox::information(
          dlg, "SuccÃ¨s",
          "Demande de congÃ© soumise !\nElle est en attente de validation.");
      motifEdit->clear();
    } else {
      QMessageBox::critical(dlg, "Erreur SQL", query.lastError().text());
    }
  });
  tabs->addTab(tabNouv, "ðŸ“¨ Nouvelle demande");

  // â”€â”€ Tab 2 : Mes demandes â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QWidget *tabMes = new QWidget();
  QVBoxLayout *mesL = new QVBoxLayout(tabMes);
  mesL->setContentsMargins(16, 16, 16, 16);
  mesL->setSpacing(10);

  QTableView *tableConges = new QTableView();
  tableConges->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  tableConges->verticalHeader()->setVisible(false);
  tableConges->setSelectionBehavior(QAbstractItemView::SelectRows);
  tableConges->setAlternatingRowColors(true);
  tableConges->setShowGrid(false);
  tableConges->setStyleSheet(
      "QTableView{background:white;border-radius:10px;border:2px solid "
      "#B3E5FC;}"
      "QTableView::item{padding:8px;color:#0D47A1;}"
      "QTableView::item:selected{background:#4FC3F7;color:white;}"
      "QHeaderView::section{background:#0288D1;color:white;font-weight:bold;"
      "padding:8px;border:none;}");

  QSqlQueryModel *modelConges = m_employe.getDemandesConge(m_connectedUserId);
  tableConges->setModel(modelConges);
  mesL->addWidget(tableConges);

  QPushButton *btnRefreshConges = new QPushButton("ðŸ”„ Actualiser");
  btnRefreshConges->setFixedHeight(36);
  btnRefreshConges->setStyleSheet(
      "QPushButton{background:#E3F2FD;color:#0D47A1;border:2px solid "
      "#81D4FA;border-radius:8px;font-weight:bold;}QPushButton:hover{"
      "background:#81D4FA;}");
  connect(btnRefreshConges, &QPushButton::clicked, [=]() {
    tableConges->setModel(m_employe.getDemandesConge(m_connectedUserId));
  });
  mesL->addWidget(btnRefreshConges);

  tabs->addTab(tabMes, "ðŸ“‹ Mes demandes");

  // â”€â”€ Tab 3 : Validation RH (visible seulement pour Admin/RH) â”€â”€
  if (m_connectedUserRole == "Administration" ||
      m_connectedUserRole == "Responsable RH") {
    QWidget *tabAdmin = new QWidget();
    QVBoxLayout *adminL = new QVBoxLayout(tabAdmin);
    adminL->setContentsMargins(16, 16, 16, 16);
    adminL->setSpacing(10);

    QLabel *lblAdmin = new QLabel("Demandes en attente de validation");
    lblAdmin->setStyleSheet("font-size:13px;font-weight:bold;color:#E65100;");
    adminL->addWidget(lblAdmin);

    QTableView *tableAttente = new QTableView();
    tableAttente->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    tableAttente->verticalHeader()->setVisible(false);
    tableAttente->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableAttente->setAlternatingRowColors(true);
    tableAttente->setShowGrid(false);
    tableAttente->setStyleSheet(tableConges->styleSheet());

    QSqlQueryModel *modelAttente = m_employe.getDemandesEnAttente();
    tableAttente->setModel(modelAttente);
    adminL->addWidget(tableAttente);

    QHBoxLayout *actRow = new QHBoxLayout();
    QLineEdit *leCommentaire = new QLineEdit();
    leCommentaire->setPlaceholderText("Commentaire RH (optionnel)...");
    leCommentaire->setFixedHeight(36);
    leCommentaire->setStyleSheet(
        "QLineEdit{background:white;border:2px solid "
        "#81D4FA;border-radius:8px;padding:6px 10px;font-size:12px;}");

    QPushButton *btnApprouver = new QPushButton("âœ… Approuver");
    QPushButton *btnRefuser = new QPushButton("âŒ Refuser");
    for (QPushButton *btn : {btnApprouver, btnRefuser}) {
      btn->setFixedHeight(36);
    }
    btnApprouver->setStyleSheet(
        "QPushButton{background:#4CAF50;color:white;border:none;border-radius:"
        "8px;font-weight:bold;padding:0 "
        "16px;}QPushButton:hover{background:#388E3C;}");
    btnRefuser->setStyleSheet(
        "QPushButton{background:#F44336;color:white;border:none;border-radius:"
        "8px;font-weight:bold;padding:0 "
        "16px;}QPushButton:hover{background:#C62828;}");

    actRow->addWidget(leCommentaire);
    actRow->addWidget(btnApprouver);
    actRow->addWidget(btnRefuser);
    adminL->addLayout(actRow);

    auto traiterDemande = [=](bool approuve) {
      QModelIndexList sel = tableAttente->selectionModel()->selectedRows();
      if (sel.isEmpty()) {
        QMessageBox::warning(dlg, "SÃ©lection",
                             "Veuillez sÃ©lectionner une demande.");
        return;
      }
      int idDemande =
          modelAttente->data(modelAttente->index(sel.first().row(), 0)).toInt();
      QString statut = approuve ? "ApprouvÃ©" : "RefusÃ©";
      QString commentaire = leCommentaire->text().trimmed();

      // UPDATE direct Oracle
      QString sql = QString("UPDATE DEMANDES_CONGE SET STATUT = '%1', "
                            "DATE_TRAITEMENT = SYSDATE, "
                            "COMMENTAIRE_RH = '%2' "
                            "WHERE ID_DEMANDE = %3")
                        .arg(statut)
                        .arg(commentaire)
                        .arg(idDemande);

      QSqlQuery query;
      if (query.exec(sql)) {
        QMessageBox::information(dlg, "SuccÃ¨s",
                                 approuve ? "âœ… Demande approuvÃ©e."
                                          : "âŒ Demande refusÃ©e.");
        // RafraÃ®chir les deux tables
        tableAttente->setModel(m_employe.getDemandesEnAttente());
        leCommentaire->clear();
      } else {
        QMessageBox::critical(dlg, "Erreur SQL", query.lastError().text());
      }
    };

    connect(btnApprouver, &QPushButton::clicked,
            [=]() { traiterDemande(true); });
    connect(btnRefuser, &QPushButton::clicked,
            [=]() { traiterDemande(false); });

    tabs->addTab(tabAdmin, "âš™ï¸ Validation RH");
  }

  root->addWidget(tabs);

  QPushButton *btnFermer = new QPushButton("âœ• Fermer");
  btnFermer->setFixedHeight(40);
  btnFermer->setStyleSheet(
      "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 "
      "#0288D1,stop:1 "
      "#29B6F6);color:white;border-radius:10px;font-size:13px;font-weight:bold;"
      "border:none;}QPushButton:hover{background:#0277BD;}");
  connect(btnFermer, &QPushButton::clicked, dlg, &QDialog::close);
  root->addWidget(btnFermer);

  dlg->exec();
  dlg->deleteLater();
}

// ==================================================================
//  GESTION DES PRÃ‰SENCES
// ==================================================================
void fishingport::employe_gererPresence() {
  QDialog *dlg = new QDialog(this);
  dlg->setWindowTitle("Gestion des PrÃ©sences");
  dlg->resize(850, 620);
  dlg->setStyleSheet("QDialog{background:#F0F8FF;}QLabel{border:none;"
                     "background:transparent;}QFrame{border:none;}");

  QVBoxLayout *root = new QVBoxLayout(dlg);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(12);

  // â”€â”€ Header â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QFrame *hdr = new QFrame();
  hdr->setFixedHeight(65);
  hdr->setStyleSheet("QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                     "stop:0 #43A047,stop:1 #66BB6A);border-radius:12px;}");
  QHBoxLayout *hL = new QHBoxLayout(hdr);
  hL->setContentsMargins(20, 0, 20, 0);
  QLabel *hIcon = new QLabel("ðŸ“…");
  hIcon->setStyleSheet("font-size:24px;");
  QLabel *hTitle = new QLabel("Gestion des PrÃ©sences");
  hTitle->setStyleSheet("font-size:17px;font-weight:bold;color:white;");
  QLabel *hRole = new QLabel(
      QString("ðŸ‘¤ %1 â€” %2").arg(m_connectedUserName, m_connectedUserRole));
  hRole->setStyleSheet("font-size:11px;color:rgba(255,255,255,0.85);background:"
                       "#2E7D32;border-radius:8px;padding:4px 10px;");
  hL->addWidget(hIcon);
  hL->addSpacing(8);
  hL->addWidget(hTitle);
  hL->addStretch();
  hL->addWidget(hRole);
  root->addWidget(hdr);

  // â”€â”€ Statut aujourd'hui â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QString statutActuel =
      m_employe.getStatutPresence(m_connectedUserId, QDate::currentDate());
  QString statutText =
      statutActuel.isEmpty() ? "âšª Non dÃ©fini" : "âœ… " + statutActuel;
  QString statutColor = statutActuel.isEmpty() ? "#78909C" : "#2E7D32";
  QLabel *lblStatut =
      new QLabel(QString("ðŸ“ Votre prÃ©sence aujourd'hui : %1").arg(statutText));
  lblStatut->setStyleSheet(
      QString("font-size:13px;font-weight:bold;color:%1;"
              "background:white;border-radius:8px;padding:8px 14px;"
              "border:2px solid #A5D6A7;")
          .arg(statutColor));
  root->addWidget(lblStatut);

  // â”€â”€ Tabs selon rÃ´le â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QTabWidget *tabs = new QTabWidget(dlg);
  tabs->setStyleSheet(
      "QTabWidget::pane{background:white;border-radius:10px;border:2px solid "
      "#C8E6C9;}"
      "QTabBar::tab{background:#E8F5E9;color:#2E7D32;padding:9px 22px;"
      "font-weight:bold;border-radius:6px 6px 0 0;margin-right:4px;}"
      "QTabBar::tab:selected{background:#43A047;color:white;}");

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  // TAB 1 : MA PRÃ‰SENCE (tous les rÃ´les)
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  QWidget *tabMoi = new QWidget();
  QVBoxLayout *moiL = new QVBoxLayout(tabMoi);
  moiL->setContentsMargins(16, 16, 16, 16);
  moiL->setSpacing(12);

  // â”€â”€ Calendrier personnel â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  QHBoxLayout *navRow = new QHBoxLayout();
  QPushButton *btnPrev = new QPushButton("â—€");
  QPushButton *btnNext = new QPushButton("â–¶");
  QLabel *lblMois = new QLabel();
  for (QPushButton *b : {btnPrev, btnNext}) {
    b->setFixedSize(30, 30);
    b->setStyleSheet(
        "QPushButton{background:#E8F5E9;color:#2E7D32;border:2px solid #A5D6A7;"
        "border-radius:6px;font-weight:bold;}QPushButton:hover{background:#"
        "A5D6A7;}");
  }
  lblMois->setStyleSheet("font-size:13px;font-weight:bold;color:#1A237E;");
  lblMois->setAlignment(Qt::AlignCenter);
  navRow->addWidget(btnPrev);
  navRow->addStretch();
  navRow->addWidget(lblMois);
  navRow->addStretch();
  navRow->addWidget(btnNext);
  moiL->addLayout(navRow);

  QTableWidget *calMoi = new QTableWidget(6, 7);
  calMoi->setHorizontalHeaderLabels(
      {"Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"});
  calMoi->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  calMoi->verticalHeader()->setVisible(false);
  calMoi->setEditTriggers(QAbstractItemView::NoEditTriggers);
  calMoi->setSelectionMode(QAbstractItemView::NoSelection);
  calMoi->setStyleSheet("QTableWidget{background:white;border-radius:10px;"
                        "border:2px solid #C8E6C9;}"
                        "QTableWidget::item{padding:4px;}"
                        "QHeaderView::section{background:#43A047;color:white;"
                        "font-weight:bold;padding:6px;border:none;}");
  for (int i = 0; i < 6; i++)
    calMoi->setRowHeight(i, 42);
  moiL->addWidget(calMoi);

  // LÃ©gende
  QHBoxLayout *legRow = new QHBoxLayout();
  struct Leg {
    QString color, label;
  };
  QList<Leg> legs = {{"#C8E6C9", "PrÃ©sent"}, {"#FFCDD2", "Absent"},
                     {"#E3F2FD", "CongÃ©"},   {"#FFF9C4", "TÃ©lÃ©travail"},
                     {"#F3E5F5", "Mission"}, {"#F5F5F5", "Week-end"}};
  for (const Leg &lg : legs) {
    QFrame *dot = new QFrame();
    dot->setFixedSize(12, 12);
    dot->setStyleSheet(
        QString(
            "QFrame{background:%1;border-radius:6px;border:1px solid #BDBDBD;}")
            .arg(lg.color));
    QLabel *lbl = new QLabel(lg.label);
    lbl->setStyleSheet("font-size:10px;color:#546E7A;");
    legRow->addWidget(dot);
    legRow->addWidget(lbl);
    legRow->addSpacing(6);
  }
  legRow->addStretch();
  moiL->addLayout(legRow);

  int *moisCal = new int(QDate::currentDate().month());
  int *anneeCal = new int(QDate::currentDate().year());

  auto remplirCal = [=]() {
    lblMois->setText(
        QDate(*anneeCal, *moisCal, 1).toString("MMMM yyyy").toUpper());
    for (int r = 0; r < 6; r++)
      for (int c = 0; c < 7; c++) {
        auto *it = new QTableWidgetItem("");
        it->setTextAlignment(Qt::AlignCenter);
        calMoi->setItem(r, c, it);
      }
    QDate premier(*anneeCal, *moisCal, 1);
    int startCol = premier.dayOfWeek() - 1, nbJours = premier.daysInMonth();
    QMap<QDate, QString> presences =
        m_employe.getPresencesMois(m_connectedUserId, *moisCal, *anneeCal);
    QMap<QString, QString> colors = {{"PrÃ©sent", "#C8E6C9"},
                                     {"Absent", "#FFCDD2"},
                                     {"CongÃ©", "#E3F2FD"},
                                     {"TÃ©lÃ©travail", "#FFF9C4"},
                                     {"Mission", "#F3E5F5"}};
    for (int j = 1; j <= nbJours; j++) {
      int pos = startCol + j - 1, row = pos / 7, col = pos % 7;
      if (row < 6) {
        QDate date(*anneeCal, *moisCal, j);
        QString statut = presences.value(date, "");
        bool weekend = (col == 5 || col == 6);
        QString bg = weekend ? "#F5F5F5" : colors.value(statut, "white");
        QString txt = QString::number(j) +
                      (statut.isEmpty() ? "" : "\n" + statut.left(4) + ".");
        auto *it = new QTableWidgetItem(txt);
        it->setTextAlignment(Qt::AlignCenter);
        it->setBackground(QBrush(QColor(bg)));
        if (date == QDate::currentDate()) {
          it->setFont(QFont("Segoe UI", 9, QFont::Bold));
          it->setForeground(QBrush(QColor("#0D47A1")));
        }
        calMoi->setItem(row, col, it);
      }
    }
  };

  connect(btnPrev, &QPushButton::clicked, [=]() {
    (*moisCal)--;
    if (*moisCal < 1) {
      *moisCal = 12;
      (*anneeCal)--;
    }
    remplirCal();
  });
  connect(btnNext, &QPushButton::clicked, [=]() {
    (*moisCal)++;
    if (*moisCal > 12) {
      *moisCal = 1;
      (*anneeCal)++;
    }
    remplirCal();
  });
  remplirCal();

  tabs->addTab(tabMoi, "ðŸ“† Ma prÃ©sence");

  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  // TAB 2 : Ã‰QUIPE (RH + Admin seulement)
  // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
  if (m_connectedUserRole == "Administration" ||
      m_connectedUserRole == "Responsable RH") {

    QWidget *tabEquipe = new QWidget();
    QVBoxLayout *equipeL = new QVBoxLayout(tabEquipe);
    equipeL->setContentsMargins(16, 16, 16, 16);
    equipeL->setSpacing(10);

    // â”€â”€ KPI aujourd'hui â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QHBoxLayout *kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(10);

    // Lire les statuts de prÃ©sence depuis la colonne PRESENCE_CONGE de la table
    // EMPLOYE
    QSqlQuery kpiQ;
    kpiQ.exec("SELECT PRESENCE_CONGE, COUNT(*) FROM EMPLOYE WHERE "
              "PRESENCE_CONGE IS NOT NULL GROUP BY PRESENCE_CONGE");
    QMap<QString, int> kpiData;
    while (kpiQ.next())
      kpiData[kpiQ.value(0).toString()] = kpiQ.value(1).toInt();
    int totalEmp = m_employe.getTotalEmployes();
    int presents = kpiData.value("PrÃ©sent", 0);
    int absents = kpiData.value("Absent", 0);
    int nonDef = totalEmp - presents - absents;

    struct KPI {
      QString icon, title, value, bg, border, color;
    };
    QList<KPI> kpis = {{"ðŸ‘¥", "Total employÃ©s", QString::number(totalEmp),
                        "#E3F2FD", "#90CAF9", "#0D47A1"},
                       {"âœ…", "PrÃ©sents", QString::number(presents), "#E8F5E9",
                        "#A5D6A7", "#2E7D32"},
                       {"âŒ", "Absents", QString::number(absents), "#FFEBEE",
                        "#EF9A9A", "#C62828"},
                       {"âšª", "Non dÃ©finis", QString::number(nonDef), "#F5F5F5",
                        "#BDBDBD", "#546E7A"}};
    for (const KPI &k : kpis) {
      QFrame *card = new QFrame();
      card->setStyleSheet(
          QString(
              "QFrame{background:%1;border:2px solid %2;border-radius:10px;}")
              .arg(k.bg, k.border));
      QVBoxLayout *cl = new QVBoxLayout(card);
      cl->setContentsMargins(12, 10, 12, 10);
      cl->setAlignment(Qt::AlignCenter);
      QLabel *iL = new QLabel(k.icon);
      iL->setAlignment(Qt::AlignCenter);
      iL->setStyleSheet("font-size:20px;");
      QLabel *vL = new QLabel(k.value);
      vL->setAlignment(Qt::AlignCenter);
      vL->setStyleSheet(
          QString("font-size:22px;font-weight:bold;color:%1;").arg(k.color));
      QLabel *tL = new QLabel(k.title);
      tL->setAlignment(Qt::AlignCenter);
      tL->setStyleSheet(
          QString("font-size:10px;font-weight:bold;color:%1;").arg(k.color));
      cl->addWidget(iL);
      cl->addWidget(vL);
      cl->addWidget(tL);
      kpiRow->addWidget(card);
    }
    equipeL->addLayout(kpiRow);

    // â”€â”€ Filtre + tableau â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QHBoxLayout *filtreRow = new QHBoxLayout();
    QDateEdit *dateFiltreDebut =
        new QDateEdit(QDate::currentDate().addDays(-7));
    QDateEdit *dateFiltreFin = new QDateEdit(QDate::currentDate());
    QComboBox *comboFiltreStatut = new QComboBox();
    comboFiltreStatut->addItems(
        {"Tous", "PrÃ©sent", "Absent", "TÃ©lÃ©travail", "CongÃ©", "Mission"});
    QComboBox *comboFiltreEmp = new QComboBox();
    comboFiltreEmp->addItem("Tous les employÃ©s", 0);
    QSqlQuery empQ;
    empQ.exec("SELECT ID_EMPLOYE, NOME FROM EMPLOYE ORDER BY NOME");
    while (empQ.next())
      comboFiltreEmp->addItem(empQ.value(1).toString(), empQ.value(0).toInt());

    for (QDateEdit *de : {dateFiltreDebut, dateFiltreFin}) {
      de->setCalendarPopup(true);
      de->setFixedHeight(32);
      de->setStyleSheet(
          "QDateEdit{background:white;border:2px solid "
          "#A5D6A7;border-radius:6px;padding:4px 8px;font-size:11px;}");
    }
    for (QComboBox *cb : {comboFiltreStatut, comboFiltreEmp}) {
      cb->setFixedHeight(32);
      cb->setStyleSheet(
          "QComboBox{background:white;border:2px solid "
          "#A5D6A7;border-radius:6px;padding:4px 8px;font-size:11px;}");
    }

    QPushButton *btnFiltrer = new QPushButton("ðŸ” Filtrer");
    btnFiltrer->setFixedHeight(32);
    btnFiltrer->setStyleSheet(
        "QPushButton{background:#43A047;color:white;border:none;border-radius:"
        "6px;font-weight:bold;padding:0 "
        "12px;}QPushButton:hover{background:#2E7D32;}");

    filtreRow->addWidget(new QLabel("Du:"));
    filtreRow->addWidget(dateFiltreDebut);
    filtreRow->addWidget(new QLabel("Au:"));
    filtreRow->addWidget(dateFiltreFin);
    filtreRow->addWidget(comboFiltreStatut);
    filtreRow->addWidget(comboFiltreEmp);
    filtreRow->addWidget(btnFiltrer);
    equipeL->addLayout(filtreRow);

    QTableView *tableEquipe = new QTableView();
    tableEquipe->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableEquipe->verticalHeader()->setVisible(false);
    tableEquipe->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableEquipe->setAlternatingRowColors(true);
    tableEquipe->setShowGrid(false);
    tableEquipe->setStyleSheet(
        "QTableView{background:white;border-radius:10px;border:2px solid "
        "#C8E6C9;}"
        "QTableView::item{padding:8px;color:#0D47A1;}"
        "QTableView::item:selected{background:#A5D6A7;color:white;}"
        "QHeaderView::section{background:#43A047;color:white;font-weight:bold;"
        "padding:8px;border:none;}");
    QSqlQueryModel *modelEquipe = new QSqlQueryModel();
    tableEquipe->setModel(modelEquipe);
    equipeL->addWidget(tableEquipe);

    auto chargerTableEquipe = [=]() {
      QString sql = "SELECT e.NOME AS \"EmployÃ©\", e.ROLE AS \"RÃ´le\", "
                    "TO_CHAR(p.DATE_PRESENCE,'DD/MM/YYYY') AS \"Date\", "
                    "p.STATUT AS \"Statut\" "
                    "FROM PRESENCES p "
                    "JOIN EMPLOYE e ON p.ID_EMPLOYE = e.ID_EMPLOYE "
                    "WHERE p.DATE_PRESENCE BETWEEN TO_DATE('" +
                    dateFiltreDebut->date().toString("yyyy-MM-dd") +
                    "','YYYY-MM-DD') AND TO_DATE('" +
                    dateFiltreFin->date().toString("yyyy-MM-dd") +
                    "','YYYY-MM-DD')";

      if (comboFiltreStatut->currentText() != "Tous")
        sql += QString(" AND p.STATUT = '%1'")
                   .arg(comboFiltreStatut->currentText());
      if (comboFiltreEmp->currentData().toInt() > 0)
        sql += QString(" AND p.ID_EMPLOYE = %1")
                   .arg(comboFiltreEmp->currentData().toInt());

      sql += " ORDER BY p.DATE_PRESENCE DESC, e.NOME";
      modelEquipe->setQuery(sql);
    };

    connect(btnFiltrer, &QPushButton::clicked, chargerTableEquipe);
    chargerTableEquipe();

    tabs->addTab(tabEquipe, "ðŸ‘¥ Ã‰quipe");
  }

  root->addWidget(tabs);

  QPushButton *btnFermer = new QPushButton("âœ• Fermer");
  btnFermer->setFixedHeight(42);
  btnFermer->setStyleSheet("QPushButton{background:qlineargradient(x1:0,y1:0,"
                           "x2:1,y2:0,stop:0 #43A047,stop:1 #66BB6A);"
                           "color:white;border-radius:10px;font-size:13px;font-"
                           "weight:bold;border:none;}"
                           "QPushButton:hover{background:#2E7D32;}");
  connect(btnFermer, &QPushButton::clicked, dlg, &QDialog::close);
  root->addWidget(btnFermer);

  dlg->exec();
  delete moisCal;
  delete anneeCal;
  dlg->deleteLater();
}
