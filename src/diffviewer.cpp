#include "diffviewer.h"
#include <QtWidgets>
#include "visuals.h"
namespace {
class Gutter : public QWidget {
    DiffEditor *editor;
public:
    Gutter(DiffEditor *owner):QWidget(owner),editor(owner){}
    void paintEvent(QPaintEvent *event) override { editor->paintGutter(event); }
};
}
DiffEditor::DiffEditor(QWidget *parent):QPlainTextEdit(parent),gutter(new Gutter(this)) {
    setReadOnly(true);setLineWrapMode(NoWrap);setFrameShape(QFrame::NoFrame);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setViewportMargins(gutterWidth,0,0,0);
    document()->setDocumentMargin(8);
    connect(this,&QPlainTextEdit::updateRequest,this,[this](const QRect &,int){gutter->update();});
}
void DiffEditor::resizeEvent(QResizeEvent *event){
    QPlainTextEdit::resizeEvent(event);
    gutter->setGeometry(contentsRect().left(),viewport()->y(),gutterWidth,viewport()->height());
}
void DiffEditor::setLines(const QString &text,const QStringList &labels,const QList<int> &kinds){
    numbers=labels;
    const auto old=toPlainText();
    if(old!=text)setPlainText(text);
    QList<QTextEdit::ExtraSelection> selections;
    auto block=document()->firstBlock();int i=0;
    while(block.isValid()&&i<kinds.size()){
        if(kinds[i]){
            QTextEdit::ExtraSelection selection;selection.cursor=QTextCursor(block);
            const bool dark=palette().color(QPalette::Base).lightness()<128;
            auto added=QColor(property("addedColor").toString()),removed=QColor(property("removedColor").toString());
            if(!added.isValid())added=QColor(dark?"#66c58a":"#1a7f37");
            if(!removed.isValid())removed=QColor(dark?"#f08080":"#cf222e");
            auto color=kinds[i]==1?added:kinds[i]==-1?removed:palette().color(QPalette::Highlight);
            color.setAlpha(dark?35:24);
            selection.format.setBackground(color);selection.format.setProperty(QTextFormat::FullWidthSelection,true);
            selections.append(selection);
        }
        block=block.next();++i;
    }
    setExtraSelections(selections);gutter->update();
}
void DiffEditor::paintGutter(QPaintEvent *event){
    QPainter painter(gutter);painter.fillRect(event->rect(),palette().color(QPalette::Window));
    painter.setFont(font());painter.setPen(palette().color(QPalette::PlaceholderText));
    auto block=firstVisibleBlock();int index=block.blockNumber();
    qreal top=blockBoundingGeometry(block).translated(contentOffset()).top();
    while(block.isValid()&&top<event->rect().bottom()){
        const auto height=blockBoundingRect(block).height();
        if(block.isVisible()&&top+height>=event->rect().top())
            painter.drawText(QRectF(4,top,gutterWidth-14,height),Qt::AlignRight|Qt::AlignVCenter,numbers.value(index));
        top+=height;block=block.next();++index;
    }
}
DiffViewer::DiffViewer(QWidget *parent):QWidget(parent){
    setObjectName("diffViewer");auto *root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(0);
    auto *toolbar=new QWidget;toolbar->setObjectName("diffTools");auto *row=new QHBoxLayout(toolbar);row->setContentsMargins(12,8,12,8);row->setSpacing(8);
    statistics=new QLabel;statistics->setObjectName("diffStatistics");statistics->setWordWrap(true);statistics->setMargin(10);root->addWidget(statistics);
    mode=new QComboBox;mode->setObjectName("diffMode");mode->addItems({"Vue unifiée","Côte à côte"});mode->setAccessibleName("Présentation du diff");row->addWidget(mode,1);
    for(int direction:{-1,1}){
        auto *button=new QPushButton(direction<0?"Précédent":"Suivant");button->setToolTip("Bloc modifié "+QString(direction<0?"précédent":"suivant"));row->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,direction]{jump(direction);});
    }
    root->addWidget(toolbar);
    auto *findRow=new QHBoxLayout;findRow->setContentsMargins(12,0,12,8);
    search=new QLineEdit;search->setObjectName("diffSearch");search->setPlaceholderText("Rechercher dans le fichier…");search->setAccessibleName("Rechercher dans le fichier");findRow->addWidget(search);
    auto *find=new QPushButton("Rechercher");findRow->addWidget(find);root->addLayout(findRow);
    views=new QStackedWidget;unified=new DiffEditor;unified->setObjectName("diffEditor");unified->setAccessibleName("Diff du fichier ou du commit");views->addWidget(unified);
    auto *split=new QSplitter;split->setObjectName("diffSideBySide");
    for(auto pair:{std::pair<DiffEditor **,QString>{&before,"Avant"},{&after,"Après"}}){
        auto *panel=new QWidget;auto *layout=new QVBoxLayout(panel);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
        auto *label=new QLabel(pair.second);label->setMargin(10);layout->addWidget(label);
        *pair.first=new DiffEditor;(*pair.first)->setObjectName(pair.second=="Avant"?"diffBefore":"diffAfter");layout->addWidget(*pair.first,1);split->addWidget(panel);
    }
    views->addWidget(split);root->addWidget(views,1);
    connect(mode,&QComboBox::currentIndexChanged,views,&QStackedWidget::setCurrentIndex);
    connect(before->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){QSignalBlocker block(after->verticalScrollBar());after->verticalScrollBar()->setValue(value);});
    connect(after->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){QSignalBlocker block(before->verticalScrollBar());before->verticalScrollBar()->setValue(value);});
    auto doFind=[this]{
        auto *target=mode->currentIndex()==0?unified:after;
        if(!target->find(search->text())){target->moveCursor(QTextCursor::Start);target->find(search->text());}
    };
    connect(find,&QPushButton::clicked,this,doFind);connect(search,&QLineEdit::returnPressed,this,doFind);
}
void DiffViewer::setContent(const QString &text,bool patch){
    content=text;isPatch=patch;hunks.clear();
    const auto lines=text.split('\n');QStringList labels,oldLines,newLines,oldLabels,newLabels;
    QList<int> kinds,oldKinds,newKinds;int oldLine=0,newLine=0,added=0,removed=0;bool inHunk=false;
    QStringList deleted,inserted;QList<int> deletedNumbers,insertedNumbers;
    auto flush=[&]{
        for(int i=0;i<qMax(deleted.size(),inserted.size());++i){
            oldLines<<deleted.value(i);newLines<<inserted.value(i);
            oldLabels<<(i<deletedNumbers.size()?QString::number(deletedNumbers[i]):QString());
            newLabels<<(i<insertedNumbers.size()?QString::number(insertedNumbers[i]):QString());
            oldKinds<<(i<deleted.size()?-1:0);newKinds<<(i<inserted.size()?1:0);
        }
        deleted.clear();inserted.clear();deletedNumbers.clear();insertedNumbers.clear();
    };
    const QRegularExpression header("^@@ -(\\d+)(?:,\\d+)? \\+(\\d+)(?:,\\d+)? @@");
    for(int i=0;i<lines.size();++i){
        const auto &line=lines[i];auto match=header.match(line);QString label;int kind=0;
        if(!patch){label=QString::number(i+1);oldLines<<line;newLines<<line;oldLabels<<label;newLabels<<label;oldKinds<<0;newKinds<<0;}
        else if(match.hasMatch()){
            flush();oldLine=match.captured(1).toInt();newLine=match.captured(2).toInt();inHunk=true;kind=2;hunks<<i;
            oldLines<<line;newLines<<line;oldLabels<<"";newLabels<<"";oldKinds<<2;newKinds<<2;
        }else if(inHunk&&line.startsWith('-')){
            label=QString::number(oldLine)+"      ";deleted<<line.mid(1);deletedNumbers<<oldLine++;kind=-1;++removed;
        }else if(inHunk&&line.startsWith('+')){
            label="      "+QString::number(newLine);inserted<<line.mid(1);insertedNumbers<<newLine++;kind=1;++added;
        }else if(inHunk&&line.startsWith(' ')){
            flush();label=QString::number(oldLine)+"  "+QString::number(newLine);
            oldLines<<line.mid(1);newLines<<line.mid(1);oldLabels<<QString::number(oldLine++);newLabels<<QString::number(newLine++);oldKinds<<0;newKinds<<0;
        }else if(line.startsWith("diff --git ")){flush();inHunk=false;}
        labels<<label;kinds<<kind;
    }
    flush();
    unified->setLines(text,labels,kinds);before->setLines(oldLines.join('\n'),oldLabels,oldKinds);after->setLines(newLines.join('\n'),newLabels,newKinds);
    statistics->setText(patch?QString("+%1 ajout(s)  −%2 suppression(s) · %3 bloc(s)").arg(added).arg(removed).arg(hunks.size()):QString("%1 lignes").arg(lines.size()));
    mode->setEnabled(patch&&!hunks.isEmpty());if(!mode->isEnabled())mode->setCurrentIndex(0);
}
void DiffViewer::refreshColors(){setContent(content,isPatch);}
void DiffViewer::jump(int direction){
    if(hunks.isEmpty())return;
    const int current=unified->textCursor().blockNumber();int target=direction>0?hunks.first():hunks.last();
    if(direction>0){for(int n:hunks)if(n>current){target=n;break;}}
    else {for(auto i=hunks.crbegin();i!=hunks.crend();++i)if(*i<current){target=*i;break;}}
    mode->setCurrentIndex(0);auto cursor=QTextCursor(unified->document()->findBlockByNumber(target));unified->setTextCursor(cursor);unified->centerCursor();
}
