#include "setupwizard.h"

#include "bookmarkmanager.h"
#include "icons.h"
#include "importer.h"
#include "preferences.h"
#include "roles.h"
#include "theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <functional>

namespace spacepenguin {

namespace {

QLabel *title(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * 1.6);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setWordWrap(true);
    return label;
}

QLabel *muted(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("muted", true);
    label->setWordWrap(true);
    return label;
}

/** A page: a title, a line under it, then its own controls, scrolling if need be. */
QWidget *page(const QString &heading, const QString &lede, QVBoxLayout **body)
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 12);
    layout->setSpacing(10);
    layout->addWidget(title(heading));
    if (!lede.isEmpty())
        layout->addWidget(muted(lede));
    layout->addSpacing(8);
    *body = layout;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

}

SetupWizard::SetupWizard(BookmarkManager *bookmarks, QWidget *parent)
    : QDialog(parent), m_bookmarks(bookmarks)
{
    setWindowTitle(tr("Set up SpacePenguin"));
    setModal(true);
    resize(640, 520);

    m_pages = new QStackedWidget;
    m_pages->addWidget(welcomePage());
    m_pages->addWidget(searchPage());
    m_pages->addWidget(lookPage());
    m_pages->addWidget(privacyPage());
    m_pages->addWidget(rolesPage());
    m_pages->addWidget(bookmarksPage());
    m_pages->addWidget(donePage());

    m_skip = new QPushButton(tr("Skip Setup"));
    m_steps = muted(QString());
    m_steps->setWordWrap(false);
    m_back = new QPushButton(tr("Back"));
    m_next = new QPushButton;
    m_next->setProperty("accent", true);
    m_next->setDefault(true);
    connect(m_skip, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_back, &QPushButton::clicked, this, [this] { showPage(m_pages->currentIndex() - 1); });
    connect(m_next, &QPushButton::clicked, this, [this] {
        if (m_pages->currentIndex() == m_pages->count() - 1)
            accept();
        else
            showPage(m_pages->currentIndex() + 1);
    });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(24, 10, 24, 18);
    buttons->addWidget(m_skip);
    buttons->addStretch(1);
    buttons->addWidget(m_steps);
    buttons->addSpacing(12);
    buttons->addWidget(m_back);
    buttons->addWidget(m_next);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_pages, 1);
    layout->addLayout(buttons);

    // Asked once: however it is closed, it does not come back.
    connect(this, &QDialog::finished, this, [] { QSettings().setValue(QStringLiteral("setup/done"), true); });
    showPage(0);
}

void SetupWizard::showPage(int index)
{
    index = qBound(0, index, m_pages->count() - 1);
    m_pages->setCurrentIndex(index);
    const bool last = index == m_pages->count() - 1;
    m_back->setVisible(index > 0 && !last);
    m_skip->setVisible(!last);
    m_next->setText(index == 0 ? tr("Get Started") : last ? tr("Start Browsing") : tr("Next"));
    // The welcome and the end are not steps.
    const int steps = m_pages->count() - 2;
    m_steps->setText(index > 0 && !last ? tr("Step %1 of %2").arg(index).arg(steps) : QString());
    m_next->setFocus();
}

QWidget *SetupWizard::welcomePage()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 36, 36, 12);
    layout->addStretch(1);
    auto *icon = new QLabel;
    icon->setPixmap(QPixmap::fromImage(Icons::appIconImage(192)).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);
    layout->addSpacing(12);
    auto *heading = title(tr("Welcome to SpacePenguin"));
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    auto *lede = muted(tr("A few questions to make it yours. It takes a minute, and every answer can be "
                          "changed later in Settings."));
    lede->setAlignment(Qt::AlignCenter);
    layout->addWidget(lede);
    layout->addStretch(2);
    return content;
}

QWidget *SetupWizard::searchPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *shell = page(tr("Search"), tr("What you type in the address bar that isn't an address is "
                                           "searched for with:"), &body);
    auto *group = new QButtonGroup(shell);
    const QString current = Preferences::instance()->searchTemplate();
    for (const SearchEngine &engine : Preferences::searchEngines()) {
        auto *button = new QRadioButton(engine.name);
        button->setChecked(engine.searchTemplate == current);
        group->addButton(button);
        body->addWidget(button);
        const QString searchTemplate = engine.searchTemplate;
        connect(button, &QRadioButton::toggled, this, [searchTemplate](bool on) {
            if (on)
                Preferences::instance()->setSearchTemplate(searchTemplate);
        });
    }
    body->addStretch(1);
    return shell;
}

QWidget *SetupWizard::lookPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *shell = page(tr("Look"), tr("Light, dark, or the same as the rest of your computer."), &body);
    auto *group = new QButtonGroup(shell);
    const Theme::Mode stored = Theme::storedMode();
    for (const Theme::Mode mode : Theme::modes()) {
        auto *button = new QRadioButton(Theme::modeLabel(mode));
        button->setChecked(mode == stored);
        group->addButton(button);
        body->addWidget(button);
        connect(button, &QRadioButton::toggled, this, [mode](bool on) {
            if (!on)
                return;
            Theme::storeMode(mode);
            Theme::apply(mode);
        });
    }
    body->addSpacing(12);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Colour:")));
    auto *colour = new QComboBox;
    for (const Theme::Palette &palette : Theme::palettes()) {
        colour->addItem(palette.name, palette.id);
        if (palette.id == Theme::storedPalette())
            colour->setCurrentIndex(colour->count() - 1);
    }
    connect(colour, &QComboBox::currentIndexChanged, this, [colour] {
        Theme::storePalette(colour->currentData().toString());
        Theme::apply(Theme::storedMode());
    });
    row->addWidget(colour);
    row->addStretch(1);
    body->addLayout(row);
    body->addStretch(1);
    return shell;
}

QWidget *SetupWizard::privacyPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *shell = page(tr("Privacy"), tr("SpacePenguin starts private. Keep what you like."), &body);
    Preferences *preferences = Preferences::instance();
    const auto option = [this, body](const QString &label, const QString &detail, bool on,
                                     std::function<void(bool)> store) {
        auto *box = new QCheckBox(label);
        box->setChecked(on);
        connect(box, &QCheckBox::toggled, this, [store](bool checked) { store(checked); });
        body->addWidget(box);
        auto *note = muted(detail);
        note->setContentsMargins(26, 0, 0, 6);
        body->addWidget(note);
    };
    option(tr("Block ads and trackers"), tr("Pages load faster, and companies see less of what you do."),
           preferences->blockingEnabled(), [preferences](bool on) { preferences->setBlockingEnabled(on); });
    option(tr("Always use secure connections"), tr("Sites load over HTTPS, and you are warned before one that has no secure version."),
           preferences->httpsOnly(), [preferences](bool on) { preferences->setHttpsOnly(on); });
    option(tr("Remove tracking from links"), tr("The parts of an address that only follow you around (utm_source and the like) are left out."),
           preferences->stripTracking(), [preferences](bool on) { preferences->setStripTracking(on); });
    option(tr("Skip sponsor segments on YouTube"), tr("With SponsorBlock's community list."),
           preferences->skipSponsors(), [preferences](bool on) { preferences->setSkipSponsors(on); });
    option(tr("Offer to save logins"), tr("Saved logins are encrypted by your computer's keyring."),
           preferences->offerToSaveLogins(), [preferences](bool on) { preferences->setOfferToSaveLogins(on); });
    body->addStretch(1);
    return shell;
}

QWidget *SetupWizard::rolesPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *shell = page(tr("What will you use it for?"),
                          tr("Each choice brings tools of its own. Pick any, or none."), &body);
    const QStringList chosen = Preferences::instance()->roles();
    QList<QCheckBox *> boxes;
    for (const Roles::Role role : Roles::all()) {
        auto *box = new QCheckBox(Roles::label(role));
        box->setChecked(chosen.contains(Roles::key(role)));
        box->setProperty("role", Roles::key(role));
        boxes << box;
        body->addWidget(box);
        auto *note = muted(Roles::description(role));
        note->setContentsMargins(26, 0, 0, 6);
        body->addWidget(note);
    }
    for (QCheckBox *box : std::as_const(boxes)) {
        connect(box, &QCheckBox::toggled, this, [boxes] {
            QStringList roles;
            for (const QCheckBox *each : boxes) {
                if (each->isChecked())
                    roles << each->property("role").toString();
            }
            Preferences::instance()->setRoles(roles);
        });
    }
    body->addStretch(1);
    return shell;
}

QWidget *SetupWizard::bookmarksPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *shell = page(tr("Bring your bookmarks"),
                          tr("From another browser on this computer, or from a bookmarks file any browser "
                             "can export. They go in a folder of their own."), &body);
    auto *result = muted(QString());
    const auto finish = [this, result](const QString &from, const QList<Importer::ImportedBookmark> &found) {
        if (!m_bookmarks)
            return;
        const int added = m_bookmarks->importBookmarks(found, tr("From %1").arg(from));
        result->setText(found.isEmpty() ? tr("No bookmarks were found in %1.").arg(from)
                        : added == 0    ? tr("All of %1's bookmarks are already here.").arg(from)
                                        : tr("Imported %n bookmark(s) from %1.", nullptr, added).arg(from));
    };
    for (const Importer::Browser &browser : Importer::installedBrowsers()) {
        auto *button = new QPushButton(tr("From %1").arg(browser.name));
        connect(button, &QPushButton::clicked, this, [browser, finish] {
            QFile file(browser.bookmarksFile);
            finish(browser.name, file.open(QIODevice::ReadOnly) ? Importer::parseChromiumBookmarks(file.readAll())
                                                                 : QList<Importer::ImportedBookmark>());
        });
        body->addWidget(button, 0, Qt::AlignLeft);
    }
    auto *fromFile = new QPushButton(tr("From a Bookmarks File (Firefox, Safari, Any Browser)…"));
    connect(fromFile, &QPushButton::clicked, this, [this, finish] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import bookmarks"), QString(),
                                                          tr("Bookmarks (*.html *.htm)"));
        QFile file(path);
        if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
            return;
        finish(tr("the file"), Importer::parseBookmarksHtml(QString::fromUtf8(file.readAll())));
    });
    body->addWidget(fromFile, 0, Qt::AlignLeft);
    body->addSpacing(6);
    body->addWidget(result);
    body->addStretch(1);
    fromFile->setEnabled(m_bookmarks != nullptr);
    return shell;
}

QWidget *SetupWizard::donePage()
{
    QVBoxLayout *body = nullptr;
    QWidget *shell = page(tr("You're all set"), tr("A few things worth knowing:"), &body);
    const QList<std::pair<QString, QString>> tips = {
        {QStringLiteral("Ctrl+T"), tr("a new tab")},
        {QStringLiteral("Ctrl+L"), tr("type an address or a search")},
        {QStringLiteral("Ctrl+Shift+N"), tr("a private window, which keeps nothing")},
        {QStringLiteral("Ctrl+D"), tr("bookmark the page")},
        {QStringLiteral("Ctrl+Alt+P"), tr("a page's video in Picture-in-Picture")},
        {QStringLiteral("F1"), tr("every shortcut")},
    };
    for (const auto &[keys, what] : tips) {
        auto *row = new QHBoxLayout;
        auto *key = new QLabel(QKeySequence(keys).toString(QKeySequence::NativeText));
        key->setMinimumWidth(130);
        QFont bold = key->font();
        bold.setWeight(QFont::DemiBold);
        key->setFont(bold);
        row->addWidget(key);
        row->addWidget(new QLabel(what), 1);
        body->addLayout(row);
    }
    body->addSpacing(10);
    body->addWidget(muted(tr("Hold the right mouse button and draw for gestures: left goes back, right goes "
                             "forward. Everything here is in Settings (Ctrl+,).")));
    body->addStretch(1);
    return shell;
}

}
