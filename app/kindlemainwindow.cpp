#include "kindlemainwindow.h"

KindleMainWindow::KindleMainWindow(Config::Class & cfg_) :
    ui(new Ui::MainWindow),
    cfg( cfg_ ),
    dictionaryBar( this, configEvents, cfg.editDictionaryCommandLine ),
    articleMaker( dictionaries, groupInstances, cfg.preferences.displayStyle ),
    articleNetMgr( this, dictionaries, articleMaker,
                   cfg.preferences.disallowContentFromOtherSites ),
    dictNetMgr( this ),
    wordFinder( this )
{
    qDebug() << "DISPLAY STYLE" << cfg.preferences.displayStyle;
    applyQtStyleSheet( cfg.preferences.displayStyle );
    ui->setupUi(this);

    QAction * actionClose = ui->actionClose;
    actionClose->setShortcut(Qt::ALT + Qt::Key_Escape);
    actionClose->setText(tr("Close") + "\tAlt+Back");
    addAction(actionClose);

    connect( ui->translateLine, SIGNAL( textChanged( QString const & ) ),
               this, SLOT( translateInputChanged( QString const & ) ) );
    connect( ui->translateLine, SIGNAL( returnPressed() ),
             this, SLOT( translateInputFinished() ) );

    articleView = new ArticleView( this, articleNetMgr, dictionaries,
                                            groupInstances, false, cfg,
                                            dictionaryBar.toggleViewAction(),
                                            groupList );
    wordList = new QListWidget( this );

    ui->stackedWidget->addWidget(articleView);
    ui->stackedWidget->addWidget(wordList);

    connect( articleView, SIGNAL( typingEvent( QString const & ) ),
               this, SLOT( typingEvent( QString const & ) ) );


    connect( &wordFinder, SIGNAL( updated() ),
             this, SLOT( prefixMatchUpdated() ) );
    connect( &wordFinder, SIGNAL( finished() ),
             this, SLOT( prefixMatchFinished() ) );

    makeDictionaries();

    articleView->showDefinition( tr( "Welcome!" ), Instances::Group::HelpGroupId );

    ui->translateLine->setFocus();
}

void KindleMainWindow::translateInputChanged( QString const & newValue )
{

  qDebug() << "Translate Input Changed:" << newValue;

  ui->stackedWidget->setCurrentIndex(1);

  // If some word is selected in the word list, unselect it. This prevents
  // triggering a set of spurious activation signals when the list changes.

  if ( wordList->selectionModel()->hasSelection() )
    wordList->setCurrentItem( 0, QItemSelectionModel::Clear );

  QString req = newValue.trimmed();

  if ( !req.size() )
  {
    // An empty request always results in an empty result
    wordFinder.cancel();
    qDebug() << "clear wordList";
    wordList->clear();
    wordList->unsetCursor();

    // Reset the noResults mark if it's on right now
    if ( ui->translateLine->property( "noResults" ).toBool() )
    {
      ui->translateLine->setProperty( "noResults", false );
      setStyleSheet( styleSheet() );
    }
    return;
  }

  wordList->setCursor( Qt::WaitCursor );

  wordFinder.prefixMatch( req, dictionaries );
}

void KindleMainWindow::translateInputFinished()
{
  ui->stackedWidget->setCurrentIndex(0);
  ui->translateLine->selectAll();

  QString word = ui->translateLine->text();

  if ( word.size() )
  {

    showTranslationFor( word );

    getCurrentArticleView()->focus();
  }
}

void KindleMainWindow::showTranslationFor(QString const & word, unsigned group )
{
    group = Instances::Group::AllGroupId;
    ArticleView * view = getCurrentArticleView();
    view->showDefinition( word, group );
}

ArticleView * KindleMainWindow::getCurrentArticleView()
{
  return articleView;
}

void KindleMainWindow::makeDictionaries()
{
  wordFinder.clear();
  dictionariesUnmuted.clear();
  loadDictionaries( this, isVisible(), cfg, dictionaries, dictNetMgr, false );
  // updateGroupList();
}

void KindleMainWindow::on_actionClose_triggered()
{
    QWSServer::instance()->enablePainting(false);
    close();
}

void KindleMainWindow::focusTranslateLine()
{
  qDebug() << "Setting focus...";
  ui->translateLine->setFocus();
  qDebug() << "Selecting ALL!";
  ui->translateLine->selectAll();
}

void KindleMainWindow::prefixMatchUpdated()
{
  qDebug() << "prefixMatchUpdated";
  updateMatchResults( false );
}

void KindleMainWindow::prefixMatchFinished()
{
  qDebug() << "prefixMatchFinished";
  updateMatchResults( true );
}

void KindleMainWindow::updateMatchResults( bool finished )
{
  WordFinder::SearchResults const & results = wordFinder.getResults();

  wordList->setUpdatesEnabled( false );

  qDebug() << "match results:" << results.size();

  for( unsigned x = 0; x < results.size(); ++x )
  {
    QListWidgetItem * i = wordList->item( x );

    if ( !i )
    {
      i = new QListWidgetItem( results[ x ].first, wordList );

      if ( results[ x ].second )
      {
        QFont f = i->font();
        f.setItalic( true );
        i->setFont( f );
      }
      wordList->addItem( i );
    }
    else
    {
      if ( i->text() != results[ x ].first )
        i->setText( results[ x ].first );

      QFont f = i->font();
      if ( f.italic() != results[ x ].second )
      {
        f.setItalic( results[ x ].second );
        i->setFont( f );
      }
    }
    if (i->text().at(0).direction() == QChar::DirR)
        i->setTextAlignment(Qt::AlignRight);
    if (i->text().at(0).direction() == QChar::DirL)
        i->setTextAlignment(Qt::AlignLeft);
  }

  while ( wordList->count() > (int) results.size() )
  {
    // Chop off any extra items that were there
    QListWidgetItem * i = wordList->takeItem( wordList->count() - 1 );

    if ( i )
      delete i;
    else
      break;
  }

  if ( wordList->count() )
  {
    qDebug() << "scroll to item";
    wordList->scrollToItem( wordList->item( 0 ), QAbstractItemView::PositionAtTop );
    wordList->setCurrentItem( 0, QItemSelectionModel::Clear );
  }

  wordList->setUpdatesEnabled( true );

  if ( finished )
  {
    wordList->unsetCursor();

    // Visually mark the input line to mark if there's no results

    bool setMark = results.empty() && !wordFinder.wasSearchUncertain();

    if ( ui->translateLine->property( "noResults" ).toBool() != setMark )
    {
      ui->translateLine->setProperty( "noResults", setMark );
      setStyleSheet( styleSheet() );
    }
  }
}

void KindleMainWindow::typingEvent( QString const & t )
{
  qDebug() << "typing Event: " << t;
  if ( t == "\n" || t == "\r" )
  {
    if( ui->translateLine->isEnabled() )
      focusTranslateLine();
  }
  else
  {
    if( ui->translateLine->isEnabled() )
    {
      ui->translateLine->setText( t );
      ui->translateLine->setFocus();
      ui->translateLine->setCursorPosition( t.size() );
    }
  }
}

void KindleMainWindow::applyQtStyleSheet( QString const & displayStyle )
{
  QFile builtInCssFile( ":/qt-style.css" );
  builtInCssFile.open( QFile::ReadOnly );
  QByteArray css = builtInCssFile.readAll();

  if ( displayStyle.size() )
  {
    // Load an additional stylesheet
    QFile builtInCssFile( QString( ":/qt-style-st-%1.css" ).arg( displayStyle ) );
    builtInCssFile.open( QFile::ReadOnly );
    css += builtInCssFile.readAll();
  }

  // Try loading a style sheet if there's one
  QFile cssFile( Config::getUserQtCssFileName() );

  if ( cssFile.open( QFile::ReadOnly ) )
    css += cssFile.readAll();

  setStyleSheet( css );
}

KindleMainWindow::~KindleMainWindow()
{
    delete ui;
}
