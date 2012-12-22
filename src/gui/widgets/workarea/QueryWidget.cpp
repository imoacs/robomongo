#include "QueryWidget.h"
//#include "BsonWidget.h"
#include "AppRegistry.h"
#include "mongo/client/dbclient.h"
#include "mongo/bson/bsonobj.h"
#include "domain/MongoCollection.h"
#include "domain/MongoDatabase.h"
#include "domain/MongoServer.h"
#include "domain/MongoShell.h"
#include <QApplication>
#include <QtGui>
#include "GuiRegistry.h"
#include "EventBus.h"
#include <QPlainTextEdit>
#include "EventBus.h"
#include "events/MongoEvents.h"
#include "BsonWidget.h"
#include "editors/PlainJavaScriptEditor.h"
#include "Qsci/qsciscintilla.h"
#include "Qsci/qscilexerjavascript.h"
#include "editors/JSLexer.h"
#include "OutputViewer.h"
#include "domain/App.h"
#include "WorkAreaTabWidget.h"
#include "settings/ConnectionRecord.h"

using namespace mongo;
using namespace Robomongo;

/*
** Constructs query widget
*/
QueryWidget::QueryWidget(const MongoShellPtr &shell, WorkAreaTabWidget *tabWidget, const QString &script, QWidget *parent) :
    QWidget(parent),
    _shell(shell),
    _tabWidget(tabWidget),
    _bus(AppRegistry::instance().bus())
{
    setObjectName("queryWidget");

    _bus.subscribe(this, DocumentListLoadedEvent::Type, shell.get());
    _bus.subscribe(this, ScriptExecutedEvent::Type, shell.get());

    // Query text widget
    _configureQueryText();
    _queryText->setFixedHeight(10);
    ui_queryLinesCountChanged();

    // Execute button
    QPushButton * executeButton = new QPushButton("Execute");
	executeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_ArrowRight));
    connect(executeButton, SIGNAL(clicked()), this, SLOT(ui_executeButtonClicked()));

    // Left button
    _leftButton = new QPushButton();
    _leftButton->setIcon(GuiRegistry::instance().leftIcon());
    _leftButton->setMaximumWidth(25);
    connect(_leftButton, SIGNAL(clicked()), SLOT(ui_leftButtonClicked()));

    // Right button
    _rightButton = new QPushButton();
    _rightButton->setIcon(GuiRegistry::instance().rightIcon());
    _rightButton->setMaximumWidth(25);
    connect(_rightButton, SIGNAL(clicked()), SLOT(ui_rightButtonClicked()));

    // Page size edit box
    _pageSizeEdit = new QLineEdit("100");
    _pageSizeEdit->setMaximumWidth(31);

    // Bson widget
    _bsonWidget = new BsonWidget(NULL);
    _viewer = new OutputViewer();
    _outputLabel = new QLabel();
    _outputLabel->setContentsMargins(0, 5, 0, 0);
    _outputLabel->setVisible(false);

    QVBoxLayout * layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0,4,4,4);
    layout->addWidget(_queryText, 0, Qt::AlignTop);
    layout->addWidget(_outputLabel, 0, Qt::AlignTop);
    layout->addWidget(_viewer, 1);
    setLayout(layout);

    _queryText->setText(script);
    _queryText->setCursorPosition(15, 1000);
    _queryText->setFocus();
}

/*
** Destructs QueryWidget
*/
QueryWidget::~QueryWidget()
{
    int a = 67;
    //delete _viewModel;
}

/*
** Handle queryText linesCountChanged event
*/
void QueryWidget::ui_queryLinesCountChanged()
{
    int pos = _queryText->fontInfo().pointSize();
    int pis = _queryText->fontInfo().pixelSize();
    int teh = _queryText->textHeight(0);
    int exa = _queryText->extraAscent();
    int exd = _queryText->extraDescent();

    QFontMetrics m(_queryText->font());
    int lineHeight = m.lineSpacing();

    #if defined(Q_OS_UNIX)
    // this fix required to calculate correct height in Linux.
    // not the best way, but for now it tested on Ubuntu.
    lineHeight++;
    #endif


    int numberOfLines = _queryText->lines();

    int height = numberOfLines * lineHeight + 8;

    int maxHeight = 18 * lineHeight + 8;
    if (height > maxHeight)
        height = maxHeight;

    _queryText->setFixedHeight(height);
}

/*
** Execute query
*/
void QueryWidget::ui_executeButtonClicked()
{
    QString query = _queryText->selectedText();

    if (query.isEmpty())
        query = _queryText->text();

    _shell->open(query);
}

/*
** Override event filter
*/
bool QueryWidget::eventFilter(QObject * o, QEvent * e)
{
	if (e->type() == QEvent::KeyPress)
	{
		QKeyEvent * keyEvent = (QKeyEvent *) e;

        bool ctrlShiftReturn = (keyEvent->modifiers() & Qt::ControlModifier) &&
                               (keyEvent->modifiers() & Qt::ShiftModifier) &&
                               (keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter);

        bool ctrlT = (keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->key()==Qt::Key_T);

        if (ctrlShiftReturn || ctrlT) {
            MongoServerPtr server(_shell->server());
            QString query = _queryText->selectedText();

            if (query.isEmpty())
                query = "";//_queryText->text();

            QString dbName = server->connectionRecord()->databaseName();
            if (_currentResults.count() > 0) {
                MongoShellResult lastResult = _currentResults.last();
                dbName = lastResult.databaseName;
            }

            AppRegistry::instance().app().openShell(server, query, dbName);

            return true;
        }
        else if (keyEvent->key() == Qt::Key_F6) {
            _queryText->setFocus();
            return true;
        }
        else if ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter) )
        {
            ui_executeButtonClicked();
            return true;
        }

	}

    return false;
}

void QueryWidget::toggleOrientation()
{
    _viewer->toggleOrientation();
}

void QueryWidget::activateTabContent()
{
    _queryText->setFocus();
}

/*
** Configure QsciScintilla query widget
*/
void QueryWidget::_configureQueryText()
{
    QFont textFont = font();
#if defined(Q_OS_MAC)
    textFont.setPointSize(12);
    textFont.setFamily("Monaco");
#elif defined(Q_OS_UNIX)
    textFont.setFamily("Monospace");
    textFont.setFixedPitch(true);
    //textFont.setWeight(QFont::Bold);
//    textFont.setPointSize(12);
#elif defined(Q_OS_WIN)
    textFont.setPointSize(font().pointSize() + 2);
    textFont.setFamily("Courier");
#endif

    QsciLexerJavaScript * javaScriptLexer = new JSLexer(this);
    javaScriptLexer->setFont(textFont);
//    javaScriptLexer->setPaper(QColor(255, 0, 0, 127));

    _queryText = new RoboScintilla;
    _queryText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _queryText->setFixedHeight(23);
    _queryText->setAutoIndent(true);
    _queryText->setIndentationsUseTabs(false);
    _queryText->setIndentationWidth(4);
    _queryText->setUtf8(true);
    _queryText->installEventFilter(this);
    _queryText->setMarginWidth(1, 0); // to hide left gray column
    _queryText->setBraceMatching(QsciScintilla::StrictBraceMatch);
    _queryText->setFont(textFont);
    _queryText->setPaper(QColor(255, 0, 0, 127));
    _queryText->setLexer(javaScriptLexer);
    _queryText->setCaretForegroundColor(QColor("#FFFFFF"));
    _queryText->setMatchedBraceBackgroundColor(QColor(48, 10, 36));
    _queryText->setMatchedBraceForegroundColor(QColor("#1AB0A6"));

    //_queryText->SendScintilla(QsciScintilla::SCI_SETFONTQUALITY, QsciScintilla::SC_EFF_QUALITY_LCD_OPTIMIZED);
    //_queryText->SendScintilla (QsciScintillaBase::SCI_SETKEYWORDS, "db");

    _queryText->setStyleSheet("QFrame {background-color: rgb(48, 10, 36); border: 1px solid #c7c5c4; border-radius: 4px; margin: 0px; padding: 0px;}");
    connect(_queryText, SIGNAL(linesChanged()), SLOT(ui_queryLinesCountChanged()));
}

/*
** Documents refreshed
*/
void QueryWidget::vm_documentsRefreshed(const QList<MongoDocumentPtr> &documents)
{
    //_bsonWidget->setDocuments(documents);
}

/*
** Shell output refreshed
*/
void QueryWidget::vm_shellOutputRefreshed(const QString & shellOutput)
{
    //_bsonWidget->setShellOutput(shellOutput);
}

void QueryWidget::_showPaging(bool show)
{
    _leftButton->setVisible(show);
    _rightButton->setVisible(show);
}

/*
** Paging visability changed
*/
void QueryWidget::vm_pagingVisibilityChanged(bool show)
{
    _showPaging(show);
}

/*
** Query updated
*/
void QueryWidget::vm_queryUpdated(const QString & query)
{
    // _queryText->setText(query);
}

void QueryWidget::handle(DocumentListLoadedEvent *event)
{
    _queryText->setText(event->query);
    QList<MongoShellResult> list;
    list << MongoShellResult("", event->list, "");
    displayData(list);
}

void QueryWidget::handle(ScriptExecutedEvent *event)
{
    _currentResults = event->results;

    setUpdatesEnabled(false);
    int thisTab = _tabWidget->indexOf(this);

    if (thisTab != -1) {
        _tabWidget->setTabToolTip(thisTab, _shell->query());
        QString tabTitle = _shell->query()
                .left(41)
                .replace(QRegExp("[\n\r\t]"), " ");

        tabTitle = tabTitle.isEmpty() ? "New Shell" : tabTitle;

        _tabWidget->setTabText(thisTab, tabTitle);
    }

    if (_queryText->text().isEmpty())
        _queryText->setText(_shell->query());
    displayData(event->results);
    _queryText->setFocus();
    setUpdatesEnabled(true);
}

void QueryWidget::displayData(const QList<MongoShellResult> &results)
{
    if (results.count() == 0 && !_queryText->text().isEmpty()) {
        _outputLabel->setText("Script executed successfully, but there is no results to show.");
        _outputLabel->setVisible(true);
    } else {
        _outputLabel->setVisible(false);
    }

    _viewer->doSomething(results);
}

/*
** Paging right clicked
*/
void QueryWidget::ui_rightButtonClicked()
{
    int pageSize = _pageSizeEdit->text().toInt();
    if (pageSize == 0)
    {
        QMessageBox::information(NULL, "Page size incorrect", "Please specify correct page size");
        return;
    }

    //QString query = _queryText->text();

    //_viewModel->loadNextPage(query, pageSize);
}

/*
** Paging left clicked
*/
void QueryWidget::ui_leftButtonClicked()
{
    int pageSize = _pageSizeEdit->text().toInt();
    if (pageSize <= 0)
    {
        QMessageBox::information(NULL, "Page size incorrect", "Please specify correct page size");
        return;
    }

    //QString query = _queryText->text();
    //_viewModel->loadPreviousPage(query, pageSize);
}
