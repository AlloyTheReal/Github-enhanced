#include "layoutdialog.h"
#include "custompanel.h"
#include "mainwindow.h"
#include "layoutcanvas.h"
#include <QtWidgets>
#include <functional>

namespace {
class LayoutListDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index)const override{
        auto styled=option;initStyleOption(&styled,index);auto size=QStyledItemDelegate::sizeHint(styled,index);
        size.setHeight(qMax(size.height(),qMax(34,QFontMetrics(styled.font).height()+16)));return size;
    }
    void paint(QPainter *painter,const QStyleOptionViewItem &option,const QModelIndex &index)const override{
        painter->save();painter->setClipRect(option.rect,Qt::IntersectClip);QStyledItemDelegate::paint(painter,option,index);painter->restore();
    }
};
void prepareList(QListWidget *list){list->setItemDelegate(new LayoutListDelegate(list));list->setUniformItemSizes(true);list->setWordWrap(false);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setSpacing(2);}
QIcon placementIcon(int side){QPixmap pixmap(40,40);pixmap.setDevicePixelRatio(2);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);const auto color=qApp->palette().color(QPalette::Text);if(side==2){painter.setPen(QPen(color,1.5));painter.drawRoundedRect(QRectF(3,6,11,11),2,2);painter.drawRoundedRect(QRectF(7,2,10,11),2,2);}else Visuals::chevron(painter,QRectF(4,4,12,12),side==0?Qt::RightArrow:side==1?Qt::DownArrow:side==3?Qt::LeftArrow:Qt::UpArrow,color);return QIcon(pixmap);}
QIcon historyIcon(bool redo){QPixmap pixmap(40,40);pixmap.setDevicePixelRatio(2);pixmap.fill(Qt::transparent);QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);if(redo){painter.translate(20,0);painter.scale(-1,1);}painter.setPen(QPen(qApp->palette().color(QPalette::Text),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));QPainterPath path;path.moveTo(15,15);path.cubicTo(19,9,12,5,5,8);painter.drawPath(path);painter.drawLine(5,8,6,3);painter.drawLine(5,8,10,10);return QIcon(pixmap);}
QPushButton *addButton(QBoxLayout *layout, const QString &text, QObject *context,
                       std::function<void()> callback) {
    auto *button = new QPushButton(text);
    layout->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, context, std::move(callback));
    return button;
}
} // namespace

LayoutDialog::LayoutDialog(MainWindow *w) : GlassDialog(w), window(w), original(w->layoutConfiguration()) {
    setObjectName("layoutEditor");
    setWindowTitle("Personnaliser l’interface");
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);
    resize(1100, 740);
    setMinimumSize(860, 640);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);
    auto *title = new QLabel("Personnaliser l’interface");
    title->setObjectName("headline");
    root->addWidget(title);
    auto *hint = new QLabel("Choisissez un onglet. Les changements sont visibles immédiatement dans la fenêtre.");
    hint->setObjectName("muted");
    hint->setWordWrap(true);
    root->addWidget(hint);
    auto *bodyWidget=new QWidget;auto *body=new QHBoxLayout(bodyWidget);body->setContentsMargins(0,0,0,0);
    auto *bodyScroll=new QScrollArea;bodyScroll->setObjectName("layoutBodyScroll");bodyScroll->setWidgetResizable(true);bodyScroll->setFrameShape(QFrame::NoFrame);bodyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);bodyScroll->setWidget(bodyWidget);root->addWidget(bodyScroll,1);
    auto *left = new QVBoxLayout;
    auto *presets=new GlassComboBox;presets->setObjectName("layoutPreset");presets->setAccessibleName("Modèle de disposition");presets->addItem("Disposition actuelle",QString());presets->addItem("Desktop","desktop");presets->addItem("Revue","review");presets->addItem("Focus","focus");left->addWidget(presets);
    connect(presets,&QComboBox::activated,this,[this,presets](int index){const auto id=presets->itemData(index).toString();if(id.isEmpty())return;remember();window->resetLayout(id);window->setLayoutEditing(true);refreshPanelList();refreshTools();});
    auto *map = new LayoutCanvas(window, window->layoutPanels());
    preview = map;
    left->addWidget(map, 1);
    auto *history = new QHBoxLayout;
    undoButton = addButton(history, "Annuler l’action", this, [this] { undo(); });
    undoButton->setObjectName("layoutUndo");
    redoButton = addButton(history, "Rétablir", this, [this] { undo(true); });
    redoButton->setObjectName("layoutRedo");
    undoButton->setText({});undoButton->setIcon(historyIcon(false));undoButton->setAccessibleName("Annuler l’action");undoButton->setToolTip("Annuler l’action");undoButton->setFixedSize(44,36);
    redoButton->setText({});redoButton->setIcon(historyIcon(true));redoButton->setAccessibleName("Rétablir l’action");redoButton->setToolTip("Rétablir l’action");redoButton->setFixedSize(44,36);
    auto *balanceButton=addButton(history, "Équilibrer", this, [this] {
        remember();
        QList<QDockWidget *> visible;
        QList<int> sizes;
        for (auto *d : window->layoutPanels())
            if (!d->isHidden() && !d->isFloating()) {
                visible.append(d);
                sizes.append(1);
            }
        window->resizeDocks(visible, sizes, Qt::Horizontal);
        window->resizeDocks(visible, sizes, Qt::Vertical);
        updateDimensions();
        preview->update();
    });
    root->addLayout(history);
    undoButton->setEnabled(false);
    redoButton->setEnabled(false);
    connect(map, &LayoutCanvas::selectedChanged, this, [this](QDockWidget *d) {
        for (int i = 0; i < panels->count(); ++i)
            if (panels->item(i)->data(Qt::UserRole).toString() == d->objectName())
                panels->setCurrentRow(i);
    });
    connect(map, &LayoutCanvas::interactionStarted, this, &LayoutDialog::remember);
    connect(map, &LayoutCanvas::layoutChanged, this, [this] { refreshPanelList(); });
    auto *panelPreview=new QWidget;panelPreview->setLayout(left);body->addWidget(panelPreview,1);
    auto *tabs = new QTabWidget;
    tabs->setMinimumWidth(320);tabs->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);

    body->addWidget(tabs,1);
    connect(tabs,&QTabWidget::currentChanged,this,[panelPreview,balanceButton](int index){panelPreview->setVisible(index==0);balanceButton->setVisible(index==0);});
    auto *panelPage = new QWidget;
    auto *panelLayout = new QVBoxLayout(panelPage);
    panels = new PanelList;prepareList(panels);
    panels->setDragEnabled(true);
    panels->setObjectName("layoutPanels");
    panels->setMaximumHeight(180);
    panelLayout->addWidget(panels);
    auto *customActions=new QHBoxLayout;
    auto *addPanel=addButton(customActions,"Créer…",this,[this]{auto definition=CustomPanel::edit(this);if(definition.isEmpty())return;auto config=window->layoutConfiguration();auto custom=config["customPanels"].toArray();if(custom.size()>=16)return;remember();custom.append(definition);auto state=window->saveState(2);window->restoreCustomPanels(custom);window->restoreState(state,2);window->setLayoutEditing(true);refreshPanelList();for(int i=0;i<panels->count();++i)if(panels->item(i)->data(Qt::UserRole).toString()==definition["id"].toString())panels->setCurrentRow(i);if(auto *panel=window->findChild<QDockWidget *>(definition["id"].toString())){panel->show();panel->raise();window->resizeDocks({panel},{280},Qt::Horizontal);}preview->update();});addPanel->setObjectName("addCustomPanel");
    addButton(customActions,"Modifier…",this,[this]{auto *panel=qobject_cast<CustomPanel *>(selectedPanel());if(!panel)return;auto definition=CustomPanel::edit(this,panel->definition());if(definition.isEmpty())return;remember();auto config=window->layoutConfiguration();auto custom=config["customPanels"].toArray();for(int i=0;i<custom.size();++i)if(custom[i].toObject()["id"]==definition["id"])custom[i]=definition;config["customPanels"]=custom;window->applyLayoutConfiguration(config);refreshPanelList();});
    addButton(customActions,"Retirer",this,[this]{auto *panel=qobject_cast<CustomPanel *>(selectedPanel());if(!panel)return;remember();auto config=window->layoutConfiguration();auto custom=config["customPanels"].toArray();for(int i=custom.size();i-->0;)if(custom[i].toObject()["id"]==panel->objectName())custom.removeAt(i);config["customPanels"]=custom;window->applyLayoutConfiguration(config);refreshPanelList();});auto *editCustom=qobject_cast<QPushButton *>(customActions->itemAt(1)->widget());auto *removeCustom=qobject_cast<QPushButton *>(customActions->itemAt(2)->widget());customActions->removeWidget(editCustom);customActions->removeWidget(removeCustom);editCustom->setParent(panelPage);removeCustom->setParent(panelPage);editCustom->hide();removeCustom->hide();
    auto *panelOptions=new QToolButton;panelOptions->setText("Options");panelOptions->setToolButtonStyle(Qt::ToolButtonTextOnly);panelOptions->setPopupMode(QToolButton::InstantPopup);auto *optionsMenu=new QMenu(panelOptions);auto *rename=optionsMenu->addAction("Renommer le panneau…",editCustom,&QPushButton::click);auto *remove=optionsMenu->addAction("Retirer le panneau",removeCustom,&QPushButton::click);panelOptions->setMenu(optionsMenu);customActions->addWidget(panelOptions);connect(optionsMenu,&QMenu::aboutToShow,this,[this,rename,remove]{const bool custom=qobject_cast<CustomPanel *>(selectedPanel());rename->setEnabled(custom);remove->setEnabled(custom);});
    panelLayout->addLayout(customActions);

    widthLabel = new QLabel;
    heightLabel = new QLabel;
    widthSlider = new QSlider(Qt::Horizontal);
    widthSlider->setObjectName("layoutWidthSlider");
    widthSlider->setAccessibleName("Largeur de la vue");
    heightSlider = new QSlider(Qt::Horizontal);
    heightSlider->setObjectName("layoutHeightSlider");
    heightSlider->setAccessibleName("Hauteur de la vue");
    panelLayout->addWidget(widthLabel);
    panelLayout->addWidget(widthSlider);
    panelLayout->addWidget(heightLabel);
    panelLayout->addWidget(heightSlider);
    auto *precise = new QToolButton;
    precise->setText("Dimensions exactes");
    precise->setToolButtonStyle(Qt::ToolButtonTextOnly);
    precise->setCheckable(true);
    panelLayout->addWidget(precise);
    auto *advanced = new QWidget;
    auto *advancedLayout = new QVBoxLayout(advanced);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    auto *form = new QFormLayout;
    advancedLayout->addLayout(form);
    position = new GlassComboBox;
    position->setObjectName("panelPosition");
    position->addItem("À gauche", int(Qt::LeftDockWidgetArea));
    position->addItem("À droite", int(Qt::RightDockWidgetArea));
    position->addItem("En haut", int(Qt::TopDockWidgetArea));
    position->addItem("En bas", int(Qt::BottomDockWidgetArea));
    position->addItem("Fenêtre détachée", -1);
    position->addItem("Masqué", 0);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);form->addRow("Position", position);
    panelWidth = new QSpinBox;
    panelWidth->setObjectName("panelWidth");
    panelWidth->setRange(140, 4000);
    panelWidth->setSuffix(" px");
    panelHeight = new QSpinBox;
    panelHeight->setObjectName("panelHeight");
    panelHeight->setRange(60, 3000);
    panelHeight->setSuffix(" px");
    form->addRow("Largeur", panelWidth);
    form->addRow("Hauteur", panelHeight);
    auto *resize = new QPushButton("Appliquer les dimensions");
    advancedLayout->addWidget(resize);
    relativeTo = new GlassComboBox;
    relativeTo->setObjectName("panelRelativeTo");
    placement = new GlassComboBox;
    placement->setObjectName("panelPlacement");
    placement->addItems({"À droite de", "En dessous de", "En onglet avec", "À gauche de", "Au-dessus de"});
    form = new QFormLayout;
    advancedLayout->addLayout(form);
    placement->setParent(advanced);placement->hide();
    auto *group = new QPushButton("Déplacer / regrouper",advanced);
    group->setObjectName("placePanel");
    group->hide();
    auto *quickPlacement=new QWidget;auto *quickLayout=new QVBoxLayout(quickPlacement);quickLayout->setContentsMargins(0,4,0,4);quickLayout->setSpacing(6);
    auto *referenceLabel=new QLabel("Placer près de");quickLayout->addWidget(referenceLabel);relativeTo->setAccessibleName("Panneau de référence");relativeTo->setToolTip("Choisissez la vue près de laquelle placer le panneau sélectionné");quickLayout->addWidget(relativeTo);
    auto *placementGrid=new QGridLayout;placementGrid->setSpacing(4);
    for(const auto &entry:QList<QPair<int,QPoint>>{{4,{1,0}},{3,{0,1}},{2,{1,1}},{0,{2,1}},{1,{1,2}}}){
        const auto side=entry.first;auto *action=new QToolButton;action->setObjectName("placePanelSide"+QString::number(side));action->setIcon(placementIcon(side));action->setAutoRaise(true);action->setMinimumSize(36,32);action->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        const QString label=QStringList{"À droite du panneau de référence","En dessous du panneau de référence","Regrouper en onglets","À gauche du panneau de référence","Au-dessus du panneau de référence"}[side];action->setToolTip(label);action->setAccessibleName(label);placementGrid->addWidget(action,entry.second.y(),entry.second.x());
        connect(action,&QToolButton::clicked,this,[this,group,side]{placement->setCurrentIndex(side);group->click();});
    }
    quickLayout->addLayout(placementGrid);auto *detach=new QPushButton("Détacher / rattacher");detach->setObjectName("togglePanelFloating");quickLayout->addWidget(detach);connect(detach,&QPushButton::clicked,this,[this]{if(auto *panel=selectedPanel()){auto area=window->dockWidgetArea(panel);if(area==Qt::NoDockWidgetArea)area=Qt::RightDockWidgetArea;position->setCurrentIndex(position->findData(panel->isFloating()?int(area):-1));QMetaObject::invokeMethod(position,"activated",Q_ARG(int,position->currentIndex()));}});
    panelLayout->insertWidget(panelLayout->indexOf(widthLabel),quickPlacement);

    advancedLayout->addStretch();
    panelLayout->addWidget(advanced);
    advanced->hide();
    connect(precise, &QToolButton::toggled, advanced, &QWidget::setVisible);
    panelLayout->addStretch();
    resizeTimer = new QTimer(this);
    resizeTimer->setSingleShot(true);
    resizeTimer->setInterval(16);
    connect(resizeTimer, &QTimer::timeout, this, [this] {
        if (auto *d = selectedPanel()) {
            if (d->isFloating())
                d->resize(widthSlider->value(), heightSlider->value());
            else {
                window->resizeDocks({d}, {widthSlider->value()}, Qt::Horizontal);
                window->resizeDocks({d}, {heightSlider->value()}, Qt::Vertical);
            }
            preview->update();
        }
    });
    for (auto *slider : {widthSlider, heightSlider}) {
        connect(slider, &QSlider::sliderPressed, this, &LayoutDialog::remember);
        connect(slider, &QSlider::valueChanged, this, [this, slider] {
            if (updating)
                return;
            if (!slider->isSliderDown())
                remember();
            widthLabel->setText(QString("Largeur · %1 px").arg(widthSlider->value()));
            heightLabel->setText(QString("Hauteur · %1 px").arg(heightSlider->value()));
            resizeTimer->start();
        });
        connect(slider, &QSlider::sliderReleased, this,
                [this] { QTimer::singleShot(30, this, &LayoutDialog::updateDimensions); });
    }
    auto *panelScroll = new QScrollArea;
    panelScroll->setWidgetResizable(true);
    panelScroll->setFrameShape(QFrame::NoFrame);
    panelScroll->setWidget(panelPage);
    tabs->addTab(panelScroll, "Panneaux");

    auto *toolPage = new QWidget;
    auto *toolLayout = new QVBoxLayout(toolPage);
    auto *toolHint = new QLabel("Cochez les commandes à afficher. Glissez pour changer leur ordre.");
    toolHint->setWordWrap(true);
    toolLayout->addWidget(toolHint);
    tools = new QListWidget;prepareList(tools);
    tools->setObjectName("toolbarItems");
    tools->setMinimumHeight(qMax(180,QFontMetrics(tools->font()).height()*4+64));
    auto *toolSearch=new QLineEdit;toolSearch->setObjectName("layoutCommandSearch");toolSearch->setPlaceholderText("Filtrer les commandes");toolSearch->setClearButtonEnabled(true);toolLayout->addWidget(toolSearch);
    connect(toolSearch,&QLineEdit::textChanged,this,[this](const QString &query){for(int i=0;i<tools->count();++i)tools->item(i)->setHidden(!tools->item(i)->text().contains(query.trimmed(),Qt::CaseInsensitive));tools->setDragEnabled(query.trimmed().isEmpty());});
    tools->setDragDropMode(QAbstractItemView::InternalMove);
    toolLayout->addWidget(tools);
    auto *order = new QHBoxLayout;
    toolLayout->addLayout(order);
    for (const auto &entry : QList<QPair<QString, int>>{{"Monter", -1}, {"Descendre", 1}})
        addButton(order, entry.first, this, [this, entry] {
            int row = tools->currentRow(), target = row + entry.second;
            while(target>=0&&target<tools->count()&&tools->item(target)->isHidden())target+=entry.second;
            if (row < 0 || target < 0 || target >= tools->count())
                return;
            auto *item = tools->takeItem(row);
            tools->insertItem(target, item);
            tools->setCurrentItem(item);
            applyTools();
        });
    auto *toolbarVisible = new QCheckBox("Afficher la barre d’outils");toolbarVisible->setObjectName("layoutToolbarVisible");
    toolbarVisible->setChecked(window->mainToolbar->isVisible());
    connect(window->mainToolbar,&QToolBar::visibilityChanged,toolbarVisible,[toolbarVisible](bool visible){QSignalBlocker blocker(toolbarVisible);toolbarVisible->setChecked(visible);});
    toolLayout->insertWidget(1,toolbarVisible);
    connect(toolbarVisible, &QCheckBox::toggled, this, [this](bool visible){remember();window->mainToolbar->setVisible(visible);});
    auto *toolbarPosition = new GlassComboBox;toolbarPosition->setObjectName("layoutToolbarPosition");
    toolbarPosition->addItem("En haut", int(Qt::TopToolBarArea));
    toolbarPosition->addItem("En bas", int(Qt::BottomToolBarArea));
    toolbarPosition->addItem("À gauche", int(Qt::LeftToolBarArea));
    toolbarPosition->addItem("À droite", int(Qt::RightToolBarArea));
    toolbarPosition->setCurrentIndex(
        toolbarPosition->findData(int(window->toolBarArea(window->mainToolbar))));
    auto *toolbarStyle = new GlassComboBox;toolbarStyle->setObjectName("layoutToolbarStyle");
    toolbarStyle->addItem("Icônes et texte", int(Qt::ToolButtonTextBesideIcon));
    toolbarStyle->addItem("Icônes seulement", int(Qt::ToolButtonIconOnly));
    toolbarStyle->addItem("Texte seulement", int(Qt::ToolButtonTextOnly));
    toolbarStyle->setCurrentIndex(toolbarStyle->findData(int(window->mainToolbar->toolButtonStyle())));
    auto *toolbarAdvanced=new QWidget;auto *toolbarAdvancedLayout=new QVBoxLayout(toolbarAdvanced);toolbarAdvancedLayout->setContentsMargins(0,0,0,0);
    toolLayout->addWidget(toolbarAdvanced);
    auto *toolForm = new QFormLayout;toolForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    toolForm->addRow("Position", toolbarPosition);
    toolForm->addRow("Présentation", toolbarStyle);
    toolbarAdvancedLayout->addLayout(toolForm);

    connect(toolbarPosition, &QComboBox::activated, this, [this, toolbarPosition] {
        remember();
        window->addToolBar(Qt::ToolBarArea(toolbarPosition->currentData().toInt()), window->mainToolbar);
    });
    connect(toolbarStyle, &QComboBox::activated, this, [this, toolbarStyle] {
        remember();
        window->mainToolbar->setToolButtonStyle(Qt::ToolButtonStyle(toolbarStyle->currentData().toInt()));
    });
    auto *toolScroll=new QScrollArea;toolScroll->setWidgetResizable(true);toolScroll->setFrameShape(QFrame::NoFrame);toolScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);toolScroll->setWidget(toolPage);tabs->addTab(toolScroll,"Barre d’outils");

    auto *profilePage = new QWidget;
    auto *profileLayout = new QVBoxLayout(profilePage);
    auto *profileHint = new QLabel(
        "Retrouvez vos dispositions, y compris l’ordre des commandes, les onglets et les panneaux détachés.");
    profileHint->setWordWrap(true);
    profileLayout->addWidget(profileHint);
    profiles = new QListWidget;prepareList(profiles);
    profiles->setObjectName("layoutProfiles");
    profileLayout->addWidget(profiles);
    addButton(profileLayout, "Enregistrer sous…", this, [this] {
        bool ok = false;
        auto name =
            glassInputText(this, "Nouvelle disposition", "Nom", QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || name.isEmpty() || name.size() > 64 || name.contains('/') || name.contains('\\'))
            return;
        auto config = window->layoutConfiguration();
        config["locked"] = true;
        window->settings.setValue("layouts/" + name, QJsonDocument(config).toJson(QJsonDocument::Compact));
        refreshProfiles();
        window->rebuildLayouts();
    });
    addButton(profileLayout, "Charger la disposition", this, [this] {
        if (!profiles->currentItem())
            return;
        auto config = QJsonDocument::fromJson(
                          window->settings.value("layouts/" + profiles->currentItem()->text()).toByteArray())
                          .object();
        if (!window->applyLayoutConfiguration(config)) {
            QMessageBox::warning(
                this, "Disposition",
                "Cette disposition utilise un ancien format. Enregistrez une nouvelle disposition.");
            return;
        }
        window->setLayoutEditing(true);
        refreshPanelList();
        refreshTools();
    });
    addButton(profileLayout, "Supprimer la disposition", this, [this] {
        if (!profiles->currentItem())
            return;
        window->settings.remove("layouts/" + profiles->currentItem()->text());
        refreshProfiles();
        window->rebuildLayouts();
    });
    auto *exchange = new QHBoxLayout;
    profileLayout->addLayout(exchange);
    addButton(exchange, "Importer…", this, [this] {
        auto path =
            QFileDialog::getOpenFileName(this, "Importer une disposition", {}, "Disposition (*.json)");
        if (path.isEmpty())
            return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 262144 ||
            !window->applyLayoutConfiguration(QJsonDocument::fromJson(file.readAll()).object())) {
            QMessageBox::warning(this, "Disposition", "Fichier de disposition invalide.");
            return;
        }
        window->setLayoutEditing(true);
        refreshPanelList();
        refreshTools();
    });
    addButton(exchange, "Exporter…", this, [this] {
        auto path = QFileDialog::getSaveFileName(this, "Exporter la disposition", "disposition.json",
                                                 "Disposition (*.json)");
        if (path.isEmpty())
            return;
        auto config = window->layoutConfiguration();
        config["locked"] = true;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(config).toJson()) < 0 ||
            !file.commit())
            QMessageBox::warning(this, "Export", file.errorString());
    });
    auto *profileScroll=new QScrollArea;profileScroll->setWidgetResizable(true);profileScroll->setFrameShape(QFrame::NoFrame);profileScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);profileScroll->setWidget(profilePage);tabs->addTab(profileScroll,"Profils");
    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    root->addWidget(box);
    box->button(QDialogButtonBox::Save)->setText("Enregistrer");
    box->button(QDialogButtonBox::Save)->setIcon({});
    box->button(QDialogButtonBox::Cancel)->setText("Annuler");
    box->button(QDialogButtonBox::Cancel)->setIcon({});
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(panels, &QListWidget::currentRowChanged, this, [this] { selectPanel(); });
    connect(panels, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (updating)
            return;
        remember();
        if (auto *d = window->findChild<QDockWidget *>(item->data(Qt::UserRole).toString())) {
            d->setVisible(item->checkState() == Qt::Checked);
            if (d->isVisible())
                d->raise();
        }
        selectPanel();
        preview->update();
    });
    connect(position, &QComboBox::activated, this, [this] {
        auto *d = selectedPanel();
        if (!d)
            return;
        remember();
        const auto area = position->currentData().toInt();
        if (!area)
            d->hide();
        else if (area == -1) {
            d->setFloating(true);
            d->show();
        } else {
            d->setFloating(false);
            window->addDockWidget(Qt::DockWidgetArea(area), d);
            d->show();
            d->raise();
        }
        refreshPanelList();
    });
    connect(resize, &QPushButton::clicked, this, [this] {
        remember();
        if (auto *d = selectedPanel()) {
            if (d->isFloating())
                d->resize(panelWidth->value(), panelHeight->value());
            else {
                window->resizeDocks({d}, {panelWidth->value()}, Qt::Horizontal);
                window->resizeDocks({d}, {panelHeight->value()}, Qt::Vertical);
            }
            preview->update();
        }
    });
    connect(group, &QPushButton::clicked, this, [this] {
        auto *d = selectedPanel();
        auto *target = window->findChild<QDockWidget *>(relativeTo->currentData().toString());
        if (!d || !target || d == target)
            return;
        remember();
        d->setFloating(false);
        target->setFloating(false);
        d->show();
        target->show();
        auto area = window->dockWidgetArea(target);
        if (area == Qt::NoDockWidgetArea)
            area = Qt::RightDockWidgetArea;
        window->addDockWidget(area, d);
        const int side = placement->currentIndex();
        if (side == 2)
            window->tabifyDockWidget(target, d);
        else {
            const auto siblings = window->tabifiedDockWidgets(target);
            const auto orientation = side == 0 || side == 3 ? Qt::Horizontal : Qt::Vertical;
            window->splitDockWidget(target, d, orientation);
            if (side == 3 || side == 4) {
                window->splitDockWidget(d, target, orientation);
                for (auto *sibling : siblings)
                    if (sibling != d)
                        window->tabifyDockWidget(target, sibling);
            }
        }
        d->raise();
        refreshPanelList();
    });
    connect(tools, &QListWidget::itemChanged, this, [this] {
        if (!updating)
            applyTools();
    });
    connect(tools->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        if (!updating)
            applyTools();
    });
    window->setLayoutEditing(true);
    refreshPanelList();
    refreshTools();
    refreshProfiles();
    auto *timer = new QTimer(this);
    timer->setInterval(200);
    connect(timer, &QTimer::timeout, this, [this] {
        preview->update();
        if (!widthSlider->isSliderDown() && !heightSlider->isSliderDown())
            updateDimensions();
    });
    timer->start();
}

QDockWidget *LayoutDialog::selectedPanel() const {
    auto *item = panels->currentItem();
    return item ? window->findChild<QDockWidget *>(item->data(Qt::UserRole).toString()) : nullptr;
}

void LayoutDialog::refreshPanelList() {
    updating = true;
    preview->setPanels(window->layoutPanels());
    const auto selected = selectedPanel() ? selectedPanel()->objectName() : QString();
    panels->clear();
    for (auto *d : window->layoutPanels()) {
        auto *item = new QListWidgetItem(d->windowTitle(), panels);
        item->setData(Qt::UserRole, d->objectName());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(d->isHidden() ? Qt::Unchecked : Qt::Checked);
        if (d->objectName() == selected)
            panels->setCurrentItem(item);
    }
    if (!panels->currentItem())
        panels->setCurrentRow(0);
    updating = false;
    selectPanel();
    preview->update();
}

void LayoutDialog::selectPanel() {
    if (updating)
        return;
    auto *d = selectedPanel();
    if (!d)
        return;
    preview->select(d);
    const int area = d->isHidden() ? 0 : d->isFloating() ? -1 : int(window->dockWidgetArea(d));
    position->setCurrentIndex(position->findData(area));
    resizeTimer->stop();
    updateDimensions();
    const auto reference=relativeTo->currentData().toString();
    relativeTo->clear();
    for (auto *other : window->layoutPanels())
        if (other != d)
            relativeTo->addItem(other->windowTitle(), other->objectName());
    relativeTo->setCurrentIndex(qMax(0,relativeTo->findData(reference)));
}

void LayoutDialog::refreshTools() {
    updating = true;
    const auto scroll=tools->verticalScrollBar()->value();const auto selected=tools->currentItem()?tools->currentItem()->data(Qt::UserRole).toString():QString();
    tools->clear();
    for (auto *a : window->mainToolbar->actions()) {
        auto *item = new QListWidgetItem(a->property("customTitle").toString(), tools);
        item->setData(Qt::UserRole, a->objectName());item->setToolTip(item->text());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(a->isVisible() ? Qt::Checked : Qt::Unchecked);if(a->objectName()==selected)tools->setCurrentItem(item);
    }
    tools->verticalScrollBar()->setValue(scroll);if(auto *search=findChild<QLineEdit *>("layoutCommandSearch"))for(int i=0;i<tools->count();++i)tools->item(i)->setHidden(!tools->item(i)->text().contains(search->text().trimmed(),Qt::CaseInsensitive));
    if(auto *visible=findChild<QCheckBox *>("layoutToolbarVisible")){QSignalBlocker block(visible);visible->setChecked(!window->mainToolbar->isHidden());}
    if(auto *position=findChild<QComboBox *>("layoutToolbarPosition"))position->setCurrentIndex(position->findData(int(window->toolBarArea(window->mainToolbar))));
    if(auto *style=findChild<QComboBox *>("layoutToolbarStyle"))style->setCurrentIndex(style->findData(int(window->mainToolbar->toolButtonStyle())));
    updating = false;
}

void LayoutDialog::applyTools() {
    remember();
    QJsonArray array;
    for (int i = 0; i < tools->count(); ++i) {
        auto *item = tools->item(i);
        array.append(QJsonObject{{"id", item->data(Qt::UserRole).toString()},
                                 {"visible", item->checkState() == Qt::Checked}});
    }
    window->applyToolbarConfiguration(array);
    preview->update();
}

void LayoutDialog::refreshProfiles() {
    profiles->clear();
    window->settings.beginGroup("layouts");
    profiles->addItems(window->settings.childKeys());
    window->settings.endGroup();
}

void LayoutDialog::remember() {
    const auto state = window->layoutConfiguration();
    if (undoStates.isEmpty() || undoStates.last() != state)
        undoStates.append(state);
    while (undoStates.size() > 40)
        undoStates.removeFirst();
    redoStates.clear();
    undoButton->setEnabled(true);
    redoButton->setEnabled(false);
}
void LayoutDialog::undo(bool redo) {
    resizeTimer->stop();
    auto &from = redo ? redoStates : undoStates;
    auto &to = redo ? undoStates : redoStates;
    if (from.isEmpty())
        return;
    to.append(window->layoutConfiguration());
    const auto state = from.takeLast();
    window->applyLayoutConfiguration(state);
    window->setLayoutEditing(true);
    refreshPanelList();
    refreshTools();
    undoButton->setEnabled(!undoStates.isEmpty());
    redoButton->setEnabled(!redoStates.isEmpty());
}
void LayoutDialog::updateDimensions() {
    auto *d = selectedPanel();
    if (!d)
        return;
    const QSignalBlocker a(widthSlider), b(heightSlider);
    widthSlider->setRange(qMax(140, d->minimumWidth()), qMax(d->width(), window->width() - 140));
    heightSlider->setRange(qMax(60, d->minimumHeight()), qMax(d->height(), window->height() - 100));
    if (!widthSlider->hasFocus())
        widthSlider->setValue(d->width());
    if (!heightSlider->hasFocus())
        heightSlider->setValue(d->height());
    if (!panelWidth->hasFocus()&&!panelWidth->isAncestorOf(QApplication::focusWidget()))
        panelWidth->setValue(d->width());
    if (!panelHeight->hasFocus()&&!panelHeight->isAncestorOf(QApplication::focusWidget()))
        panelHeight->setValue(d->height());
    widthLabel->setText(QString("Largeur · %1 px").arg(d->width()));
    heightLabel->setText(QString("Hauteur · %1 px").arg(d->height()));
}
void LayoutDialog::done(int result) {
    resizeTimer->stop();
    if (result == QDialog::Accepted) {
        window->setLayoutEditing(false);
        window->saveLayoutSettings();
    } else
        window->applyLayoutConfiguration(original);
    GlassDialog::done(result);
}
