#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QMediaMetaData>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
QT_END_NAMESPACE

//! [0]
class MetaDataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetaDataDialog(QWidget *parent = nullptr);

    QLineEdit *m_metaDataFields[QMediaMetaData::NumMetaData] = {};

private slots:
    void openThumbnailImage();
    void openCoverArtImage();
};
//! [0]

#endif
