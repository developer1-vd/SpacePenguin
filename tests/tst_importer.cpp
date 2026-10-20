#include <QtTest>

#include "bookmarkmanager.h"
#include "importer.h"

using namespace spacepenguin;

class TestImporter : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void chromiumBookmarks();
    void bookmarksHtml();
    void loginsCsv();
    void importKeepsFoldersAndSkipsDuplicates();
    void removingAFolderRemovesWhatItHolds();
};

void TestImporter::init()
{
    QSettings().remove(QStringLiteral("bookmarks"));
}

void TestImporter::chromiumBookmarks()
{
    const QByteArray json = R"({"roots":{
        "bookmark_bar":{"type":"folder","name":"Bookmarks bar","children":[
            {"type":"url","name":"Qt","url":"https://www.qt.io/"},
            {"type":"folder","name":"News","children":[{"type":"url","name":"LWN","url":"https://lwn.net/"}]}]},
        "other":{"type":"folder","name":"Other bookmarks","children":[
            {"type":"url","name":"KDE","url":"https://kde.org/"}]},
        "synced":{"type":"folder","name":"Mobile bookmarks","children":[]}}})";
    const QList<Importer::ImportedBookmark> found = Importer::parseChromiumBookmarks(json);
    QCOMPARE(found.size(), 3);
    QCOMPARE(found.at(0).title, QStringLiteral("Qt"));
    QVERIFY(found.at(0).folders.isEmpty());
    QCOMPARE(found.at(1).folders, QStringList{QStringLiteral("News")});
    QCOMPARE(found.at(2).folders, QStringList{QStringLiteral("Other bookmarks")});
}

void TestImporter::bookmarksHtml()
{
    const QString html = QStringLiteral(
        "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<DL><p>\n"
        "<DT><H3 PERSONAL_TOOLBAR_FOLDER=\"true\">Bookmarks Toolbar</H3>\n<DL><p>\n"
        "<DT><A HREF=\"https://www.mozilla.org/\" ADD_DATE=\"1\">Mozilla &amp; friends</A>\n"
        "<DT><H3>Dev</H3>\n<DL><p>\n<DT><A HREF=\"https://doc.qt.io/\">Qt docs</A>\n</DL><p>\n"
        "</DL><p>\n<DT><A HREF=\"place:sort=8\">Recent</A>\n"
        "<DT><H3>Reading</H3>\n<DL><p>\n<DT><A HREF=\"https://example.org/a\">A</A>\n</DL><p>\n</DL>\n");
    const QList<Importer::ImportedBookmark> found = Importer::parseBookmarksHtml(html);
    QCOMPARE(found.size(), 3);
    QCOMPARE(found.at(0).title, QStringLiteral("Mozilla & friends"));
    QVERIFY(found.at(0).folders.isEmpty()); // the toolbar's contents stay at the top
    QCOMPARE(found.at(1).folders, QStringList{QStringLiteral("Dev")});
    QCOMPARE(found.at(2).folders, QStringList{QStringLiteral("Reading")});
}

void TestImporter::loginsCsv()
{
    const QString chrome = QStringLiteral("name,url,username,password,note\n"
                                          "example.com,https://example.com/login,alice,\"p,ss\"\"word\",\n"
                                          "bad,not a url,bob,x,\n");
    QList<Login> logins = Importer::parseLoginsCsv(chrome);
    QCOMPARE(logins.size(), 1);
    QCOMPARE(logins.first().origin, QStringLiteral("https://example.com"));
    QCOMPARE(logins.first().username, QStringLiteral("alice"));
    QCOMPARE(logins.first().password, QStringLiteral("p,ss\"word"));

    const QString firefox = QStringLiteral("\"url\",\"username\",\"password\",\"httpRealm\"\r\n"
                                           "\"https://site.test\",\"carol\",\"secret\",\"\"\r\n");
    logins = Importer::parseLoginsCsv(firefox);
    QCOMPARE(logins.size(), 1);
    QCOMPARE(logins.first().username, QStringLiteral("carol"));
}

void TestImporter::importKeepsFoldersAndSkipsDuplicates()
{
    BookmarkManager bookmarks;
    Bookmark existing;
    existing.title = QStringLiteral("Qt");
    existing.url = QUrl(QStringLiteral("https://www.qt.io/"));
    bookmarks.addBookmark(existing);

    const QList<Importer::ImportedBookmark> found = {
        {{}, QStringLiteral("Qt"), QUrl(QStringLiteral("https://www.qt.io/"))},
        {{}, QStringLiteral("First"), QUrl(QStringLiteral("https://one.test/"))},
        {{QStringLiteral("Dev")}, QStringLiteral("Second"), QUrl(QStringLiteral("https://two.test/"))},
    };
    QCOMPARE(bookmarks.importBookmarks(found, QStringLiteral("From Chrome")), 2);

    const QList<Bookmark> top = bookmarks.children(QString());
    QCOMPARE(top.size(), 2);
    QCOMPARE(top.last().title, QStringLiteral("From Chrome"));
    const QList<Bookmark> inside = bookmarks.children(top.last().id);
    QCOMPARE(inside.size(), 2);
    QCOMPARE(inside.at(0).title, QStringLiteral("First")); // in the order they were exported
    QVERIFY(BookmarkManager::isFolder(inside.at(1)));
    QCOMPARE(bookmarks.children(inside.at(1).id).first().title, QStringLiteral("Second"));
    QCOMPARE(bookmarks.folderPaths().last().second, QStringLiteral("From Chrome › Dev"));
}

void TestImporter::removingAFolderRemovesWhatItHolds()
{
    BookmarkManager bookmarks;
    const QString folder = bookmarks.createFolder(QStringLiteral("Work"));
    const QString inner = bookmarks.createFolder(QStringLiteral("Old"), folder);
    Bookmark page;
    page.url = QUrl(QStringLiteral("https://work.test/"));
    page.folderId = inner;
    bookmarks.addBookmark(page);
    QCOMPARE(bookmarks.bookmarks().size(), 3);
    QVERIFY(bookmarks.removeBookmark(folder));
    QVERIFY(bookmarks.bookmarks().isEmpty());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // A settings file of its own, not the browser's.
    QCoreApplication::setOrganizationName(QStringLiteral("SpacePenguinTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_importer"));
    QStandardPaths::setTestModeEnabled(true);
    TestImporter test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_importer.moc"
