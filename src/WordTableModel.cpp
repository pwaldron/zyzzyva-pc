//---------------------------------------------------------------------------
// WordTableModel.cpp
//
// A model for representing word lists.
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

#include "WordTableModel.h"
#include "WordEngine.h"
#include "MainSettings.h"

using namespace std;

const QChar WordTableModel::PARENT_HOOK_CHAR = '-';
const QString FRONT_HOOK_HEADER = "Hooks";
const QString WORD_HEADER = "Word";
const QString BACK_HOOK_HEADER = "Hooks";
const QString DEFINITION_HEADER = "Definition";
const int ITEM_MARGIN = 5;
const int DEFAULT_COLUMN_WIDTH = 100;
const int NUM_COLUMNS = 4;

//---------------------------------------------------------------------------
//  lessThan
//
//! A comparison function that compares by lexical value.
//
//! @param a the first word item to compare
//! @param b the second word item to compare
//! @return true if a is lexically less than b
//---------------------------------------------------------------------------
bool
lessThan (const WordTableModel::WordItem& a,
          const WordTableModel::WordItem& b)
{
    return (a.getWord().toUpper() < b.getWord().toUpper());
}

//---------------------------------------------------------------------------
//  lengthLessThan
//
//! A comparison function that compares by length as well as lexical value.
//
//! @param a the first word item to compare
//! @param b the second word item to compare
//! @return true if a is shorter than b, or else a is lexically less than b
//---------------------------------------------------------------------------
bool
lengthLessThan (const WordTableModel::WordItem& a,
                const WordTableModel::WordItem& b)
{
    if (a.getWord().length() < b.getWord().length()) {
        return true;
    }
    else if (a.getWord().length() > b.getWord().length()) {
        return false;
    }

    return lessThan (a, b);
}

//---------------------------------------------------------------------------
//  WordTableModel
//
//! Constructor.
//
//! @param parent the parent object
//---------------------------------------------------------------------------
WordTableModel::WordTableModel (WordEngine* e, QObject* parent)
    : QAbstractTableModel (parent), wordEngine (e), lastAddedIndex (-1)
{
}

//---------------------------------------------------------------------------
//  ~WordTableModel
//
//! Destructor.
//---------------------------------------------------------------------------
WordTableModel::~WordTableModel()
{
}

//---------------------------------------------------------------------------
//  clear
//
//! Clear the word list.
//
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::clear()
{
    bool ok = removeRows (0, rowCount());
    emit wordsChanged();
    return ok;
}

//---------------------------------------------------------------------------
//  addWord
//
//! Add a word to the model.
//
//! @param word the word to add
//! @param type the type associated with the word
//! @param updateLastAdded whether to update the last added index
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::addWord (const QString& word, WordType type, bool
                         updateLastAdded)
{
    int row = rowCount();
    bool ok = insertRow (row);
    if (!ok)
        return false;
    setData (index (row, 0), word, Qt::EditRole);
    setData (index (row, 0), type, Qt::UserRole);
    sort (WORD_COLUMN);
    lastAddedIndex = updateLastAdded ? wordList.indexOf (WordItem (word, type))
                                     : -1;
    emit wordsChanged();
    return true;
}

//---------------------------------------------------------------------------
//  addWords
//
//! Add a list of words to the model.
//
//! @param words the words to add
//! @param type the type associated with the words
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::addWords (const QStringList& words, WordType type)
{
    int row = rowCount();
    bool ok = insertRows (row, words.size());
    if (!ok)
        return false;
    QString word;
    foreach (word, words) {
        setData (index (row, 0), word, Qt::EditRole);
        setData (index (row, 0), type, Qt::UserRole);
        ++row;
    }
    sort (WORD_COLUMN);
    lastAddedIndex = -1;
    emit wordsChanged();
    return true;
}

//---------------------------------------------------------------------------
//  rowCount
//
//! Return the number of rows under the given parent.  Reimplemented from
//! QAbstractTableModel.
//
//! @param parent the parent index
//! @return the number of rows
//---------------------------------------------------------------------------
int
WordTableModel::rowCount (const QModelIndex&) const
{
    return wordList.count();
}

//---------------------------------------------------------------------------
//  columnCount
//
//! Return the number of columns for the given parent.  Reimplemented from
//! QAbstractTableModel.
//
//! @param parent the parent index
//! @return the number of columns
//---------------------------------------------------------------------------
int
WordTableModel::columnCount (const QModelIndex&) const
{
    return NUM_COLUMNS;
}

//---------------------------------------------------------------------------
//  data
//
//! Return the data stored under the given role for the item referred to by
//! the index.  Reimplemented from QAbstractTableModel.
//
//! @param index the index
//! @param role the display role
//! @return the appropriate header data, or an empty object if invalid
//---------------------------------------------------------------------------
QVariant
WordTableModel::data (const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if ((index.row() < 0) || (index.row() >= wordList.count()))
        return QVariant();

    if (role == Qt::UserRole)
        return wordList.at (index.row()).getType();

    if ((role != Qt::DisplayRole) && (role != Qt::EditRole))
        return QVariant();

    QString word = wordList.at (index.row()).getWord();
    QString wordUpper = word.toUpper();
    switch (index.column()) {
        case FRONT_HOOK_COLUMN:
        return MainSettings::getWordListShowHooks() ?
            getFrontHookLetters (wordUpper) : QString::null;

        case BACK_HOOK_COLUMN:
        return MainSettings::getWordListShowHooks() ?
            getBackHookLetters (wordUpper) : QString::null;

        case WORD_COLUMN:
        if (role == Qt::EditRole)
            return wordUpper;
        else if (role == Qt::DisplayRole) {
            if (MainSettings::getWordListShowHookParents()) {
                return
                    (isFrontHook (wordUpper) ? PARENT_HOOK_CHAR : QChar (' '))
                    + word +
                    (isBackHook (wordUpper) ? PARENT_HOOK_CHAR : QChar (' '));
            }
            else
                return word;
        }
        else
            return word;

        case DEFINITION_COLUMN:
        return MainSettings::getWordListShowDefinitions() ?
            wordEngine->getDefinition (wordUpper) : QString::null;

        default: return word;
    }
}

//---------------------------------------------------------------------------
//  headerData
//
//! Return the data for the given role and section in the header with the
//! specified orientation.  Reimplemented from QAbstractTableModel.
//
//! @param section the sectionindex
//! @param orientation the header orientation
//! @param role the display role
//! @return the appropriate header data, or an empty object if invalid
//---------------------------------------------------------------------------
QVariant
WordTableModel::headerData (int section, Qt::Orientation orientation, int
                            role) const
{
    if ((section < 0)  || (section > NUM_COLUMNS - 1))
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant();

    if (role == Qt::DisplayRole) {
        switch (section) {
            case FRONT_HOOK_COLUMN:
            return MainSettings::getWordListShowHooks() ?
                FRONT_HOOK_HEADER : QString::null;

            case BACK_HOOK_COLUMN:
            return MainSettings::getWordListShowHooks() ?
                BACK_HOOK_HEADER : QString::null;

            case WORD_COLUMN:
            return WORD_HEADER;

            case DEFINITION_COLUMN:
            return MainSettings::getWordListShowDefinitions() ?
                DEFINITION_HEADER : QString::null;

            default:
            return "Unknown";
        }
    }
    else
        return QVariant();
}

//---------------------------------------------------------------------------
//  insertRows
//
//! Insert rows into the model before the given row.  Reimplemented from
//! QAbstractTableModel.
//
//! @param row the row number to begin inserting rows
//! @param count the number of rows to insert
//! @param parent the parent index
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::insertRows (int row, int count, const QModelIndex&)
{
    beginInsertRows (QModelIndex(), row, row + count - 1);

    for (int i = 0; i < count; ++i) {
        wordList.insert (row, WordItem ("", WordNormal));
    }

    endInsertRows();
    return true;
}

//---------------------------------------------------------------------------
//  removeRows
//
//! Remove rows starting with the given row under parent from the model.
//! Reimplemented from QAbstractTableModel.
//
//! @param row the row number to begin removing rows
//! @param count the number of rows to remove
//! @param parent the parent index
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::removeRows (int row, int count, const QModelIndex&)
{
    beginRemoveRows (QModelIndex(), row, row + count - 1);

    for (int i = 0; i < count; ++i) {
        wordList.removeAt (row);
    }

    endRemoveRows();
    return true;
}

//---------------------------------------------------------------------------
//  setData
//
//! Set the role data for the item at index to value.  Reimplemented from
//! QAbstractTableModel.
//
//! @param index the index
//! @param value the value to set
//! @param role the role for which to set data
//! @return true if successful, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::setData (const QModelIndex& index, const QVariant& value, int
                         role)
{
    if (index.isValid() && (role == Qt::EditRole)) {
        wordList[index.row()].setWord (value.toString());
        emit dataChanged (index, index);
        return true;
    }
    else if (index.isValid() && (role == Qt::UserRole)) {
        wordList[index.row()].setType (WordType (value.toInt()));
        emit dataChanged (index, index);
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
//  sort
//
//! Sort the model by column in the given order.  Reimplemented from
//! QAbstractItemModel.  NOTE: Both parameters are currently ignored.
//
//! @param column the column to sort by
//! @param order the sort order
//---------------------------------------------------------------------------
void
WordTableModel::sort (int, Qt::SortOrder)
{
    if (MainSettings::getWordListSortByLength())
        qSort (wordList.begin(), wordList.end(), lengthLessThan);
    else
        qSort (wordList.begin(), wordList.end(), lessThan);

    emit dataChanged (index (0, 0),
                      index (wordList.size() - 1, DEFINITION_COLUMN));
}

//---------------------------------------------------------------------------
//  isFrontHook
//
//! Determine whether a word is a front hook.  A word is a front hook if its
//! first letter can be removed to form a valid word.
//
//! @param word the word, assumed to be upper case
//! @return true if the word is a front hook, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::isFrontHook (const QString& word) const
{
    return wordEngine->isAcceptable (word.right (word.length() - 1));
}

//---------------------------------------------------------------------------
//  isBackHook
//
//! Determine whether a word is a back hook.  A word is a back hook if its
//! last letter can be removed to form a valid word.
//
//! @param word the word, assumed to be upper case
//! @return true if the word is a back hook, false otherwise
//---------------------------------------------------------------------------
bool
WordTableModel::isBackHook (const QString& word) const
{
    return wordEngine->isAcceptable (word.left (word.length() - 1));
}

//---------------------------------------------------------------------------
//  getFrontHookLetters
//
//! Get a string of letters that can be added to the front of a word to make
//! other valid words.
//
//! @param word the word, assumed to be upper case
//! @return a string containing lower case letters representing front hooks
//---------------------------------------------------------------------------
QString
WordTableModel::getFrontHookLetters (const QString& word) const
{
    SearchSpec spec;
    SearchCondition condition;
    condition.type = SearchCondition::PatternMatch;
    condition.stringValue = "?" + word;
    spec.conditions.append (condition);

    // Put first letter of each word in a set, for alphabetical order
    QStringList words = wordEngine->search (spec, true);
    set<QChar> letters;
    QStringList::iterator it;
    for (it = words.begin(); it != words.end(); ++it)
        letters.insert ((*it).at (0).toLower());

    QString ret;
    set<QChar>::iterator sit;
    for (sit = letters.begin(); sit != letters.end(); ++sit)
        ret += *sit;
    return ret;
}

//---------------------------------------------------------------------------
//  getBackHookLetters
//
//! Get a string of letters that can be added to the back of a word to make
//! other valid words.
//
//! @param word the word, assumed to be upper case
//! @return a string containing lower case letters representing back hooks
//---------------------------------------------------------------------------
QString
WordTableModel::getBackHookLetters (const QString& word) const
{
    SearchSpec spec;
    SearchCondition condition;
    condition.type = SearchCondition::PatternMatch;
    condition.stringValue = word + "?";
    spec.conditions.append (condition);

    // Put first letter of each word in a set, for alphabetical order
    QStringList words = wordEngine->search (spec, true);
    set<QChar> letters;
    QStringList::iterator it;
    for (it = words.begin(); it != words.end(); ++it)
        letters.insert ((*it).at ((*it).length() - 1).toLower());

    QString ret;
    set<QChar>::iterator sit;
    for (sit = letters.begin(); sit != letters.end(); ++sit)
        ret += *sit;
    return ret;
}
