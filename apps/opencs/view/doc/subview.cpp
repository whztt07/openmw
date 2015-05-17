#include "subview.hpp"

#include "view.hpp"

CSVDoc::SubView::SubView (const CSMWorld::UniversalId& id)
 : mUniversalId (id)
{
    /// \todo  add a button to the title bar that clones this sub view

    setWindowTitle (QString::fromUtf8 (mUniversalId.toString().c_str()));
    setAttribute(Qt::WA_DeleteOnClose);
}

CSMWorld::UniversalId CSVDoc::SubView::getUniversalId() const
{
    return mUniversalId;
}

void CSVDoc::SubView::setStatusBar (bool show) {}

void CSVDoc::SubView::useHint (const std::string& hint) {}

void CSVDoc::SubView::updateUserSetting (const QString &, const QStringList &)
{}

void CSVDoc::SubView::setUniversalId (const CSMWorld::UniversalId& id)
{
    mUniversalId = id;
    setWindowTitle (QString::fromUtf8(mUniversalId.toString().c_str()));
}

void CSVDoc::SubView::closeEvent (QCloseEvent *event)
{
    emit updateSubViewIndicies (this);
}

std::string CSVDoc::SubView::getTitle() const
{
    return mUniversalId.toString();
}

void CSVDoc::SubView::closeRequest()
{
    emit closeRequest (this);
}

CSVDoc::SizeHintWidget::SizeHintWidget(QWidget *parent) : QWidget(parent)
{}

CSVDoc::SizeHintWidget::~SizeHintWidget()
{}

QSize CSVDoc::SizeHintWidget::sizeHint() const
{
    return mSize;
}

void CSVDoc::SizeHintWidget::setSizeHint(const QSize &size)
{
    mSize = size;
}
