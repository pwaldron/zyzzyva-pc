//---------------------------------------------------------------------------
// WordListView.cpp
//
// A customized list view.
//
// Copyright 2005 Michael W Thelen <mike@pietdepsi.com>.
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

#include "WordListView.h"
#include "WordListViewItem.h"
#include "DefinitionDialog.h"
#include "HookDialog.h"
#include "WordEngine.h"
#include "WordPopupMenu.h"

//---------------------------------------------------------------------------
//  WordListView
//
//! Constructor.
//
//! @param parent the parent widget
//! @param name the name of this widget
//! @param f widget flags
//---------------------------------------------------------------------------
WordListView::WordListView (WordEngine* e, QWidget* parent, const char* name, WFlags
                      f)
    : QListView (parent, name, f), wordEngine (e)
{
    // Only connect certain slots of word engine is non-null
    if (wordEngine) {
        // XXX: This signal should be connected regardless... only certain
        // popup menu items should be available in the absence of a word
        // engine.
        connect (this, SIGNAL (contextMenuRequested (QListViewItem*, const
                                                     QPoint&, int)),
                 SLOT (doPopupMenu (QListViewItem*, const QPoint&, int)));

        connect (this, SIGNAL (returnPressed (QListViewItem*)),
                 SLOT (doReturnPressed (QListViewItem*)));
    }
}

//---------------------------------------------------------------------------
//  ~WordListView
//
//! Destructor.
//---------------------------------------------------------------------------
WordListView::~WordListView()
{
}

//---------------------------------------------------------------------------
//  setFont
//
//! Set the font of the listview and all listview items.
//---------------------------------------------------------------------------
void
WordListView::setFont (const QFont& font)
{
    QListView::setFont (font);
    WordListViewItem* item = static_cast<WordListViewItem*> (firstChild());
    for (; item; item = static_cast<WordListViewItem*> (item->nextSibling()))
        item->setFont (font);
}

//---------------------------------------------------------------------------
//  doReturnPressed
//
//! Called when return is pressed on an item in the response list.  Displays
//! the selected word's definition.
//
//! @param item the selected listview item
//---------------------------------------------------------------------------
void
WordListView::doReturnPressed (QListViewItem* item)
{
    if (!item)
        return;
    displayDefinition (item->text (0).upper());
}

//---------------------------------------------------------------------------
//  doPopupMenu
//
//! Called when a right-click menu is requested.  Creates a popup menu and
//! allows the user to choose an action for the selected item.
//! @param item the selected listview item
//! @param point the point at which the menu was requested
//---------------------------------------------------------------------------
void
WordListView::doPopupMenu (QListViewItem* item, const QPoint& point, int)
{
    if (!item)
        return;

    WordPopupMenu* menu = new WordPopupMenu (this, "menu");
    int choice = menu->exec (point);
    delete menu;

    switch (choice) {
        case WordPopupMenu::ShowDefinition:
        displayDefinition (item->text (0).upper());
        break;

        case WordPopupMenu::ShowHooks:
        displayHooks (item->text (0).upper());
        break;
    }
}

//---------------------------------------------------------------------------
//  displayDefinition
//
//! Displays the definition of a word.
//
//! @param word the word whose definition to display
//---------------------------------------------------------------------------
void
WordListView::displayDefinition (const QString& word)
{
    if (!wordEngine)
        return;

    DefinitionDialog* dialog = new DefinitionDialog (wordEngine, word, this,
                                                     "dialog", true);
    dialog->exec();
    delete dialog;
}

//---------------------------------------------------------------------------
//  displayHooks
//
//! Displays the hooks of a word.
//
//! @param word the word whose definition to display
//---------------------------------------------------------------------------
void
WordListView::displayHooks (const QString& word)
{
    if (!wordEngine)
        return;

    HookDialog* dialog = new HookDialog (wordEngine, word, this, "dialog",
                                         true);
    dialog->exec();
    delete dialog;
}
