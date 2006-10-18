//---------------------------------------------------------------------------
// WordListSaveDialog.cpp
//
// The dialog for saving a list of words.
//
// Copyright 2006 Michael W Thelen <mthelen@gmail.com>.
//
// This file is part of Zyzzyva.
//
// Zyzzyva is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Zyzzyva is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//---------------------------------------------------------------------------

#include "WordListSaveDialog.h"
#include "ZPushButton.h"
#include "Auxil.h"
#include "Defs.h"
#include <QHBoxLayout>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

const QString DIALOG_CAPTION = "Save Word List";

using namespace Defs;

//---------------------------------------------------------------------------
//  WordListSaveDialog
//
//! Constructor.
//
//! @param parent the parent widget
//! @param f widget flags
//---------------------------------------------------------------------------
WordListSaveDialog::WordListSaveDialog (QWidget* parent, Qt::WFlags f)
    : QDialog (parent, f)
{
    QVBoxLayout* mainVlay = new QVBoxLayout (this);
    mainVlay->setMargin (MARGIN);
    mainVlay->setSpacing (SPACING);
    Q_CHECK_PTR (mainVlay);

    QHBoxLayout* listHlay = new QHBoxLayout;
    listHlay->setSpacing (SPACING);
    Q_CHECK_PTR (listHlay);
    mainVlay->addLayout (listHlay);

    unselectedAttrList = new QListWidget (this);
    Q_CHECK_PTR (unselectedAttrList);
    unselectedAttrList->setSelectionMode (QAbstractItemView::ExtendedSelection);
    listHlay->addWidget (unselectedAttrList);

    QVBoxLayout* arrowVlay = new QVBoxLayout;
    Q_CHECK_PTR (arrowVlay);
    listHlay->addLayout (arrowVlay);

    arrowVlay->addStretch (1);

    QToolButton* selectButton = new QToolButton (this);
    Q_CHECK_PTR (selectButton);
    selectButton->setIcon (QIcon (":/right-arrow-icon"));
    connect (selectButton, SIGNAL (clicked()), SLOT (selectClicked()));
    arrowVlay->addWidget (selectButton);

    QToolButton* deselectButton = new QToolButton (this);
    Q_CHECK_PTR (deselectButton);
    deselectButton->setIcon (QIcon (":/left-arrow-icon"));
    connect (deselectButton, SIGNAL (clicked()), SLOT (deselectClicked()));
    arrowVlay->addWidget (deselectButton);

    arrowVlay->addStretch (1);

    selectedAttrList = new QListWidget (this);
    Q_CHECK_PTR (selectedAttrList);
    selectedAttrList->setSelectionMode (QAbstractItemView::ExtendedSelection);
    listHlay->addWidget (selectedAttrList);

    QHBoxLayout* buttonHlay = new QHBoxLayout;
    buttonHlay->setSpacing (SPACING);
    Q_CHECK_PTR (buttonHlay);
    mainVlay->addLayout (buttonHlay);

    buttonHlay->addStretch (1);

    ZPushButton* okButton = new ZPushButton ("&OK");
    Q_CHECK_PTR (okButton);
    okButton->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
    okButton->setDefault (true);
    connect (okButton, SIGNAL (clicked()), SLOT (accept()));
    buttonHlay->addWidget (okButton);

    ZPushButton* cancelButton = new ZPushButton ("Cancel");
    Q_CHECK_PTR (cancelButton);
    cancelButton->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect (cancelButton, SIGNAL (clicked()), SLOT (reject()));
    buttonHlay->addWidget (cancelButton);

    initializeLists();
    setWindowTitle (DIALOG_CAPTION);
}

//---------------------------------------------------------------------------
//  ~WordListSaveDialog
//
//! Destructor.
//---------------------------------------------------------------------------
WordListSaveDialog::~WordListSaveDialog()
{
}

//---------------------------------------------------------------------------
//  getSelectedAttributes
//
//! Get a list of selected word attributes in order.
//
//! @return the ordered list of selected attributes
//---------------------------------------------------------------------------
QList<WordAttribute>
WordListSaveDialog::getSelectedAttributes() const
{
    QList<WordAttribute> attributes;
    for (int i = 0; i < selectedAttrList->count(); ++i) {
        QString str = selectedAttrList->item (i)->text();
        WordAttribute attr = Auxil::stringToWordAttribute (str);
        if (attr != WordAttrInvalid)
            attributes.append (attr);
    }
    return attributes;
}

//---------------------------------------------------------------------------
//  selectClicked
//
//! Called when the select button is clicked.
//---------------------------------------------------------------------------
void
WordListSaveDialog::selectClicked()
{
    moveSelection (unselectedAttrList, selectedAttrList);
}

//---------------------------------------------------------------------------
//  deselectClicked
//
//! Called when the deselect button is clicked.
//---------------------------------------------------------------------------
void
WordListSaveDialog::deselectClicked()
{
    moveSelection (selectedAttrList, unselectedAttrList);
}

//---------------------------------------------------------------------------
//  initializeLists
//
//! Initialize the lists of selected and unselected attributes.
//---------------------------------------------------------------------------
void
WordListSaveDialog::initializeLists()
{
    unselectedAttrList->addItem
        (Auxil::wordAttributeToString (WordAttrDefinition));
    unselectedAttrList->addItem
        (Auxil::wordAttributeToString (WordAttrFrontHooks));
    unselectedAttrList->addItem
        (Auxil::wordAttributeToString (WordAttrBackHooks));
    unselectedAttrList->addItem
        (Auxil::wordAttributeToString (WordAttrInnerHooks));
    unselectedAttrList->addItem
        (Auxil::wordAttributeToString (WordAttrProbabilityOrder));

    selectedAttrList->addItem
        (Auxil::wordAttributeToString (WordAttrWord));
}

//---------------------------------------------------------------------------
//  moveSelection
//
//! Move selected items from one list widget to another
//
//! @param src the source list widget
//! @param dest the destination list widget
//---------------------------------------------------------------------------
void
WordListSaveDialog::moveSelection (QListWidget* src, QListWidget* dest)
{
    // Build a sorted list of selected rows
    QItemSelectionModel* selectionModel = src->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
    QListIterator<QModelIndex> it (selectedIndexes);
    QList<int> selectedRows;
    while (it.hasNext()) {
        selectedRows.append (it.next().row());
    }
    qSort (selectedRows.begin(), selectedRows.end(), qGreater<int>());

    // Move rows in reverse order so row numbers are not messed up
    QListIterator<int> jt (selectedRows);
    int insertRow = dest->count();
    while (jt.hasNext()) {
        int row = jt.next();
        QString attrStr = src->item (row)->text();
        dest->insertItem (insertRow, src->takeItem (row));
    }
}
